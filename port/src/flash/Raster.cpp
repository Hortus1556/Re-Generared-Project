#include "flash/Raster.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <optional>
#include <vector>

#include "platform/DrawList.h"
#include "platform/Platform.h"
#include "plutovg.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace flash {

namespace {

constexpr int kMaxSide = 4096;
constexpr int kPad = 2;  // texels around the art, so filtering sees transparency
constexpr int kMorphSteps = 64;

int levelFor(float scale) { return static_cast<int>(std::ceil(std::log2(std::max(scale, 1e-3f)) * 2.0f)); }
float scaleFor(int level) { return std::pow(2.0f, level / 2.0f); }

plutovg_color_t colour(const uint8_t* c) {
    return plutovg_color_t{c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, c[3] / 255.0f};
}

void buildPath(plutovg_path_t* path, const std::vector<PathOp>& ops) {
    for (const PathOp& o : ops) {
        switch (o.kind) {
        case PathOp::kMove: plutovg_path_move_to(path, o.x, o.y); break;
        case PathOp::kLine: plutovg_path_line_to(path, o.x, o.y); break;
        case PathOp::kCurve: plutovg_path_quad_to(path, o.cx, o.cy, o.x, o.y); break;
        }
    }
}

void setPaint(plutovg_canvas_t* canvas, const FillStyle& f, const uint8_t* fallback, const Swf* movie = nullptr) {
    if (f.type == FillStyle::kBitmap && movie) {
        // The bitmap, decoded for this rasterising (once a shape and size),
        // its pixels placed by the fill's matrix (the paint keeps it).
        auto b = movie->bitmaps.find(f.bitmap);
        if (b != movie->bitmaps.end() && !b->second.data.empty()) {
            plutovg_surface_t* image = plutovg_surface_load_from_image_data(
                b->second.data.data(), static_cast<int>(b->second.data.size()));
            if (image) {
                plutovg_matrix_t m{f.matrix.a, f.matrix.b, f.matrix.c, f.matrix.d, f.matrix.tx, f.matrix.ty};
                plutovg_canvas_set_texture(canvas, image, f.clipped ? PLUTOVG_TEXTURE_TYPE_PLAIN : PLUTOVG_TEXTURE_TYPE_TILED,
                                           1.0f, &m);
                plutovg_surface_destroy(image);
                return;
            }
        }
    }
    if (f.type == FillStyle::kSolid || f.stops.empty()) {
        plutovg_color_t c = colour(f.type == FillStyle::kSolid ? f.rgba : fallback);
        plutovg_canvas_set_color(canvas, &c);
        return;
    }
    std::vector<plutovg_gradient_stop_t> stops;
    for (const GradientStop& s : f.stops) stops.push_back({s.ratio, colour(s.rgba)});
    plutovg_matrix_t m{f.matrix.a, f.matrix.b, f.matrix.c, f.matrix.d, f.matrix.tx, f.matrix.ty};
    auto spread = f.spread == 1   ? PLUTOVG_SPREAD_METHOD_REFLECT
                  : f.spread == 2 ? PLUTOVG_SPREAD_METHOD_REPEAT
                                  : PLUTOVG_SPREAD_METHOD_PAD;
    if (f.type == FillStyle::kLinear)
        plutovg_canvas_set_linear_gradient(canvas, -16384, 0, 16384, 0, spread, stops.data(),
                                           static_cast<int>(stops.size()), &m);
    else
        plutovg_canvas_set_radial_gradient(canvas, 0, 0, 16384, f.focal * 16384, 0, 0, spread, stops.data(),
                                           static_cast<int>(stops.size()), &m);
}

// The shape's layers, under the canvas matrix (which maps twips to texels);
// `movie` has the bitmaps its bitmap fills show.
void drawShape(plutovg_canvas_t* canvas, const Shape& shape, float minLineTwips, const uint8_t* tint = nullptr,
               const Swf* movie = nullptr) {
    static const uint8_t black[4] = {0, 0, 0, 255};
    for (const ShapeLayer& layer : shape.layers) {
        plutovg_canvas_set_fill_rule(canvas, PLUTOVG_FILL_RULE_EVEN_ODD);
        for (const StyledPath& p : layer.fillPaths) {
            if (p.style < 0 || p.style >= static_cast<int>(layer.fills.size())) continue;
            plutovg_path_t* path = plutovg_path_create();
            buildPath(path, p.ops);
            if (tint) {
                plutovg_color_t c = colour(tint);
                plutovg_canvas_set_color(canvas, &c);
            } else {
                setPaint(canvas, layer.fills[p.style], black, movie);
            }
            plutovg_canvas_fill_path(canvas, path);
            plutovg_path_destroy(path);
        }
        for (const StyledPath& p : layer.linePaths) {
            if (p.style < 0 || p.style >= static_cast<int>(layer.lines.size())) continue;
            const LineStyle& l = layer.lines[p.style];
            plutovg_path_t* path = plutovg_path_create();
            buildPath(path, p.ops);
            if (l.hasFill) setPaint(canvas, l.fill, l.rgba);
            else {
                plutovg_color_t c = colour(l.rgba);
                plutovg_canvas_set_color(canvas, &c);
            }
            plutovg_canvas_set_line_width(canvas, std::max(l.width, minLineTwips));
            plutovg_canvas_set_line_cap(canvas, l.startCap == 1   ? PLUTOVG_LINE_CAP_BUTT
                                                : l.startCap == 2 ? PLUTOVG_LINE_CAP_SQUARE
                                                                  : PLUTOVG_LINE_CAP_ROUND);
            plutovg_canvas_set_line_join(canvas, l.join == 1   ? PLUTOVG_LINE_JOIN_BEVEL
                                                 : l.join == 2 ? PLUTOVG_LINE_JOIN_MITER
                                                               : PLUTOVG_LINE_JOIN_ROUND);
            plutovg_canvas_set_miter_limit(canvas, l.miterLimit);
            plutovg_canvas_stroke_path(canvas, path);
            plutovg_path_destroy(path);
        }
    }
}


uint64_t bakedKey(const ColorTransform* c) {
    if (!c) return 0;
    // Quantised steps, as scales are (levelFor): a baked colour that
    // animates (a damage flash's added red, fading every pass) re-rasterises
    // a few dozen times over the effect instead of once per pass, the same
    // trade a growing or shrinking clip already makes on its scale. The
    // drawn colour is exact whenever a step is freshly baked; between two
    // steps it is the nearer one, not interpolated.
    constexpr float kStep = 1.0f / 32;
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < 4; ++i)
        for (float v : {c->mult[i], c->add[i]}) {
            h ^= static_cast<uint64_t>(std::lround(v / kStep) + 100000);
            h *= 1099511628211ull;
        }
    return h;
}

// The colour transform applied to premultiplied BGRA texels.
void bake(plutovg_surface_t* s, const ColorTransform& c) {
    unsigned char* d = plutovg_surface_get_data(s);
    int w = plutovg_surface_get_width(s), h = plutovg_surface_get_height(s), stride = plutovg_surface_get_stride(s);
    for (int y = 0; y < h; ++y) {
        unsigned char* p = d + static_cast<size_t>(y) * stride;
        for (int x = 0; x < w; ++x, p += 4) {
            float a = p[3] / 255.0f;
            if (a <= 0) continue;
            float rgb[3] = {p[2] / 255.0f / a, p[1] / 255.0f / a, p[0] / 255.0f / a};
            float na = std::clamp(a * c.mult[3] + c.add[3] / 255.0f, 0.0f, 1.0f);
            for (int k = 0; k < 3; ++k) rgb[k] = std::clamp(rgb[k] * c.mult[k] + c.add[k] / 255.0f, 0.0f, 1.0f);
            p[2] = static_cast<unsigned char>(std::lround(rgb[0] * na * 255));
            p[1] = static_cast<unsigned char>(std::lround(rgb[1] * na * 255));
            p[0] = static_cast<unsigned char>(std::lround(rgb[2] * na * 255));
            p[3] = static_cast<unsigned char>(std::lround(na * 255));
        }
    }
}

// ---------------------------------------------------------------- filters

// A filter in texels, as baked into a texture.
struct Effect {
    Filter::Type type;
    int bx, by, passes;  // box widths (1: none) and how many times
    float strength;
    float rgba[4];       // 0..1
    int dx, dy;          // a shadow's offset
    bool inner, knockout;
};
using EffectList = std::vector<Effect>;

// A size in screen pixels, in steps an animated filter does not show (and
// whose textures are not made again for each frame of its tween).
float quantise(float px) {
    if (px < 4) return std::round(px * 2) / 2;
    if (px < 16) return std::round(px);
    return std::round(px / 2) * 2;
}

