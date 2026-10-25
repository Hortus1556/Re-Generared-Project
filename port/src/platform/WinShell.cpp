#include "platform/WinShell.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>
// clang-format off
#include <initguid.h>
#include <knownfolders.h>
#include <shlobj.h>
// clang-format on

namespace platform {

std::string savedGamesFolder(const std::string& game) {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_CREATE, nullptr, &path))) return {};
    std::wstring name(game.size(), L'\0');
    name.resize(MultiByteToWideChar(CP_UTF8, 0, game.data(), static_cast<int>(game.size()), name.data(),
                                    static_cast<int>(name.size())));
    std::wstring dir = std::wstring(path) + L"\\" + name;
    CoTaskMemFree(path);
    CreateDirectoryW(dir.c_str(), nullptr);
    int n = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string utf8(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, utf8.data(), n, nullptr, nullptr);
    return utf8 + "\\";
}

void useExeIcon(void* hwnd) {
    HINSTANCE self = GetModuleHandleW(nullptr);
    auto load = [&](int size) {
        return static_cast<HICON>(LoadImageW(self, MAKEINTRESOURCEW(1), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    };
    HWND w = static_cast<HWND>(hwnd);
    if (HICON big = load(GetSystemMetrics(SM_CXICON))) SendMessageW(w, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    if (HICON small = load(GetSystemMetrics(SM_CXSMICON)))
        SendMessageW(w, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
}

void heapUse(size_t& processHeap, size_t& otherHeaps) {
    processHeap = otherHeaps = 0;
    HANDLE heaps[256];
    DWORD n = GetProcessHeaps(256, heaps);
    for (DWORD i = 0; i < n && i < 256; ++i) {
        HEAP_SUMMARY s{};
        s.cb = sizeof s;
        if (!HeapSummary(heaps[i], 0, &s)) continue;
        (heaps[i] == GetProcessHeap() ? processHeap : otherHeaps) += s.cbAllocated;
    }
}

std::string memoryMap() {
    size_t priv = 0, mapped = 0, image = 0;
    std::map<void*, size_t> bases;
    std::map<void*, unsigned long> protects;
    MEMORY_BASIC_INFORMATION m;
    for (char* at = nullptr; VirtualQuery(at, &m, sizeof m) == sizeof m; at = static_cast<char*>(m.BaseAddress) + m.RegionSize) {
        if (m.State != MEM_COMMIT) continue;
        if (m.Type == MEM_PRIVATE) {
            priv += m.RegionSize;
            bases[m.AllocationBase] += m.RegionSize;
            protects[m.AllocationBase] |= m.Protect;
        } else if (m.Type == MEM_MAPPED) {
            mapped += m.RegionSize;
        } else if (m.Type == MEM_IMAGE && (m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) {
            image += m.RegionSize;
        }
    }
    // Which of them are heaps' regions.
    std::map<void*, bool> heapBases;
    std::string bigBusy;  // (heap blocks of 1 MB and more)
    size_t heapCommitted = 0;
    HANDLE heaps[256];
    DWORD nh = GetProcessHeaps(256, heaps);
    for (DWORD i = 0; i < nh && i < 256; ++i) {
        HEAP_SUMMARY hs{};
        hs.cb = sizeof hs;
        if (HeapSummary(heaps[i], 0, &hs)) heapCommitted += hs.cbCommitted;
        if (!HeapLock(heaps[i])) continue;
        PROCESS_HEAP_ENTRY e{};
        while (HeapWalk(heaps[i], &e))
            if (e.wFlags & PROCESS_HEAP_REGION) heapBases[e.lpData] = true;
            else if ((e.wFlags & PROCESS_HEAP_ENTRY_BUSY) && e.cbData >= (1 << 20) && (bigBusy += " " + std::to_string(e.cbData >> 20), true)) {
                MEMORY_BASIC_INFORMATION q;
                if (VirtualQuery(e.lpData, &q, sizeof q)) heapBases[q.AllocationBase] = true;
            }
        HeapUnlock(heaps[i]);
    }
    size_t inHeaps = 0;
    for (auto& [b, n] : bases)
        if (heapBases.count(b)) inHeaps += n;
    std::vector<std::pair<size_t, void*>> big;
    for (auto& [b, n] : bases)
        if (!heapBases.count(b)) big.push_back({n, b});
    std::sort(big.rbegin(), big.rend());
    std::map<size_t, int> sizes;  // MB -> how many allocations
    for (auto& [n, b] : big) ++sizes[n >> 20];
    std::string out = "private " + std::to_string(priv >> 20) + " MB in " + std::to_string(bases.size()) +
                      " allocations, mapped " + std::to_string(mapped >> 20) + " MB, image rw " +
                      std::to_string(image >> 20) + " MB; heaps committed " + std::to_string(heapCommitted >> 20) +
                      " MB, private in heap regions " + std::to_string(inHeaps >> 20) + " MB; biggest others";
    for (size_t i = 0; i < big.size() && i < 8; ++i) {
        char hex[16];
        std::snprintf(hex, sizeof hex, "%lx", protects[big[i].second]);
        out += " " + std::to_string(big[i].first >> 20) + "MB(" + hex + ")";
    }
    size_t small = 0;
    for (auto& [n, b] : big)
        if (n < (1 << 20)) small += n;
    out += "; under 1 MB each: " + std::to_string(small >> 20) + " MB; heap blocks over 1 MB:" + bigBusy;
    return out;
}

bool cursorInClient(void* hwnd, float& x, float& y, float& width, float& height) {
    POINT p;
    RECT r;
    if (!GetCursorPos(&p) || !ScreenToClient(static_cast<HWND>(hwnd), &p) || !GetClientRect(static_cast<HWND>(hwnd), &r))
        return false;
    x = static_cast<float>(p.x);
    y = static_cast<float>(p.y);
    width = static_cast<float>(r.right - r.left);
    height = static_cast<float>(r.bottom - r.top);
    return width > 0 && height > 0;
}

}  // namespace platform

#else

// Elsewhere (Android): no Saved Games folder (the save goes where SDL says),
// no exe icon.
namespace platform {
std::string savedGamesFolder(const std::string&) { return std::string(); }
void useExeIcon(void*) {}
void heapUse(size_t& processHeap, size_t& otherHeaps) { processHeap = otherHeaps = 0; }
std::string memoryMap() { return std::string(); }
bool cursorInClient(void*, float&, float&, float&, float&) { return false; }
}  // namespace platform

#endif
