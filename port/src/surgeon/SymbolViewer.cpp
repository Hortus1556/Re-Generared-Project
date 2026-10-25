#include "surgeon/SymbolViewer.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <cstdlib>

#include "surgeon/SurgeonGame.h"

SymbolViewer* SymbolViewer::fromEnvironment(SurgeonGame& game) {
    const char* s = platform::debugMode() ? std::getenv("AS_VECTOR_SYMBOL") : nullptr;
    if (!s) return nullptr;
    auto* v = new SymbolViewer(game);
    v->symbol = game.art().symbol(s);
    if (const char* f = std::getenv("AS_VECTOR_FRAME")) v->frame = std::atoi(f);
    if (const char* at = std::getenv("AS_VECTOR_AT")) std::sscanf(at, "%f,%f", &v->x, &v->y);
    if (const char* sc = std::getenv("AS_VECTOR_SCALE")) v->scale = static_cast<float>(std::atof(sc));
    if (const char* a = std::getenv("AS_VECTOR_TREE")) game.art().dumpTree(v->symbol, v->frame, std::atol(a));
    if (std::getenv("AS_VECTOR_BOUNDS")) {
        // The symbol's bounds over all its frames (its own pixels), and where
        // the main timeline places it (twips matrix), for comparing with
        // other renderers' frame images.
        VectorArt& art = game.art();
        flash::Rect u{1, 1, 0, 0};
        for (int f = 1; f <= art.frameCount(v->symbol); ++f) {
            flash::Rect b = art.bounds(v->symbol, f, 0, VectorTransform{});
            if (b.empty()) continue;
            u = u.empty() ? b : flash::Rect{std::min(u.xMin, b.xMin), std::min(u.yMin, b.yMin), std::max(u.xMax, b.xMax),
                                            std::max(u.yMax, b.yMax)};
        }
        std::fprintf(stderr, "bounds %.2f %.2f %.2f %.2f\n", u.xMin, u.yMin, u.xMax, u.yMax);
        for (int f = 1; f <= art.frameCount(0); ++f) {
            VectorTransform t;
            if (art.placement(0, f, "#" + std::to_string(v->symbol), t)) {
                std::fprintf(stderr, "placed on main frame %d: %.4f %.4f %.4f %.4f %.1f %.1f\n", f, t.base.a, t.base.b,
                             t.base.c, t.base.d, t.base.tx, t.base.ty);
                break;
            }
        }
    }
    return v;
}

void SymbolViewer::draw() {
    VectorTransform t;
    t.x = x;
    t.y = y;
    t.scaleX = t.scaleY = scale;
    game.art().draw(symbol, frame, age, t, serial());
}
