#include "flash/VectorMesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "flash/Raster.h"
#include "platform/Platform.h"
#include "plutovg.h"
#include "tesselator.h"

namespace flash {

namespace {

constexpr int kMorphSteps = 64;
const uint8_t kWhite[4] = {255, 255, 255, 255};  // (glyphs, coloured by their field)
constexpr float kPi = 3.14159265358979f;
// How far a flattened curve or a round join may stray from the true one, in
// screen pixels (well under the multisampling's eighth of a pixel steps
// that anyone could tell).
constexpr float kTolerance = 0.2f;
constexpr int kRampSize = 256;    // a linear gradient's texels
constexpr int kRadialSize = 128;  // a radial gradient's texels a side

int levelFor(float scale) { return static_cast<int>(std::ceil(std::log2(std::max(scale, 1e-3f)) * 2.0f)); }
float scaleFor(int level) { return std::pow(2.0f, level / 2.0f); }

struct Point {
    float x, y;
};

// A gradient's colour at `t` (0..1 after its spread), premultiplied BGRA.
void gradientAt(const FillStyle& f, float t, unsigned char* out) {
    const std::vector<GradientStop>& s = f.stops;
    float rgba[4];
    if (t <= s.front().ratio) {
        for (int k = 0; k < 4; ++k) rgba[k] = s.front().rgba[k];
    } else if (t >= s.back().ratio) {
        for (int k = 0; k < 4; ++k) rgba[k] = s.back().rgba[k];
    } else {
        size_t i = 1;
        while (i < s.size() && s[i].ratio < t) ++i;
        const GradientStop &a = s[i - 1], &b = s[i];
        const float u = b.ratio > a.ratio ? (t - a.ratio) / (b.ratio - a.ratio) : 0;
        for (int k = 0; k < 4; ++k) rgba[k] = a.rgba[k] + (b.rgba[k] - a.rgba[k]) * u;
    }
    const float a = rgba[3] / 255;
    out[0] = static_cast<unsigned char>(std::lround(rgba[2] * a));
    out[1] = static_cast<unsigned char>(std::lround(rgba[1] * a));
    out[2] = static_cast<unsigned char>(std::lround(rgba[0] * a));
    out[3] = static_cast<unsigned char>(std::lround(rgba[3]));
}

float spreadOf(const FillStyle& f, float t) {
    if (f.spread == 2) return t - std::floor(t);  // repeat
    if (f.spread == 1) {                           // reflect
        float m = std::fmod(std::fabs(t), 2.0f);
        return m > 1 ? 2 - m : m;
    }
    return std::clamp(t, 0.0f, 1.0f);
}

// What the triangles of one fill or stroke take their colour from: a solid
// colour per vertex, or a texture mapped from the art's twips.
struct Paint {
    GLuint texture = 0;
    unsigned char rgba[4] = {255, 255, 255, 255};
    // twips -> texture coordinates
    Matrix toUv;
};

// Builds a mesh, its parts in drawing order.
class Builder {
public:
    Builder(const Swf& swf, float tolerance, float minLine) : swf(swf), tolerance(tolerance), minLine(minLine) {}

    // The shape's layers, each fill then each stroke, under `m` (the shape's
    // twips -> the mesh's); `tint` replaces every colour (glyphs).
    void addShape(const Shape& shape, const Matrix& m, const uint8_t* tint) {
        const float lineScale = std::sqrt(std::fabs(m.a * m.d - m.b * m.c));
        for (const ShapeLayer& layer : shape.layers) {
            for (const StyledPath& p : layer.fillPaths) {
                if (p.style < 0 || p.style >= static_cast<int>(layer.fills.size())) continue;
                Paint paint = paintFor(layer.fills[p.style], m, tint);
                fill(p.ops, m, paint);
            }
            for (const StyledPath& p : layer.linePaths) {
                if (p.style < 0 || p.style >= static_cast<int>(layer.lines.size())) continue;
                const LineStyle& l = layer.lines[p.style];
                Paint paint;
                if (l.hasFill && !tint) paint = paintFor(l.fill, m, nullptr);
                else std::memcpy(paint.rgba, tint ? tint : l.rgba, 4);
                stroke(p.ops, m, l, std::max(l.width * lineScale, minLine), paint);
            }
        }
    }

