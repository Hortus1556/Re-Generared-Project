#include "as3/EditorLevel.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <cstring>
#include <set>

#include "as3/AreasDefUtils.h"
#include "as3/Content.h"
#include "as3/Log.h"
#include "as3/SurgeryData.h"
#include "engine/cocos/CCFileUtils.h"
#include "platform/Platform.h"
#include "stb_image.h"

// stb_image_write's zlib compressor (compiled in platform/ThirdParty.cpp)
extern "C" unsigned char* stbi_zlib_compress(unsigned char* data, int data_len, int* out_len, int quality);

namespace mt {
namespace editor {

namespace {

// ---- reading, checked ----------------------------------------------------

// The first problem found while reading a level (the message the player sees).
struct Check {
    std::string error;
    bool ok() const { return error.empty(); }
    void fail(const std::string& where, const std::string& what) {
        if (error.empty()) error = where.empty() ? what : where + ": " + what;
    }
};

bool isInt(const json::Value& v, double lo, double hi) {
    return v.type == json::Value::kNumber && std::isfinite(v.number) && v.number >= lo && v.number <= hi;
}

int readInt(Check& c, const json::Value& obj, const char* key, double lo, double hi, int fallback,
            const std::string& where, bool required = false) {
    if (!obj.has(key)) {
        if (required) c.fail(where, std::string("\"") + key + "\" is missing");
        return fallback;
    }
    const json::Value& v = obj[key];
    if (!isInt(v, lo, hi)) {
        char range[64];
        std::snprintf(range, sizeof range, " must be a number from %g to %g", lo, hi);
        c.fail(where, std::string("\"") + key + "\"" + range);
        return fallback;
    }
    return static_cast<int>(std::lround(v.number));
}

bool readBool(Check& c, const json::Value& obj, const char* key, bool fallback, const std::string& where) {
    if (!obj.has(key)) return fallback;
    const json::Value& v = obj[key];
    if (v.type != json::Value::kBool) {
        c.fail(where, std::string("\"") + key + "\" must be true or false");
        return fallback;
    }
    return v.boolean;
}

std::string readString(Check& c, const json::Value& obj, const char* key, size_t maxLen, const std::string& where,
                       bool required = false) {
    if (!obj.has(key) || obj[key].isNull()) {
        if (required) c.fail(where, std::string("\"") + key + "\" is missing");
        return "";
    }
    const json::Value& v = obj[key];
    if (v.type != json::Value::kString || v.string.size() > maxLen) {
        c.fail(where, std::string("\"") + key + "\" must be a short text");
        return "";
    }
    return v.string;
}

// a name the game gives an injury: letters, digits, '_' and '-'
bool goodName(const std::string& s) {
    if (s.empty() || s.size() > static_cast<size_t>(kMaxName)) return false;
    for (unsigned char ch : s)
        if (!std::isalnum(ch) && ch != '_' && ch != '-') return false;
    return true;
}

// a title the game's fonts can show: printable ASCII
bool goodTitle(const std::string& s) {
    if (s.empty() || s.size() > static_cast<size_t>(kMaxTitle)) return false;
    for (unsigned char ch : s)
        if (ch < 32 || ch > 126) return false;
    return true;
}

void onlyKeys(Check& c, const json::Value& obj, std::initializer_list<const char*> keys, const std::string& where,
              bool twiceOk = false) {
    // (a key given twice is refused too: readers differ on which one counts;
    // the shipped files have some, read as the game reads them, the first)
    std::set<std::string> seen;
    for (const auto& m : obj.members)
        if (!seen.insert(m.first).second && !twiceOk) c.fail(where, "\"" + m.first.substr(0, 32) + "\" is given twice");
    for (const auto& m : obj.members) {
        bool known = false;
        for (const char* k : keys)
            if (m.first == k) known = true;
        if (!known) c.fail(where, "unknown key \"" + m.first.substr(0, 32) + "\"");
    }
}

// JSON nested deeper than a level can be: refused before parsing (the
// parser recurses, and a code is untrusted)
bool tooDeep(const std::string& text, int limit) {
    int depth = 0;
    bool inString = false, escaped = false;
    for (char ch : text) {
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
        } else if (ch == '"') {
            inString = true;
        } else if (ch == '[' || ch == '{') {
            if (++depth > limit) return true;
        } else if (ch == ']' || ch == '}') {
            --depth;
        }
    }
    return false;
}

// ---- base64url ------------------------------------------------------------

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64url(const unsigned char* data, size_t n) {
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = static_cast<unsigned>(data[i]) << 16;
        if (i + 1 < n) v |= static_cast<unsigned>(data[i + 1]) << 8;
        if (i + 2 < n) v |= data[i + 2];
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        if (i + 1 < n) out += kB64[(v >> 6) & 63];
        if (i + 2 < n) out += kB64[v & 63];
    }
    return out;
}

bool unbase64url(const std::string& s, std::vector<unsigned char>& out) {
    unsigned v = 0;
    int bits = 0;
    for (char ch : s) {
        int d;
        if (ch >= 'A' && ch <= 'Z') d = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') d = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') d = ch - '0' + 52;
        else if (ch == '-') d = 62;
        else if (ch == '_') d = 63;
        else if (ch == '=') break;  // padding: the end
        else return false;
        v = (v << 6) | static_cast<unsigned>(d);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((v >> bits) & 0xff));
        }
    }
    return true;
}

