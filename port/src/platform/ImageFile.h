// Decoding a picture file into pixels, for every game's textures.
//
// PNG and JPEG go through stb_image. PVR textures (PowerVR's container,
// versions 2 and 3: PVRTC at 2 or 4 bits a pixel, and the uncompressed
// formats cocos2d-x read) are decoded on the CPU, as desktop GL has no PVRTC;
// a CCZ file (cocos2d's zlib wrapper, AS3's .pvr.ccz sheets) is inflated
// first. Only a texture's first level is read (mipmaps are not used).
#pragma once

#include <cstddef>
#include <vector>

namespace platform {

// The file's pixels as RGBA, rows top to bottom (a PVR stored flipped is
// turned right way up). Straight alpha, unless `premultiplied` is given: then
// a PVR is left as stored and *premultiplied says whether it is so (a v3
// file's flag; for a v2 one, which has none, false: the game says, as
// cocos2d's CCTexture2D::PVRImagesHavePremultipliedAlpha). A PNG or JPEG is
// always straight. False if the file is none of the formats above or is
// damaged.
bool decodeImage(const unsigned char* bytes, size_t size, int& width, int& height,
                 std::vector<unsigned char>& rgba, bool* premultiplied = nullptr);
// Whether the bytes are a PVR texture (plain or in a CCZ).
bool isPvr(const unsigned char* bytes, size_t size);

}  // namespace platform