    std::shared_ptr<const platform::VectorMesh> finish() {
        if (mesh->xy.empty()) {
            for (GLuint t : mesh->textures) platform::releaseTexture(t);
            return nullptr;
        }
        float* b = mesh->bounds;
        b[0] = b[1] = 1e30f;
        b[2] = b[3] = -1e30f;
        for (size_t i = 0; i < mesh->xy.size(); i += 2) {
            b[0] = std::min(b[0], mesh->xy[i]);
            b[1] = std::min(b[1], mesh->xy[i + 1]);
            b[2] = std::max(b[2], mesh->xy[i]);
            b[3] = std::max(b[3], mesh->xy[i + 1]);
        }
        if (!mesh->textured) mesh->uv.clear();
        else mesh->uv.resize(mesh->xy.size());
        mesh->xy.shrink_to_fit();
        mesh->uv.shrink_to_fit();
        mesh->rgba.shrink_to_fit();
        mesh->indices.shrink_to_fit();
        return mesh;
    }

private:
    const Swf& swf;
    float tolerance;  // twips of the mesh
    float minLine;    // twips of the mesh: a hairline's width
    std::shared_ptr<platform::VectorMesh> mesh = std::make_shared<platform::VectorMesh>();
    std::vector<std::vector<Point>> contours;

    // The path's pieces under `m` as polylines, curves flattened.
    void flatten(const std::vector<PathOp>& ops, const Matrix& m) {
        contours.clear();
        Point at{0, 0};
        auto put = [&](float x, float y) {
            Point p;
            m.apply(x, y, p.x, p.y);
            return p;
        };
        for (const PathOp& o : ops) {
            if (o.kind == PathOp::kMove || contours.empty()) {
                contours.emplace_back();
                if (o.kind == PathOp::kMove) {
                    at = put(static_cast<float>(o.x), static_cast<float>(o.y));
                    contours.back().push_back(at);
                    continue;
                }
                contours.back().push_back(at);
            }
            std::vector<Point>& c = contours.back();
            const Point to = put(static_cast<float>(o.x), static_cast<float>(o.y));
            if (o.kind == PathOp::kCurve) {
                const Point ctl = put(static_cast<float>(o.cx), static_cast<float>(o.cy));
                // (a quadratic strays |p0 - 2c + p1| / 4 from its chord; n
                // pieces, n squared times less)
                const float dd = std::hypot(at.x - 2 * ctl.x + to.x, at.y - 2 * ctl.y + to.y);
                const int n = std::clamp(static_cast<int>(std::ceil(std::sqrt(dd / (4 * tolerance)))), 1, 100);
                for (int i = 1; i < n; ++i) {
                    const float t = static_cast<float>(i) / n, u = 1 - t;
                    c.push_back({u * u * at.x + 2 * u * t * ctl.x + t * t * to.x,
                                 u * u * at.y + 2 * u * t * ctl.y + t * t * to.y});
                }
            }
            c.push_back(to);
            at = to;
        }
    }

    // Triangles over shared vertices, made before they go in the mesh.
    struct Piece {
        std::vector<Point> points;
        std::vector<unsigned> indices;
        unsigned add(Point p) {
            points.push_back(p);
            return static_cast<unsigned>(points.size() - 1);
        }
        void triangle(unsigned a, unsigned b, unsigned c) { indices.insert(indices.end(), {a, b, c}); }
        void clear() {
            points.clear();
            indices.clear();
        }
    };
    Piece piece;