// ---- files ----------------------------------------------------------------

bool readFile(const std::string& path, std::string& out, size_t limit) {
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path.c_str(), &info) || info.type != SDL_PATHTYPE_FILE || info.size > limit) return false;
    size_t n = 0;
    void* raw = SDL_LoadFile(path.c_str(), &n);
    if (!raw) return false;
    out.assign(static_cast<const char*>(raw), n);
    SDL_free(raw);
    return true;
}

bool writeFile(const std::string& path, const std::string& text) {
    // a temporary file renamed over it: a crash mid-write leaves the old one whole
    std::string tmp = path + ".tmp";
    return SDL_SaveFile(tmp.c_str(), text.data(), text.size()) && SDL_RenamePath(tmp.c_str(), path.c_str());
}

std::vector<std::string> filesIn(const std::string& dir, const char* pattern) {
    std::vector<std::string> names;
    int count = 0;
    if (char** list = SDL_GlobDirectory(dir.c_str(), pattern, SDL_GLOB_CASEINSENSITIVE, &count)) {
        for (int i = 0; i < count; ++i)
            if (list[i] && !std::strchr(list[i], '/') && !std::strchr(list[i], '\\')) names.push_back(list[i]);
        SDL_free(list);
    }
    return names;
}

// "<id>_v<version>_<date>.json" -> id, version
bool splitFileName(const std::string& file, std::string& id, int& version) {
    size_t v = file.rfind("_v");
    while (v != std::string::npos) {
        size_t k = v + 2;
        while (k < file.size() && std::isdigit(static_cast<unsigned char>(file[k]))) ++k;
        if (k > v + 2 && k < file.size() && (file[k] == '_' || file[k] == '.')) {
            id = file.substr(0, v);
            version = std::atoi(file.c_str() + v + 2);
            return !id.empty();
        }
        if (v == 0) break;
        v = file.rfind("_v", v - 1);
    }
    return false;
}