// The filters of `fx` for art drawn at `scale` screen pixels per Flash
// pixel, rasterised at `level`, and a key telling them apart (0: none that
// shows). The key is the filters' sizes on the screen, whatever the level:
// another level's texture with the same filters can stand in while one is
// made. `drop`: levels to rasterise below `level`, art under a blur needing
// less detail (half the texels across for 8 screen pixels of blur, a
// quarter for 16): the drawing and the blur cost far less.
EffectList effectsFor(const Effects* fx, float scale, int level, uint64_t& key, int& drop) {
    EffectList out;
    key = 0;
    drop = 0;
    if (!fx || !fx->filters) return out;
    float widest = 0;
    for (const Filter& f : *fx->filters)
        if (f.type == Filter::kBlur)
            widest = std::max(widest, quantise(std::max(f.blurX, f.blurY) * fx->screenPerStagePixel));
    drop = widest >= 16 ? 4 : widest >= 8 ? 2 : widest >= 4 ? 1 : 0;
    float texelsPerScreen = scaleFor(level - drop) / std::max(scale, 1e-3f);
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](long v) {
        h ^= static_cast<uint64_t>(v + 1000000);
        h *= 1099511628211ull;
    };
    auto texels = [&](float screen) { return static_cast<int>(std::lround(screen * texelsPerScreen)); };
    for (const Filter& f : *fx->filters) {
        float bx = quantise(f.blurX * fx->screenPerStagePixel), by = quantise(f.blurY * fx->screenPerStagePixel);
        float sx = 0, sy = 0;
        if (f.type == Filter::kDropShadow) {
            sx = quantise(std::cos(f.angle) * f.distance * fx->screenPerStagePixel);
            sy = quantise(std::sin(f.angle) * f.distance * fx->screenPerStagePixel);
        }
        Effect e{f.type, std::max(1, texels(bx)), std::max(1, texels(by)), std::clamp(f.passes, 1, 3),
                 std::round(f.strength * 10) / 10,
                 {f.rgba[0] / 255.0f, f.rgba[1] / 255.0f, f.rgba[2] / 255.0f, f.rgba[3] / 255.0f}, texels(sx), texels(sy),
                 f.inner, f.knockout};
        bool spread = e.bx > 1 || e.by > 1;
        if (f.type == Filter::kBlur && !spread) continue;
        if (f.type == Filter::kGlow && (!spread || e.strength <= 0 || e.rgba[3] <= 0)) continue;
        if (f.type == Filter::kDropShadow && ((!spread && !e.dx && !e.dy) || e.strength <= 0 || e.rgba[3] <= 0)) continue;
        if (f.type == Filter::kOther) continue;
        mix(e.type);
        mix(std::lround(bx * 2));
        mix(std::lround(by * 2));
        mix(e.passes);
        mix(std::lround(e.strength * 10));
        for (float c : e.rgba) mix(std::lround(c * 255));
        mix(std::lround(sx * 2));
        mix(std::lround(sy * 2));
        mix(e.inner * 2 + e.knockout);
        out.push_back(e);
    }
    if (!out.empty()) key = h | 1;
    return out;
}

// How far the filters reach past the art, in texels.
int effectsMargin(const EffectList& effects) {
    int m = 0;
    for (const Effect& e : effects)
        m += std::max(e.bx, e.by) / 2 * e.passes + 1 + std::max(std::abs(e.dx), std::abs(e.dy));
    return m;
}

// Flash's blur: a box `bx` wide across, then `by` high, `passes` times, on
// `channels` interleaved floats; outside the image is 0. Running sums: each
// texel costs the same whatever the box's size.
void boxBlur(std::vector<float>& img, int w, int h, int channels, int bx, int by, int passes) {
    std::vector<float> out(img.size());
    std::vector<float> sums;
    const size_t row = static_cast<size_t>(w) * channels;
    for (int pass = 0; pass < passes; ++pass) {
        if (bx > 1) {  // across, row by row
            int before = (bx - 1) / 2, after = bx / 2;
            float scale = 1.0f / bx;
            sums.assign(channels, 0.0f);
            for (int y = 0; y < h; ++y) {
                const float* in = &img[y * row];
                float* o = &out[y * row];
                std::fill(sums.begin(), sums.end(), 0.0f);
                for (int x = 0; x <= after && x < w; ++x)
                    for (int c = 0; c < channels; ++c) sums[c] += in[x * channels + c];
                for (int x = 0; x < w; ++x) {
                    for (int c = 0; c < channels; ++c) o[x * channels + c] = sums[c] * scale;
                    int gone = x - before, come = x + after + 1;
                    if (gone >= 0)
                        for (int c = 0; c < channels; ++c) sums[c] -= in[gone * channels + c];
                    if (come < w)
                        for (int c = 0; c < channels; ++c) sums[c] += in[come * channels + c];
                }
            }
            img.swap(out);
        }
        if (by > 1) {  // down, all columns at once
            int before = (by - 1) / 2, after = by / 2;
            float scale = 1.0f / by;
            sums.assign(row, 0.0f);
            for (int y = 0; y <= after && y < h; ++y)
                for (size_t i = 0; i < row; ++i) sums[i] += img[y * row + i];
            for (int y = 0; y < h; ++y) {
                float* o = &out[y * row];
                for (size_t i = 0; i < row; ++i) o[i] = sums[i] * scale;
                int gone = y - before, come = y + after + 1;
                if (gone >= 0)
                    for (size_t i = 0; i < row; ++i) sums[i] -= img[gone * row + i];
                if (come < h)
                    for (size_t i = 0; i < row; ++i) sums[i] += img[come * row + i];
            }
            img.swap(out);
        }
    }
}

// The filters applied, in order, to premultiplied BGRA texels.
void applyEffects(unsigned char* data, int stride, int w, int h, const EffectList& effects) {
    size_t n = static_cast<size_t>(w) * h;
    std::vector<float> img(n * 4);  // premultiplied r, g, b, a
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = data + static_cast<size_t>(y) * stride + x * 4;
            float* q = &img[(static_cast<size_t>(y) * w + x) * 4];
            q[0] = p[2] / 255.0f;
            q[1] = p[1] / 255.0f;
            q[2] = p[0] / 255.0f;
            q[3] = p[3] / 255.0f;
        }
    for (const Effect& e : effects) {
        if (e.type == Filter::kBlur) {
            boxBlur(img, w, h, 4, e.bx, e.by, e.passes);
            continue;
        }
        // Glow and drop shadow: the art's alpha blurred (and offset), in the
        // filter's colour, under the art (outer) or within it (inner).
        std::vector<float> spread(n, 0.0f);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                int sx = x - e.dx, sy = y - e.dy;
                if (sx >= 0 && sx < w && sy >= 0 && sy < h) spread[static_cast<size_t>(y) * w + x] = img[(static_cast<size_t>(sy) * w + sx) * 4 + 3];
            }
        boxBlur(spread, w, h, 1, e.bx, e.by, e.passes);
        for (size_t i = 0; i < n; ++i) {
            float* q = &img[i * 4];
            float sa = q[3];
            float g = e.inner ? std::clamp((1 - spread[i]) * e.strength, 0.0f, 1.0f) * sa
                              : std::clamp(spread[i] * e.strength, 0.0f, 1.0f);
            g *= e.rgba[3];
            float glow[4] = {e.rgba[0] * g, e.rgba[1] * g, e.rgba[2] * g, g};
            if (e.inner) {
                // within the art, over it
                for (int c = 0; c < 4; ++c) q[c] = e.knockout ? glow[c] : glow[c] + q[c] * (1 - g);
                if (!e.knockout) q[3] = sa;
            } else {
                // around the art, under it
                for (int c = 0; c < 4; ++c) q[c] = e.knockout ? glow[c] * (1 - sa) : q[c] + glow[c] * (1 - sa);
            }
        }
    }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char* p = data + static_cast<size_t>(y) * stride + x * 4;
            const float* q = &img[(static_cast<size_t>(y) * w + x) * 4];
            float a = std::clamp(q[3], 0.0f, 1.0f);
            auto byte = [&](float v) { return static_cast<unsigned char>(std::lround(std::clamp(v, 0.0f, a) * 255)); };
            p[2] = byte(q[0]);
            p[1] = byte(q[1]);
            p[0] = byte(q[2]);
            p[3] = static_cast<unsigned char>(std::lround(a * 255));
        }
}

// Rasterises `draw` (which paints in twips) over `bounds` at `scale` screen
// pixels per Flash pixel into premultiplied BGRA texels (any thread), with
// `effects` applied (the texels then reach past `bounds`).
// A texture side rounded up to one of eight sizes an octave (multiples of
// 16 below 128): art of about the same size gets textures of the same size,
// recycled (platform::recycleTexture) as art comes and goes. The rest is
// transparent texels beyond the art's right and bottom.
int sizeClass(int n) {
    int step = 16;
    while (step * 8 < n) step *= 2;
    return std::max(n, std::min(kMaxSide, (n + step - 1) / step * step));
}

// With `area`, only that tile of the art's grid (its texels from bounds'
// corner less kPad), with kOverlap texels of the art around it so that
// tiles drawn side by side filter seamlessly.
constexpr int kOverlap = 1;
void finish(Pixels& p);

