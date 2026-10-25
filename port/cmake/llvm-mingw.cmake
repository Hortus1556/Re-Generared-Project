# Toolchain file: build for x86_64 Windows with the llvm-mingw toolchain
# downloaded into <workspace>/tools (see docs/port.md).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

get_filename_component(_ws "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
file(GLOB _llvm_mingw "${_ws}/tools/llvm-mingw-*-ucrt-x86_64")
if(NOT _llvm_mingw)
  message(FATAL_ERROR "llvm-mingw not found under ${_ws}/tools")
endif()

set(CMAKE_C_COMPILER   "${_llvm_mingw}/bin/x86_64-w64-mingw32-clang.exe")
set(CMAKE_CXX_COMPILER "${_llvm_mingw}/bin/x86_64-w64-mingw32-clang++.exe")
set(CMAKE_RC_COMPILER  "${_llvm_mingw}/bin/x86_64-w64-mingw32-windres.exe")

file(GLOB _sdl3 "${_ws}/tools/SDL3-*/x86_64-w64-mingw32")
list(APPEND CMAKE_PREFIX_PATH ${_sdl3})
