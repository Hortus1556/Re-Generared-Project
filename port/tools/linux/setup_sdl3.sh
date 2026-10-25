#!/bin/bash
# One-time, idempotent: builds SDL3 for Linux desktop (X11, ALSA,
# PulseAudio; no Wayland, Vulkan, pipewire or camera -- the engine needs
# none of those, and the Wayland protocol headers available without
# installing anything (see below) are older than this SDL3 expects).
#
# Needs no sudo: the -dev packages it needs (X11/EGL/GL/ALSA/PulseAudio
# headers -- not already present in a minimal WSL/server image) are
# fetched with `apt-get download` (no root required, unlike `apt
# install`) and extracted with `dpkg-deb -x` into ~/linuxdeps/root,
# never touching the system's own paths.
#
# Safe to re-run: skips straight to the end if already done.
set -e

DEPS=~/linuxdeps/root
SDL3_PREFIX=~/linuxdeps/sdl3-install
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(cd "$HERE/../../../tools/sdl3-src" && echo "$PWD")/$(ls "$HERE/../../../tools/sdl3-src" | grep ^SDL3- | head -1)"

if [ -f "$SDL3_PREFIX/lib/libSDL3.a" ]; then
  echo "SDL3 for Linux already built at $SDL3_PREFIX"
  exit 0
fi

echo "=== fetching dev headers (no sudo: apt-get download + dpkg-deb -x) ==="
mkdir -p ~/linuxdeps/pkgs "$DEPS"
cd ~/linuxdeps/pkgs
TOP="libgl-dev libegl-dev libx11-dev libxext-dev libasound2-dev libxkbcommon-dev \
libxrandr-dev libxcursor-dev libxinerama-dev libxi-dev libpulse-dev libxfixes-dev \
libxss-dev"
ALL=$(apt-cache depends --recurse --no-recommends --no-suggests --no-conflicts \
  --no-breaks --no-replaces --no-enhances $TOP 2>/dev/null | grep -E '^\w' | sort -u)
apt-get download $ALL
for f in *.deb; do dpkg-deb -x "$f" "$DEPS" || true; done

echo "=== building SDL3 (X11 only -- no Wayland: the available wayland-protocols"
echo "    package is older than this SDL3 expects; no Vulkan/pipewire/camera:"
echo "    the engine doesn't use them) ==="
export PKG_CONFIG_PATH="$DEPS/usr/lib/x86_64-linux-gnu/pkgconfig:$DEPS/usr/share/pkgconfig"
BUILD=~/linuxdeps/sdl3-build
rm -rf "$BUILD" "$SDL3_PREFIX"
cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$SDL3_PREFIX" \
  -DSDL_STATIC=ON -DSDL_SHARED=OFF -DSDL_TEST_LIBRARY=OFF \
  -DSDL_PIPEWIRE=OFF -DSDL_CAMERA=OFF -DSDL_VULKAN=OFF -DSDL_WAYLAND=OFF \
  -DCMAKE_C_FLAGS="-I$DEPS/usr/include -I$DEPS/usr/include/x86_64-linux-gnu" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$DEPS/usr/lib/x86_64-linux-gnu"
cmake --build "$BUILD" -j"$(nproc)"
cmake --install "$BUILD"
echo "SDL3 for Linux installed at $SDL3_PREFIX"
