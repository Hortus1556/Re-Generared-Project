#include "flash/Swf.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "stb_image.h"  // stbi_zlib_decode_malloc: the compressed (CWS) movies

namespace flash {

// ---------------------------------------------------------------- maths

Matrix Matrix::operator*(const Matrix& n) const {
    Matrix r;
    r.a = a * n.a + c * n.b;
    r.b = b * n.a + d * n.b;
    r.c = a * n.c + c * n.d;
    r.d = b * n.c + d * n.d;
    r.tx = a * n.tx + c * n.ty + tx;
    r.ty = b * n.tx + d * n.ty + ty;
    return r;
}

Matrix Matrix::inverse() const {
    float det = a * d - b * c;
    Matrix r;
    if (std::fabs(det) < 1e-12f) return r;
    r.a = d / det;
    r.b = -b / det;
    r.c = -c / det;
    r.d = a / det;
    r.tx = -(r.a * tx + r.c * ty);
    r.ty = -(r.b * tx + r.d * ty);
    return r;
}

ColorTransform ColorTransform::operator*(const ColorTransform& inner) const {
    ColorTransform r;
    for (int i = 0; i < 4; ++i) {
        r.mult[i] = inner.mult[i] * mult[i];
        r.add[i] = inner.add[i] * mult[i] + add[i];
    }
    return r;
}

bool ColorTransform::isIdentity() const {
    for (int i = 0; i < 4; ++i)
        if (mult[i] != 1 || add[i] != 0) return false;
    return true;
}

// --------------------------------------------------------------- reading

struct Swf::Reader {
    const unsigned char* d;
    size_t size, pos = 0;
    uint32_t bitBuf = 0;
    int bitCount = 0;

