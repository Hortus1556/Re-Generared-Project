// The game's log (0x1e0d4) and error log (0x1e13c): the engine's debug trace
// (-d; also debug.log).
#pragma once

#include "platform/Platform.h"

namespace mt {

template <class... Args>
void log(const char* format, Args... args) {
    platform::debugTrace(format, args...);
}

template <class... Args>
void logError(const char* format, Args... args) {
    platform::debugTrace(format, args...);
}

}  // namespace mt
