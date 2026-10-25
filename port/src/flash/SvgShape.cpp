#include "flash/SvgShape.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

namespace flash {

namespace {

constexpr float kSquare = 819.2f;  // the Flash gradient square, 16384 twips, in pixels

// ------------------------------------------------------------- writing

std::string twips(int32_t v) {
    // v / 20 exactly: at most two decimals.
    char b[32];
    std::snprintf(b, sizeof b, "%.2f", v / 20.0);
    std::string s = b;
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    return s;
}

std::string num(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.9g", v);
    return b;
}

std::string hex(const uint8_t rgba[4]) {
    char b[8];
    std::snprintf(b, sizeof b, "#%02x%02x%02x", rgba[0], rgba[1], rgba[2]);
    return b;
}

std::string opacity(uint8_t a) { return num(a / 255.0); }

std::string pathData(const std::vector<PathOp>& ops) {
    std::string d;
    for (const PathOp& op : ops) {
        if (!d.empty()) d += ' ';
        switch (op.kind) {
        case PathOp::kMove: d += "M" + twips(op.x) + " " + twips(op.y); break;
        case PathOp::kLine: d += "L" + twips(op.x) + " " + twips(op.y); break;
        case PathOp::kCurve: d += "Q" + twips(op.cx) + " " + twips(op.cy) + " " + twips(op.x) + " " + twips(op.y); break;
        }
    }
    return d;
}

bool isGradient(const FillStyle& f) {
    return f.type == FillStyle::kLinear || f.type == FillStyle::kRadial || f.type == FillStyle::kFocal;
}

std::string gradient(const FillStyle& f, const std::string& id) {
    static const char* const spreads[] = {"pad", "reflect", "repeat"};
    const Matrix& m = f.matrix;
    std::string s;
    std::string common = " id=\"" + id + "\" gradientUnits=\"userSpaceOnUse\" gradientTransform=\"matrix(" + num(m.a) + " " +
                         num(m.b) + " " + num(m.c) + " " + num(m.d) + " " + num(m.tx / 20.0) + " " + num(m.ty / 20.0) +
                         ")\" spreadMethod=\"" + spreads[f.spread >= 0 && f.spread <= 2 ? f.spread : 0] + "\" data-tx=\"" +
                         num(m.tx) + "\" data-ty=\"" + num(m.ty) + "\"";
    if (f.type == FillStyle::kLinear) {
        s = "<linearGradient" + common + " x1=\"" + num(-kSquare) + "\" y1=\"0\" x2=\"" + num(kSquare) + "\" y2=\"0\">";
    } else {
        s = "<radialGradient" + common + " cx=\"0\" cy=\"0\" r=\"" + num(kSquare) + "\"";
        if (f.type == FillStyle::kFocal) s += " fx=\"" + num(f.focal * kSquare) + "\" fy=\"0\" data-focal=\"" + num(f.focal) + "\"";
        s += ">";
    }
    for (const GradientStop& st : f.stops)
        s += "<stop offset=\"" + num(st.ratio) + "\" stop-color=\"" + hex(st.rgba) + "\" stop-opacity=\"" + opacity(st.rgba[3]) + "\"/>";
    s += f.type == FillStyle::kLinear ? "</linearGradient>" : "</radialGradient>";
    return s;
}

std::string fillAttrs(const FillStyle& f, const std::string& id) {
    if (f.type == FillStyle::kBitmap) {
        // The bitmap (its id), its pixels -> twips, clipped and smoothed:
        // drawn by the game's rasteriser (an SVG viewer shows no fill).
        std::string s = "fill=\"none\" data-type=\"bitmap\"";
        if (f.bitmap < 0) return s;
        const Matrix& m = f.matrix;
        return s + " data-bitmap=\"" + std::to_string(f.bitmap) + "\" data-matrix=\"" + num(m.a) + " " + num(m.b) + " " +
               num(m.c) + " " + num(m.d) + " " + num(m.tx) + " " + num(m.ty) + "\" data-clipped=\"" +
               (f.clipped ? "1" : "0") + "\" data-smoothed=\"" + (f.smoothed ? "1" : "0") + "\"";
    }
    if (isGradient(f)) return "fill=\"url(#" + id + ")\"";
    return "fill=\"" + hex(f.rgba) + "\" fill-opacity=\"" + opacity(f.rgba[3]) + "\"";
}

std::string lineAttrs(const LineStyle& l, const std::string& id) {
    static const char* const caps[] = {"round", "butt", "square"};
    static const char* const joins[] = {"round", "bevel", "miter"};
    auto cap = [](int c) { return c >= 0 && c <= 2 ? caps[c] : caps[0]; };
    std::string s = "fill=\"none\"";
    if (l.hasFill) {
        s += " stroke=\"" + (isGradient(l.fill) ? "url(#" + id + ")" : hex(l.fill.rgba)) + "\" data-fill-type=\"" +
             std::to_string(l.fill.type) + "\"";
        if (!isGradient(l.fill)) s += " stroke-opacity=\"" + opacity(l.fill.rgba[3]) + "\"";
        s += " data-color=\"" + hex(l.rgba) + "\" data-opacity=\"" + opacity(l.rgba[3]) + "\"";
    } else {
        s += " stroke=\"" + hex(l.rgba) + "\" stroke-opacity=\"" + opacity(l.rgba[3]) + "\"";
    }
    s += " stroke-width=\"" + num(l.width / 20.0) + "\" data-width=\"" + num(l.width) + "\" stroke-linecap=\"" +
         cap(l.startCap) + "\"";
    if (l.endCap != l.startCap) s += " data-endcap=\"" + std::string(cap(l.endCap)) + "\"";
    s += " stroke-linejoin=\"" + std::string(joins[l.join >= 0 && l.join <= 2 ? l.join : 0]) + "\" stroke-miterlimit=\"" +
         num(l.miterLimit) + "\"";
    return s;
}

// ------------------------------------------------------------- reading

struct Tag {
    std::string name;  // "/name" for a closing tag
    std::map<std::string, std::string> attrs;
    bool selfClosing = false;
};

// The tags of the document in order (text between them is ignored).
bool tags(const std::string& s, std::vector<Tag>& out) {
    size_t i = 0;
    while ((i = s.find('<', i)) != std::string::npos) {
        if (s.compare(i, 4, "<!--") == 0) {
            size_t e = s.find("-->", i);
            if (e == std::string::npos) return false;
            i = e + 3;
            continue;
        }
        if (s.compare(i, 2, "<?") == 0 || s.compare(i, 2, "<!") == 0) {
            size_t e = s.find('>', i);
            if (e == std::string::npos) return false;
            i = e + 1;
            continue;
        }
        Tag t;
        ++i;
        size_t n = i;
        while (n < s.size() && !std::isspace(static_cast<unsigned char>(s[n])) && s[n] != '>' && s[n] != '/') ++n;
        if (n < s.size() && s[n] == '/' && n == i) {  // "</name>"
            ++n;
            while (n < s.size() && s[n] != '>') ++n;
        }
        t.name = s.substr(i, n - i);
        i = n;
        for (;;) {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (i >= s.size()) return false;
            if (s[i] == '>') {
                ++i;
                break;
            }
            if (s[i] == '/') {
                t.selfClosing = true;
                ++i;
                continue;
            }
            size_t eq = s.find('=', i);
            if (eq == std::string::npos) return false;
            std::string key = s.substr(i, eq - i);
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
            size_t q = s.find('"', eq);
            if (q == std::string::npos) return false;
            size_t qe = s.find('"', q + 1);
            if (qe == std::string::npos) return false;
            t.attrs[key] = s.substr(q + 1, qe - q - 1);
            i = qe + 1;
        }
        out.push_back(std::move(t));
    }
    return true;
}

std::string attr(const Tag& t, const char* key) {
    auto it = t.attrs.find(key);
    return it == t.attrs.end() ? std::string() : it->second;
}

double attrNum(const Tag& t, const char* key, double fallback = 0) {
    auto it = t.attrs.find(key);
    return it == t.attrs.end() ? fallback : std::atof(it->second.c_str());
}

int32_t toTwips(double px) { return static_cast<int32_t>(std::lround(px * 20)); }

uint8_t toByte(double unit) { return static_cast<uint8_t>(std::clamp(std::lround(unit * 255), 0L, 255L)); }

void parseColor(const std::string& hexText, double opacityValue, uint8_t rgba[4]) {
    unsigned v = hexText.size() == 7 ? static_cast<unsigned>(std::strtoul(hexText.c_str() + 1, nullptr, 16)) : 0;
    rgba[0] = static_cast<uint8_t>(v >> 16);
    rgba[1] = static_cast<uint8_t>(v >> 8);
    rgba[2] = static_cast<uint8_t>(v);
    rgba[3] = toByte(opacityValue);
}

bool parsePath(const std::string& d, std::vector<PathOp>& ops) {
    const char* p = d.c_str();
    auto next = [&](int32_t& v) {
        char* e = nullptr;
        double x = std::strtod(p, &e);
        if (e == p) return false;
        v = toTwips(x);
        p = e;
        return true;
    };
    for (;;) {
        while (*p == ' ' || *p == ',') ++p;
        if (!*p) return true;
        char c = *p++;
        PathOp op{};
        if (c == 'M' || c == 'L') {
            op.kind = c == 'M' ? PathOp::kMove : PathOp::kLine;
            if (!next(op.x) || !next(op.y)) return false;
        } else if (c == 'Q') {
            op.kind = PathOp::kCurve;
            if (!next(op.cx) || !next(op.cy) || !next(op.x) || !next(op.y)) return false;
        } else {
            return false;
        }
        ops.push_back(op);
    }
}

// A gradient definition and its stops, from the tag at `k`.
bool parseGradient(const std::vector<Tag>& t, size_t& k, FillStyle& f) {
    const Tag& g = t[k];
    static const std::map<std::string, int> spreads = {{"pad", 0}, {"reflect", 1}, {"repeat", 2}};
    f.type = g.name == "linearGradient" ? FillStyle::kLinear : g.attrs.count("data-focal") ? FillStyle::kFocal : FillStyle::kRadial;
    if (f.type == FillStyle::kFocal) f.focal = static_cast<float>(attrNum(g, "data-focal"));
    auto sp = spreads.find(attr(g, "spreadMethod"));
    f.spread = sp == spreads.end() ? 0 : sp->second;
    std::string m = attr(g, "gradientTransform");
    double v[6] = {1, 0, 0, 1, 0, 0};
    if (std::sscanf(m.c_str(), "matrix(%lf %lf %lf %lf %lf %lf)", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
    f.matrix = {static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]), static_cast<float>(v[3]),
                static_cast<float>(attrNum(g, "data-tx", v[4] * 20)), static_cast<float>(attrNum(g, "data-ty", v[5] * 20))};
    std::string end = "/" + g.name;
    for (++k; k < t.size() && t[k].name != end; ++k) {
        if (t[k].name != "stop") continue;
        GradientStop st{};
        st.ratio = static_cast<float>(attrNum(t[k], "offset"));
        parseColor(attr(t[k], "stop-color"), attrNum(t[k], "stop-opacity", 1), st.rgba);
        f.stops.push_back(st);
    }
    return k < t.size();
}

FillStyle fillFrom(const Tag& p, const std::map<std::string, FillStyle>& gradients, const std::string& paint,
                   const char* opacityKey) {
    FillStyle f;
    if (paint.rfind("url(#", 0) == 0) {
        auto g = gradients.find(paint.substr(5, paint.size() - 6));
        if (g != gradients.end()) f = g->second;
    } else {
        f.type = FillStyle::kSolid;
        parseColor(paint, attrNum(p, opacityKey, 1), f.rgba);
    }
    return f;
}

}  // namespace

std::string shapeToSvg(const Shape& shape) {
    const Rect& b = shape.bounds;
    std::string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" + num(b.xMin / 20.0) + " " + num(b.yMin / 20.0) +
                    " " + num((b.xMax - b.xMin) / 20.0) + " " + num((b.yMax - b.yMin) / 20.0) + "\" data-bounds=\"" +
                    num(b.xMin) + " " + num(b.yMin) + " " + num(b.xMax) + " " + num(b.yMax) + "\">\n";
    std::string defs;
    for (size_t l = 0; l < shape.layers.size(); ++l) {
        const ShapeLayer& layer = shape.layers[l];
        for (size_t i = 0; i < layer.fills.size(); ++i)
            if (isGradient(layer.fills[i])) defs += gradient(layer.fills[i], "L" + std::to_string(l) + "F" + std::to_string(i)) + "\n";
        for (size_t i = 0; i < layer.lines.size(); ++i)
            if (layer.lines[i].hasFill && isGradient(layer.lines[i].fill))
                defs += gradient(layer.lines[i].fill, "L" + std::to_string(l) + "S" + std::to_string(i)) + "\n";
    }
    if (!defs.empty()) s += "<defs>\n" + defs + "</defs>\n";
    for (size_t l = 0; l < shape.layers.size(); ++l) {
        const ShapeLayer& layer = shape.layers[l];
        s += "<g data-layer=\"" + std::to_string(l) + "\">\n";
        std::vector<bool> used(layer.fills.size());
        for (const StyledPath& p : layer.fillPaths) {
            std::string id = "L" + std::to_string(l) + "F" + std::to_string(p.style);
            if (p.style >= 0 && p.style < static_cast<int>(used.size())) used[p.style] = true;
            const FillStyle& f = layer.fills.at(p.style);
            s += "<path data-fill=\"" + std::to_string(p.style) + "\" " + fillAttrs(f, id) + " fill-rule=\"evenodd\" d=\"" +
                 pathData(p.ops) + "\"/>\n";
        }
        for (size_t i = 0; i < used.size(); ++i)
            if (!used[i])
                s += "<path data-fill=\"" + std::to_string(i) + "\" " +
                     fillAttrs(layer.fills[i], "L" + std::to_string(l) + "F" + std::to_string(i)) + " d=\"\"/>\n";
        std::vector<bool> usedLines(layer.lines.size());
        for (const StyledPath& p : layer.linePaths) {
            if (p.style >= 0 && p.style < static_cast<int>(usedLines.size())) usedLines[p.style] = true;
            s += "<path data-line=\"" + std::to_string(p.style) + "\" " +
                 lineAttrs(layer.lines.at(p.style), "L" + std::to_string(l) + "S" + std::to_string(p.style)) + " d=\"" +
                 pathData(p.ops) + "\"/>\n";
        }
        for (size_t i = 0; i < usedLines.size(); ++i)
            if (!usedLines[i])
                s += "<path data-line=\"" + std::to_string(i) + "\" " +
                     lineAttrs(layer.lines[i], "L" + std::to_string(l) + "S" + std::to_string(i)) + " d=\"\"/>\n";
        s += "</g>\n";
    }
    return s + "</svg>\n";
}

bool shapeFromSvg(const std::string& svg, Shape& shape, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    std::vector<Tag> t;
    if (!tags(svg, t)) return fail("malformed tags");
    shape = Shape{};
    std::map<std::string, FillStyle> gradients;
    for (size_t k = 0; k < t.size(); ++k) {
        if (t[k].name == "svg") {
            double v[4] = {0, 0, 0, 0};
            std::sscanf(attr(t[k], "data-bounds").c_str(), "%lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3]);
            shape.bounds = {static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]), static_cast<float>(v[3])};
        } else if (t[k].name == "linearGradient" || t[k].name == "radialGradient") {
            std::string id = attr(t[k], "id");
            FillStyle f;
            if (!parseGradient(t, k, f)) return fail("bad gradient " + id);
            gradients[id] = f;
        }
    }
    ShapeLayer* layer = nullptr;
    for (const Tag& p : t) {
        if (p.name == "g") {
            shape.layers.emplace_back();
            layer = &shape.layers.back();
            continue;
        }
        if (p.name != "path" || !layer) continue;
        bool isFill = p.attrs.count("data-fill") != 0;
        int style = std::atoi(attr(p, isFill ? "data-fill" : "data-line").c_str());
        if (style < 0) return fail("bad style index");
        if (isFill) {
            if (static_cast<int>(layer->fills.size()) <= style) layer->fills.resize(style + 1);
            FillStyle f = attr(p, "data-type") == "bitmap" ? FillStyle{} : fillFrom(p, gradients, attr(p, "fill"), "fill-opacity");
            if (attr(p, "data-type") == "bitmap") {
                f.type = FillStyle::kBitmap;
                double v[6];
                if (p.attrs.count("data-bitmap") &&
                    std::sscanf(attr(p, "data-matrix").c_str(), "%lf %lf %lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3],
                                &v[4], &v[5]) == 6) {
                    f.bitmap = std::atoi(attr(p, "data-bitmap").c_str());
                    f.matrix = {static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]),
                                static_cast<float>(v[3]), static_cast<float>(v[4]), static_cast<float>(v[5])};
                    f.clipped = attr(p, "data-clipped") != "0";
                    f.smoothed = attr(p, "data-smoothed") != "0";
                }
            }
            layer->fills[style] = f;
        } else {
            if (static_cast<int>(layer->lines.size()) <= style) layer->lines.resize(style + 1);
            LineStyle l;
            static const std::map<std::string, int> caps = {{"round", 0}, {"butt", 1}, {"square", 2}};
            static const std::map<std::string, int> joins = {{"round", 0}, {"bevel", 1}, {"miter", 2}};
            l.width = static_cast<float>(attrNum(p, "data-width", attrNum(p, "stroke-width") * 20));
            auto c = caps.find(attr(p, "stroke-linecap"));
            l.startCap = l.endCap = c == caps.end() ? 0 : c->second;
            if (p.attrs.count("data-endcap")) {
                auto e = caps.find(attr(p, "data-endcap"));
                l.endCap = e == caps.end() ? 0 : e->second;
            }
            auto j = joins.find(attr(p, "stroke-linejoin"));
            l.join = j == joins.end() ? 0 : j->second;
            l.miterLimit = static_cast<float>(attrNum(p, "stroke-miterlimit", 3));
            if (p.attrs.count("data-fill-type")) {
                l.hasFill = true;
                l.fill = fillFrom(p, gradients, attr(p, "stroke"), "stroke-opacity");
                l.fill.type = static_cast<FillStyle::Type>(std::atoi(attr(p, "data-fill-type").c_str()));
                parseColor(attr(p, "data-color"), attrNum(p, "data-opacity", 1), l.rgba);
            } else {
                parseColor(attr(p, "stroke"), attrNum(p, "stroke-opacity", 1), l.rgba);
            }
            layer->lines[style] = l;
        }
        std::string d = attr(p, "d");
        if (d.empty()) continue;
        StyledPath sp;
        sp.style = style;
        if (!parsePath(d, sp.ops)) return fail("bad path data");
        (isFill ? layer->fillPaths : layer->linePaths).push_back(std::move(sp));
    }
    return true;
}

}  // namespace flash