    // The piece into the mesh in `paint`: in the last part if it is in that
    // paint and has room, else in a new one (one too big for any part goes
    // triangle by triangle).
    void append(const Paint& paint, const Piece& pc) {
        if (pc.indices.empty()) return;
        if (paint.texture) mesh->textured = true;
        if (pc.points.size() > static_cast<size_t>(platform::kMeshPartVertices)) {
            Piece one;
            for (size_t i = 0; i < pc.indices.size(); i += 3) {
                one.clear();
                one.triangle(one.add(pc.points[pc.indices[i]]), one.add(pc.points[pc.indices[i + 1]]),
                             one.add(pc.points[pc.indices[i + 2]]));
                append(paint, one);
            }
            return;
        }
        const int vertices = static_cast<int>(mesh->xy.size() / 2);
        if (mesh->parts.empty() || mesh->parts.back().texture != paint.texture ||
            mesh->parts.back().vertexCount + static_cast<int>(pc.points.size()) > platform::kMeshPartVertices)
            mesh->parts.push_back({paint.texture, vertices, 0, static_cast<int>(mesh->indices.size()), 0});
        platform::VectorMesh::Part& part = mesh->parts.back();
        if (paint.texture) mesh->uv.resize(mesh->xy.size());
        for (const Point& p : pc.points) {
            mesh->xy.push_back(p.x);
            mesh->xy.push_back(p.y);
            mesh->rgba.insert(mesh->rgba.end(), paint.rgba, paint.rgba + 4);
            if (paint.texture) {
                float u, v;
                paint.toUv.apply(p.x, p.y, u, v);
                mesh->uv.push_back(u);
                mesh->uv.push_back(v);
            }
        }
        for (unsigned i : pc.indices) mesh->indices.push_back(static_cast<unsigned>(vertices) + i);
        part.vertexCount += static_cast<int>(pc.points.size());
        part.indexCount += static_cast<int>(pc.indices.size());
    }

    void fill(const std::vector<PathOp>& ops, const Matrix& m, const Paint& paint) {
        flatten(ops, m);
        TESStesselator* tess = tessNewTess(nullptr);
        if (!tess) return;
        bool any = false;
        for (const std::vector<Point>& c : contours)
            if (c.size() >= 3) {
                tessAddContour(tess, 2, c.data(), sizeof(Point), static_cast<int>(c.size()));
                any = true;
            }
        if (any && tessTesselate(tess, TESS_WINDING_ODD, TESS_POLYGONS, 3, 2, nullptr)) {
            const TESSreal* v = tessGetVertices(tess);
            const TESSindex* e = tessGetElements(tess);
            const int n = tessGetElementCount(tess), count = tessGetVertexCount(tess);
            piece.clear();
            for (int i = 0; i < count; ++i) piece.add({v[i * 2], v[i * 2 + 1]});
            for (int i = 0; i < n; ++i, e += 3)
                if (e[0] != TESS_UNDEF && e[1] != TESS_UNDEF && e[2] != TESS_UNDEF)
                    piece.triangle(static_cast<unsigned>(e[0]), static_cast<unsigned>(e[1]), static_cast<unsigned>(e[2]));
            append(paint, piece);
        }
        tessDeleteTess(tess);
    }

    // Pieces of a round join or cap of radius `r`: steps of at most this angle.
    float arcStep(float r) const {
        if (r <= tolerance) return kPi / 2;
        return std::clamp(2 * std::acos(1 - tolerance / r), 2 * kPi / 64, 2 * kPi / 6);
    }
    // A fan around the point `centre` (an index) from `from` (an index, at
    // angle a0) by `sweep` radians at radius r, ending on `to` (an index).
    void fan(unsigned centre, unsigned from, unsigned to, float a0, float sweep, float r) {
        const Point c = piece.points[centre];
        const int n = std::max(1, static_cast<int>(std::ceil(std::fabs(sweep) / arcStep(r))));
        unsigned prev = from;
        for (int i = 1; i <= n; ++i) {
            const float a = a0 + sweep * i / n;
            const unsigned p = i == n ? to : piece.add({c.x + std::cos(a) * r, c.y + std::sin(a) * r});
            piece.triangle(centre, prev, p);
            prev = p;
        }
    }