    void align() { bitCount = 0; }
    bool ok(size_t n) const { return pos + n <= size; }
    uint8_t u8() {
        align();
        return pos < size ? d[pos++] : 0;
    }
    uint16_t u16() {
        uint16_t lo = u8();
        return static_cast<uint16_t>(lo | (u8() << 8));
    }
    int16_t s16() { return static_cast<int16_t>(u16()); }
    uint32_t u32() {
        uint32_t v = u16();
        return v | (static_cast<uint32_t>(u16()) << 16);
    }
    float fixed8() { return s16() / 256.0f; }
    float fixed() { return static_cast<int32_t>(u32()) / 65536.0f; }
    float f32() {
        uint32_t v = u32();
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
    uint32_t ub(int n) {
        uint32_t v = 0;
        while (n-- > 0) {
            if (bitCount == 0) {
                bitBuf = pos < size ? d[pos++] : 0;
                bitCount = 8;
            }
            v = (v << 1) | ((bitBuf >> --bitCount) & 1);
        }
        return v;
    }
    int32_t sb(int n) {
        if (n == 0) return 0;
        uint32_t v = ub(n);
        if (v & (1u << (n - 1))) v |= ~((1u << n) - 1);
        return static_cast<int32_t>(v);
    }
    float fb(int n) { return sb(n) / 65536.0f; }
    std::string str() {
        align();
        std::string s;
        while (pos < size && d[pos]) s += static_cast<char>(d[pos++]);
        ++pos;
        return s;
    }
    Rect rect() {
        align();
        int n = static_cast<int>(ub(5));
        Rect r;
        r.xMin = static_cast<float>(sb(n));
        r.xMax = static_cast<float>(sb(n));
        r.yMin = static_cast<float>(sb(n));
        r.yMax = static_cast<float>(sb(n));
        align();
        return r;
    }
    Matrix matrix() {
        align();
        Matrix m;
        if (ub(1)) {
            int n = static_cast<int>(ub(5));
            m.a = fb(n);
            m.d = fb(n);
        }
        if (ub(1)) {
            int n = static_cast<int>(ub(5));
            m.b = fb(n);
            m.c = fb(n);
        }
        int n = static_cast<int>(ub(5));
        m.tx = static_cast<float>(sb(n));
        m.ty = static_cast<float>(sb(n));
        align();
        return m;
    }
    ColorTransform cxform(bool alpha) {
        align();
        bool hasAdd = ub(1), hasMult = ub(1);
        int n = static_cast<int>(ub(4));
        ColorTransform c;
        int k = alpha ? 4 : 3;
        if (hasMult)
            for (int i = 0; i < k; ++i) c.mult[i] = sb(n) / 256.0f;
        if (hasAdd)
            for (int i = 0; i < k; ++i) c.add[i] = static_cast<float>(sb(n));
        align();
        return c;
    }
    void rgb(uint8_t* c) {
        c[0] = u8();
        c[1] = u8();
        c[2] = u8();
        c[3] = 255;
    }
    void rgba(uint8_t* c) {
        for (int i = 0; i < 4; ++i) c[i] = u8();
    }
};

// ------------------------------------------------------------ shape build

namespace {

struct Pt {
    int32_t x, y;
    bool operator==(const Pt& o) const { return x == o.x && y == o.y; }
};

// One edge of a shape record list, with its geometry at the start and (for
// morph shapes) at the end of the morph.
struct Edge {
    int fill0, fill1, line;  // 1-based styles of the current layer, 0 none
    bool curve;
    Pt p0[2], c[2], p1[2];   // [0] start geometry, [1] end geometry
};

struct LayerEdges {
    std::vector<Edge> edges;
};

uint64_t key(Pt p) { return (static_cast<uint64_t>(static_cast<uint32_t>(p.x)) << 32) | static_cast<uint32_t>(p.y); }

// Turns a layer's edges into closed contours per fill (fill1 on one side,
// fill0 on the other: fill0 edges are reversed so that each fill's edges
// join head to tail) and polylines per line style. Linking uses the start
// geometry; `geometries` paths are built in step from each geometry, so a
// morph's start and end paths have the same structure.
void buildLayer(const LayerEdges& in, int geometries, ShapeLayer* out[2]) {
    struct Seg {
        int edge;
        bool reversed;
    };
    std::map<int, std::vector<Seg>> perFill;
    for (int i = 0; i < static_cast<int>(in.edges.size()); ++i) {
        const Edge& e = in.edges[i];
        if (e.fill1) perFill[e.fill1].push_back({i, false});
        if (e.fill0) perFill[e.fill0].push_back({i, true});
    }
    auto from = [&](const Seg& s, int g) { return s.reversed ? in.edges[s.edge].p1[g] : in.edges[s.edge].p0[g]; };
    auto to = [&](const Seg& s, int g) { return s.reversed ? in.edges[s.edge].p0[g] : in.edges[s.edge].p1[g]; };
    for (auto& [style, segs] : perFill) {
        std::unordered_multimap<uint64_t, int> starts;
        for (int i = 0; i < static_cast<int>(segs.size()); ++i) starts.emplace(key(from(segs[i], 0)), i);
        std::vector<bool> used(segs.size(), false);
        StyledPath paths[2];
        for (int g = 0; g < geometries; ++g) paths[g].style = style - 1;
        for (int first = 0; first < static_cast<int>(segs.size()); ++first) {
            if (used[first]) continue;
            Pt begin = from(segs[first], 0);
            for (int g = 0; g < geometries; ++g) {
                Pt b = from(segs[first], g);
                paths[g].ops.push_back({PathOp::kMove, 0, 0, b.x, b.y});
            }
            int cur = first;
            while (cur >= 0) {
                used[cur] = true;
                const Edge& e = in.edges[segs[cur].edge];
                for (int g = 0; g < geometries; ++g) {
                    Pt t = to(segs[cur], g);
                    if (e.curve) paths[g].ops.push_back({PathOp::kCurve, e.c[g].x, e.c[g].y, t.x, t.y});
                    else paths[g].ops.push_back({PathOp::kLine, 0, 0, t.x, t.y});
                }
                Pt end = to(segs[cur], 0);
                if (end == begin) break;
                int next = -1;
                auto range = starts.equal_range(key(end));
                for (auto it = range.first; it != range.second; ++it)
                    if (!used[it->second]) {
                        next = it->second;
                        break;
                    }
                cur = next;
            }
        }
        for (int g = 0; g < geometries; ++g) out[g]->fillPaths.push_back(std::move(paths[g]));
    }
    // Strokes: runs of joined edges with the same line style.
    StyledPath lines[2];
    int curLine = 0;
    Pt last{INT32_MIN, INT32_MIN};
    auto flush = [&] {
        if (curLine && !lines[0].ops.empty())
            for (int g = 0; g < geometries; ++g) out[g]->linePaths.push_back(std::move(lines[g]));
        for (int g = 0; g < geometries; ++g) lines[g] = StyledPath{};
    };
    for (const Edge& e : in.edges) {
        if (!e.line) continue;
        if (e.line != curLine || !(e.p0[0] == last)) {
            flush();
            curLine = e.line;
            for (int g = 0; g < geometries; ++g) {
                lines[g].style = e.line - 1;
                lines[g].ops.push_back({PathOp::kMove, 0, 0, e.p0[g].x, e.p0[g].y});
            }
        }
        for (int g = 0; g < geometries; ++g) {
            if (e.curve) lines[g].ops.push_back({PathOp::kCurve, e.c[g].x, e.c[g].y, e.p1[g].x, e.p1[g].y});
            else lines[g].ops.push_back({PathOp::kLine, 0, 0, e.p1[g].x, e.p1[g].y});
        }
        last = e.p1[0];
    }
    flush();
}

}  // namespace

// Fill and line style arrays of a DefineShape version (1..4).
static void readFill(Swf::Reader&, int, FillStyle&);

namespace {

void readGradient(Swf::Reader& r, int shapeVersion, FillStyle& f, bool focal) {
    r.align();
    f.spread = static_cast<int>(r.ub(2));
    r.ub(2);  // interpolation (linear RGB not supported)
    int n = static_cast<int>(r.ub(4));
    for (int i = 0; i < n; ++i) {
        GradientStop s;
        s.ratio = r.u8() / 255.0f;
        if (shapeVersion >= 3) r.rgba(s.rgba);
        else r.rgb(s.rgba);
        f.stops.push_back(s);
    }
    if (focal) f.focal = r.fixed8();
}

}  // namespace

static void readFill(Swf::Reader& r, int v, FillStyle& f) {
    int type = r.u8();
    if (type == 0x00) {
        f.type = FillStyle::kSolid;
        if (v >= 3) r.rgba(f.rgba);
        else r.rgb(f.rgba);
    } else if (type == 0x10 || type == 0x12 || type == 0x13) {
        f.type = type == 0x10 ? FillStyle::kLinear : type == 0x12 ? FillStyle::kRadial : FillStyle::kFocal;
        f.matrix = r.matrix();
        readGradient(r, v, f, type == 0x13);
    } else {  // 0x40-0x43: a bitmap, repeated or clipped, smoothed or not
        f.type = FillStyle::kBitmap;
        f.bitmap = r.u16();
        f.matrix = r.matrix();
        f.clipped = (type & 1) != 0;
        f.smoothed = (type & 2) == 0;
    }
}

namespace {

void readStyles(Swf::Reader& r, int v, ShapeLayer& layer) {
    int n = r.u8();
    if (n == 0xFF && v >= 2) n = r.u16();
    layer.fills.resize(n);
    for (auto& f : layer.fills) readFill(r, v, f);
    n = r.u8();
    if (n == 0xFF && v >= 2) n = r.u16();
    layer.lines.resize(n);
    for (auto& l : layer.lines) {
        l.width = r.u16();
        if (v == 4) {
            r.align();
            l.startCap = static_cast<int>(r.ub(2));
            l.join = static_cast<int>(r.ub(2));
            l.hasFill = r.ub(1);
            r.ub(3);  // no h scale, no v scale, pixel hinting
            r.ub(5);  // reserved
            r.ub(1);  // no close
            l.endCap = static_cast<int>(r.ub(2));
            if (l.join == 2) l.miterLimit = r.fixed8();
            if (l.hasFill) {
                readFill(r, v, l.fill);
                if (l.fill.type == FillStyle::kSolid) std::memcpy(l.rgba, l.fill.rgba, 4);
            } else {
                r.rgba(l.rgba);
            }
        } else if (v == 3) {
            r.rgba(l.rgba);
        } else {
            r.rgb(l.rgba);
        }
    }
}

// Shape records into per-layer edges. `styles`: read new style arrays into
// a new layer (DefineShape2+); null for glyphs and morph end edges.
// With `geometry` 1 the coordinates go into the end geometry of `into`'s
// existing edges (morph end edges, matched edge by edge).
void readRecords(Swf::Reader& r, int v, std::vector<ShapeLayer>* layers, std::vector<LayerEdges>& edges) {
    r.align();
    int fillBits = static_cast<int>(r.ub(4)), lineBits = static_cast<int>(r.ub(4));
    int fill0 = 0, fill1 = 0, line = 0;
    int32_t x = 0, y = 0;
    if (edges.empty()) edges.emplace_back();
    for (;;) {
        if (r.ub(1) == 0) {
            int flags = static_cast<int>(r.ub(5));
            if (flags == 0) break;
            if (flags & 1) {
                int n = static_cast<int>(r.ub(5));
                x = r.sb(n);
                y = r.sb(n);
            }
            if (flags & 2) fill0 = static_cast<int>(r.ub(fillBits));
            if (flags & 4) fill1 = static_cast<int>(r.ub(fillBits));
            if (flags & 8) line = static_cast<int>(r.ub(lineBits));
            if (flags & 16) {
                if (layers) {
                    layers->emplace_back();
                    readStyles(r, v, layers->back());
                }
                edges.emplace_back();
                r.align();
                fillBits = static_cast<int>(r.ub(4));
                lineBits = static_cast<int>(r.ub(4));
            }
            continue;
        }
        Edge e{};
        e.fill0 = fill0;
        e.fill1 = fill1;
        e.line = line;
        e.p0[0] = e.p0[1] = {x, y};
        if (r.ub(1)) {  // straight
            int n = static_cast<int>(r.ub(4)) + 2;
            int32_t dx = 0, dy = 0;
            if (r.ub(1)) {
                dx = r.sb(n);
                dy = r.sb(n);
            } else if (r.ub(1)) {
                dy = r.sb(n);
            } else {
                dx = r.sb(n);
            }
            e.curve = false;
            x += dx;
            y += dy;
        } else {
            int n = static_cast<int>(r.ub(4)) + 2;
            int32_t cdx = r.sb(n), cdy = r.sb(n), adx = r.sb(n), ady = r.sb(n);
            e.curve = true;
            e.c[0] = e.c[1] = {x + cdx, y + cdy};
            x += cdx + adx;
            y += cdy + ady;
        }
        e.p1[0] = e.p1[1] = {x, y};
        edges.back().edges.push_back(e);
    }
}

}  // namespace

void Swf::parseShape(Reader& r, int tag, int id) {
    int v = tag == 2 ? 1 : tag == 22 ? 2 : tag == 32 ? 3 : 4;
    Shape s;
    s.bounds = r.rect();
    if (v == 4) {
        r.rect();  // edge bounds
        r.u8();    // flags
    }
    s.layers.emplace_back();
    readStyles(r, v, s.layers[0]);
    std::vector<LayerEdges> edges;
    readRecords(r, v, &s.layers, edges);
    for (size_t i = 0; i < s.layers.size() && i < edges.size(); ++i) {
        ShapeLayer* out[2] = {&s.layers[i], nullptr};
        buildLayer(edges[i], 1, out);
    }
    shapes[id] = std::move(s);
}

void Swf::parseMorph(Reader& r, int tag, size_t end) {
    int id = r.u16();
    Shape start, finish;
    start.bounds = r.rect();
    finish.bounds = r.rect();
    if (tag == 84) {
        r.rect();
        r.rect();
        r.u8();
    }
    uint32_t offset = r.u32();
    size_t endEdges = r.pos + offset;
    ShapeLayer s0, s1;
    int n = r.u8();
    if (n == 0xFF) n = r.u16();
    for (int i = 0; i < n; ++i) {
        FillStyle a, b;
        int type = r.u8();
        if (type == 0x00) {
            r.rgba(a.rgba);
            r.rgba(b.rgba);
        } else if (type == 0x10 || type == 0x12 || type == 0x13) {
            a.type = b.type = type == 0x10 ? FillStyle::kLinear : type == 0x12 ? FillStyle::kRadial : FillStyle::kFocal;
            a.matrix = r.matrix();
            b.matrix = r.matrix();
            r.align();
            int k = r.u8() & 15;
            for (int j = 0; j < k; ++j) {
                GradientStop ga, gb;
                ga.ratio = r.u8() / 255.0f;
                r.rgba(ga.rgba);
                gb.ratio = r.u8() / 255.0f;
                r.rgba(gb.rgba);
                a.stops.push_back(ga);
                b.stops.push_back(gb);
            }
            if (type == 0x13) {
                a.focal = r.fixed8();
                b.focal = r.fixed8();
            }
        } else {
            a.type = b.type = FillStyle::kBitmap;
            a.bitmap = b.bitmap = r.u16();
            a.matrix = r.matrix();
            b.matrix = r.matrix();
            a.clipped = b.clipped = (type & 1) != 0;
            a.smoothed = b.smoothed = (type & 2) == 0;
        }
        s0.fills.push_back(a);
        s1.fills.push_back(b);
    }
    n = r.u8();
    if (n == 0xFF) n = r.u16();
    for (int i = 0; i < n; ++i) {
        LineStyle a, b;
        a.width = r.u16();
        b.width = r.u16();
        if (tag == 84) {
            r.align();
            a.startCap = b.startCap = static_cast<int>(r.ub(2));
            a.join = b.join = static_cast<int>(r.ub(2));
            bool hasFill = r.ub(1);
            r.ub(3);  // no h scale, no v scale, pixel hinting
            r.ub(5);  // reserved
            r.ub(1);  // no close
            a.endCap = b.endCap = static_cast<int>(r.ub(2));
            if (a.join == 2) a.miterLimit = b.miterLimit = r.fixed8();
            if (hasFill) {  // a gradient line: its first colours stand in
                int type = r.u8();
                if (type == 0) {
                    r.rgba(a.rgba);
                    r.rgba(b.rgba);
                } else {
                    r.pos = end;  // not in these movies
                    return;
                }
            } else {
                r.rgba(a.rgba);
                r.rgba(b.rgba);
            }
        } else {
            r.rgba(a.rgba);
            r.rgba(b.rgba);
        }
        s0.lines.push_back(a);
        s1.lines.push_back(b);
    }
    std::vector<LayerEdges> startEdges, endEdges_;
    readRecords(r, 3, nullptr, startEdges);
    r.pos = endEdges;
    readRecords(r, 3, nullptr, endEdges_);
    // Pair the end edges with the start edges, one by one: a straight edge
    // paired with a curve becomes a curve through its midpoint.
    LayerEdges merged;
    const auto& se = startEdges[0].edges;
    std::vector<Edge> ee;
    for (auto& l : endEdges_) ee.insert(ee.end(), l.edges.begin(), l.edges.end());
    for (size_t i = 0; i < se.size(); ++i) {
        Edge e = se[i];
        if (i < ee.size()) {
            const Edge& f = ee[i];
            e.p0[1] = f.p0[0];
            e.p1[1] = f.p1[0];
            if (e.curve || f.curve) {
                if (!e.curve) e.c[0] = {(e.p0[0].x + e.p1[0].x) / 2, (e.p0[0].y + e.p1[0].y) / 2};
                e.c[1] = f.curve ? f.c[0] : Pt{(f.p0[0].x + f.p1[0].x) / 2, (f.p0[0].y + f.p1[0].y) / 2};
                e.curve = true;
            }
        }
        merged.edges.push_back(e);
    }
    ShapeLayer* out[2] = {&s0, &s1};
    buildLayer(merged, 2, out);
    start.layers.push_back(std::move(s0));
    finish.layers.push_back(std::move(s1));
    morphs[id] = {std::move(start), std::move(finish)};
    r.pos = end;
}

// --------------------------------------------------------- other characters

void Swf::parseButton2(Reader& r, size_t end) {
    int id = r.u16();
    r.u8();   // track as menu
    r.u16();  // action offset
    Button b;
    while (r.pos < end) {
        uint8_t flags = r.u8();
        if (flags == 0) break;
        ButtonRecord rec;
        rec.states = flags & 15;
        rec.character = r.u16();
        rec.depth = r.u16();
        rec.matrix = r.matrix();
        rec.color = r.cxform(true);
        if (flags & 16) {  // filters: skipped
            int n = r.u8();
            for (int i = 0; i < n; ++i) {
                int f = r.u8();
                static const int sizes[] = {23, 9, 15, 27, -1, -1, 80, -1};
                int sz = f < 8 ? sizes[f] : -1;
                if (sz < 0) {
                    r.pos = end;
                    break;
                }
                r.pos += sz;
            }
        }
        if (flags & 32) r.u8();  // blend mode
        b.records.push_back(rec);
    }
    buttons[id] = std::move(b);
    r.pos = end;
}

void Swf::parseText(Reader& r, int tag) {
    int id = r.u16();
    StaticText t;
    t.bounds = r.rect();
    t.matrix = r.matrix();
    int glyphBits = r.u8(), advanceBits = r.u8();
    TextRecord cur;
    for (;;) {
        uint8_t flags = r.u8();
        if (flags == 0) break;
        if (flags & 8) cur.font = r.u16();
        if (flags & 4) {
            if (tag == 33) r.rgba(cur.rgba);
            else r.rgb(cur.rgba);
        }
        if (flags & 1) cur.x = r.s16();
        if (flags & 2) cur.y = r.s16();
        if (flags & 8) cur.height = r.u16();
        int n = r.u8();
        cur.glyphs.clear();
        for (int i = 0; i < n; ++i) {
            TextGlyph g;
            g.index = static_cast<int>(r.ub(glyphBits));
            g.advance = static_cast<float>(r.sb(advanceBits));
            cur.glyphs.push_back(g);
        }
        r.align();
        t.records.push_back(cur);
        for (auto& g : cur.glyphs) cur.x += g.advance;  // records may continue on the same line
    }
    texts[id] = std::move(t);
}

void Swf::parseEditText(Reader& r) {
    int id = r.u16();
    EditText t;
    t.bounds = r.rect();
    uint8_t f1 = r.u8(), f2 = r.u8();
    bool hasText = f1 & 0x80, hasColor = f1 & 0x04, hasMax = f1 & 0x02, hasFont = f1 & 0x01;
    t.wordWrap = f1 & 0x40;
    t.multiline = f1 & 0x20;
    bool hasFontClass = f2 & 0x80, hasLayout = f2 & 0x20;
    t.noSelect = f2 & 0x10;
    t.border = f2 & 0x08;
    t.html = f2 & 0x02;
    if (hasFont) t.font = r.u16();
    if (hasFontClass) r.str();
    if (hasFont) t.height = r.u16();
    if (hasColor) r.rgba(t.rgba);
    if (hasMax) r.u16();
    if (hasLayout) {
        t.align = r.u8();
        t.leftMargin = r.u16();
        t.rightMargin = r.u16();
        t.indent = r.u16();
        t.leading = r.s16();
    }
    t.variable = r.str();
    if (hasText) t.text = r.str();
    editTexts[id] = std::move(t);
}

void Swf::parseFont(Reader& r, int tag, size_t end) {
    int id = r.u16();
    Font f;
    uint8_t flags = r.u8();
    bool hasLayout = flags & 0x80, wideOffsets = flags & 0x08, wideCodes = flags & 0x04;
    f.italic = flags & 0x02;
    f.bold = flags & 0x01;
    r.u8();  // language
    int nameLen = r.u8();
    for (int i = 0; i < nameLen; ++i) f.name += static_cast<char>(r.u8());
    if (!f.name.empty() && f.name.back() == 0) f.name.pop_back();
    int n = r.u16();
    size_t table = r.pos;
    std::vector<uint32_t> offsets(n);
    for (auto& o : offsets) o = wideOffsets ? r.u32() : r.u16();
    uint32_t codeOffset = n ? (wideOffsets ? r.u32() : r.u16()) : 0;
    f.unitsPerEm = tag == 75 ? 1024 * 20 : 1024;
    for (int i = 0; i < n; ++i) {
        r.pos = table + offsets[i];
        Shape g;
        g.layers.emplace_back();
        FillStyle black;
        g.layers[0].fills.push_back(black);
        std::vector<LayerEdges> edges;
        readRecords(r, 1, nullptr, edges);
        ShapeLayer* out[2] = {&g.layers[0], nullptr};
        if (!edges.empty()) buildLayer(edges[0], 1, out);
        f.glyphs.push_back(std::move(g));
    }
    if (n) r.pos = table + codeOffset;
    for (int i = 0; i < n; ++i) f.codes.push_back(wideCodes ? r.u16() : r.u8());
    if (hasLayout && r.pos < end) {
        f.ascent = r.u16();
        f.descent = r.u16();
        f.leading = r.s16();
        for (int i = 0; i < n; ++i) f.advances.push_back(r.s16());
    }
    fonts[id] = std::move(f);
    r.pos = end;
}

void Swf::parseSound(Reader& r, size_t end) {
    int id = r.u16();
    Sound s;
    r.align();
    s.format = static_cast<int>(r.ub(4));
    static const int rates[] = {5512, 11025, 22050, 44100};
    s.rate = rates[r.ub(2)];
    s.is16 = r.ub(1);
    s.stereo = r.ub(1);
    s.samples = r.u32();
    if (s.format == 2) s.seekSamples = r.s16();
    s.data.assign(r.d + r.pos, r.d + end);
    sounds[id] = std::move(s);
    r.pos = end;
}

PlaceOp Swf::parsePlace(Reader& r, int tag, size_t end) {
    PlaceOp p;
    uint8_t f1 = r.u8(), f2 = tag == 70 ? r.u8() : 0;
    p.depth = r.u16();
    p.move = f1 & 1;
    if (tag == 70 && ((f2 & 8) || ((f2 & 16) && (f1 & 2)))) r.str();  // class name
    if (f1 & 2) p.character = r.u16();
    if (f1 & 4) {
        p.hasMatrix = true;
        p.matrix = r.matrix();
    }
    if (f1 & 8) {
        p.hasColor = true;
        p.color = r.cxform(true);
    }
    if (f1 & 16) {
        p.hasRatio = true;
        p.ratio = r.u16() / 65535.0f;
    }
    if (f1 & 32) {
        p.hasName = true;
        p.name = r.str();
    }
    if (f1 & 64) {
        p.hasClip = true;
        p.clipDepth = r.u16();
    }
    if (tag == 70) {
        if (f2 & 1) {
            int n = r.u8();
            for (int i = 0; i < n && r.pos < end; ++i) {
                Filter flt;
                int id = r.u8();
                if (id == 0 || id == 2) {  // drop shadow, glow
                    flt.type = id == 0 ? Filter::kDropShadow : Filter::kGlow;
                    r.rgba(flt.rgba);
                    flt.blurX = r.fixed();
                    flt.blurY = r.fixed();
                    if (id == 0) {
                        flt.angle = r.fixed();
                        flt.distance = r.fixed();
                    }
                    flt.strength = r.fixed8();
                    r.align();
                    flt.inner = r.ub(1);
                    flt.knockout = r.ub(1);
                    r.ub(1);
                    flt.passes = static_cast<int>(r.ub(5));
                } else if (id == 1) {
                    flt.type = Filter::kBlur;
                    flt.blurX = r.fixed();
                    flt.blurY = r.fixed();
                    r.align();
                    flt.passes = static_cast<int>(r.ub(5));
                    r.ub(3);
                } else if (id == 3) {
                    r.pos += 27;
                } else if (id == 6) {
                    r.pos += 80;
                } else {
                    r.pos = end;  // gradient filters, convolution: not in these movies
                    return p;
                }
                p.filters.push_back(flt);
            }
        }
        if (f2 & 2) p.blend = r.u8();
    }
    return p;
}

// ------------------------------------------------------------------ tags

namespace {

// A DoAction's plain frame actions (see FrameAction); none if the script
// branches, defines functions or acts on another clip.
std::vector<FrameAction> frameActions(const unsigned char* p, size_t n) {
    std::vector<FrameAction> out;
    // Constants pushed, to read gotoAndStop(random(N) + K): a number, or a
    // random(N) (+K) still on the stack.
    struct Slot {
        double value = 0;
        int randomCount = 0;  // > 0: random(randomCount) + value
        bool known = false;
    };
    std::vector<Slot> stack;
    auto pop = [&] {
        Slot s;
        if (!stack.empty()) {
            s = stack.back();
            stack.pop_back();
        }
        return s;
    };
    size_t i = 0;
    while (i < n) {
        uint8_t code = p[i++];
        if (code == 0) break;
        size_t len = 0;
        if (code >= 0x80) {
            if (i + 2 > n) break;
            len = p[i] | (p[i + 1] << 8);
            i += 2;
        }
        const unsigned char* data = p + i;
        if (i + len > n) break;
        i += len;
        switch (code) {
        case 0x07: out.push_back({FrameAction::kStop}); break;
        case 0x06: out.push_back({FrameAction::kPlay}); break;
        case 0x04: out.push_back({FrameAction::kNext}); break;
        case 0x05: out.push_back({FrameAction::kPrev}); break;
        case 0x81:
            if (len >= 2) out.push_back({FrameAction::kGoto, (data[0] | (data[1] << 8)) + 1});
            break;
        case 0x96:  // push
            for (size_t k = 0; k < len;) {
                uint8_t type = data[k++];
                Slot s;
                if (type == 7 && k + 4 <= len) {  // int
                    s.value = static_cast<int32_t>(data[k] | (data[k + 1] << 8) | (data[k + 2] << 16) | (data[k + 3] << 24));
                    s.known = true;
                    k += 4;
                } else if (type == 6 && k + 8 <= len) {  // double (high word first)
                    unsigned char b[8] = {data[k + 4], data[k + 5], data[k + 6], data[k + 7],
                                          data[k], data[k + 1], data[k + 2], data[k + 3]};
                    std::memcpy(&s.value, b, 8);
                    s.known = true;
                    k += 8;
                } else if (type == 1 && k + 4 <= len) {  // float
                    float f;
                    std::memcpy(&f, data + k, 4);
                    s.value = f;
                    s.known = true;
                    k += 4;
                } else if (type == 0) {
                    while (k < len && data[k]) ++k;
                    ++k;
                } else if (type == 4 || type == 5 || type == 8) {
                    k += 1;
                } else if (type == 9) {
                    k += 2;
                }
                stack.push_back(s);
            }
            break;
        case 0x30: {  // random(n)
            Slot n0 = pop();
            Slot r;
            r.known = n0.known;
            r.randomCount = n0.known ? static_cast<int>(n0.value) : 0;
            stack.push_back(r);
            break;
        }
        case 0x0A: case 0x47: {  // add
            Slot b = pop(), a = pop();
            Slot r;
            r.known = a.known && b.known && !(a.randomCount && b.randomCount);
            r.randomCount = a.randomCount + b.randomCount;
            r.value = a.value + b.value;
            stack.push_back(r);
            break;
        }
        case 0x9F: {  // gotoAndStop/gotoAndPlay(expression)
            Slot f = pop();
            bool play = len >= 1 && (data[0] & 1);
            if (!f.known) return {};
            if (f.randomCount > 0) {
                FrameAction a{FrameAction::kGotoRandom, static_cast<int>(f.value), f.randomCount, play};
                out.push_back(a);
            } else {
                out.push_back({FrameAction::kGoto, static_cast<int>(f.value)});
                if (play) out.push_back({FrameAction::kPlay});
            }
            break;
        }
        case 0x8C: {
            FrameAction a{FrameAction::kGotoLabel};
            a.label.assign(reinterpret_cast<const char*>(data), strnlen(reinterpret_cast<const char*>(data), len));
            out.push_back(a);
            break;
        }
        // Branches, functions, with, other targets: logic.
        case 0x9D: case 0x99: case 0x9B: case 0x8E: case 0x94: case 0x8B: case 0x20:
            return {};
        default:
            // Anything else works on values we do not follow.
            stack.assign(1, Slot{});
            break;
        }
    }
    return out;
}

}  // namespace

void Swf::parseTags(Reader& r, size_t end, Timeline& t, bool main) {
    std::vector<FrameOp> ops;
    std::vector<FrameAction> actions;
    bool script = false;
    while (r.pos + 2 <= end) {
        r.align();
        uint16_t code = r.u16();
        int tag = code >> 6;
        size_t len = code & 63;
        if (len == 63) len = r.u32();
        size_t start = r.pos, next = start + len;
        if (next > end) break;
        switch (tag) {
        case 0: next = end; break;
        case 1:
            t.frames.push_back(std::move(ops));
            t.hasScript.push_back(script);
            t.actions.push_back(std::move(actions));
            ops.clear();
            actions.clear();
            script = false;
            break;
        case 2: case 22: case 32: case 83: {
            int id = r.u16();
            parseShape(r, tag, id);
            break;
        }
        case 46: case 84: parseMorph(r, tag, next); break;
        case 34: parseButton2(r, next); break;
        case 11: case 33: parseText(r, tag); break;
        case 37: parseEditText(r); break;
        case 48: case 75: parseFont(r, tag, next); break;
        case 14: parseSound(r, next); break;
        case 21: {  // DefineBitsJPEG2: the JPEG file as is
            int id = r.u16();
            const uint8_t* b = r.d + r.pos;
            size_t n = next - r.pos;
            // (Some carry an empty end-and-start marker pair first.)
            if (n >= 4 && b[0] == 0xFF && b[1] == 0xD9 && b[2] == 0xFF && b[3] == 0xD8) b += 4, n -= 4;
            bitmaps[id].data.assign(b, b + n);
            break;
        }
        case 9:
            if (main) {
                uint8_t c[4];
                r.rgb(c);
                std::memcpy(background, c, 3);
            }
            break;
        case 26: case 70: {
            FrameOp op{FrameOp::kPlace};
            op.place = parsePlace(r, tag, next);
            ops.push_back(std::move(op));
            break;
        }
        case 5: case 28: {
            FrameOp op{FrameOp::kRemove};
            if (tag == 5) r.u16();
            op.depth = r.u16();
            ops.push_back(std::move(op));
            break;
        }
        case 43: {
            // Flash finds frame labels whatever their case: kept lower-case.
            // A label on several frames goes to its first, as Flash's goto
            // does: AS1's extraction wound has "worm" on frame 10 (the worm)
            // and again on 17 (a copy of the crystal's), and the last one
            // pulled a crystal out of patient 16.
            std::string label = r.str();
            for (char& c : label) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            t.labels.emplace(label, static_cast<int>(t.frames.size()) + 1);
            break;
        }
        case 12: {
            script = true;
            std::vector<FrameAction> a = frameActions(r.d + start, len);
            actions.insert(actions.end(), a.begin(), a.end());
            break;
        }
        case 39: {
            int id = r.u16();
            Timeline& s = sprites[id];
            s.frameCount = r.u16();
            parseTags(r, next, s, false);
            if (s.frames.empty()) {
                s.frames.emplace_back();
                s.hasScript.push_back(false);
                s.actions.emplace_back();
            }
            break;
        }
        case 56:    // ExportAssets (AS1/AS2 linkage names)
        case 76: {  // SymbolClass (AS3 class names, the same layout)
            int n = r.u16();
            for (int i = 0; i < n; ++i) {
                int id = r.u16();
                exports[r.str()] = id;
            }
            break;
        }
        default: break;
        }
        r.pos = next;
    }
    if (!ops.empty() || !actions.empty()) {
        t.frames.push_back(std::move(ops));
        t.hasScript.push_back(script);
        t.actions.push_back(std::move(actions));
    }
}

std::unique_ptr<Swf> Swf::load(const std::vector<unsigned char>& bytes) {
    if (bytes.size() < 8 || bytes[1] != 'W' || bytes[2] != 'S' || (bytes[0] != 'F' && bytes[0] != 'C')) return nullptr;
    std::vector<unsigned char> data;
    if (bytes[0] == 'C') {
        int outLen = 0;
        char* out = stbi_zlib_decode_malloc(reinterpret_cast<const char*>(bytes.data() + 8),
                                            static_cast<int>(bytes.size() - 8), &outLen);
        if (!out) return nullptr;
        data.assign(bytes.begin(), bytes.begin() + 8);
        data.insert(data.end(), out, out + outLen);
        free(out);
    } else {
        data = bytes;
    }
    auto swf = std::make_unique<Swf>();
    swf->version = data[3];
    Reader r{data.data(), data.size()};
    r.pos = 8;
    Rect stage = r.rect();
    swf->width = (stage.xMax - stage.xMin) / 20.0f;
    swf->height = (stage.yMax - stage.yMin) / 20.0f;
    r.u8();
    swf->fps = static_cast<float>(r.u8());
    {
        // fps is 8.8 fixed: the low byte read first is the fraction
        size_t p = r.pos - 2;
        swf->fps = data[p + 1] + data[p] / 256.0f;
    }
    Timeline& main = swf->sprites[0];
    main.frameCount = r.u16();
    swf->parseTags(r, data.size(), main, true);
    return swf;
}

const Shape* Swf::shape(int id) const {
    auto it = shapes.find(id);
    if (it == shapes.end()) return nullptr;
    if (readShape) {
        ShapeState& s = shapeStates[id];
        s.used = shapeClock;
        if (!s.read) {
            s.read = true;
            Shape loaded;
            if (readShape(id, loaded)) it->second = std::move(loaded);
        }
    }
    return &it->second;
}

size_t Swf::unloadShapes(uint64_t unused) const {
    size_t n = 0;
    for (auto& [id, s] : shapeStates)
        if (s.read && s.used + unused < shapeClock) {
            s.read = false;
            std::vector<ShapeLayer>().swap(shapes.at(id).layers);
            ++n;
        }
    return n;
}

Swf::Kind Swf::kind(int id) const {
    if (shapes.count(id)) return Kind::kShape;
    if (morphs.count(id)) return Kind::kMorph;
    if (sprites.count(id)) return Kind::kSprite;
    if (buttons.count(id)) return Kind::kButton;
    if (texts.count(id)) return Kind::kText;
    if (editTexts.count(id)) return Kind::kEditText;
    if (fonts.count(id)) return Kind::kFont;
    if (sounds.count(id)) return Kind::kSound;
    return Kind::kNone;
}

}  // namespace flash
