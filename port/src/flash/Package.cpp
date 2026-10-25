#include "flash/Package.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

#include "core/Json.h"
#include "flash/SvgShape.h"

namespace flash {

namespace {

namespace fs = std::filesystem;

// ------------------------------------------------------------- writing

void writeMatrix(json::Writer& w, const Matrix& m) {
    w.beginArray().value(m.a).value(m.b).value(m.c).value(m.d).value(m.tx).value(m.ty).endArray();
}

void writeColor(json::Writer& w, const ColorTransform& c) {
    w.beginArray();
    for (float f : c.mult) w.value(f);
    for (float f : c.add) w.value(f);
    w.endArray();
}

void writeRgba(json::Writer& w, const uint8_t rgba[4]) {
    w.beginArray().value(static_cast<int>(rgba[0])).value(static_cast<int>(rgba[1])).value(static_cast<int>(rgba[2])).value(
        static_cast<int>(rgba[3]));
    w.endArray();
}

void writeRect(json::Writer& w, const Rect& r) { w.beginArray().value(r.xMin).value(r.yMin).value(r.xMax).value(r.yMax).endArray(); }

const char* actionName(FrameAction::Kind k) {
    switch (k) {
    case FrameAction::kStop: return "stop";
    case FrameAction::kPlay: return "play";
    case FrameAction::kGoto: return "goto";
    case FrameAction::kGotoLabel: return "gotoLabel";
    case FrameAction::kGotoRandom: return "gotoRandom";
    case FrameAction::kNext: return "next";
    case FrameAction::kPrev: return "prev";
    case FrameAction::kRepeat: return "repeat";
    }
    return "stop";
}

std::string timelineJson(const Timeline& t) {
    json::Writer w;
    w.beginObject().field("frameCount", t.frameCount);
    w.key("labels").beginObject();
    for (const auto& [name, frame] : t.labels) w.field(name, frame);
    w.endObject();
    w.key("frames").beginArray();
    for (const auto& frame : t.frames) {
        w.beginArray();
        for (const FrameOp& op : frame) {
            w.beginObject();
            if (op.kind == FrameOp::kRemove) {
                w.field("remove", op.depth);
            } else {
                const PlaceOp& p = op.place;
                w.field("place", p.depth);
                if (p.move) w.field("move", true);
                if (p.character >= 0) w.field("char", p.character);
                if (p.hasMatrix) {
                    w.key("matrix");
                    writeMatrix(w, p.matrix);
                }
                if (p.hasColor) {
                    w.key("color");
                    writeColor(w, p.color);
                }
                if (p.hasRatio) w.field("ratio", p.ratio);
                if (p.hasName) w.field("name", p.name);
                if (p.hasClip) w.field("clip", p.clipDepth);
                if (p.blend) w.field("blend", p.blend);
                if (!p.filters.empty()) {
                    w.key("filters").beginArray();
                    for (const Filter& f : p.filters) {
                        w.beginObject().field("type", static_cast<int>(f.type));
                        w.key("rgba");
                        writeRgba(w, f.rgba);
                        w.field("blurX", f.blurX).field("blurY", f.blurY).field("strength", f.strength).field("angle", f.angle);
                        w.field("distance", f.distance).field("inner", f.inner).field("knockout", f.knockout).field("passes", f.passes);
                        w.endObject();
                    }
                    w.endArray();
                }
            }
            w.endObject();
        }
        w.endArray();
    }
    w.endArray();
    w.key("scripts").beginArray();
    for (size_t k = 0; k < t.hasScript.size(); ++k)
        if (t.hasScript[k]) w.value(static_cast<int>(k + 1));
    w.endArray();
    w.field("scriptFrames", static_cast<int>(t.hasScript.size()));
    w.key("actions").beginArray();
    for (const auto& frame : t.actions) {
        w.beginArray();
        for (const FrameAction& a : frame) {
            w.beginObject().field("do", actionName(a.kind));
            if (a.frame) w.field("frame", a.frame);
            if (a.count) w.field("count", a.count);
            if (a.play) w.field("play", true);
            if (!a.choices.empty()) {
                w.key("choices").beginArray();
                for (int c : a.choices) w.value(c);
                w.endArray();
            }
            if (!a.label.empty()) w.field("label", a.label);
            w.endObject();
        }
        w.endArray();
    }
    w.endArray();
    w.endObject();
    return w.text();
}

std::string safeName(const std::string& s) {
    std::string out;
    for (char c : s) out += std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' ? c : '_';
    return out.empty() ? "_" : out;
}

void putU32(std::string& s, uint32_t v) {
    for (int k = 0; k < 4; ++k) s += static_cast<char>((v >> (8 * k)) & 0xFF);
}

void putU16(std::string& s, uint16_t v) {
    s += static_cast<char>(v & 0xFF);
    s += static_cast<char>(v >> 8);
}

std::string wav(const Sound& snd) {
    uint16_t channels = snd.stereo ? 2 : 1, bits = snd.is16 ? 16 : 8;
    std::string h = "RIFF";
    putU32(h, static_cast<uint32_t>(36 + snd.data.size()));
    h += "WAVEfmt ";
    putU32(h, 16);
    putU16(h, 1);
    putU16(h, channels);
    putU32(h, static_cast<uint32_t>(snd.rate));
    putU32(h, static_cast<uint32_t>(snd.rate) * channels * bits / 8);
    putU16(h, static_cast<uint16_t>(channels * bits / 8));
    putU16(h, bits);
    h += "data";
    putU32(h, static_cast<uint32_t>(snd.data.size()));
    return h + std::string(snd.data.begin(), snd.data.end());
}

bool writeFile(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(f);
}

template <class Map>
std::vector<int> sortedIds(const Map& m) {
    std::vector<int> ids;
    for (const auto& [id, v] : m) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// ------------------------------------------------------------- reading

std::string text(const std::vector<unsigned char>& b) { return std::string(b.begin(), b.end()); }

Matrix readMatrix(const json::Value& v) {
    return {static_cast<float>(v[0].num()), static_cast<float>(v[1].num()), static_cast<float>(v[2].num()),
            static_cast<float>(v[3].num()), static_cast<float>(v[4].num()), static_cast<float>(v[5].num())};
}

ColorTransform readColor(const json::Value& v) {
    ColorTransform c;
    for (int k = 0; k < 4; ++k) c.mult[k] = static_cast<float>(v[k].num());
    for (int k = 0; k < 4; ++k) c.add[k] = static_cast<float>(v[4 + k].num());
    return c;
}

void readRgba(const json::Value& v, uint8_t rgba[4]) {
    for (int k = 0; k < 4; ++k) rgba[k] = static_cast<uint8_t>(v[k].integer());
}

Rect readRect(const json::Value& v) {
    return {static_cast<float>(v[0].num()), static_cast<float>(v[1].num()), static_cast<float>(v[2].num()),
            static_cast<float>(v[3].num())};
}

FrameAction::Kind actionKind(const std::string& s) {
    static const std::map<std::string, FrameAction::Kind> kinds = {
        {"stop", FrameAction::kStop},     {"play", FrameAction::kPlay},           {"goto", FrameAction::kGoto},
        {"gotoLabel", FrameAction::kGotoLabel}, {"gotoRandom", FrameAction::kGotoRandom}, {"next", FrameAction::kNext},
        {"prev", FrameAction::kPrev},     {"repeat", FrameAction::kRepeat}};
    auto it = kinds.find(s);
    return it == kinds.end() ? FrameAction::kStop : it->second;
}

Timeline readTimeline(const json::Value& v) {
    Timeline t;
    t.frameCount = v["frameCount"].integer(1);
    for (const auto& [name, frame] : v["labels"].members) t.labels[name] = frame.integer();
    for (const json::Value& frame : v["frames"].items) {
        std::vector<FrameOp> ops;
        for (const json::Value& o : frame.items) {
            FrameOp op{};
            if (o.has("remove")) {
                op.kind = FrameOp::kRemove;
                op.depth = o["remove"].integer();
            } else {
                op.kind = FrameOp::kPlace;
                PlaceOp& p = op.place;
                p.depth = o["place"].integer();
                p.move = o["move"].flag();
                p.character = o.has("char") ? o["char"].integer() : -1;
                if ((p.hasMatrix = o.has("matrix"))) p.matrix = readMatrix(o["matrix"]);
                if ((p.hasColor = o.has("color"))) p.color = readColor(o["color"]);
                if ((p.hasRatio = o.has("ratio"))) p.ratio = static_cast<float>(o["ratio"].num());
                if ((p.hasName = o.has("name"))) p.name = o["name"].str();
                if ((p.hasClip = o.has("clip"))) p.clipDepth = o["clip"].integer();
                p.blend = o["blend"].integer();
                for (const json::Value& f : o["filters"].items) {
                    Filter fl;
                    fl.type = static_cast<Filter::Type>(f["type"].integer());
                    readRgba(f["rgba"], fl.rgba);
                    fl.blurX = static_cast<float>(f["blurX"].num());
                    fl.blurY = static_cast<float>(f["blurY"].num());
                    fl.strength = static_cast<float>(f["strength"].num());
                    fl.angle = static_cast<float>(f["angle"].num());
                    fl.distance = static_cast<float>(f["distance"].num());
                    fl.inner = f["inner"].flag();
                    fl.knockout = f["knockout"].flag();
                    fl.passes = f["passes"].integer(1);
                    p.filters.push_back(fl);
                }
            }
            ops.push_back(std::move(op));
        }
        t.frames.push_back(std::move(ops));
    }
    t.hasScript.assign(static_cast<size_t>(v["scriptFrames"].integer()), false);
    for (const json::Value& f : v["scripts"].items) {
        size_t k = static_cast<size_t>(f.integer() - 1);
        if (k < t.hasScript.size()) t.hasScript[k] = true;
    }
    for (const json::Value& frame : v["actions"].items) {
        std::vector<FrameAction> list;
        for (const json::Value& a : frame.items) {
            FrameAction fa{actionKind(a["do"].str())};
            fa.frame = a["frame"].integer();
            fa.count = a["count"].integer();
            fa.play = a["play"].flag();
            for (const json::Value& c : a["choices"].items) fa.choices.push_back(c.integer());
            fa.label = a["label"].str();
            list.push_back(std::move(fa));
        }
        t.actions.push_back(std::move(list));
    }
    return t;
}

}  // namespace

bool writePackage(const Swf& movie, const std::string& dir, std::string* report) {
    fs::path root(dir);
    std::error_code ec;
    for (const char* sub : {"vector", "audio"}) fs::remove_all(root / sub, ec);
    fs::remove(root / "library.json", ec);

    // The names sounds are saved under: their linkage, else their id.
    std::map<int, std::string> exported;
    for (const auto& [name, id] : movie.exports) {
        auto it = exported.find(id);
        if (it == exported.end() || name < it->second) exported[id] = name;
    }

    for (int id : sortedIds(movie.shapes))
        if (!writeFile(root / "vector" / "shapes" / (std::to_string(id) + ".svg"), shapeToSvg(*movie.shape(id)))) return false;
    for (int id : sortedIds(movie.morphs)) {
        const auto& m = movie.morphs.at(id);
        writeFile(root / "vector" / "morphs" / (std::to_string(id) + "_start.svg"), shapeToSvg(m.first));
        writeFile(root / "vector" / "morphs" / (std::to_string(id) + "_end.svg"), shapeToSvg(m.second));
    }
    for (int id : sortedIds(movie.sprites))
        writeFile(root / "vector" / "symbols" / (std::to_string(id) + ".json"), timelineJson(movie.sprites.at(id)));
    for (int id : sortedIds(movie.bitmaps)) {
        const std::vector<uint8_t>& b = movie.bitmaps.at(id).data;
        writeFile(root / "vector" / "bitmaps" / (std::to_string(id) + ".jpg"), std::string(b.begin(), b.end()));
    }

    {
        json::Writer w(true);
        w.beginObject();
        for (int id : sortedIds(movie.buttons)) {
            w.key(std::to_string(id)).beginArray();
            for (const ButtonRecord& r : movie.buttons.at(id).records) {
                w.beginObject().field("states", r.states).field("char", r.character).field("depth", r.depth);
                w.key("matrix");
                writeMatrix(w, r.matrix);
                w.key("color");
                writeColor(w, r.color);
                w.endObject();
            }
            w.endArray();
        }
        w.endObject();
        writeFile(root / "vector" / "buttons.json", w.text());
    }
    {
        json::Writer w(true);
        w.beginObject();
        for (int id : sortedIds(movie.texts)) {
            const StaticText& t = movie.texts.at(id);
            w.key(std::to_string(id)).beginObject();
            w.key("bounds");
            writeRect(w, t.bounds);
            w.key("matrix");
            writeMatrix(w, t.matrix);
            w.key("records").beginArray();
            for (const TextRecord& r : t.records) {
                w.beginObject().field("font", r.font);
                w.key("rgba");
                writeRgba(w, r.rgba);
                w.field("x", r.x).field("y", r.y).field("height", r.height);
                w.key("glyphs").beginArray();
                for (const TextGlyph& g : r.glyphs) w.beginArray().value(g.index).value(g.advance).endArray();
                w.endArray().endObject();
            }
            w.endArray().endObject();
        }
        w.endObject();
        writeFile(root / "vector" / "texts.json", w.text());
    }
    {
        json::Writer w(true);
        w.beginObject();
        for (int id : sortedIds(movie.editTexts)) {
            const EditText& e = movie.editTexts.at(id);
            w.key(std::to_string(id)).beginObject();
            w.key("bounds");
            writeRect(w, e.bounds);
            w.field("font", e.font).field("height", e.height);
            w.key("rgba");
            writeRgba(w, e.rgba);
            w.field("align", e.align).field("leftMargin", e.leftMargin).field("rightMargin", e.rightMargin);
            w.field("indent", e.indent).field("leading", e.leading).field("wordWrap", e.wordWrap).field("multiline", e.multiline);
            w.field("html", e.html).field("border", e.border).field("noSelect", e.noSelect);
            w.field("variable", e.variable).field("text", e.text);
            w.endObject();
        }
        w.endObject();
        writeFile(root / "vector" / "edittexts.json", w.text());
    }
    for (int id : sortedIds(movie.fonts)) {
        const Font& f = movie.fonts.at(id);
        json::Writer w(true);
        w.beginObject().field("name", f.name).field("bold", f.bold).field("italic", f.italic).field("unitsPerEm", f.unitsPerEm);
        w.field("ascent", f.ascent).field("descent", f.descent).field("leading", f.leading);
        w.key("codes").beginArray();
        for (uint16_t c : f.codes) w.value(static_cast<int>(c));
        w.endArray();
        w.key("advances").beginArray();
        for (float a : f.advances) w.value(a);
        w.endArray();
        w.key("glyphs").beginArray();
        for (const Shape& g : f.glyphs) w.value(shapeToSvg(g));
        w.endArray();
        w.endObject();
        writeFile(root / "vector" / "fonts" / (std::to_string(id) + ".json"), w.text());
    }

    std::set<std::string> used;
    json::Writer audio(true);
    audio.beginArray();
    for (int id : sortedIds(movie.sounds)) {
        const Sound& s = movie.sounds.at(id);
        std::string base = exported.count(id) ? safeName(exported[id]) : "sound_" + std::to_string(id);
        std::string name = base;
        for (int k = 2; used.count(name); ++k) name = base + "_" + std::to_string(k);
        used.insert(name);
        std::string ext = s.format == 2 ? ".mp3" : s.format == 0 || s.format == 3 ? ".wav" : ".bin";
        std::string file = "audio/" + name + ext;
        writeFile(root / file, ext == ".wav" ? wav(s) : std::string(s.data.begin(), s.data.end()));
        audio.beginObject().field("id", id).field("file", file).field("format", s.format).field("rate", s.rate);
        audio.field("is16", s.is16).field("stereo", s.stereo).field("samples", static_cast<long long>(s.samples));
        audio.field("seek", s.seekSamples).endObject();
    }
    audio.endArray();
    writeFile(root / "audio" / "index.json", audio.text());

    json::Writer lib(true);
    lib.beginObject().field("format", 1).field("version", movie.version).field("width", movie.width).field("height", movie.height);
    lib.field("fps", movie.fps);
    lib.key("background").beginArray();
    for (uint8_t c : movie.background) lib.value(static_cast<int>(c));
    lib.endArray();
    lib.key("exports").beginObject();
    std::vector<std::pair<std::string, int>> names(movie.exports.begin(), movie.exports.end());
    std::sort(names.begin(), names.end());
    for (const auto& [name, id] : names) lib.field(name, id);
    lib.endObject();
    auto ids = [&](const char* key, const std::vector<int>& list) {
        lib.key(key).beginArray();
        for (int id : list) lib.value(id);
        lib.endArray();
    };
    ids("shapes", sortedIds(movie.shapes));
    ids("morphs", sortedIds(movie.morphs));
    ids("sprites", sortedIds(movie.sprites));
    ids("fonts", sortedIds(movie.fonts));
    // (Only a movie with bitmaps lists them: the others' packages as before.)
    if (!movie.bitmaps.empty()) ids("bitmaps", sortedIds(movie.bitmaps));
    lib.endObject();
    if (!writeFile(root / "library.json", lib.text())) return false;

    if (report)
        *report = std::to_string(movie.shapes.size()) + " shapes, " + std::to_string(movie.morphs.size()) + " morphs, " +
                  std::to_string(movie.sprites.size()) + " symbols, " + std::to_string(movie.buttons.size()) + " buttons, " +
                  std::to_string(movie.texts.size() + movie.editTexts.size()) + " texts, " +
                  std::to_string(movie.fonts.size()) + " fonts, " + std::to_string(movie.sounds.size()) + " sounds";
    return true;
}

std::unique_ptr<Swf> loadPackage(const FileReader& read, std::string* error) {
    auto fail = [&](const std::string& why) -> std::unique_ptr<Swf> {
        if (error) *error = why;
        return nullptr;
    };
    std::vector<unsigned char> bytes;
    json::Value lib;
    std::string err;
    if (!read("library.json", bytes) || !json::parse(text(bytes), lib, &err)) return fail("library.json: " + err);
    auto movie = std::make_unique<Swf>();
    Swf& m = *movie;
    m.version = lib["version"].integer();
    m.width = static_cast<float>(lib["width"].num());
    m.height = static_cast<float>(lib["height"].num());
    m.fps = static_cast<float>(lib["fps"].num());
    for (int k = 0; k < 3; ++k) m.background[k] = static_cast<uint8_t>(lib["background"][k].integer());
    for (const auto& [name, id] : lib["exports"].members) m.exports[name] = id.integer();

    // Shapes are read when first drawn (Swf::shape): most of a game's art
    // is not on screen at any one time.
    for (const json::Value& v : lib["shapes"].items) m.shapes[v.integer()];
    m.readShape = [read](int id, Shape& out) {
        std::vector<unsigned char> svg;
        return read("vector/shapes/" + std::to_string(id) + ".svg", svg) &&
               shapeFromSvg(std::string(svg.begin(), svg.end()), out, nullptr);
    };
    for (const json::Value& v : lib["morphs"].items) {
        int id = v.integer();
        auto& pair = m.morphs[id];
        for (int k = 0; k < 2; ++k) {
            std::string path = "vector/morphs/" + std::to_string(id) + (k ? "_end.svg" : "_start.svg");
            if (!read(path, bytes) || !shapeFromSvg(text(bytes), k ? pair.second : pair.first, &err)) return fail(path + ": " + err);
        }
    }
    for (const json::Value& v : lib["bitmaps"].items) {
        int id = v.integer();
        std::string path = "vector/bitmaps/" + std::to_string(id) + ".jpg";
        if (!read(path, bytes)) return fail(path + ": missing");
        m.bitmaps[id].data.assign(bytes.begin(), bytes.end());
    }
    for (const json::Value& v : lib["sprites"].items) {
        int id = v.integer();
        std::string path = "vector/symbols/" + std::to_string(id) + ".json";
        json::Value t;
        if (!read(path, bytes) || !json::parse(text(bytes), t, &err)) return fail(path + ": " + err);
        m.sprites[id] = readTimeline(t);
    }

    json::Value doc;
    if (!read("vector/buttons.json", bytes) || !json::parse(text(bytes), doc, &err)) return fail("buttons.json: " + err);
    for (const auto& [key, recs] : doc.members) {
        Button& b = m.buttons[std::atoi(key.c_str())];
        for (const json::Value& r : recs.items)
            b.records.push_back({r["states"].integer(), r["char"].integer(), r["depth"].integer(), readMatrix(r["matrix"]),
                                 readColor(r["color"])});
    }
    if (!read("vector/texts.json", bytes) || !json::parse(text(bytes), doc, &err)) return fail("texts.json: " + err);
    for (const auto& [key, v] : doc.members) {
        StaticText& t = m.texts[std::atoi(key.c_str())];
        t.bounds = readRect(v["bounds"]);
        t.matrix = readMatrix(v["matrix"]);
        for (const json::Value& r : v["records"].items) {
            TextRecord rec;
            rec.font = r["font"].integer(-1);
            readRgba(r["rgba"], rec.rgba);
            rec.x = static_cast<float>(r["x"].num());
            rec.y = static_cast<float>(r["y"].num());
            rec.height = static_cast<float>(r["height"].num());
            for (const json::Value& g : r["glyphs"].items) rec.glyphs.push_back({g[0].integer(), static_cast<float>(g[1].num())});
            t.records.push_back(std::move(rec));
        }
    }
    if (!read("vector/edittexts.json", bytes) || !json::parse(text(bytes), doc, &err)) return fail("edittexts.json: " + err);
    for (const auto& [key, v] : doc.members) {
        EditText& e = m.editTexts[std::atoi(key.c_str())];
        e.bounds = readRect(v["bounds"]);
        e.font = v["font"].integer(-1);
        e.height = static_cast<float>(v["height"].num());
        readRgba(v["rgba"], e.rgba);
        e.align = v["align"].integer();
        e.leftMargin = static_cast<float>(v["leftMargin"].num());
        e.rightMargin = static_cast<float>(v["rightMargin"].num());
        e.indent = static_cast<float>(v["indent"].num());
        e.leading = static_cast<float>(v["leading"].num());
        e.wordWrap = v["wordWrap"].flag();
        e.multiline = v["multiline"].flag();
        e.html = v["html"].flag();
        e.border = v["border"].flag();
        e.noSelect = v["noSelect"].flag();
        e.variable = v["variable"].str();
        e.text = v["text"].str();
    }
    for (const json::Value& v : lib["fonts"].items) {
        int id = v.integer();
        std::string path = "vector/fonts/" + std::to_string(id) + ".json";
        json::Value f;
        if (!read(path, bytes) || !json::parse(text(bytes), f, &err)) return fail(path + ": " + err);
        Font& font = m.fonts[id];
        font.name = f["name"].str();
        font.bold = f["bold"].flag();
        font.italic = f["italic"].flag();
        font.unitsPerEm = static_cast<float>(f["unitsPerEm"].num(1024));
        font.ascent = static_cast<float>(f["ascent"].num());
        font.descent = static_cast<float>(f["descent"].num());
        font.leading = static_cast<float>(f["leading"].num());
        for (const json::Value& c : f["codes"].items) font.codes.push_back(static_cast<uint16_t>(c.integer()));
        for (const json::Value& a : f["advances"].items) font.advances.push_back(static_cast<float>(a.num()));
        for (const json::Value& g : f["glyphs"].items) {
            font.glyphs.emplace_back();
            if (!shapeFromSvg(g.str(), font.glyphs.back(), &err)) return fail(path + " glyph: " + err);
        }
    }

    if (!read("audio/index.json", bytes) || !json::parse(text(bytes), doc, &err)) return fail("audio/index.json: " + err);
    for (const json::Value& v : doc.items) {
        Sound& s = m.sounds[v["id"].integer()];
        s.format = v["format"].integer();
        s.rate = v["rate"].integer();
        s.is16 = v["is16"].flag();
        s.stereo = v["stereo"].flag();
        s.samples = static_cast<uint32_t>(v["samples"].num());
        s.seekSamples = v["seek"].integer();
        const std::string& file = v["file"].str();
        if (!read(file, bytes)) return fail(file + " missing");
        if (file.size() > 4 && file.compare(file.size() - 4, 4, ".wav") == 0) {
            // The samples are the "data" chunk.
            size_t at = 12;
            while (at + 8 <= bytes.size()) {
                uint32_t size = bytes[at + 4] | bytes[at + 5] << 8 | bytes[at + 6] << 16 | static_cast<uint32_t>(bytes[at + 7]) << 24;
                if (std::memcmp(bytes.data() + at, "data", 4) == 0) {
                    s.data.assign(bytes.begin() + static_cast<long>(at + 8),
                                  bytes.begin() + static_cast<long>(std::min<size_t>(at + 8 + size, bytes.size())));
                    break;
                }
                at += 8 + size + (size & 1);
            }
        } else {
            s.data = std::move(bytes);
            bytes.clear();
        }
    }
    return movie;
}

}  // namespace flash
