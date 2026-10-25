// The few Windows shell services the port uses directly.
#pragma once

#include <string>

namespace platform {

// "<user>\Saved Games\<game>\" (UTF-8, with a trailing separator),
// created if missing; empty if Windows cannot say where Saved Games is.
std::string savedGamesFolder(const std::string& game);

// Gives the window (an HWND) the exe's own icon (resource 1), large and
// small, for its title bar and the taskbar.
void useExeIcon(void* hwnd);

// (debug: memory) bytes allocated in the process heap (malloc/new) and in
// the process's other heaps (drivers', libraries'); 0 elsewhere.
void heapUse(size_t& processHeap, size_t& otherHeaps);
// (debug: memory) the address space's committed memory: private, mapped,
// image, and the biggest private allocations with the module they lie in.
std::string memoryMap();

// Where the mouse pointer is in the window's client area (an HWND), and the
// area's size, in the same units (whatever the process's DPI awareness):
// read from the system at once, from any thread. False elsewhere.
bool cursorInClient(void* hwnd, float& x, float& y, float& width, float& height);

}  // namespace platform
