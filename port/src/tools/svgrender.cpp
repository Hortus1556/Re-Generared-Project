// svgrender <in.svg> <out.png> <scale> [element-id]: renders an SVG at
// `scale` times its size with lunasvg, the renderer the game uses for vector
// art, so the asset tools (tools/xmas_flash.py, tools/xmas_intro.py) can check
// what the game will show. With an element id, that element (a hidden frame
// group of an animation) is shown first. Prints the size and the time taken.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "lunasvg.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main(int argc, char** argv) {
    if (argc != 4 && argc != 5) {
        std::fprintf(stderr, "usage: svgrender <in.svg> <out.png> <scale> [element-id]\n");
        return 2;
    }
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    auto doc = lunasvg::Document::loadFromFile(argv[1]);
    if (!doc) {
        std::fprintf(stderr, "cannot load %s\n", argv[1]);
        return 1;
    }
    auto t1 = clock::now();
    if (argc == 5) {
        lunasvg::Element e = doc->getElementById(argv[4]);
        if (!e) {
            std::fprintf(stderr, "no element %s\n", argv[4]);
            return 1;
        }
        e.setAttribute("display", "inline");
    }
    float scale = static_cast<float>(std::atof(argv[3]));
    int w = static_cast<int>(doc->width() * scale + 0.5f), h = static_cast<int>(doc->height() * scale + 0.5f);
    lunasvg::Bitmap bitmap = doc->renderToBitmap(w, h, 0x00000000);
    auto t2 = clock::now();
    if (bitmap.isNull()) {
        std::fprintf(stderr, "cannot render %s\n", argv[1]);
        return 1;
    }
    bitmap.convertToRGBA();
    if (!stbi_write_png(argv[2], bitmap.width(), bitmap.height(), 4, bitmap.data(), bitmap.stride())) return 1;
    auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
    std::printf("%d %d load %.0f ms render %.0f ms\n", bitmap.width(), bitmap.height(), ms(t0, t1), ms(t1, t2));
    return 0;
}
