#!/bin/bash
# Builds the given dist targets natively for Linux (X11 desktop), run
# from inside WSL (or any Linux shell) by build_all.py's build_linux().
#
# usage: build.sh <target> [<target> ...]
#   e.g. build.sh AmateurSurgeon_dist AmateurSurgeonXmas_dist
#
# AS2's and AS3's AAC music and movies won't play yet on this platform
# (platform/MovieDecoder.cpp has no Linux decoder) -- AS1 and Christmas
# need neither and are unaffected. See docs/port.md.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$HERE/setup_sdl3.sh"

DEPS=~/linuxdeps/root
PROJ="$(cd "$HERE/../../.." && pwd)"
export PKG_CONFIG_PATH="$DEPS/usr/lib/x86_64-linux-gnu/pkgconfig:$DEPS/usr/share/pkgconfig"

cmake -S "$PROJ/port" -B "$PROJ/port/build-linux" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=~/linuxdeps/sdl3-install \
  -DCMAKE_C_FLAGS="-I$DEPS/usr/include -I$DEPS/usr/include/x86_64-linux-gnu" \
  -DCMAKE_CXX_FLAGS="-I$DEPS/usr/include -I$DEPS/usr/include/x86_64-linux-gnu" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$DEPS/usr/lib/x86_64-linux-gnu"
cmake --build "$PROJ/port/build-linux" -j"$(nproc)" --target "$@"