Pixels rasterize(const Rect& bounds, float scale, const std::function<void(plutovg_canvas_t*, float)>& draw,
                 const ColorTransform* baked = nullptr, const EffectList* effects = nullptr,
                 const TileArea* area = nullptr) {
    Pixels p;
    if (bounds.empty()) return p;
    if (area && area->index == kWholeAt) {
        scale = area->scale;
        area = nullptr;
    } else if (area && area->index < 0) {  // (kPlain: the whole art, without its filters)
        area = nullptr;
        effects = nullptr;
    }
    float texelsPerTwip;
    if (area) {
        texelsPerTwip = area->scale / 20.0f;
        p.twipsPerTexel = 1.0f / texelsPerTwip;
        int w = area->x1 - area->x0, h = area->y1 - area->y0;
        p.width = sizeClass(w + 2 * kOverlap);
        p.height = sizeClass(h + 2 * kOverlap);
        p.x0 = bounds.xMin + (area->x0 - kPad - kOverlap) * p.twipsPerTexel;
        p.y0 = bounds.yMin + (area->y0 - kPad - kOverlap) * p.twipsPerTexel;
        p.u0 = static_cast<float>(kOverlap) / p.width;
        p.v0 = static_cast<float>(kOverlap) / p.height;
        p.u1 = static_cast<float>(kOverlap + w) / p.width;
        p.v1 = static_cast<float>(kOverlap + h) / p.height;
        p.tile = area->index;
    } else {
        texelsPerTwip = scale / 20.0f;
        float w = (bounds.xMax - bounds.xMin) * texelsPerTwip, h = (bounds.yMax - bounds.yMin) * texelsPerTwip;
        int pad = kPad + (effects ? effectsMargin(*effects) : 0);
        float shrink = std::min(1.0f, (kMaxSide - 2 * pad) / std::max(w, h));
        texelsPerTwip *= shrink;
        p.width = sizeClass(static_cast<int>(std::ceil((bounds.xMax - bounds.xMin) * texelsPerTwip)) + 2 * pad);
        p.height = sizeClass(static_cast<int>(std::ceil((bounds.yMax - bounds.yMin) * texelsPerTwip)) + 2 * pad);
        p.twipsPerTexel = 1.0f / texelsPerTwip;
        p.x0 = bounds.xMin - pad * p.twipsPerTexel;
        p.y0 = bounds.yMin - pad * p.twipsPerTexel;
    }
    plutovg_surface_t* surface = plutovg_surface_create(p.width, p.height);
    if (!surface) return Pixels{};
    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);
    plutovg_matrix_t m;
    plutovg_matrix_init_scale(&m, texelsPerTwip, texelsPerTwip);
    plutovg_matrix_translate(&m, -p.x0, -p.y0);
    plutovg_canvas_set_matrix(canvas, &m);
    draw(canvas, p.twipsPerTexel);  // hairlines: at least one texel wide
    plutovg_canvas_destroy(canvas);
    if (baked) bake(surface, *baked);
    if (effects && !effects->empty())
        applyEffects(plutovg_surface_get_data(surface), plutovg_surface_get_stride(surface), p.width, p.height, *effects);
    const unsigned char* d = plutovg_surface_get_data(surface);
    int stride = plutovg_surface_get_stride(surface);
    p.bgra.resize(static_cast<size_t>(p.width) * p.height * 4);
    for (int y = 0; y < p.height; ++y)
        std::memcpy(&p.bgra[static_cast<size_t>(y) * p.width * 4], d + static_cast<size_t>(y) * stride,
                    static_cast<size_t>(p.width) * 4);
    plutovg_surface_destroy(surface);
    finish(p);
    return p;
}

// The texels as they go to a texture: where they are drawn, a single
// texel if one colour all over (or none if empty), the platform's order.
void finish(Pixels& p) {
    p.drawn[0] = p.x0 + p.u0 * p.width * p.twipsPerTexel;
    p.drawn[1] = p.y0 + p.v0 * p.height * p.twipsPerTexel;
    p.drawn[2] = p.x0 + p.u1 * p.width * p.twipsPerTexel;
    p.drawn[3] = p.y0 + p.v1 * p.height * p.twipsPerTexel;
    // One colour all over (the inside of a big flat fill, or nothing at
    // all: a tile of a background, past a shape's edge): a single texel
    // stretched over its place, or no texture.
    uint32_t first;
    std::memcpy(&first, p.bgra.data(), 4);
    bool flat = true;
    for (size_t i = 4; flat && i < p.bgra.size(); i += 4) flat = std::memcmp(&first, &p.bgra[i], 4) == 0;
    if (flat) {
        if (!first) {
            p = Pixels{};
            return;
        }
        p.width = p.height = 1;
        p.bgra.resize(4);
        p.u0 = p.u1 = p.v0 = p.v1 = 0.5f;
    }
    platform::toTexelOrder(p.bgra.data(), static_cast<size_t>(p.width) * p.height);
}

// A GL texture for the texels, its texels still to upload (GL thread).
size_t gAllocatedTotal = 0;  // (debug: memory) texels ever allocated

