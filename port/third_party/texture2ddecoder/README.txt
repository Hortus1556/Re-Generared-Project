PVRTC decoder from texture2ddecoder 1.0.6 (https://github.com/K0lb3/texture2ddecoder, MIT, see LICENSE),
used by the engine to read PVR textures (AS3's .pvr.ccz sheets).
Changed: decode_pvrtc reads the blocks and decodes their rows on several threads, and the 4 bpp blocks weigh their neighbours along x then y (the output is the same, bit for bit).