std::string slug(const std::string& title) {
    std::string s;
    for (unsigned char ch : title) {
        if (std::isalnum(ch)) s += static_cast<char>(std::tolower(ch));
        else if (!s.empty() && s.back() != '_') s += '_';
        if (s.size() >= 24) break;
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s.empty() ? "surgery" : s;
}

std::string stamp() {
    // the original's "%d_%d_%d_%d_%d_%d" (year, month, day, hour, minute, second)
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[48];
    std::snprintf(buf, sizeof buf, "%d_%d_%d_%d_%d_%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                  tm.tm_min, tm.tm_sec);
    return buf;
}

std::string surgeryFileName(const std::string& id, int mode) {
    return id + (mode == 1 ? "_challenge1" : mode == 2 ? "_challenge2" : "");
}

}  // namespace

// ---- the level ------------------------------------------------------------

Level newLevel() {
    Level l;
    for (int t = 0; t < kToolCount; ++t) l.tools.push_back(kToolNames[t]);
    LevelStage s;
    s.bg = "bg_mrgibletsChest.png";
    s.bgm = "ingame_1.caf";
    s.health = 100;
    s.time = 300;
    s.toSurgeryStage = 1;
    l.stages.push_back(s);
    return l;
}

json::Value toJson(const Level& level, bool compact) {
    std::string text = toText(level, false, compact);
    json::Value v;
    json::parse(text, v);
    return v;
}

std::string toText(const Level& level, bool pretty, bool compact) {
    // (createSurgeryJson 0x759858's keys, in its order)
    json::Writer w(pretty);
    w.beginObject();
    w.key("editor").beginObject();
    w.field("format", 1);
    w.field("title", level.title);
    if (level.imported) w.field("imported", true);
    w.endObject();
    w.field("is_challenge", level.isChallenge);
    w.field("is_sudden_death", level.isSuddenDeath);
    for (int i = 0; i < 3; ++i) {
        w.field("min_score_to_" + std::to_string(i + 1) + "_star", level.minScore[i]);
        w.field("min_time_to_" + std::to_string(i + 1) + "_star", level.minTime[i]);
    }
    if (!compact) w.field("tutorial_filename", "");
    if (level.damageMultiplier > 0) w.field("injury_damage_multiplier", level.damageMultiplier);
    if (level.hideExtraction) w.field("hide_extraction", true);
    w.key("stages").beginArray();
    for (size_t s = 0; s < level.stages.size(); ++s) {
        const LevelStage& st = level.stages[s];
        w.beginObject();
        w.field("name", static_cast<int>(s + 1));
        w.field("bg", st.bg);
        w.field("bg_flip_x", st.flipX);
        w.field("bg_flip_y", st.flipY);
        w.field("bgm", st.bgm);
        w.field("body_level", st.bodyLevel);
        w.field("health", st.health);
        w.field("time", st.time);
        w.field("to_surgery_stage", st.toSurgeryStage);
        w.key("injuries").beginArray();
        for (const LevelInjury& i : st.injuries) {
            w.beginObject();
            w.field("class_name", i.className);
            w.field("name", i.name);
            w.field("rotation", i.rotation);
            w.field("to_surgery_stage", i.toSurgeryStage);
            w.field("stage_end_to_swap", i.stageEndToSwap);
            w.field("x", i.x);
            w.field("y", i.y);
            if (!i.interactionDeps.empty() || !compact) {
                w.key("interaction_dependencies").beginArray();
                for (const std::string& d : i.interactionDeps) w.value(d);
                w.endArray();
            }
            if (!i.visualDeps.empty() || !compact) {
                w.key("visual_dependencies").beginArray();
                for (const std::string& d : i.visualDeps) w.value(d);
                w.endArray();
            }
            if (!i.allowNonEssential) w.field("allow_non_ess_injury_creation", false);
            w.endObject();
        }
        w.endArray();
        // (the original wrote no "rotation" for these, so a turned one came
        // back straight; the port writes it)
        if (st.bgs.empty() && compact) {
        } else if (st.bgs.empty()) {
            w.key("additional_bgs").null();
        } else {
            w.key("additional_bgs").beginArray();
            for (const LevelBg& b : st.bgs) {
                w.beginObject();
                w.field("filename", b.filename);
                w.field("x", b.x);
                w.field("y", b.y);
                w.field("rotation", b.rotation);
                w.field("flip_x", b.flipX);
                w.field("flip_y", b.flipY);
                w.endObject();
            }
            w.endArray();
        }
        w.endObject();
    }
    w.endArray();
    w.key("tools").beginArray();
    for (const std::string& t : level.tools) w.value(t);
    w.endArray();
    if (!compact) {
        const Catalog& cat = Catalog::shared();
        w.key("sprite_sheets").beginArray();
        for (const std::string& s : cat.sheetsFor(level)) w.value(s);
        w.endArray();
        w.key("sounds").beginArray();
        if (!level.scream.empty())
            for (int k = 1; k <= 4; ++k) w.value(level.scream + "_scream_" + std::to_string(k) + ".caf");
        w.endArray();
    }
    w.field("scream", level.scream);
    w.endObject();
    return w.text();
}

bool fromJson(const json::Value& root, Level& out, std::string& error, bool fromShipped) {
    const Catalog& cat = Catalog::shared();
    Check c;
    Level l;
    l.tools.clear();
    if (root.type != json::Value::kObject) {
        error = "This is not a surgery.";
        return false;
    }
    onlyKeys(c, root,
             {"editor", "is_challenge", "is_sudden_death", "min_score_to_1_star", "min_score_to_2_star",
              "min_score_to_3_star", "min_time_to_1_star", "min_time_to_2_star", "min_time_to_3_star",
              "tutorial_filename", "injury_damage_multiplier", "hide_extraction", "tag_allowed", "stages", "tools",
              "sprite_sheets", "sounds", "scream"},
             "", fromShipped);
    if (root.has("editor")) {
        const json::Value& e = root["editor"];
        if (e.type != json::Value::kObject) c.fail("", "\"editor\" must be an object");
        onlyKeys(c, e, {"format", "title", "imported"}, "editor");
        int format = readInt(c, e, "format", 1, 1000, 1, "editor");
        if (format > 1) c.fail("", "this surgery was made by a newer version of the game");
        std::string title = readString(c, e, "title", kMaxTitle, "editor");
        if (!title.empty() && !goodTitle(title)) c.fail("editor", "the title has letters the game can't show");
        if (!title.empty()) l.title = title;
        l.imported = readBool(c, e, "imported", false, "editor");
    }
    l.isChallenge = readBool(c, root, "is_challenge", false, "");
    l.isSuddenDeath = readBool(c, root, "is_sudden_death", false, "");
    l.hideExtraction = readBool(c, root, "hide_extraction", false, "");
    readBool(c, root, "tag_allowed", true, "");  // (checked; a played level has no tag partner)
    for (int i = 0; i < 3; ++i) {
        l.minScore[i] = readInt(c, root, ("min_score_to_" + std::to_string(i + 1) + "_star").c_str(), 0, 10000000, 0, "");
        l.minTime[i] = readInt(c, root, ("min_time_to_" + std::to_string(i + 1) + "_star").c_str(), 0, 36000, 0, "");
    }
    std::string tutorial = readString(c, root, "tutorial_filename", 64, "");
    if (!tutorial.empty() && !fromShipped) c.fail("", "a shared surgery can't have a tutorial");
    if (root.has("injury_damage_multiplier")) {
        const json::Value& v = root["injury_damage_multiplier"];
        if (v.type != json::Value::kNumber || !std::isfinite(v.number) || v.number < 0.1 || v.number > 5)
            c.fail("", "\"injury_damage_multiplier\" must be from 0.1 to 5");
        else
            l.damageMultiplier = static_cast<float>(v.number);
    }
    // the tools, by their names
    const json::Value& tools = root["tools"];
    if (tools.type != json::Value::kArray || tools.size() > static_cast<size_t>(kToolCount)) {
        c.fail("", "\"tools\" must list the tools");
    } else {
        for (const json::Value& t : tools.items) {
            if (t.type != json::Value::kString || toolByName(t.string) == kNoTool) {
                c.fail("tools", "unknown tool");
                break;
            }
            if (std::find(l.tools.begin(), l.tools.end(), t.string) == l.tools.end()) l.tools.push_back(t.string);
        }
    }
    // the derived lists: checked for shape, made again from the catalog
    for (const char* key : {"sprite_sheets", "sounds"}) {
        if (!root.has(key) || root[key].isNull() || fromShipped) continue;
        const json::Value& v = root[key];
        if (v.type != json::Value::kArray || v.size() > 64) c.fail("", std::string("\"") + key + "\" must be a list");
        else
            for (const json::Value& s : v.items)
                if (s.type != json::Value::kString || s.string.size() > 96) c.fail(key, "must list names");
    }
    l.scream = readString(c, root, "scream", 32, "");
    if (std::find(cat.screams.begin(), cat.screams.end(), l.scream) == cat.screams.end()) {
        if (fromShipped) l.scream = "male";
        else c.fail("", "unknown scream \"" + l.scream.substr(0, 32) + "\"");
    }
    // the stages
    const json::Value& stages = root["stages"];
    if (stages.type != json::Value::kArray || stages.size() < 1 || stages.size() > static_cast<size_t>(kMaxStages)) {
        c.fail("", "a surgery has 1 to 10 stages");
    }
    const int n = static_cast<int>(stages.type == json::Value::kArray ? stages.size() : 0);
    int total = 0;
    for (int s = 0; s < n && c.ok(); ++s) {
        const json::Value& st = stages[static_cast<size_t>(s)];
        const std::string where = "stage " + std::to_string(s + 1);
        if (st.type != json::Value::kObject) {
            c.fail(where, "not a stage");
            break;
        }
        if (fromShipped)
            onlyKeys(c, st, {"name", "bg", "bg_flip_x", "bg_flip_y", "bgm", "body_level", "health", "time",
                             "to_surgery_stage", "injuries", "additional_bgs", "tutorial_data"}, where, true);
        else
            onlyKeys(c, st, {"name", "bg", "bg_flip_x", "bg_flip_y", "bgm", "body_level", "health", "time",
                             "to_surgery_stage", "injuries", "additional_bgs"}, where);
        LevelStage g;
        readInt(c, st, "name", 0, 100, s + 1, where);  // (renumbered)
        g.bg = readString(c, st, "bg", 96, where, true);
        if (c.ok() && !cat.findBg(g.bg)) c.fail(where, "unknown background \"" + g.bg.substr(0, 40) + "\"");
        g.flipX = readBool(c, st, "bg_flip_x", false, where);
        g.flipY = readBool(c, st, "bg_flip_y", false, where);
        g.bgm = readString(c, st, "bgm", 64, where);
        if (std::find(cat.music.begin(), cat.music.end(), g.bgm) == cat.music.end())
            c.fail(where, "unknown music \"" + g.bgm.substr(0, 40) + "\"");
        g.bodyLevel = readInt(c, st, "body_level", 0, 9, 1, where);
        g.health = readInt(c, st, "health", -1, 1000, -1, where);
        g.time = readInt(c, st, "time", -1, 36000, -1, where);
        if (g.health == 0) c.fail(where, "\"health\" must be -1 or more than 0");
        if (g.time >= 0 && g.time < 10) c.fail(where, "\"time\" must be -1 or 10 seconds at least");
        g.toSurgeryStage = readInt(c, st, "to_surgery_stage", 0, n, 0, where);
        const json::Value& bgs = st["additional_bgs"];
        if (!bgs.isNull()) {
            if (bgs.type != json::Value::kArray || bgs.size() > static_cast<size_t>(kMaxExtraBgs))
                c.fail(where, "\"additional_bgs\" must list 8 pictures at most");
            else
                for (const json::Value& b : bgs.items) {
                    if (b.type != json::Value::kObject) {
                        c.fail(where, "an extra picture is not an object");
                        break;
                    }
                    onlyKeys(c, b, {"filename", "x", "y", "rotation", "flip_x", "flip_y"}, where, fromShipped);
                    LevelBg e;
                    e.filename = readString(c, b, "filename", 96, where, true);
                    if (c.ok() && !cat.findExtraBg(e.filename))
                        c.fail(where, "unknown picture \"" + e.filename.substr(0, 40) + "\"");
                    e.x = readInt(c, b, "x", -kCanvasW, 2 * kCanvasW, 320, where, true);
                    e.y = readInt(c, b, "y", -kCanvasH, 2 * kCanvasH, 192, where, true);
                    e.rotation = readInt(c, b, "rotation", -360, 360, 0, where);
                    e.flipX = readBool(c, b, "flip_x", false, where);
                    e.flipY = readBool(c, b, "flip_y", false, where);
                    g.bgs.push_back(e);
                }
        }
        const json::Value& injuries = st["injuries"];
        if (injuries.type != json::Value::kArray || injuries.size() > static_cast<size_t>(kMaxInjuriesPerStage)) {
            c.fail(where, "\"injuries\" must list 50 injuries at most");
            break;
        }
        std::set<std::string> names;
        for (size_t k = 0; k < injuries.size() && c.ok(); ++k) {
            const json::Value& inj = injuries[k];
            if (inj.type != json::Value::kObject) {
                c.fail(where, "an injury is not an object");
                break;
            }
            onlyKeys(c, inj, {"class_name", "name", "rotation", "to_surgery_stage", "stage_end_to_swap", "x", "y",
                              "interaction_dependencies", "visual_dependencies", "allow_non_ess_injury_creation"},
                     where, fromShipped);
            LevelInjury i;
            i.className = readString(c, inj, "class_name", 64, where, true);
            const Catalog::Class* cls = c.ok() ? cat.findClass(i.className) : nullptr;
            if (c.ok() && !cls) c.fail(where, "unknown injury \"" + i.className.substr(0, 40) + "\"");
            i.name = readString(c, inj, "name", 64, where, true);
            if (c.ok() && !goodName(i.name)) c.fail(where, "bad injury name \"" + i.name.substr(0, 40) + "\"");
            if (c.ok() && fromShipped && names.count(i.name)) {
                // (a shipped file's name used twice: the game's dependencies
                // find the first, so the second gets a name of its own)
                std::string base = i.name.substr(0, kMaxName - 4);
                for (int k = 2; names.count(i.name); ++k) i.name = base + "_" + std::to_string(k);
            }
            if (c.ok() && !names.insert(i.name).second) c.fail(where, "two injuries are called \"" + i.name + "\"");
            i.rotation = readInt(c, inj, "rotation", -720, 720, 0, where);
            i.x = readInt(c, inj, "x", 0, kCanvasW, 320, where, true);
            i.y = readInt(c, inj, "y", 0, kCanvasH, 192, where, true);
            i.toSurgeryStage = readInt(c, inj, "to_surgery_stage", 0, n, 0, where);
            i.stageEndToSwap = readInt(c, inj, "stage_end_to_swap", 0, fromShipped ? 1000 : cls ? cls->stages : 0, 0, where);
            if (fromShipped && cls && i.stageEndToSwap > cls->stages) {
                // (some shipped files name a step past the class's last:
                // the jump never comes, SurgeryScreen::drainInjuryEvents 0x33464; the same said plainly)
                i.stageEndToSwap = 0;
                i.toSurgeryStage = 0;
            }
            i.allowNonEssential = readBool(c, inj, "allow_non_ess_injury_creation", true, where);
            for (int list = 0; list < 2; ++list) {
                const char* key = list ? "visual_dependencies" : "interaction_dependencies";
                const json::Value& d = inj[key];
                if (d.isNull()) continue;
                if (d.type != json::Value::kArray || d.size() > static_cast<size_t>(kMaxInjuriesPerStage)) {
                    c.fail(where, std::string("\"") + key + "\" must list injuries");
                    break;
                }
                for (const json::Value& v : d.items) {
                    if (v.type != json::Value::kString || !goodName(v.string)) {
                        c.fail(where, std::string("bad name in \"") + key + "\"");
                        break;
                    }
                    (list ? i.visualDeps : i.interactionDeps).push_back(v.string);
                }
            }
            g.injuries.push_back(i);
        }
        // the dependencies name injuries of the same stage, not themselves
        // (a shipped file's that name none: left out, as the game leaves them,
        // SurgeryScreen::buildStages's "Found dependency with unexisting injury")
        if (fromShipped)
            for (LevelInjury& i : g.injuries)
                for (std::vector<std::string>* deps : {&i.visualDeps, &i.interactionDeps})
                    deps->erase(std::remove_if(deps->begin(), deps->end(),
                                               [&](const std::string& d) { return d == i.name || !names.count(d); }),
                                deps->end());
        for (const LevelInjury& i : g.injuries)
            for (const std::vector<std::string>* deps : {&i.visualDeps, &i.interactionDeps})
                for (const std::string& d : *deps)
                    if (c.ok() && (d == i.name || !names.count(d)))
                        c.fail(where, "\"" + i.name + "\" waits for \"" + d + "\", which isn't in its stage");
        total += static_cast<int>(g.injuries.size());
        l.stages.push_back(g);
    }
    if (c.ok() && total > kMaxInjuries) c.fail("", "a surgery has 200 injuries at most");
    if (!c.ok()) {
        error = c.error;
        return false;
    }
    out = l;
    return true;
}

bool playable(const Level& level, std::string& why) {
    int total = 0;
    for (const LevelStage& s : level.stages) total += static_cast<int>(s.injuries.size());
    if (level.stages.empty()) why = "This surgery has no stages.";
    else if (level.stages[0].health < 1) why = "The first stage needs the patient's heart (health).";
    else if (level.stages[0].time < 10) why = "The first stage needs a clock (time).";
    else if (total == 0) why = "This surgery has no injuries.";
    else if (level.tools.empty()) why = "This surgery has no tools.";
    else return true;
    return false;
}

bool fromText(const std::string& text, Level& out, std::string& error) {
    if (text.size() > kMaxJsonBytes) {
        error = "This surgery is too big.";
        return false;
    }
    if (tooDeep(text, 8)) {
        error = "This is not a surgery.";
        return false;
    }
    json::Value root;
    std::string parseError;
    if (!json::parse(text, root, &parseError)) {
        error = "This is not a surgery (it isn't JSON).";
        return false;
    }
    return fromJson(root, out, error);
}

std::string encodeCode(const Level& level) {
    std::string text = toText(level, false, true);
    int n = 0;
    unsigned char* z = stbi_zlib_compress(reinterpret_cast<unsigned char*>(&text[0]), static_cast<int>(text.size()), &n, 9);
    if (!z) return "";
    std::string code = kCodePrefix + base64url(z, static_cast<size_t>(n));
    std::free(z);  // (STBIW_FREE)
    return code;
}

bool decodeCode(const std::string& pasted, Level& out, std::string& error) {
    std::string code;
    for (char ch : pasted)
        if (!std::isspace(static_cast<unsigned char>(ch))) code += ch;
    if (code.empty()) {
        error = "There is no code to paste.";
        return false;
    }
    if (code.size() > kMaxCodeChars) {
        error = "This code is too long.";
        return false;
    }
    // "AS3L<version>:"
    if (code.compare(0, 4, "AS3L") != 0 || code.find(':') == std::string::npos) {
        error = "This is not a surgery code (they begin with AS3L1:).";
        return false;
    }
    std::string version = code.substr(4, code.find(':') - 4);
    if (version != "1") {
        bool number = !version.empty() && version.size() < 6 &&
                      std::all_of(version.begin(), version.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
        error = number ? "This code was made by a newer version of the game." : "This is not a surgery code.";
        return false;
    }
    std::vector<unsigned char> z;
    if (!unbase64url(code.substr(code.find(':') + 1), z) || z.size() < 6) {
        error = "This code is damaged.";
        return false;
    }
    std::vector<char> text(kMaxJsonBytes);
    int len = stbi_zlib_decode_buffer(text.data(), static_cast<int>(text.size()), reinterpret_cast<const char*>(z.data()),
                                      static_cast<int>(z.size()));
    if (len <= 0) {
        error = "This code is damaged (or too big).";
        return false;
    }
    return fromText(std::string(text.data(), static_cast<size_t>(len)), out, error) && playable(out, error);
}

// ---- the catalog ----------------------------------------------------------

const Catalog& Catalog::shared() {
    static Catalog* c = [] {
        Catalog* k = new Catalog;
        k->music.push_back("");
        k->screams.push_back("");
        json::Value info;
        readJson("Data/Surgeries/surgeries_info.json", info);
        // the areas in the story's order (the Bunker's classes first: a
        // class's home is the first surgery that has it), the others after
        std::vector<const AreaDef*> areas;
        for (const char* id : {"area_bunker", "area_shanksalot", "area_mumbojungle", "area_lunacyfalls", "area_robotopia",
                               "area_exileclinic"})
            if (const AreaDef* a = AreasDefUtils::shared()->byId(id)) areas.push_back(a);
        for (const AreaDef& a : AreasDefUtils::shared()->areas)
            if (std::find(areas.begin(), areas.end(), &a) == areas.end()) areas.push_back(&a);
        for (const AreaDef* ap : areas) {
            const AreaDef& a = *ap;
            for (const std::string& id : a.surgeries) {
                const json::Value& i = info["surgeries_info"][id];
                k->patients.push_back({id, i["patient_title"].str().empty() ? id : i["patient_title"].str()});
                for (int mode = 0; mode < 3; ++mode) {
                    std::string file = surgeryFileName(id, mode);
                    if (!cocos2d::CCFileUtils::sharedFileUtils()->isFileExist("Data/Surgeries/" + file + ".json"))
                        continue;
                    json::Value root;
                    if (!readContent("Surgeries/" + file + ".json", root)) continue;
                    std::vector<std::string> sheets;
                    for (const json::Value& v : root["sprite_sheets"].items) sheets.push_back(v.str());
                    k->m_sheets.push_back({file, sheets});
                    k->m_areaOf.push_back({file, a.areaId});
                    const std::string& scream = root["scream"].str();
                    if (!scream.empty() && std::find(k->screams.begin(), k->screams.end(), scream) == k->screams.end())
                        k->screams.push_back(scream);
                    for (const json::Value& st : root["stages"].items) {
                        const std::string& bg = st["bg"].str();
                        if (!bg.empty() && !k->findBg(bg)) k->bgs.push_back({bg, file});
                        for (const json::Value& b : st["additional_bgs"].items)
                            if (!b["filename"].str().empty() && !k->findExtraBg(b["filename"].str()))
                                k->extraBgs.push_back({b["filename"].str(), file});
                        const std::string& bgm = st["bgm"].str();
                        if (std::find(k->music.begin(), k->music.end(), bgm) == k->music.end()) k->music.push_back(bgm);
                        for (const json::Value& inj : st["injuries"].items) {
                            const std::string& name = inj["class_name"].str();
                            if (name.empty() || k->findClass(name)) continue;
                            if (!cocos2d::CCFileUtils::sharedFileUtils()->isFileExist("Data/Injuries/" + name + ".json"))
                                continue;
                            InjuryDef def = loadInjuryDef(name);
                            if (def.stages.empty()) continue;
                            k->classes.push_back({name, file, static_cast<int>(def.stages.size()), def.base});
                        }
                    }
                }
            }
        }
        log("editor: catalog %d injury classes, %d backgrounds, %d pictures, %d patients", static_cast<int>(k->classes.size()),
            static_cast<int>(k->bgs.size()), static_cast<int>(k->extraBgs.size()), static_cast<int>(k->patients.size()));
        for (const Class& cl : k->classes) log("editor: class %s (%s)", cl.name.c_str(), cl.home.c_str());
        return k;
    }();
    return *c;
}

const Catalog::Class* Catalog::findClass(const std::string& name) const {
    for (const Class& c : classes)
        if (c.name == name) return &c;
    return nullptr;
}

const Catalog::Bg* Catalog::findBg(const std::string& frame) const {
    for (const Bg& b : bgs)
        if (b.frame == frame) return &b;
    return nullptr;
}

const Catalog::Bg* Catalog::findExtraBg(const std::string& frame) const {
    for (const Bg& b : extraBgs)
        if (b.frame == frame) return &b;
    return nullptr;
}

std::vector<std::string> Catalog::sheetsOf(const std::string& surgery) const {
    for (const auto& s : m_sheets)
        if (s.first == surgery) return s.second;
    return {};
}

std::vector<std::string> Catalog::sheetsForClass(const std::string& name) const {
    const Class* c = findClass(name);
    return c ? sheetsOf(c->home) : std::vector<std::string>();
}

std::vector<std::string> Catalog::sheetsForBg(const std::string& frame) const {
    const Bg* b = findBg(frame);
    if (!b) b = findExtraBg(frame);
    return b ? sheetsOf(b->home) : std::vector<std::string>();
}

std::vector<std::string> Catalog::sheetsFor(const Level& level) const {
    // the HUD's sheet first (every surgery has it), then each home's once
    std::vector<std::string> out = {"surgery_common1_sheet"};
    std::vector<std::string> homes;
    auto addHome = [&](const std::string& home) {
        if (home.empty() || std::find(homes.begin(), homes.end(), home) != homes.end()) return;
        homes.push_back(home);
        for (const std::string& s : sheetsOf(home))
            if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
    };
    for (const LevelStage& st : level.stages) {
        if (const Bg* b = findBg(st.bg)) addHome(b->home);
        for (const LevelBg& e : st.bgs)
            if (const Bg* b = findExtraBg(e.filename)) addHome(b->home);
        for (const LevelInjury& i : st.injuries)
            if (const Class* c = findClass(i.className)) addHome(c->home);
    }
    return out;
}

const AreaDef* Catalog::areaFor(const Level& level) const {
    std::string area = "area_bunker";
    if (!level.stages.empty())
        if (const Bg* b = findBg(level.stages[0].bg))
            for (const auto& a : m_areaOf)
                if (a.first == b->home) area = a.second;
    const AreaDef* d = AreasDefUtils::shared()->byId(area);
    if (!d && !AreasDefUtils::shared()->areas.empty()) d = &AreasDefUtils::shared()->areas[0];
    return d;
}

bool Catalog::levelFromSurgery(const std::string& id, Level& out, std::string& error) const {
    json::Value root;
    if (!readContent("Surgeries/" + id + ".json", root)) {
        error = "This surgery can't be read.";
        return false;
    }
    if (!fromJson(root, out, error, true)) return false;
    for (const Patient& p : patients)
        if (p.id == id) out.title = p.title.substr(0, kMaxTitle);
    if (!goodTitle(out.title)) out.title = "My Surgery";
    out.imported = false;
    return true;
}

// ---- the files ------------------------------------------------------------

std::string levelsDir() {
    std::string dir = platform::prefPath() + "editedLevels/";
    SDL_CreateDirectory(dir.c_str());
    return dir;
}

std::vector<LevelFile> listLevels() {
    std::string dir = levelsDir();
    std::vector<std::pair<long long, LevelFile>> found;
    for (const std::string& name : filesIn(dir, "*.json")) {
        LevelFile f;
        f.file = name;
        if (!splitFileName(name, f.id, f.version)) {
            f.id = name.substr(0, name.size() - 5);
            f.version = 1;
        }
        Level l;
        if (loadLevel(name, l, f.error)) {
            f.title = l.title;
            f.imported = l.imported;
        }
        SDL_PathInfo info{};
        SDL_GetPathInfo((dir + name).c_str(), &info);
        found.push_back({static_cast<long long>(info.modify_time), f});
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<LevelFile> out;
    for (auto& f : found) out.push_back(f.second);
    return out;
}

bool loadLevel(const std::string& file, Level& out, std::string& error) {
    if (file.find('/') != std::string::npos || file.find('\\') != std::string::npos || file.find("..") != std::string::npos) {
        error = "Bad file name.";
        return false;
    }
    std::string text;
    if (!readFile(levelsDir() + file, text, kMaxJsonBytes)) {
        error = "The file can't be read (or is too big).";
        return false;
    }
    return fromText(text, out, error);
}

bool saveLevel(const Level& level, std::string& id, std::string& file, bool newVersion, std::string& error) {
    std::string dir = levelsDir();
    std::vector<std::string> files = filesIn(dir, "*.json");
    if (id.empty()) {
        // a new level: an id of its own from its title
        std::string base = slug(level.title), tryId = base;
        for (int n = 2;; ++n) {
            bool used = false;
            for (const std::string& f : files) {
                std::string fid;
                int v = 0;
                if (splitFileName(f, fid, v) && fid == tryId) used = true;
            }
            if (!used) break;
            tryId = base + "_" + std::to_string(n);
        }
        id = tryId;
        newVersion = true;
    }
    if (newVersion || file.empty()) {
        // the next version (saveSurgeryNew 0x44f958: the highest found + 1)
        int version = 1;
        for (const std::string& f : files) {
            std::string fid;
            int v = 0;
            if (splitFileName(f, fid, v) && fid == id) version = std::max(version, v + 1);
        }
        file = id + "_v" + std::to_string(version) + "_" + stamp() + ".json";
    }
    if (!writeFile(dir + file, toText(level, true, false))) {
        error = std::string("The surgery can't be saved: ") + SDL_GetError();
        return false;
    }
    log("editor: saved %s", file.c_str());
    return true;
}

bool deleteLevel(const std::string& file) {
    if (file.empty() || file.find('/') != std::string::npos || file.find('\\') != std::string::npos ||
        file.find("..") != std::string::npos)
        return false;
    bool ok = SDL_RemovePath((levelsDir() + file).c_str());
    log("editor: deleted %s", file.c_str());
    return ok;
}

int importFolder(std::string& message) {
    std::string dir = levelsDir() + "import/";
    SDL_CreateDirectory(dir.c_str());
    int imported = 0;
    std::string bad;
    std::vector<std::string> names = filesIn(dir, "*.json");
    for (const std::string& t : filesIn(dir, "*.txt")) names.push_back(t);
    for (const std::string& name : names) {
        std::string text, error;
        Level l;
        bool ok = readFile(dir + name, text, kMaxJsonBytes) || (error = "can't be read (or is too big)", false);
        if (ok) {
            bool code = name.size() > 4 && (name.compare(name.size() - 4, 4, ".txt") == 0 ||
                                            name.compare(name.size() - 4, 4, ".TXT") == 0);
            ok = code ? decodeCode(text, l, error) : fromText(text, l, error) && playable(l, error);
        }
        std::string id, file;
        if (ok) {
            l.imported = true;
            ok = saveLevel(l, id, file, true, error);
        }
        if (ok) {
            SDL_RemovePath((dir + name).c_str());
            ++imported;
        } else if (bad.size() < 400) {
            bad += "\n" + name.substr(0, 40) + ": " + error;
        }
    }
    if (names.empty())
        message = "No files to import. Put .json files (or .txt files holding a code) in\n" + dir;
    else
        message = "Imported " + std::to_string(imported) + " of " + std::to_string(names.size()) + " files." + bad;
    log("editor: import: %s", message.c_str());
    return imported;
}

bool exportLevel(const Level& level, const std::string& id, std::string& path, std::string& error) {
    std::string dir = levelsDir() + "export/";
    SDL_CreateDirectory(dir.c_str());
    path = dir + (id.empty() ? slug(level.title) : id) + ".json";
    if (!writeFile(path, toText(level, true, false))) {
        error = std::string("The file can't be written: ") + SDL_GetError();
        return false;
    }
    log("editor: exported %s", path.c_str());
    return true;
}

}  // namespace editor
}  // namespace mt