Texture allocate(const Pixels& p) {
    Texture t;
    if (p.bgra.empty()) return t;
    t.width = p.width;
    t.height = p.height;
    t.x0 = p.drawn[0];
    t.y0 = p.drawn[1];
    t.x1 = p.drawn[2];
    t.y1 = p.drawn[3];
    t.u0 = p.u0;
    t.v0 = p.v0;
    t.u1 = p.u1;
    t.v1 = p.v1;
    t.tile = p.tile;
    if ((t.id = platform::reuseTexture(t.width, t.height))) return t;
    gAllocatedTotal += static_cast<size_t>(p.width) * p.height * 4;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // No mipmaps: art is rasterised at the size it is shown (at most 1.41x
    // larger), and the driver making them stalled for big textures.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.width, t.height, 0, platform::kTexelFormat, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

// Rows [from, to) of the texels into it.
void uploadRows(const Texture& t, const Pixels& p, int from, int to) {
    if (!t.id || to <= from) return;
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, from, t.width, to - from, platform::kTexelFormat, GL_UNSIGNED_BYTE,
                    p.bgra.data() + static_cast<size_t>(from) * t.width * 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// The texels as a GL texture (GL thread). With a display thread the texels
// go to it (platform::setTexels): only the texture's name is made here.
// The texture let go (its page's slot, or the texture itself, kept a while
// for art of its size).
void release(const Texture& t) {
    if (t.slot.page) platform::releaseSlot(t.slot);
    else platform::recycleTexture(t.id, t.width, t.height);
}

Texture upload(Pixels&& p, bool drawnNow = true) {
    // Small art: in a shared page (drawn with its neighbours in one go).
    platform::AtlasSlot slot;
    if (!p.bgra.empty() && platform::atlasPut(p.width, p.height, p.bgra, drawnNow, slot)) {
        const float side = static_cast<float>(slot.pageSide);
        Texture t;
        t.id = slot.page;
        t.slot = slot;
        t.width = p.width;
        t.height = p.height;
        t.x0 = p.drawn[0];
        t.y0 = p.drawn[1];
        t.x1 = p.drawn[2];
        t.y1 = p.drawn[3];
        t.u0 = (slot.x + 1 + p.u0 * p.width) / side;
        t.v0 = (slot.y + 1 + p.v0 * p.height) / side;
        t.u1 = (slot.x + 1 + p.u1 * p.width) / side;
        t.v1 = (slot.y + 1 + p.v1 * p.height) / side;
        t.tile = p.tile;
        return t;
    }
    if (!platform::texelsQueued()) {
        Texture t = allocate(p);
        uploadRows(t, p, 0, p.height);
        return t;
    }
    Texture t;
    if (p.bgra.empty()) return t;
    t.width = p.width;
    t.height = p.height;
    t.x0 = p.drawn[0];
    t.y0 = p.drawn[1];
    t.x1 = p.drawn[2];
    t.y1 = p.drawn[3];
    t.u0 = p.u0;
    t.v0 = p.v0;
    t.u1 = p.u1;
    t.v1 = p.v1;
    t.tile = p.tile;
    bool fresh = false;
    if (!(t.id = platform::reuseTexture(t.width, t.height))) {
        gAllocatedTotal += static_cast<size_t>(p.width) * p.height * 4;
        t.id = platform::newTexture();
        fresh = true;
    }
    platform::setTexels(t.id, t.width, t.height, fresh, std::move(p.bgra), drawnNow);
    return t;
}

// UTF-8 (an HTML field's markup reduced to its text and line breaks) as
// UTF-16, the font's codes.
std::u16string decodeText(const std::string& content, bool html) {
    std::u16string text;
    for (size_t i = 0; i < content.size();) {
        unsigned char c0 = static_cast<unsigned char>(content[i]);
        if (html && c0 == '<') {
            size_t e = content.find('>', i);
            if (e == std::string::npos) break;
            std::string tag = content.substr(i + 1, e - i - 1);
            for (char& ch : tag) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (tag == "br" || tag == "br/" || tag == "/p") text += u'\n';
            i = e + 1;
            continue;
        }
        if (html && c0 == '&') {
            size_t e = content.find(';', i);
            std::string ent = e == std::string::npos ? "" : content.substr(i + 1, e - i - 1);
            char16_t ch = ent == "amp" ? u'&' : ent == "lt" ? u'<' : ent == "gt" ? u'>' : ent == "quot" ? u'"'
                        : ent == "apos" ? u'\'' : ent == "nbsp" ? u' ' : 0;
            if (ch) {
                text += ch;
                i = e + 1;
                continue;
            }
        }
        uint32_t cp = c0;
        int extra = c0 >= 0xF0 ? 3 : c0 >= 0xE0 ? 2 : c0 >= 0xC0 ? 1 : 0;
        if (extra) cp = c0 & (0x3F >> extra);
        for (int k = 0; k < extra && i + 1 + k < content.size(); ++k) cp = (cp << 6) | (content[i + 1 + k] & 0x3F);
        i += 1 + extra;
        text += static_cast<char16_t>(cp > 0xFFFF ? u'?' : cp);
    }
    if (html && !text.empty() && text.back() == u'\n') text.pop_back();
    return text;
}

}  // namespace

Shape lerpShape(const Shape& a, const Shape& b, float t) {
    Shape s = a;
    auto mix = [t](float x, float y) { return x + (y - x) * t; };
    auto mixi = [t](int32_t x, int32_t y) { return static_cast<int32_t>(std::lround(x + (y - x) * t)); };
    auto mixc = [&](uint8_t* o, const uint8_t* x, const uint8_t* y) {
        for (int i = 0; i < 4; ++i) o[i] = static_cast<uint8_t>(std::lround(mix(x[i], y[i])));
    };
    s.bounds = {mix(a.bounds.xMin, b.bounds.xMin), mix(a.bounds.yMin, b.bounds.yMin), mix(a.bounds.xMax, b.bounds.xMax),
                mix(a.bounds.yMax, b.bounds.yMax)};
    for (size_t l = 0; l < s.layers.size() && l < b.layers.size(); ++l) {
        ShapeLayer& L = s.layers[l];
        const ShapeLayer& B = b.layers[l];
        for (size_t i = 0; i < L.fills.size() && i < B.fills.size(); ++i) {
            mixc(L.fills[i].rgba, a.layers[l].fills[i].rgba, B.fills[i].rgba);
            const Matrix &ma = a.layers[l].fills[i].matrix, &mb = B.fills[i].matrix;
            L.fills[i].matrix = {mix(ma.a, mb.a), mix(ma.b, mb.b), mix(ma.c, mb.c), mix(ma.d, mb.d), mix(ma.tx, mb.tx),
                                 mix(ma.ty, mb.ty)};
            for (size_t k = 0; k < L.fills[i].stops.size() && k < B.fills[i].stops.size(); ++k) {
                L.fills[i].stops[k].ratio = mix(a.layers[l].fills[i].stops[k].ratio, B.fills[i].stops[k].ratio);
                mixc(L.fills[i].stops[k].rgba, a.layers[l].fills[i].stops[k].rgba, B.fills[i].stops[k].rgba);
            }
        }
        for (size_t i = 0; i < L.lines.size() && i < B.lines.size(); ++i) {
            L.lines[i].width = mix(a.layers[l].lines[i].width, B.lines[i].width);
            mixc(L.lines[i].rgba, a.layers[l].lines[i].rgba, B.lines[i].rgba);
        }
        auto mixPaths = [&](std::vector<StyledPath>& out, const std::vector<StyledPath>& other) {
            for (size_t p = 0; p < out.size() && p < other.size(); ++p)
                for (size_t k = 0; k < out[p].ops.size() && k < other[p].ops.size(); ++k) {
                    PathOp& o = out[p].ops[k];
                    const PathOp& q = other[p].ops[k];
                    o.x = mixi(o.x, q.x);
                    o.y = mixi(o.y, q.y);
                    o.cx = mixi(o.cx, q.cx);
                    o.cy = mixi(o.cy, q.cy);
                }
        };
        mixPaths(L.fillPaths, B.fillPaths);
        mixPaths(L.linePaths, B.linePaths);
    }
    return s;
}

// ------------------------------------------------------------- the cache

Raster::Raster(const Swf& swf) : swf(swf) {
    // (all the processors but the game's and the display's threads: a phone's
    // eight cores make a screen's new art side by side six at a time)
    const unsigned cores = std::thread::hardware_concurrency();
    unsigned n = std::max(2u, std::min(8u, cores > 2 ? cores - 2 : 1u));
    for (unsigned i = 0; i < n; ++i) workers.emplace_back([this] { work(); });
}

Raster::~Raster() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        quitting = true;
    }
    wake.notify_all();
    for (std::thread& t : workers) t.join();
    for (auto& [k, e] : cache) release(e.tex);
    if (uploading.active) platform::releaseTexture(uploading.tex.id);
}

void Raster::work() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this] { return quitting || !jobs.empty(); });
            if (quitting) return;
            job = std::move(jobs.front());
            jobs.pop_front();
        }
        Pixels p = job.make();
        {
            std::lock_guard<std::mutex> lock(mutex);
            done.emplace_back(job.key, std::move(p));
        }
        made.notify_all();
    }
}

void Raster::keep(const Key& key) {
    if (prefetchKeep <= 0) return;
    uint64_t until = frame + static_cast<uint64_t>(prefetchKeep);
    auto it = cache.find(key);
    if (it != cache.end()) it->second.lastUsed = std::max(it->second.lastUsed, until);
    else keepUntil[key] = std::max(keepUntil[key], until);
}

void Raster::enqueue(const Key& key, std::function<Pixels()> make) {
    if (!pending.insert(key).second) return;
    std::lock_guard<std::mutex> lock(mutex);
    jobs.push_back({key, std::move(make)});
    wake.notify_one();
}

void Raster::defer(const Key& key, std::function<Pixels()> make) {
    deferredKeys.push_back(key);
    std::lock_guard<std::mutex> lock(mutex);
    if (pending.insert(key).second) {
        jobs.push_front({key, std::move(make)});
        wake.notify_one();
        return;
    }
    // (Asked for ahead and still queued: its turn now.)
    auto it = std::find_if(jobs.begin(), jobs.end(), [&](const Job& j) { return j.key == key; });
    if (it != jobs.end() && it != jobs.begin()) {
        Job job = std::move(*it);
        jobs.erase(it);
        jobs.push_front(std::move(job));
    }
}

void Raster::finishDeferred() {
    if (deferredKeys.empty()) return;
    auto t0 = std::chrono::steady_clock::now();
    std::unordered_set<Key, KeyHash> wanted(deferredKeys.begin(), deferredKeys.end());
    {
        std::unique_lock<std::mutex> lock(mutex);
        for (;;) {
            // Still being made: any wanted key pending but not among the done.
            bool waiting = false;
            for (const Key& k : wanted) {
                if (!pending.count(k) || (uploading.active && uploading.key == k)) continue;
                if (std::none_of(done.begin(), done.end(), [&](const auto& d) { return d.first == k; })) {
                    waiting = true;
                    break;
                }
            }
            if (!waiting) break;
            // This thread makes the next wanted piece too; else waits for one.
            if (!jobs.empty() && wanted.count(jobs.front().key)) {
                Job job = std::move(jobs.front());
                jobs.pop_front();
                lock.unlock();
                Pixels p = job.make();
                lock.lock();
                done.emplace_back(job.key, std::move(p));
                continue;
            }
            made.wait_for(lock, std::chrono::milliseconds(20));
            // (never stuck: what is still not made by then is made at once
            // when drawn again)
            if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(1)) break;
        }
    }
    static const bool profiling = std::getenv("AS_PROFILE") != nullptr;  // (debug: which art stalls)
    for (const Key& k : wanted) {
        if (pending.count(k)) takeDone(k);
        if (profiling)
            platform::debugTrace("raster made side by side: kind %d id %d ratio %d level %d effects %llu tile %d", k.kind,
                                 k.id, k.ratio, k.level, static_cast<unsigned long long>(k.effects), k.tile);
    }
    syncCount += static_cast<int>(wanted.size());
    syncMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    deferredKeys.clear();
}

Raster::Entry& Raster::put(const Key& key, const Texture& tex) {
    Entry& e = cache[key];
    release(e.tex);
    bytes -= bytesOf(e.tex);
    e.tex = tex;
    bytes += bytesOf(e.tex);
    madeThisFrame += bytesOf(e.tex);
    e.lastUsed = frame;
    return e;
}

void Raster::drop(std::unordered_map<Key, Entry, KeyHash>::iterator it) {
    release(it->second.tex);
    bytes -= bytesOf(it->second.tex);
    cache.erase(it);
}