    // A stroke: each segment a quad between its ends' two corners. Where the
    // line turns so little that a mitre is within the tolerance of a round
    // or bevelled join (along a flattened curve), both segments share one
    // pair of corners there; elsewhere each has its own and the join fills
    // the outer side between them. Caps at open ends.
    void stroke(const std::vector<PathOp>& ops, const Matrix& m, const LineStyle& l, float width, const Paint& paint) {
        flatten(ops, m);
        const float h = width / 2;
        for (std::vector<Point>& c : contours) {
            // (points one on another dropped)
            c.erase(std::unique(c.begin(), c.end(),
                                [](const Point& a, const Point& b) {
                                    return std::fabs(a.x - b.x) < 1e-3f && std::fabs(a.y - b.y) < 1e-3f;
                                }),
                    c.end());
            piece.clear();
            if (c.size() == 1) {  // a dot
                if (l.startCap == 0) {
                    const unsigned centre = piece.add(c[0]), first = piece.add({c[0].x + h, c[0].y});
                    fan(centre, first, first, 0, 2 * kPi, h);
                }
                append(paint, piece);
                continue;
            }
            const bool closed =
                c.size() > 3 && std::fabs(c.front().x - c.back().x) < 1e-3f && std::fabs(c.front().y - c.back().y) < 1e-3f;
            if (closed) c.pop_back();
            const size_t n = c.size();                  // points
            const size_t segments = closed ? n : n - 1;  // segment i: c[i] -> c[(i + 1) % n]
            std::vector<Point> dir(segments);
            for (size_t i = 0; i < segments; ++i) {
                const Point &a = c[i], &b = c[(i + 1) % n];
                const float dx = b.x - a.x, dy = b.y - a.y, len = std::hypot(dx, dy);
                dir[i] = {dx / len, dy / len};
            }
            // Each segment's corners at its start and at its end (left: along
            // its normal (-dy, dx), right: against it).
            struct Ends {
                unsigned startL, startR, endL, endR;
            };
            std::vector<Ends> ends(segments);
            auto corners = [&](Point p, Point d, unsigned& left, unsigned& right) {
                left = piece.add({p.x - d.y * h, p.y + d.x * h});
                right = piece.add({p.x + d.y * h, p.y - d.x * h});
            };
            for (size_t i = 0; i < n; ++i) {
                const bool start = !closed && i == 0, end = !closed && i == n - 1;
                const Point p = c[i];
                if (start || end) {
                    if (start) corners(p, dir[0], ends[0].startL, ends[0].startR);
                    else corners(p, dir[segments - 1], ends[segments - 1].endL, ends[segments - 1].endR);
                    continue;
                }
                const size_t in = (i + segments - 1) % segments, out = i % segments;
                const Point d0 = dir[in], d1 = dir[out];
                const float cross = d0.x * d1.y - d0.y * d1.x, dot = d0.x * d1.x + d0.y * d1.y;
                const float halfCos = std::sqrt(std::max(0.0f, (1 + dot) / 2));  // cos of half the turn
                if (halfCos > 0.5f && h * (1 / halfCos - 1) <= tolerance) {
                    // (one pair: the mitre, along the normals' mean)
                    float mx = -d0.y - d1.y, my = d0.x + d1.x;
                    const float ml = std::hypot(mx, my);
                    mx = mx / ml * h / halfCos;
                    my = my / ml * h / halfCos;
                    ends[in].endL = ends[out].startL = piece.add({p.x + mx, p.y + my});
                    ends[in].endR = ends[out].startR = piece.add({p.x - mx, p.y - my});
                    continue;
                }
                corners(p, d0, ends[in].endL, ends[in].endR);
                corners(p, d1, ends[out].startL, ends[out].startR);
                // The outer side: away from the turn (its left when turning right).
                const bool left = cross < 0;
                const unsigned e0 = left ? ends[in].endL : ends[in].endR, e1 = left ? ends[out].startL : ends[out].startR;
                const unsigned centre = piece.add(p);
                if (l.join == 0) {
                    const Point a = piece.points[e0], b = piece.points[e1];
                    const float a0 = std::atan2(a.y - p.y, a.x - p.x);
                    float sweep = std::atan2(b.y - p.y, b.x - p.x) - a0;
                    while (sweep > kPi) sweep -= 2 * kPi;
                    while (sweep < -kPi) sweep += 2 * kPi;
                    fan(centre, e0, e1, a0, sweep, h);
                    continue;
                }
                if (l.join == 2 && halfCos > 1e-3f && 1 / halfCos <= l.miterLimit) {
                    const Point a = piece.points[e0], b = piece.points[e1];
                    float mx = (a.x + b.x) / 2 - p.x, my = (a.y + b.y) / 2 - p.y;
                    const float ml = std::hypot(mx, my);
                    if (ml > 1e-6f) {
                        const float k = h / halfCos / ml;
                        const unsigned tip = piece.add({p.x + mx * k, p.y + my * k});
                        piece.triangle(centre, e0, tip);
                        piece.triangle(centre, tip, e1);
                        continue;
                    }
                }
                piece.triangle(centre, e0, e1);  // bevel
            }
            for (const Ends& e : ends) {
                piece.triangle(e.startL, e.startR, e.endL);
                piece.triangle(e.endL, e.startR, e.endR);
            }
            if (!closed) {
                // Caps at both ends, facing out.
                for (int end = 0; end < 2; ++end) {
                    const int cap = end ? l.endCap : l.startCap;
                    if (cap == 1) continue;
                    Point d = end ? dir[segments - 1] : Point{-dir[0].x, -dir[0].y};
                    const Point p = end ? c[n - 1] : c[0];
                    // (the corner on the cap's left and on its right, facing out)
                    const unsigned a = end ? ends[segments - 1].endL : ends[0].startR;
                    const unsigned b = end ? ends[segments - 1].endR : ends[0].startL;
                    if (cap == 0) {
                        const Point pa = piece.points[a];
                        fan(piece.add(p), a, b, std::atan2(pa.y - p.y, pa.x - p.x), -kPi, h);
                    } else {
                        const Point pa = piece.points[a], pb = piece.points[b];
                        const unsigned e = piece.add({pa.x + d.x * h, pa.y + d.y * h});
                        const unsigned f = piece.add({pb.x + d.x * h, pb.y + d.y * h});
                        piece.triangle(a, b, e);
                        piece.triangle(e, b, f);
                    }
                }
            }
            append(paint, piece);
        }
    }

