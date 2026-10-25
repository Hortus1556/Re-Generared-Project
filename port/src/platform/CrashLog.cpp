// A crash (an access violation or other fault, or an uncaught C++
// exception) writes what happened and the stack (each frame as
// module+offset): in debug mode (-d) through debugTrace (debug.log), else
// into crash.log in the save folder, so a player's crash can be traced too
// (llvm-symbolizer --obj=<exe> resolves the exe's offsets added to its
// image base, 0x140000000; a dist exe is stripped: the same build's
// unstripped exe in port/build).
#include <cstdarg>
#include <cstdio>

#include "platform/Platform.h"

#include <cstdlib>
#include <exception>
#include <string>
#include <typeinfo>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace platform {

namespace {

// To debug.log in debug mode, else to crash.log (made at the first line).
void crashTrace(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (traceEnabled()) {
        debugTrace("%s", buf);
        return;
    }
    static FILE* f = std::fopen((prefPath() + "crash.log").c_str(), "w");
    if (!f) return;
    std::fputs(buf, f);
    std::fputc('\n', f);
    std::fflush(f);
}

void logStack() {
    void* frames[64];
    USHORT n = RtlCaptureStackBackTrace(0, 64, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        HMODULE mod = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(frames[i]), &mod)) {
            GetModuleFileNameA(mod, name, MAX_PATH);
        }
        const char* base = name;
        for (const char* p = name; *p; ++p)
            if (*p == '\\' || *p == '/') base = p + 1;
        crashTrace("  #%u %s+0x%llx", static_cast<unsigned>(i), base,
                   static_cast<unsigned long long>(reinterpret_cast<char*>(frames[i]) - reinterpret_cast<char*>(mod)));
    }
}

// An uncaught C++ exception (std::terminate): its type and message.
void logTerminate() {
    std::string what = "?", type = "?";
    if (std::exception_ptr e = std::current_exception()) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& x) {
            what = x.what();
            type = typeid(x).name();
        } catch (...) {
        }
    }
    crashTrace("crash: terminate (%s: %s)", type.c_str(), what.c_str());
    logStack();
    std::abort();
}

LONG WINAPI logCrash(EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD* r = info->ExceptionRecord;
    crashTrace("crash: exception 0x%08lx at %p (accessing %p)", static_cast<unsigned long>(r->ExceptionCode),
               r->ExceptionAddress, r->NumberParameters > 1 ? reinterpret_cast<void*>(r->ExceptionInformation[1]) : nullptr);
    logStack();
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void installCrashLog() {
    SetUnhandledExceptionFilter(logCrash);
    std::set_terminate(logTerminate);
}

}  // namespace platform

#else

namespace platform {
void installCrashLog() {}
}  // namespace platform

#endif