const Texture* Raster::install(const Key& key, const Texture& tex) {
    pending.erase(key);
    // Made at once meanwhile (drawn before its turn came): that texture
    // stays, the same art twice over is dropped. A texture swapped under
    // a piece on screen would cost it its pairing with the last pass (the
    // display holds it in place for a pass while the rest moves on: AS1's
    // tool icon left behind by its cursor).
    if (auto have = cache.find(key); have != cache.end() && have->second.tex.id) {
        release(tex);
        return &have->second.tex;
    }
    Entry& e = put(key, tex);
    noteEffects(key);
    if (auto k = keepUntil.find(key); k != keepUntil.end()) {
        e.lastUsed = std::max(e.lastUsed, k->second);
        keepUntil.erase(k);
    }
    return e.tex.id ? &e.tex : nullptr;
}

const Texture* Raster::takeDone(const Key& key) {
    auto t0 = std::chrono::steady_clock::now();
    auto counted = [&](const Texture* t) {
        ++uploadCount;
        uploadMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return t;
    };
    if (uploading.active && uploading.key == key) {
        // Being uploaded: the rest now.
        uploadRows(uploading.tex, uploading.pixels, uploading.rows, uploading.pixels.height);
        uploading.active = false;
        uploading.pixels = Pixels{};
        return counted(install(key, uploading.tex));
    }
    Pixels p;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = std::find_if(done.begin(), done.end(), [&](const auto& d) { return d.first == key; });
        if (it == done.end()) return nullptr;
        p = std::move(it->second);
        done.erase(it);
    }
    return counted(install(key, upload(std::move(p))));
}

const Texture* Raster::get(const Key& key, std::function<Pixels()> make) {
    if (prefetching) keep(key);
    auto it = cache.find(key);
    if (it != cache.end()) {
        it->second.lastUsed = std::max(it->second.lastUsed, frame);
        if (!prefetching && it->second.tex.id) noteEffects(key);
        return it->second.tex.id ? &it->second.tex : nullptr;
    }
    if (prefetching) {  // made in the background, for later
        enqueue(key, std::move(make));
        return nullptr;
    }
    // Made in the background already: uploaded now (being made, while
    // deferring: waited for).
    if (pending.count(key)) {
        if (const Texture* t = takeDone(key)) return t;
        if (deferring && !prefetching) {
            defer(key, std::move(make));
            return nullptr;
        }
    }
    // The same art as last drawn with other filters (a blur being
    // tweened), or without any, is drawn while this one is made on a worker.
    if (key.effects) {
        Key base = key;
        base.effects = 0;
        auto last = lastEffects.find(artOf(key));
        for (const Key& k : {last != lastEffects.end() ? last->second : base, base}) {
            auto n = cache.find(k);
            if (n == cache.end() || !n->second.tex.id) continue;
            n->second.lastUsed = frame;
            enqueue(key, std::move(make));
            return &n->second.tex;
        }
    }
    // Another resolution of the same art is drawn (scaled) while this one
    // is made on a worker: a zoom or a window resize never stalls. (Not a
    // tile: another resolution's tile of that number is another part.)
    for (int d = key.tile ? 9 : 1; d <= 8; ++d)
        for (int level : {key.level - d, key.level + d}) {
            Key other = key;
            other.level = level;
            auto n = cache.find(other);
            if (n == cache.end() || !n->second.tex.id) continue;
            n->second.lastUsed = frame;
            enqueue(key, std::move(make));
            return &n->second.tex;
        }
    // Art nobody asked for ahead is made at once (deferring: by the
    // workers, with the rest of the pass's new art).
    if (deferring) {
        defer(key, std::move(make));
        return nullptr;
    }
    auto t0 = std::chrono::steady_clock::now();
    Entry& e = put(key, upload(make()));
    noteEffects(key);
    ++syncCount;
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    syncMs += ms;
    static const bool profiling = std::getenv("AS_PROFILE") != nullptr;  // (debug: which art stalls)
    if (profiling)
        platform::debugTrace("raster made at once: kind %d id %d ratio %d level %d text %llu effects %llu%s, %dx%d, %.1f ms",
                     key.kind, key.id, key.ratio, key.level, static_cast<unsigned long long>(key.text),
                     static_cast<unsigned long long>(key.effects), key.effects ? " filtered" : "", e.tex.width,
                     e.tex.height, ms);
    return e.tex.id ? &e.tex : nullptr;
}

void Raster::pump(int ms) {
    if (platform::texelsQueued()) {
        // A display thread uploads: the art made is handed to it, a few
        // megabytes a pass (it uploads between display frames), and at once
        // when drawn before its turn.
        size_t budget = static_cast<size_t>(ms) << 20;
        for (;;) {
            std::pair<Key, Pixels> next;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (done.empty()) return;
                next = std::move(done.front());
                done.erase(done.begin());
            }
            const size_t size = next.second.bgra.size();
            install(next.first, upload(std::move(next.second), false));  // (drawn from the next pass on)
            if (size >= budget) return;
            budget -= size;
        }
    }
    // Uploads take a few milliseconds a frame at most, a strip of rows at
    // a time; the rest waits (and is uploaded at once if drawn before).
    constexpr int kStrip = 1 << 18;  // texels a strip (1 MB)
    auto start = std::chrono::steady_clock::now();
    auto over = [&] { return std::chrono::steady_clock::now() - start > std::chrono::milliseconds(ms); };
    for (;;) {
        if (!uploading.active) {
            std::pair<Key, Pixels> next;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (done.empty()) return;
                next = std::move(done.front());
                done.erase(done.begin());
            }
            if (next.second.width <= platform::kAtlasLargest && next.second.height <= platform::kAtlasLargest) {
                install(next.first, upload(std::move(next.second), false));
                if (over()) return;
                continue;
            }
            uploading.active = true;
            uploading.key = next.first;
            uploading.pixels = std::move(next.second);
            uploading.tex = allocate(uploading.pixels);
            uploading.rows = 0;
        }
        Uploading& u = uploading;
        int rows = std::max(1, kStrip / std::max(1, u.pixels.width));
        while (u.rows < u.pixels.height) {
            int to = std::min(u.pixels.height, u.rows + rows);
            uploadRows(u.tex, u.pixels, u.rows, to);
            u.rows = to;
            if (u.rows < u.pixels.height && over()) return;
        }
        u.active = false;
        u.pixels = Pixels{};
        install(u.key, u.tex);
        if (over()) return;
    }
}

bool Raster::busy() { return !pending.empty(); }

size_t Raster::textureBytes(int& largestW, int& largestH) const {
    size_t bytes = 0;
    largestW = largestH = 0;
    for (const auto& [k, e] : cache) {
        bytes += static_cast<size_t>(e.tex.width) * e.tex.height * 4;
        if (static_cast<long>(e.tex.width) * e.tex.height > static_cast<long>(largestW) * largestH) {
            largestW = e.tex.width;
            largestH = e.tex.height;
        }
    }
    return bytes;
}

std::string Raster::churn() const {
    size_t drawn = 0, n = 0;
    std::map<std::pair<int, int>, size_t> by;
    for (const auto& [k, e] : cache)
        if (e.lastUsed + 2 >= frame && e.lastUsed <= frame) {
            drawn += bytesOf(e.tex);
            ++n;
            by[{k.kind, k.id}] += bytesOf(e.tex);
        }
    std::vector<std::pair<size_t, std::pair<int, int>>> v;
    for (auto& [k, b] : by) v.push_back({b, k});
    std::sort(v.rbegin(), v.rend());
    std::string top;
    for (size_t i = 0; i < v.size() && i < 6; ++i)
        top += " " + std::to_string(v[i].second.first) + ":" + std::to_string(v[i].second.second) + " " +
               std::to_string(v[i].first >> 20) + "MB";
    return "drawn " + std::to_string(n) + " textures " + std::to_string(drawn >> 20) + " MB (" + top + "), " +
           "textures ever allocated " + std::to_string(gAllocatedTotal >> 20) + " MB, made " +
           std::to_string(static_cast<long>(madeAverage) >> 10) + " KB/frame, morph detail -" + std::to_string(morphDrop) +
           ", " + platform::atlasStats();
}

std::string Raster::topArt(int n) const {
    std::map<std::pair<int, int>, std::pair<int, size_t>> by;
    for (const auto& [k, e] : cache) {
        auto& b = by[{k.kind, k.id}];
        ++b.first;
        b.second += static_cast<size_t>(e.tex.width) * e.tex.height * 4;
    }
    std::vector<std::pair<std::pair<int, int>, std::pair<int, size_t>>> list(by.begin(), by.end());
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.second.second > b.second.second; });
    std::string out;
    for (int i = 0; i < n && i < static_cast<int>(list.size()); ++i) {
        std::set<int> ratios, levels;
        std::set<uint64_t> colours;
        for (const auto& [k, e] : cache)
            if (k.kind == list[i].first.first && k.id == list[i].first.second) {
                ratios.insert(k.ratio);
                levels.insert(k.level);
                colours.insert(k.text);
            }
        out += std::to_string(list[i].first.first) + ":" + std::to_string(list[i].first.second) + " x" +
               std::to_string(list[i].second.first) + " " + std::to_string(list[i].second.second >> 20) + "MB (r" +
               std::to_string(ratios.size()) + " l" + std::to_string(levels.size()) + " c" +
               std::to_string(colours.size()) + " L" + std::to_string(*levels.begin()) + ".." +
               std::to_string(*levels.rbegin()) + "); ";
    }
    return out;
}