    GLuint upload(int w, int h, std::vector<unsigned char> bgra) {
        platform::toTexelOrder(bgra.data(), static_cast<size_t>(w) * h);
        const GLuint id = platform::newTexture();
        platform::setTexels(id, w, h, true, std::move(bgra));
        mesh->textures.push_back(id);
        return id;
    }

    // `f` under `m`: a solid colour, or its gradient's or bitmap's texture.
    Paint paintFor(const FillStyle& f, const Matrix& m, const uint8_t* tint) {
        Paint p;
        if (tint || f.type == FillStyle::kSolid || (f.type != FillStyle::kBitmap && f.stops.empty())) {
            std::memcpy(p.rgba, tint ? tint : f.type == FillStyle::kSolid ? f.rgba : f.stops.empty() ? f.rgba : f.stops.back().rgba, 4);
            return p;
        }
        const Matrix toFill = (m * f.matrix).inverse();  // mesh twips -> the fill's own space
        if (f.type == FillStyle::kBitmap) {
            auto b = swf.bitmaps.find(f.bitmap);
            plutovg_surface_t* image =
                b == swf.bitmaps.end() || b->second.data.empty()
                    ? nullptr
                    : plutovg_surface_load_from_image_data(b->second.data.data(), static_cast<int>(b->second.data.size()));
            if (!image) {
                p.rgba[0] = p.rgba[1] = p.rgba[2] = 128;
                return p;
            }
            const int w = plutovg_surface_get_width(image), h = plutovg_surface_get_height(image);
            const int stride = plutovg_surface_get_stride(image);
            std::vector<unsigned char> texels(static_cast<size_t>(w) * h * 4);
            const unsigned char* d = plutovg_surface_get_data(image);
            for (int y = 0; y < h; ++y)
                std::memcpy(&texels[static_cast<size_t>(y) * w * 4], d + static_cast<size_t>(y) * stride,
                            static_cast<size_t>(w) * 4);
            plutovg_surface_destroy(image);
            p.texture = upload(w, h, std::move(texels));
            // (the fill's space: the bitmap's pixels)
            Matrix px;
            px.a = 1.0f / w;
            px.d = 1.0f / h;
            p.toUv = px * toFill;
            return p;
        }
        // Gradients: their square (-16384..16384) on the texture.
        if (f.type == FillStyle::kLinear) {
            std::vector<unsigned char> ramp(kRampSize * 4);
            for (int i = 0; i < kRampSize; ++i) gradientAt(f, static_cast<float>(i) / (kRampSize - 1), &ramp[i * 4]);
            p.texture = upload(kRampSize, 1, std::move(ramp));
            // texel i's centre at ratio i / (size - 1); v the row's centre
            Matrix g;
            g.a = (kRampSize - 1.0f) / kRampSize / 32768;
            g.tx = 0.5f / kRampSize + (kRampSize - 1.0f) / kRampSize / 2;
            g.c = g.b = g.d = 0;
            g.ty = 0.5f;
            p.toUv = g * toFill;
            return p;
        }
        // Radial (and focal): the gradient over its square, each texel's
        // ratio worked out where it is.
        std::vector<unsigned char> img(static_cast<size_t>(kRadialSize) * kRadialSize * 4);
        const float fx = std::clamp(f.type == FillStyle::kFocal ? f.focal : 0.0f, -0.99f, 0.99f);
        for (int y = 0; y < kRadialSize; ++y)
            for (int x = 0; x < kRadialSize; ++x) {
                const float px = (x + 0.5f) / kRadialSize * 2 - 1, py = (y + 0.5f) / kRadialSize * 2 - 1;
                float t;
                if (fx == 0) {
                    t = std::hypot(px, py);
                } else {
                    // where the ray from the focal point through (px, py)
                    // meets the circle: t = |p - f| / |edge - f|
                    const float dx = px - fx, dy = py, dd = dx * dx + dy * dy;
                    if (dd < 1e-12f) t = 0;
                    else {
                        const float fd = fx * dx, k = (-fd + std::sqrt(std::max(0.0f, fd * fd - dd * (fx * fx - 1)))) / dd;
                        t = k > 0 ? 1 / k : 1;
                    }
                }
                gradientAt(f, spreadOf(f, t), &img[(static_cast<size_t>(y) * kRadialSize + x) * 4]);
            }
        p.texture = upload(kRadialSize, kRadialSize, std::move(img));
        Matrix g;
        g.a = g.d = 1.0f / 32768;
        g.tx = g.ty = 0.5f;
        p.toUv = g * toFill;
        return p;
    }
};

}  // namespace

MeshCache::MeshCache(const Swf& swf) : swf(swf) {}

MeshCache::~MeshCache() {
    for (auto& [k, e] : cache) drop(e);
}

void MeshCache::drop(const Entry& e) {
    if (!e.mesh) return;
    total -= e.mesh->bytes();
    for (GLuint t : e.mesh->textures) platform::releaseTexture(t);
}

template <class Build>
std::shared_ptr<const platform::VectorMesh> MeshCache::get(const Key& key, Build build) {
    auto it = cache.find(key);
    if (it == cache.end()) {
        // Curves flattened, hairlines as wide as a pixel, at its size step.
        const float twipsPerPixel = 20 / scaleFor(key.level);
        Builder b(swf, kTolerance * twipsPerPixel, twipsPerPixel);
        build(b);
        Entry e{b.finish(), 0};
        if (e.mesh) total += e.mesh->bytes();
        it = cache.emplace(key, std::move(e)).first;
    }
    it->second.lastUsed = frame;
    return it->second.mesh;
}

std::shared_ptr<const platform::VectorMesh> MeshCache::shape(int id, float scale) {
    const Shape* s = swf.shape(id);
    if (!s) return nullptr;
    return get(Key{0, id, 0, levelFor(scale)}, [&](Builder& b) { b.addShape(*s, Matrix{}, nullptr); });
}

std::shared_ptr<const platform::VectorMesh> MeshCache::morph(int id, float ratio, float scale) {
    auto it = swf.morphs.find(id);
    if (it == swf.morphs.end()) return nullptr;
    const int r = static_cast<int>(std::lround(std::clamp(ratio, 0.0f, 1.0f) * kMorphSteps));
    return get(Key{1, id, r, levelFor(scale)}, [&](Builder& b) {
        b.addShape(lerpShape(it->second.first, it->second.second, r / static_cast<float>(kMorphSteps)), Matrix{}, nullptr);
    });
}

std::shared_ptr<const platform::VectorMesh> MeshCache::text(int id, float scale) {
    auto it = swf.texts.find(id);
    if (it == swf.texts.end()) return nullptr;
    const StaticText& t = it->second;
    return get(Key{2, id, 0, levelFor(scale)}, [&](Builder& b) {
        for (const TextRecord& rec : t.records) {
            auto f = swf.fonts.find(rec.font);
            if (f == swf.fonts.end()) continue;
            const float s = rec.height / f->second.unitsPerEm;
            float x = rec.x;
            for (const TextGlyph& g : rec.glyphs) {
                if (g.index >= 0 && g.index < static_cast<int>(f->second.glyphs.size())) {
                    Matrix gm{t.matrix.a * s, t.matrix.b * s, t.matrix.c * s, t.matrix.d * s,
                              t.matrix.a * x + t.matrix.c * rec.y + t.matrix.tx,
                              t.matrix.b * x + t.matrix.d * rec.y + t.matrix.ty};
                    b.addShape(f->second.glyphs[g.index], gm, rec.rgba);
                }
                x += g.advance;
            }
        }
    });
}

std::shared_ptr<const platform::VectorMesh> MeshCache::editText(int id, const std::string& content, float scale) {
    auto it = swf.editTexts.find(id);
    if (it == swf.editTexts.end() || content.empty()) return nullptr;
    const EditText& t = it->second;
    auto f = swf.fonts.find(t.font);
    if (f == swf.fonts.end()) return nullptr;
    const Key key{3, id, 0, levelFor(scale), std::hash<std::string>()(content)};
    if (auto e = cache.find(key); e != cache.end()) {
        e->second.lastUsed = frame;
        return e->second.mesh;
    }
    // Its glyphs' meshes (in the font's units, at the size they are drawn),
    // placed and coloured: a field typed a letter a pass is a copy of a few
    // dozen small meshes each pass, never cut into triangles again.
    float s = 1;
    Rect drawn;
    const std::vector<PlacedGlyph> glyphs = layoutEditText(t, f->second, content, s, drawn);
    auto mesh = std::make_shared<platform::VectorMesh>();
    platform::VectorMesh::Part* part = nullptr;
    for (const PlacedGlyph& g : glyphs) {
        const Shape& glyph = f->second.glyphs[g.glyph];
        const auto one = get(Key{4, t.font << 16 | g.glyph, 0, levelFor(scale * s)},
                             [&](Builder& b) { b.addShape(glyph, Matrix{}, kWhite); });
        if (!one) continue;
        const int base = static_cast<int>(mesh->xy.size() / 2), count = static_cast<int>(one->xy.size() / 2);
        if (!part || part->vertexCount + count > platform::kMeshPartVertices) {
            mesh->parts.push_back({0, base, 0, static_cast<int>(mesh->indices.size()), 0});
            part = &mesh->parts.back();
        }
        for (size_t i = 0; i < one->xy.size(); i += 2) {
            mesh->xy.push_back(one->xy[i] * s + g.x);
            mesh->xy.push_back(one->xy[i + 1] * s + g.y);
        }
        for (int i = 0; i < count; ++i) mesh->rgba.insert(mesh->rgba.end(), t.rgba, t.rgba + 4);  // (its colour)
        for (unsigned i : one->indices) mesh->indices.push_back(static_cast<unsigned>(base) + i);
        part->vertexCount += count;
        part->indexCount += static_cast<int>(one->indices.size());
    }
    Entry e{nullptr, frame};
    if (!mesh->xy.empty()) {
        float* b = mesh->bounds;
        b[0] = b[1] = 1e30f;
        b[2] = b[3] = -1e30f;
        for (size_t i = 0; i < mesh->xy.size(); i += 2) {
            b[0] = std::min(b[0], mesh->xy[i]);
            b[1] = std::min(b[1], mesh->xy[i + 1]);
            b[2] = std::max(b[2], mesh->xy[i]);
            b[3] = std::max(b[3], mesh->xy[i + 1]);
        }
        e.mesh = mesh;
        total += mesh->bytes();
    }
    return cache.emplace(key, std::move(e)).first->second.mesh;
}

void MeshCache::endFrame(int frames) {
    ++frame;
    if (frame % 25) return;
    for (auto it = cache.begin(); it != cache.end();) {
        // (a text field's content not shown again soon goes soon: a field
        // typed out letter by letter leaves one a letter)
        const uint64_t keep = it->first.kind == 3 ? 25 : static_cast<uint64_t>(frames);
        if (it->second.lastUsed + keep < frame) {
            drop(it->second);
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace flash
