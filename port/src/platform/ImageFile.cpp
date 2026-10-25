#include "platform/ImageFile.h"

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

#include "stb_image.h"
#include "texture2ddecoder/pvrtc.h"

namespace platform {
namespace {

uint32_t le32(const unsigned char* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }
uint32_t be32(const unsigned char* p) { return static_cast<uint32_t>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
uint64_t le64(const unsigned char* p) { return le32(p) | static_cast<uint64_t>(le32(p + 4)) << 32; }

// The pixel formats a PVR can hold, as cocos2d-x 2.1 named them (CCTexturePVR.cpp).
enum class Pvr { RGBA4444, RGBA5551, RGBA8888, RGB565, RGB888, I8, AI88, PVRTC2, PVRTC4, BGRA8888, A8, None };

Pvr pvr2Format(uint32_t f) {
    switch (f & 0xff) {
        case 0x10: return Pvr::RGBA4444;
        case 0x11: return Pvr::RGBA5551;
        case 0x12: return Pvr::RGBA8888;
        case 0x13: return Pvr::RGB565;
        case 0x15: return Pvr::RGB888;
        case 0x16: return Pvr::I8;
        case 0x17: return Pvr::AI88;
        case 0x18: return Pvr::PVRTC2;
        case 0x19: return Pvr::PVRTC4;
        case 0x1A: return Pvr::BGRA8888;
        case 0x1B: return Pvr::A8;
        default: return Pvr::None;
    }
}

Pvr pvr3Format(uint64_t f) {
    switch (f) {
        case 0: case 1: return Pvr::PVRTC2;
        case 2: case 3: return Pvr::PVRTC4;
        case 0x0808080861726762ULL: return Pvr::BGRA8888;
        case 0x0808080861626772ULL: return Pvr::RGBA8888;
        case 0x0404040461626772ULL: return Pvr::RGBA4444;
        case 0x0105050561626772ULL: return Pvr::RGBA5551;
        case 0x0005060500626772ULL: return Pvr::RGB565;
        case 0x0008080800626772ULL: return Pvr::RGB888;
        case 0x0000000800000061ULL: return Pvr::A8;
        case 0x000000080000006cULL: return Pvr::I8;
        case 0x000008080000616cULL: return Pvr::AI88;
        default: return Pvr::None;
    }
}

size_t pvrBytes(Pvr f, int w, int h) {
    size_t n = static_cast<size_t>(w) * h;
    switch (f) {
        case Pvr::RGBA8888: case Pvr::BGRA8888: return n * 4;
        case Pvr::RGB888: return n * 3;
        case Pvr::RGBA4444: case Pvr::RGBA5551: case Pvr::RGB565: case Pvr::AI88: return n * 2;
        case Pvr::I8: case Pvr::A8: return n;
        // PVRTC: blocks of 8x4 (2 bpp) or 4x4 (4 bpp) pixels, at least 2x2 blocks
        case Pvr::PVRTC2: return static_cast<size_t>(w < 16 ? 16 : w) * (h < 8 ? 8 : h) / 4;
        case Pvr::PVRTC4: return static_cast<size_t>(w < 8 ? 8 : w) * (h < 8 ? 8 : h) / 2;
        default: return 0;
    }
}

void expand(Pvr f, const unsigned char* s, int w, int h, unsigned char* d) {
    size_t n = static_cast<size_t>(w) * h;
    if (f == Pvr::PVRTC2 || f == Pvr::PVRTC4) {
        // decoded in place as b | g << 8 | r << 16 | a << 24, then red and
        // blue swapped (on several threads for a big one, as the decoder)
        decode_pvrtc(s, w, h, reinterpret_cast<uint32_t*>(d), f == Pvr::PVRTC2 ? 1 : 0);
        auto swap = [d](size_t from, size_t to) {
            for (unsigned char* p = d + from * 4; p < d + to * 4; p += 4) std::swap(p[0], p[2]);
        };
        unsigned threads = n >= (1u << 20) ? std::min(8u, std::max(1u, std::thread::hardware_concurrency())) : 1;
        std::vector<std::thread> pool;
        size_t per = (n + threads - 1) / threads;
        for (unsigned t = 1; t < threads; ++t) pool.emplace_back(swap, std::min(n, per * t), std::min(n, per * (t + 1)));
        swap(0, std::min(n, per));
        for (std::thread& t : pool) t.join();
        return;
    }
    for (size_t i = 0; i < n; ++i, d += 4) {
        unsigned v = 0;
        switch (f) {
            case Pvr::RGBA8888: std::memcpy(d, s + i * 4, 4); break;
            case Pvr::BGRA8888: d[0] = s[i * 4 + 2]; d[1] = s[i * 4 + 1]; d[2] = s[i * 4]; d[3] = s[i * 4 + 3]; break;
            case Pvr::RGB888: d[0] = s[i * 3]; d[1] = s[i * 3 + 1]; d[2] = s[i * 3 + 2]; d[3] = 255; break;
            case Pvr::RGBA4444:
                v = s[i * 2] | s[i * 2 + 1] << 8;
                d[0] = (v >> 12 & 15) * 17; d[1] = (v >> 8 & 15) * 17; d[2] = (v >> 4 & 15) * 17; d[3] = (v & 15) * 17;
                break;
            case Pvr::RGBA5551:
                v = s[i * 2] | s[i * 2 + 1] << 8;
                d[0] = (v >> 11 & 31) * 255 / 31; d[1] = (v >> 6 & 31) * 255 / 31; d[2] = (v >> 1 & 31) * 255 / 31;
                d[3] = (v & 1) * 255;
                break;
            case Pvr::RGB565:
                v = s[i * 2] | s[i * 2 + 1] << 8;
                d[0] = (v >> 11 & 31) * 255 / 31; d[1] = (v >> 5 & 63) * 255 / 63; d[2] = (v & 31) * 255 / 31; d[3] = 255;
                break;
            case Pvr::I8: d[0] = d[1] = d[2] = s[i]; d[3] = 255; break;
            case Pvr::AI88: d[0] = d[1] = d[2] = s[i * 2]; d[3] = s[i * 2 + 1]; break;
            // GL_ALPHA: the colour comes from what it is drawn with
            case Pvr::A8: d[0] = d[1] = d[2] = 255; d[3] = s[i]; break;
            default: break;
        }
    }
}

bool decodePvr(const unsigned char* p, size_t size, int& w, int& h, std::vector<unsigned char>& rgba, bool* keepPremultiplied) {
    Pvr f = Pvr::None;
    size_t data = 0;
    bool flipped = false, premultiplied = false;
    if (size >= 52 && le32(p) == 0x03525650) {  // "PVR\3"
        uint32_t flags = le32(p + 4);
        f = pvr3Format(le64(p + 8));
        h = static_cast<int>(le32(p + 24));
        w = static_cast<int>(le32(p + 28));
        data = 52 + le32(p + 48);
        premultiplied = flags & 2;
    } else if (size >= 52 && le32(p) == 52 && std::memcmp(p + 44, "PVR!", 4) == 0) {
        h = static_cast<int>(le32(p + 4));
        w = static_cast<int>(le32(p + 8));
        uint32_t flags = le32(p + 16);
        f = pvr2Format(flags);
        flipped = flags & (1u << 16);
        data = 52;
    } else {
        return false;
    }
    if (f == Pvr::None || w <= 0 || h <= 0 || data + pvrBytes(f, w, h) > size) return false;
    rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    expand(f, p + data, w, h, rgba.data());
    if (flipped) {
        size_t row = static_cast<size_t>(w) * 4;
        std::vector<unsigned char> tmp(row);
        for (int y = 0; y < h / 2; ++y) {
            unsigned char* a = rgba.data() + y * row;
            unsigned char* b = rgba.data() + (h - 1 - y) * row;
            std::memcpy(tmp.data(), a, row);
            std::memcpy(a, b, row);
            std::memcpy(b, tmp.data(), row);
        }
    }
    if (keepPremultiplied) {
        *keepPremultiplied = premultiplied;
    } else if (premultiplied) {
        for (size_t i = 0; i < rgba.size(); i += 4) {
            unsigned a = rgba[i + 3];
            if (!a) continue;
            for (int c = 0; c < 3; ++c) {
                unsigned v = (rgba[i + c] * 255u + a / 2) / a;
                rgba[i + c] = static_cast<unsigned char>(v > 255 ? 255 : v);
            }
        }
    }
    return true;
}

}  // namespace

bool isPvr(const unsigned char* bytes, size_t size) {
    if (size >= 4 && std::memcmp(bytes, "CCZ!", 4) == 0) return true;
    return size >= 52 && (le32(bytes) == 0x03525650 || (le32(bytes) == 52 && std::memcmp(bytes + 44, "PVR!", 4) == 0));
}

bool decodeImage(const unsigned char* bytes, size_t size, int& width, int& height,
                 std::vector<unsigned char>& rgba, bool* premultiplied) {
    if (premultiplied) *premultiplied = false;
    // CCZ: "CCZ!", compression type (0 zlib) and version (BE 16 bits), a
    // reserved word, the inflated length (BE 32 bits), then the zlib stream
    if (size >= 16 && std::memcmp(bytes, "CCZ!", 4) == 0) {
        if ((bytes[4] << 8 | bytes[5]) != 0) return false;
        uint32_t len = be32(bytes + 12);
        std::vector<unsigned char> plain(len);
        int got = stbi_zlib_decode_buffer(reinterpret_cast<char*>(plain.data()), static_cast<int>(len),
                                          reinterpret_cast<const char*>(bytes + 16), static_cast<int>(size - 16));
        if (got != static_cast<int>(len)) return false;
        return decodePvr(plain.data(), plain.size(), width, height, rgba, premultiplied);
    }
    if (decodePvr(bytes, size, width, height, rgba, premultiplied)) return true;
    int n = 0;
    unsigned char* px = stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &n, 4);
    if (!px) return false;
    rgba.assign(px, px + static_cast<size_t>(width) * height * 4);
    stbi_image_free(px);
    return true;
}

}  // namespace platform