void Raster::backlog(size_t& waiting, size_t& made, size_t& bytes) {
    std::lock_guard<std::mutex> lock(mutex);
    waiting = jobs.size();
    made = done.size();
    bytes = 0;
    for (const auto& d : done) bytes += d.second.bgra.size();
}

Raster::Key Raster::artOf(const Key& key) {
    // The art itself: any level, colour or filters (an edit text: its text).
    Key art = key;
    art.effects = 0;
    art.level = 0;
    if (art.kind != 3) art.text = 0;
    return art;
}

void Raster::noteEffects(const Key& key) {
    if (!key.effects) return;
    lastEffects[artOf(key)] = key;
}

void Raster::place(Key key, const Rect& grid, const Rect* visible,
                   const std::function<Pixels(const TileArea*)>& make, std::vector<const Texture*>& out,
                   bool* complete) {
    bool wasPrefetching = prefetching;
    if (complete) {
        *complete = true;
        prefetching = true;  // (what is missing only queued)
    }
    auto add = [&](const Key& k, const Texture* t) {
        if (t) out.push_back(t);
        else if (complete && !cache.count(k)) *complete = false;
    };
    struct Restore {
        bool& flag;
        bool was;
        ~Restore() { flag = was; }
    } restore{prefetching, wasPrefetching};
    auto texels = [&](float twips, float scale) { return static_cast<int>(std::ceil(twips * scale / 20)) + 2 * kPad; };
    float scale = scaleFor(key.level);
    int w = texels(grid.xMax - grid.xMin, scale), h = texels(grid.yMax - grid.yMin, scale);
    if (key.effects || grid.empty() || std::max(w, h) <= kTileAbove) {
        // Filtered art not made yet (a motion blur being tweened: a new
        // texture each pass, each a blur over thousands of texels) is made
        // in the background while its unfiltered art, far cheaper, stands
        // in (made now if need be; get() draws it): no pass waits for a blur.
        if (key.effects && !prefetching && !cache.count(key)) {
            Key base = key;
            base.effects = 0;
            if (!cache.count(base)) get(base, [make] { return make(&kPlain); });
        }
        add(key, get(key, [make] { return make(nullptr); }));
        return;
    }
    // (At most 255 tiles a side: art zoomed further is made coarser.)
    while (std::max(w, h) > 255 * kTile) {
        scale = scaleFor(--key.level);
        w = texels(grid.xMax - grid.xMin, scale);
        h = texels(grid.yMax - grid.yMin, scale);
    }
    float twipsPerTexel = 20 / scale;
    float gx = grid.xMin - kPad * twipsPerTexel, gy = grid.yMin - kPad * twipsPerTexel;
    // The tiles `visible` touches are drawn; those in a ring a tenth of it
    // wider are made in the background ahead of being needed (art moving
    // or zooming in then rarely waits for a tile).
    int i0 = 0, j0 = 0, i1 = (w - 1) / kTile, j1 = (h - 1) / kTile;
    int a0 = i0, b0 = j0, a1 = i1, b1 = j1;  // the ring's
    if (visible) {
        auto range = [&](const Rect& r, int& x0, int& y0, int& x1, int& y1) {
            auto texel = [&](float twips, float from, int n) {
                return static_cast<int>(
                    std::clamp(std::floor((twips - from) / twipsPerTexel), 0.0f, static_cast<float>(n - 1)));
            };
            if (r.xMax < gx || r.yMax < gy || r.xMin > gx + w * twipsPerTexel || r.yMin > gy + h * twipsPerTexel)
                return false;
            x0 = texel(r.xMin, gx, w) / kTile;
            x1 = texel(r.xMax, gx, w) / kTile;
            y0 = texel(r.yMin, gy, h) / kTile;
            y1 = texel(r.yMax, gy, h) / kTile;
            return true;
        };
        float mx = (visible->xMax - visible->xMin) * 0.1f, my = (visible->yMax - visible->yMin) * 0.1f;
        Rect ring{visible->xMin - mx, visible->yMin - my, visible->xMax + mx, visible->yMax + my};
        bool shown = range(*visible, i0, j0, i1, j1);
        if (!range(ring, a0, b0, a1, b1)) return;
        if (!shown) i0 = j0 = 1, i1 = j1 = 0;  // (only ahead)
    }
    // Tiles on screen not made yet (new art, a zoom, a fast pan) are made in
    // the background, and the art drawn meanwhile as another size's tiles
    // where those cover the screen, else whole at a coarse size (one walk
    // of the art, not a screenful: a story screen's art made each tile at
    // once and stuttered).
    auto tileKey = [&](const Key& base, int i, int j) {
        Key k = base;
        k.tile = j * 256 + i + 1;
        return k;
    };
    auto ready = [&](const Key& k) { return cache.count(k) || (pending.count(k) && takeDone(k)); };
    bool missing = false;
    if (visible && !prefetching && !complete)
        for (int j = j0; j <= j1 && !missing; ++j)
            for (int i = i0; i <= i1 && !missing; ++i) missing = !ready(tileKey(key, i, j));
    if (missing) {
        prefetching = true;  // (queued; restored on return)
        for (int j = b0; j <= b1; ++j)
            for (int i = a0; i <= a1; ++i) {
                TileArea t{j * 256 + i + 1, i * kTile, j * kTile, std::min(w, (i + 1) * kTile),
                           std::min(h, (j + 1) * kTile), scale};
                get(tileKey(key, i, j), [make, t] { return make(&t); });
            }
        prefetching = wasPrefetching;
        for (int d = 1; d <= 6; ++d)
            for (int level : {key.level - d, key.level + d}) {
                float s = scaleFor(level);
                int lw = texels(grid.xMax - grid.xMin, s), lh = texels(grid.yMax - grid.yMin, s);
                if (std::max(lw, lh) <= kTileAbove || std::max(lw, lh) > 255 * kTile) continue;
                float tpt = 20 / s;
                float lx = grid.xMin - kPad * tpt, ly = grid.yMin - kPad * tpt;
                auto texel = [&](float twips, float from, int n) {
                    return static_cast<int>(std::clamp(std::floor((twips - from) / tpt), 0.0f, static_cast<float>(n - 1)));
                };
                int x0 = texel(visible->xMin, lx, lw) / kTile, x1 = texel(visible->xMax, lx, lw) / kTile;
                int y0 = texel(visible->yMin, ly, lh) / kTile, y1 = texel(visible->yMax, ly, lh) / kTile;
                Key other = key;
                other.level = level;
                bool whole = true;
                for (int j = y0; j <= y1 && whole; ++j)
                    for (int i = x0; i <= x1 && whole; ++i) whole = cache.count(tileKey(other, i, j)) != 0;
                if (!whole) continue;
                for (int j = y0; j <= y1; ++j)
                    for (int i = x0; i <= x1; ++i) {
                        Entry& e = cache.at(tileKey(other, i, j));
                        e.lastUsed = std::max(e.lastUsed, frame);
                        if (e.tex.id) out.push_back(&e.tex);
                    }
                return;
            }
        Key coarse = key;
        while (std::max(texels(grid.xMax - grid.xMin, scaleFor(coarse.level)),
                        texels(grid.yMax - grid.yMin, scaleFor(coarse.level))) > kCoarse)
            --coarse.level;
        TileArea whole{kWholeAt, 0, 0, 0, 0, scaleFor(coarse.level)};
        add(coarse, get(coarse, [make, whole] { return make(&whole); }));
        return;
    }
    for (int j = b0; j <= b1; ++j)
        for (int i = a0; i <= a1; ++i) {
            TileArea a{j * 256 + i + 1, i * kTile, j * kTile, std::min(w, (i + 1) * kTile), std::min(h, (j + 1) * kTile),
                       scale};
            Key k = key;
            k.tile = a.index;
            if (i >= i0 && i <= i1 && j >= j0 && j <= j1) {
                add(k, get(k, [make, a] { return make(&a); }));
            } else if (!cache.count(k)) {  // ahead: queued, not drawn
                bool was = prefetching;
                prefetching = true;
                get(k, [make, a] { return make(&a); });
                prefetching = was;
            } else {
                cache.at(k).lastUsed = std::max(cache.at(k).lastUsed, frame);  // (kept while near)
            }
        }
}

