// Shared texture pages small art is packed in, so that what a frame draws
// from them goes to the GPU in a few draws instead of one per piece (a
// frame of the Flash games was some 800 draws, each with its own texture:
// what a phone's driver, OpenGL ES 1.1 emulated, does worst).
//
// Each slot holds its texels with a ring of one transparent texel around
// them, so drawing a slot never filters in its neighbours'. Slots are let
// go as textures are (releaseSlot: once no recorded frame can still draw
// them), and a page goes when it has none left.
#pragma once

#include <string>
#include <vector>

#include "platform/GL.h"

namespace platform {

struct AtlasSlot {
    GLuint page = 0;      // 0: none
    int pageSide = 0;     // the page's side, texels
    int x = 0, y = 0;     // its block's corner in the page (the texels at x + 1, y + 1)
    int width = 0, height = 0;  // the texels'
};

constexpr int kAtlasLargest = 256;  // art up to this many texels a side goes in pages

// A slot for width x height texels, their texels written (premultiplied
// BGRA in kTexelFormat; platform::setTexels' rules), or false (too big).
bool atlasPut(int width, int height, const std::vector<unsigned char>& texels, bool drawnNow, AtlasSlot& slot);
// Lets the slot go once no recorded frame can still draw it.
void releaseSlot(const AtlasSlot& slot);
// The pages' bytes, and the bytes in them no slot holds (memory budgets).
size_t atlasBytes();
// How full the page is, 0..1 (eviction: what leaves an emptying page lets it go).
float atlasPageFill(GLuint page);
size_t atlasUnusedBytes();
// (debug: memory) pages, and the share of them in use
std::string atlasStats();

// (DrawList, when a released slot can go)
void atlasFree(const AtlasSlot& slot);

}  // namespace platform