void Raster::shape(int id, float scale, const ColorTransform* baked, const Effects* effects, const Rect* visible,
                   std::vector<const Texture*>& out) {
    const Shape* s = swf.shape(id);
    if (!s) return;
    Key key{0, id, 0, levelFor(scale), bakedKey(baked)};
    int drop = 0;
    EffectList fx = effectsFor(effects, scale, key.level, key.effects, drop);
    std::optional<ColorTransform> bk;
    if (baked) bk = *baked;
    const Swf* movie = &swf;
    place(key, s->bounds, visible, [s, bk, fx, movie, level = key.level - drop](const TileArea* area) {
        return rasterize(
            s->bounds, scaleFor(level),
            [&](plutovg_canvas_t* c, float minLine) { drawShape(c, *s, minLine, nullptr, movie); }, bk ? &*bk : nullptr,
            &fx, area);
    }, out);
}

void Raster::morph(int id, float ratio, float scale, const ColorTransform* baked, const Effects* effects,
                   const Rect* visible, std::vector<const Texture*>& out) {
    auto it = swf.morphs.find(id);
    if (it == swf.morphs.end()) return;
    // 64 steps: a tween re-rasterises a few dozen times, not every frame
    // (under load, endFrame: up to four scale steps coarser, then 32, 16 or 8).
    int every = 1 << std::max(0, morphDrop - 4);
    int r = static_cast<int>(std::lround(std::clamp(ratio, 0.0f, 1.0f) * kMorphSteps / every)) * every;
    Key key{1, id, r, levelFor(scale) - std::min(morphDrop, 4), bakedKey(baked)};
    int drop = 0;
    EffectList fx = effectsFor(effects, scale, key.level, key.effects, drop);
    const auto* m = &it->second;
    std::optional<ColorTransform> bk;
    if (baked) bk = *baked;
    // Its tiles are cut from both ends' bounds (the same grid at every step).
    const Rect &from = m->first.bounds, &to = m->second.bounds;
    Rect grid{std::min(from.xMin, to.xMin), std::min(from.yMin, to.yMin), std::max(from.xMax, to.xMax),
              std::max(from.yMax, to.yMax)};
    place(key, grid, visible, [m, r, bk, fx, grid, level = key.level - drop](const TileArea* area) {
        Shape s = lerpShape(m->first, m->second, r / static_cast<float>(kMorphSteps));
        return rasterize(area ? grid : s.bounds, scaleFor(level),
                         [&](plutovg_canvas_t* c, float minLine) { drawShape(c, s, minLine); }, bk ? &*bk : nullptr,
                         &fx, area);
    }, out);
}

void Raster::text(int id, float scale, const ColorTransform* baked, const Effects* effects, const Rect* visible,
                  std::vector<const Texture*>& out) {
    auto it = swf.texts.find(id);
    if (it == swf.texts.end()) return;
    Key key{2, id, 0, levelFor(scale), bakedKey(baked)};
    int drop = 0;
    EffectList fx = effectsFor(effects, scale, key.level, key.effects, drop);
    const StaticText* t = &it->second;
    const Swf* movie = &swf;
    std::optional<ColorTransform> bk;
    if (baked) bk = *baked;
    place(key, t->bounds, visible, [t, movie, bk, fx, level = key.level - drop](const TileArea* area) {
        return rasterize(t->bounds, scaleFor(level), [&](plutovg_canvas_t* c, float minLine) {
            for (const TextRecord& rec : t->records) {
                auto f = movie->fonts.find(rec.font);
                if (f == movie->fonts.end()) continue;
                float s = rec.height / f->second.unitsPerEm;
                float x = rec.x;
                for (const TextGlyph& g : rec.glyphs) {
                    if (g.index >= 0 && g.index < static_cast<int>(f->second.glyphs.size())) {
                        plutovg_canvas_save(c);
                        plutovg_matrix_t gm{t->matrix.a * s, t->matrix.b * s, t->matrix.c * s, t->matrix.d * s,
                                            t->matrix.a * x + t->matrix.c * rec.y + t->matrix.tx,
                                            t->matrix.b * x + t->matrix.d * rec.y + t->matrix.ty};
                        plutovg_canvas_transform(c, &gm);
                        drawShape(c, f->second.glyphs[g.index], minLine / s, rec.rgba);
                        plutovg_canvas_restore(c);
                    }
                    x += g.advance;
                }
            }
        }, bk ? &*bk : nullptr, &fx, area);
    }, out);
}

void Raster::group(int clip, const std::vector<GroupPiece>& pieces, float scale, const Effects* effects,
                   const Rect* visible, std::vector<const Texture*>& out, bool* complete) {
    // Each piece's shape (a morph's at its step) and where it lands.
    struct Placed {
        const Shape* shape;
        const std::pair<Shape, Shape>* morph;
        int step;
        Matrix matrix;
        float alpha;
    };
    std::vector<Placed> placed;
    Rect bounds{1e30f, 1e30f, -1e30f, -1e30f};
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    for (const GroupPiece& p : pieces) {
        Placed q{nullptr, nullptr, 0, p.matrix, p.alpha};
        Rect b;
        if (p.kind == Swf::Kind::kShape) {
            q.shape = swf.shape(p.id);
            if (!q.shape) continue;
            b = q.shape->bounds;
        } else if (p.kind == Swf::Kind::kMorph) {
            auto it = swf.morphs.find(p.id);
            if (it == swf.morphs.end()) continue;
            q.morph = &it->second;
            q.step = static_cast<int>(std::lround(std::clamp(p.ratio, 0.0f, 1.0f) * kMorphSteps));
            const Rect &s = it->second.first.bounds, &e = it->second.second.bounds;
            b = {std::min(s.xMin, e.xMin), std::min(s.yMin, e.yMin), std::max(s.xMax, e.xMax), std::max(s.yMax, e.yMax)};
        } else {
            continue;
        }
        for (float x : {b.xMin, b.xMax})
            for (float y : {b.yMin, b.yMax}) {
                float px, py;
                p.matrix.apply(x, y, px, py);
                bounds = {std::min(bounds.xMin, px), std::min(bounds.yMin, py), std::max(bounds.xMax, px),
                          std::max(bounds.yMax, py)};
            }
        mix(static_cast<uint64_t>(p.kind) << 32 | static_cast<uint32_t>(p.id));
        mix(static_cast<uint64_t>(q.step));
        mix(static_cast<uint64_t>(std::lround(p.alpha * 255)));
        for (float v : {p.matrix.a, p.matrix.b, p.matrix.c, p.matrix.d, p.matrix.tx, p.matrix.ty})
            mix(static_cast<uint64_t>(std::llround(v * 1000)));
        placed.push_back(q);
    }
    if (placed.empty()) return;
    Key key{4, clip, 0, levelFor(scale), h | 1};
    int drop = 0;
    EffectList fx = effectsFor(effects, scale, key.level, key.effects, drop);
    const Swf* movie = &swf;
    place(key, bounds, visible, [placed, bounds, fx, movie, level = key.level - drop](const TileArea* area) {
        return rasterize(bounds, scaleFor(level), [&](plutovg_canvas_t* c, float minLine) {
            for (const Placed& q : placed) {
                // A faded piece is drawn alone, then faded as one (its own
                // overlapping fills do not show through each other).
                plutovg_canvas_t* on = c;
                plutovg_surface_t* alone = nullptr;
                plutovg_surface_t* target = plutovg_canvas_get_surface(c);
                if (q.alpha < 1) {
                    alone = plutovg_surface_create(plutovg_surface_get_width(target), plutovg_surface_get_height(target));
                    if (!alone) continue;
                    on = plutovg_canvas_create(alone);
                    plutovg_matrix_t cm;
                    plutovg_canvas_get_matrix(c, &cm);
                    plutovg_canvas_set_matrix(on, &cm);
                }
                const Matrix& m = q.matrix;
                plutovg_canvas_save(on);
                plutovg_matrix_t pm{m.a, m.b, m.c, m.d, m.tx, m.ty};
                plutovg_canvas_transform(on, &pm);
                float s = std::max(std::sqrt(std::fabs(m.a * m.d - m.b * m.c)), 1e-3f);
                if (q.shape) drawShape(on, *q.shape, minLine / s, nullptr, movie);
                else drawShape(on, lerpShape(q.morph->first, q.morph->second, q.step / static_cast<float>(kMorphSteps)), minLine / s);
                plutovg_canvas_restore(on);
                if (alone) {
                    plutovg_canvas_destroy(on);
                    plutovg_canvas_save(c);
                    plutovg_canvas_reset_matrix(c);
                    plutovg_matrix_t identity;
                    plutovg_matrix_init_identity(&identity);
                    plutovg_canvas_set_texture(c, alone, PLUTOVG_TEXTURE_TYPE_PLAIN, q.alpha, &identity);
                    plutovg_canvas_fill_rect(c, 0, 0, static_cast<float>(plutovg_surface_get_width(target)),
                                             static_cast<float>(plutovg_surface_get_height(target)));
                    plutovg_canvas_restore(c);
                    plutovg_surface_destroy(alone);
                }
            }
        }, nullptr, &fx, area);
    }, out, complete);
}

const Texture* Raster::editText(int id, const std::string& content, float scale, const ColorTransform* baked,
                                const Effects* effects) {
    auto it = swf.editTexts.find(id);
    if (it == swf.editTexts.end() || content.empty()) return nullptr;
    const EditText* t = &it->second;
    auto f = swf.fonts.find(t->font);
    if (f == swf.fonts.end()) return nullptr;
    Key key{3, id, 0, levelFor(scale), std::hash<std::string>()(content) ^ bakedKey(baked)};
    int drop = 0;
    EffectList fx = effectsFor(effects, scale, key.level, key.effects, drop);
    const Font* font = &f->second;
    std::optional<ColorTransform> bk;
    if (baked) bk = *baked;
    return get(key, [t, font, content, bk, fx, level = key.level - drop] {
        Rect drawn;
        float s = 1;
        const std::vector<PlacedGlyph> glyphs = layoutEditText(*t, *font, content, s, drawn);
        return rasterize(drawn.empty() ? t->bounds : drawn, scaleFor(level), [&](plutovg_canvas_t* c, float minLine) {
            for (const PlacedGlyph& g : glyphs) {
                plutovg_canvas_save(c);
                plutovg_matrix_t gm{s, 0, 0, s, g.x, g.y};
                plutovg_canvas_transform(c, &gm);
                drawShape(c, font->glyphs[g.glyph], minLine / s, t->rgba);
                plutovg_canvas_restore(c);
            }
        }, bk ? &*bk : nullptr, &fx);
    });
}

std::vector<PlacedGlyph> layoutEditText(const EditText& field, const Font& font, const std::string& content, float& glyphScale,
                                        Rect& drawn) {
    const EditText* t = &field;
    const Font* f = &font;
    const float s = t->height / f->unitsPerEm;
    glyphScale = s;
    auto glyphOf = [&](char16_t ch) {
        for (size_t i = 0; i < f->codes.size(); ++i)
            if (f->codes[i] == ch) return static_cast<int>(i);
        return -1;
    };
    auto advanceOf = [&](int g) {
        return g >= 0 && g < static_cast<int>(f->advances.size()) ? f->advances[g] * s : t->height * 0.5f;
    };
    auto widthOf = [&](const std::u16string& line) {
        float w = 0;
        for (char16_t ch : line) w += advanceOf(glyphOf(ch));
        return w;
    };
    float room = (t->bounds.xMax - t->bounds.xMin) - 80 - t->leftMargin - t->rightMargin;
    // One line per '\r' or '\n', and in a word-wrapping field at the last
    // space that fits; aligned within the field's width.
    std::vector<std::u16string> lines(1);
    for (char16_t ch : decodeText(content, t->html)) {
        if (ch == u'\r' || ch == u'\n') {
            lines.emplace_back();
            continue;
        }
        lines.back() += ch;
        if (t->wordWrap && ch != u' ' && widthOf(lines.back()) > room) {
            std::u16string& line = lines.back();
            size_t sp = line.find_last_of(u' ');
            if (sp != std::u16string::npos && sp > 0) {
                std::u16string rest = line.substr(sp + 1);
                line.erase(sp);
                lines.push_back(rest);
            }
        }
    }
    float ascent = f->ascent > 0 ? f->ascent * s : t->height * 0.8f;
    float lineHeight = (f->ascent + f->descent > 0 ? (f->ascent + f->descent) * s : t->height) + t->leading;
    auto lineStart = [&](float width) {
        float x = t->bounds.xMin + 40 + t->leftMargin;
        if (t->align == 1) x += room - width;
        else if (t->align == 2) x += (room - width) / 2;
        return x;
    };
    // Only where the lines are (with a glyph's height of slack), within
    // the field: a dialogue box typed a letter a pass was a new texture
    // of the whole box each letter.
    drawn = Rect{1e30f, t->bounds.yMin, -1e30f, t->bounds.yMin + 40 + lines.size() * lineHeight + t->height};
    for (const std::u16string& line : lines) {
        float width = widthOf(line), x = lineStart(width);
        drawn.xMin = std::min(drawn.xMin, x - t->height);
        drawn.xMax = std::max(drawn.xMax, x + width + t->height);
    }
    drawn.xMin = std::max(drawn.xMin, t->bounds.xMin);
    drawn.xMax = std::min(drawn.xMax, t->bounds.xMax);
    drawn.yMax = std::min(drawn.yMax, t->bounds.yMax);
    std::vector<PlacedGlyph> out;
    float y = t->bounds.yMin + 40 + ascent;  // Flash's 2-pixel gutter
    for (const std::u16string& line : lines) {
        float x = lineStart(widthOf(line));
        for (char16_t ch : line) {
            int g = glyphOf(ch);
            if (g >= 0 && g < static_cast<int>(f->glyphs.size())) out.push_back({g, x, y});
            x += advanceOf(g);
        }
        y += lineHeight;
    }
    return out;
}

bool Raster::shapeContains(int id, float x, float y) {
    const Shape* found = swf.shape(id);
    if (!found) return false;
    const Shape& s = *found;
    if (x < s.bounds.xMin || x > s.bounds.xMax || y < s.bounds.yMin || y > s.bounds.yMax) return false;
    plutovg_surface_t* surface = plutovg_surface_create(1, 1);
    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);
    bool inside = false;
    for (const ShapeLayer& layer : s.layers) {
        plutovg_canvas_set_fill_rule(canvas, PLUTOVG_FILL_RULE_EVEN_ODD);
        for (const StyledPath& p : layer.fillPaths) {
            plutovg_canvas_new_path(canvas);
            plutovg_path_t* path = plutovg_path_create();
            buildPath(path, p.ops);
            plutovg_canvas_add_path(canvas, path);
            inside = plutovg_canvas_fill_contains(canvas, x, y);
            plutovg_path_destroy(path);
            if (inside) break;
        }
        if (inside) break;
    }
    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);
    return inside;
}

void Raster::endFrame(int frames, int uploadMs) {
    pump(uploadMs);
    ++frame;
    // Shapes read for art no longer drawn are let go after five minutes,
    // while no art is being made from them.
    swf.shapeClock = frame;
    if (frame % 600 == 0 && pending.empty()) swf.unloadShapes(7500);
    // Shape tweens' detail under load (a swarm of big tweening morphs needs
    // new textures every pass): while much art is made frame after frame,
    // morphs are made a scale step coarser (four at most, drawn scaled up),
    // then with half the tween's steps (three times at most), and back to
    // full detail once it has been quiet for a while.
    madeAverage = madeAverage * 0.95 + static_cast<double>(madeThisFrame) * 0.05;
    madeThisFrame = 0;
    if (frame % 50 == 0) {
        if (madeAverage > kBusyBytes && morphDrop < 7) {
            ++morphDrop;
            detailChanged = frame;
        } else if (madeAverage < kBusyBytes / 4 && morphDrop > 0 && frame > detailChanged + 500) {
            --morphDrop;
            detailChanged = frame;
        }
    }
    // (the shared pages' room no art holds counts too: art scattered over
    // pages keeps them all)
    const size_t held = bytes + platform::atlasUnusedBytes();
    if (held > kBudget) {
        // Over the budget: the least recently drawn go, down to three
        // quarters of it. What was drawn in the last second stays
        // (else art in use is made again and again: a stutter) unless the
        // cache is past twice the budget (a swarm of tweening morphs at
        // several sizes); then only this frame's, and what was asked for
        // ahead, stays.
        uint64_t recent = held > 2 * kBudget ? 1 : 25;
        std::vector<std::pair<uint64_t, Key>> old;
        for (const auto& [k, e] : cache)
            if (e.lastUsed + recent < frame && !pending.count(k)) old.emplace_back(e.lastUsed, k);
        // Least recently drawn first, but the art in pages under half full
        // before the rest: what leaves an emptying page lets it go.
        auto sparse = [this](const Key& k) {
            const Texture& t = cache.at(k).tex;
            return t.slot.page && platform::atlasPageFill(t.slot.page) < 0.5f;
        };
        std::sort(old.begin(), old.end(), [&](const auto& a, const auto& b) {
            const bool sa = sparse(a.second), sb = sparse(b.second);
            return sa != sb ? sa : a.first < b.first;
        });
        for (const auto& [used, k] : old) {
            if (bytes + platform::atlasUnusedBytes() <= kBudget / 4 * 3) break;
            drop(cache.find(k));
        }
    }
    if (frame % 60) return;
    for (auto it = cache.begin(); it != cache.end();) {
        auto next = std::next(it);
        if (frame > it->second.lastUsed + static_cast<uint64_t>(frames) && !pending.count(it->first)) drop(it);
        it = next;
    }
}

}  // namespace flash
