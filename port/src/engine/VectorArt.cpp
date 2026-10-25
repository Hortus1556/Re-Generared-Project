#include "engine/VectorArt.h"

#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <tuple>

#include "flash/Package.h"
#include "miniaudio.h"
#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/GL.h"
#include "platform/Platform.h"

namespace {

constexpr float kPi = 3.14159265358979f;

flash::Rect transformRect(const flash::Matrix& m, const flash::Rect& r) {
    if (r.empty()) return r;
    float xs[4], ys[4];
    m.apply(r.xMin, r.yMin, xs[0], ys[0]);
    m.apply(r.xMax, r.yMin, xs[1], ys[1]);
    m.apply(r.xMin, r.yMax, xs[2], ys[2]);
    m.apply(r.xMax, r.yMax, xs[3], ys[3]);
    return {*std::min_element(xs, xs + 4), *std::min_element(ys, ys + 4), *std::max_element(xs, xs + 4),
            *std::max_element(ys, ys + 4)};
}

flash::Matrix matrixOf(const VectorTransform& t) {
    float r = t.rotation * kPi / 180;
    float c = std::cos(r), s = std::sin(r);
    flash::Matrix m;
    m.a = c * t.scaleX;
    m.b = s * t.scaleX;
    m.c = -s * t.scaleY;
    m.d = c * t.scaleY;
    m.tx = t.x * 20;
    m.ty = t.y * 20;
    return m * t.base;
}

flash::ColorTransform colorOf(const VectorTransform& t) {
    flash::ColorTransform c = t.color;
    c.mult[3] *= t.alpha;
    c.add[3] *= t.alpha;
    return c;
}

// The scale the current GL modelview applies (a zoomed screen).
float glScale() {
    float m[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    return std::sqrt(std::fabs(m[0] * m[5] - m[1] * m[4]));
}

}  // namespace

// What walk() reports to; visitors override what they need.
// A path whose last segment is "#<id>" names the unnamed clip of that
// character directly in the clip at the rest of the path ("" for the
// symbol itself, as Flash's for..in meets it as instanceN).
struct UnnamedChild {
    std::string prefix;
    int id = -1;
    explicit UnnamedChild(const std::string& path) {
        size_t d = path.rfind('#');
        if (d == std::string::npos || path.find('.', d) != std::string::npos) return;
        if (d == 0 || (d == 1 && path[0] == '/')) {
            id = std::atoi(path.c_str() + d + 1);
            return;
        }
        if (path[d - 1] != '.') return;
        prefix = path.substr(0, d - 1);
        id = std::atoi(path.c_str() + d + 1);
    }
};

struct VisitBase {
    bool masking() const { return false; }  // draw masks (stencil calls)?
    // Walk masks' clips as pieces (finding clips by name: a script plays a
    // mask, AS1's credits' wipe; drawing them, masking() does instead)?
    bool walksMasks() const { return false; }
    bool drawsStill() const { return false; }  // still clips as one picture (flat)?
    bool flat(int, const std::vector<flash::Raster::GroupPiece>&, const flash::Matrix&, const flash::ColorTransform&,
              unsigned long long, const flash::Matrix&, unsigned long long) {
        return false;
    }
    void stencil(int) {}
    void maskLevel(int) {}
    void enter(int /*character*/, const std::string& /*names*/) {}  // into an inner sprite or button...
    void leave() {}                   // ...and out of it
    void shown(int /*sprite*/, const std::string& /*names*/, int /*frame*/, long /*age*/, long /*inFrame*/) {}
    // A named piece about to be drawn: its path, transform in its parent and
    // in the walk's space, character, and instance (see partInfo).
    void placed(const std::string& /*names*/, const flash::Matrix& /*local*/, const flash::Matrix& /*world*/,
                int /*character*/, unsigned long long /*instance*/) {}
    // An unnamed sprite placed directly in the clip at `parent` (Flash
    // names these instance1, instance2...).
    void placedUnnamed(const std::string& /*parent*/, const flash::Matrix& /*local*/, const flash::Matrix& /*world*/,
                       int /*character*/, unsigned long long /*instance*/) {}
    // The next enter() is into a piece placed on frame `placedFrame` at
    // `depth` of its parent's timeline.
    void piece(int /*depth*/, int /*placedFrame*/) {}
    bool includesHidden() const { return false; }  // walk hidden pieces too (bounds)?
    // Whether a faded, filtered clip is drawn as one picture (drawing): its
    // pieces given to group() instead of leaf().
    bool drawsGroups() const { return false; }
    // False: the picture is not ready, the clip is walked piece by piece.
    bool group(int /*clip*/, const std::vector<flash::Raster::GroupPiece>& /*pieces*/, const flash::Matrix& /*world*/,
               const flash::ColorTransform& /*color*/, const std::vector<flash::Filter>& /*filters*/,
               unsigned long long /*owner*/) {
        return true;
    }
    bool showsTexts() const { return false; }      // texts' contents wanted (drawing)?
    int buttonStates(int character, const VectorLook* look) const {
        if (look) {
            auto it = look->buttons.find(character);
            if (it != look->buttons.end()) return it->second;
        }
        return 1;
    }
};

// What a walk over a symbol meets: its drawn pieces (shapes, morphs,
// texts) and the masks around some of them.
struct VectorArt::Leaf {
    flash::Swf::Kind kind;
    int character;
    float ratio;
    const std::string* text;  // edit texts
    flash::Matrix world;      // twips -> movie twips
    flash::ColorTransform color;
    unsigned long long owner;
    std::vector<flash::Filter> filters;  // of the clips it is in, innermost first
    // The clip holding it: its world matrix and its instance (smooth motion
    // moves a piece that changes drawing with it: platform::artPiece).
    flash::Matrix parentWorld;
    unsigned long long parentPath = 0;
};


// ------------------------------------------------------------- the library

bool VectorArt::smoothTweens = true;

namespace {

// The clip holding a piece, as smooth motion knows it (platform::artPiece's
// parentId): its path of depths and its Flash instance. A piece going away
// rides with the clip that held it, never with another clip placed at that
// depth since: AS1's bullet hole, its holder stepping to a frame that puts
// the closed cut at the hole's depth, slid off up and away, twice its size,
// on the cut's placement.
unsigned long long clipId(unsigned long long path, unsigned long long instance) {
    return instance ? path ^ (instance * 0xff51afd7ed558ccdull) : path;
}

struct LoadedMovie {
    std::unique_ptr<flash::Swf> movie;
    std::string error;
};

// Reads a movie (any thread: only file reads and parsing).
LoadedMovie readMovie(const std::string& name) {
    LoadedMovie m;
    const std::string suffix = "library.json";
    if (name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
        // A game package (flash/Package.h): its files beside library.json.
        std::string folder = name.substr(0, name.size() - suffix.size());
        m.movie = flash::loadPackage(
            [folder](const std::string& path, std::vector<unsigned char>& bytes) { return assets::read(folder + path, bytes); },
            &m.error);
    } else {
        std::vector<unsigned char> bytes;
        if (assets::read(name, bytes)) m.movie = flash::Swf::load(bytes);
    }
    return m;
}

// Movies being read on workers (preload), by name.
std::mutex gPreloadMutex;
std::unordered_map<std::string, std::future<LoadedMovie>>& preloads() {
    static std::unordered_map<std::string, std::future<LoadedMovie>> m;
    return m;
}

std::unordered_map<std::string, std::unique_ptr<VectorArt>>& libraries() {
    static std::unordered_map<std::string, std::unique_ptr<VectorArt>> m;
    return m;
}

}  // namespace

void VectorArt::preload(const std::string& name) {
    if (libraries().count(name)) return;
    std::lock_guard<std::mutex> lock(gPreloadMutex);
    auto& f = preloads()[name];
    if (!f.valid()) f = std::async(std::launch::async, [name] { return readMovie(name); });
}

bool VectorArt::preloaded(const std::string& name) {
    if (libraries().count(name)) return true;
    std::lock_guard<std::mutex> lock(gPreloadMutex);
    auto it = preloads().find(name);
    return it == preloads().end() || !it->second.valid() ||
           it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

VectorArt& VectorArt::library(const std::string& name) {
    auto& lib = libraries()[name];
    if (!lib) {
        LoadedMovie m;
        std::future<LoadedMovie> pending;
        {
            std::lock_guard<std::mutex> lock(gPreloadMutex);
            auto it = preloads().find(name);
            if (it != preloads().end()) {
                pending = std::move(it->second);
                preloads().erase(it);
            }
        }
        m = pending.valid() ? pending.get() : readMovie(name);
        if (!m.movie)
            platform::fatal("The game's art (" + name + ") is missing or unreadable." + (m.error.empty() ? "" : " " + m.error));
        lib.reset(new VectorArt(std::move(m.movie)));
    }
    return *lib;
}

VectorArt::VectorArt(std::unique_ptr<flash::Swf> movie) : swf(std::move(movie)) {
    if (swf) raster = std::make_unique<flash::Raster>(*swf);
    // (debug AS_RASTER_ART=1: every piece rasterised, as before the meshes)
    if (swf && !(platform::debugMode() && std::getenv("AS_RASTER_ART"))) meshes = std::make_unique<flash::MeshCache>(*swf);
}

int VectorArt::symbol(const std::string& name) const {
    auto it = swf->exports.find(name);
    if (it != swf->exports.end()) return it->second;
    if (!name.empty() && std::all_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        int id = std::stoi(name);
        return swf->kind(id) != flash::Swf::Kind::kNone ? id : -1;
    }
    // Flash finds a linkage name whatever its case (AS1's operation attaches
    // "evilTapeworm", exported as "evilTapeWorm").
    auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string key = lower(name);
    for (const auto& [n, id] : swf->exports)
        if (n.size() == name.size() && lower(n) == key) return id;
    return -1;
}

int VectorArt::frameCount(int sprite) const {
    auto it = swf->sprites.find(sprite);
    return it == swf->sprites.end() ? 1 : std::max(1, static_cast<int>(it->second.frames.size()));
}

int VectorArt::frameOf(int sprite, const std::string& label) const {
    auto it = swf->sprites.find(sprite);
    if (it == swf->sprites.end()) return 0;
    std::string key = label;
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto l = it->second.labels.find(key);
    return l == it->second.labels.end() ? 0 : l->second;
}

std::string VectorArt::labelStartingWith(int sprite, const std::string& prefix) const {
    auto it = swf->sprites.find(sprite);
    if (it == swf->sprites.end() || prefix.empty()) return std::string();
    std::string key = prefix;
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    // (The labels are sorted: those beginning so are together.)
    const auto& labels = it->second.labels;
    auto l = labels.lower_bound(key);
    if (l == labels.end() || l->first.compare(0, key.size(), key) != 0) return std::string();
    auto next = std::next(l);
    if (next != labels.end() && next->first.compare(0, key.size(), key) == 0) return std::string();
    return l->first;
}

bool VectorArt::placement(int sprite, int frame, const std::string& name, VectorTransform& t, int* character) {
    if (swf->sprites.find(sprite) == swf->sprites.end()) return false;
    Timeline& tl = timeline(sprite);
    if (frame < 1 || frame > static_cast<int>(tl.frames.size())) return false;
    for (const Piece& p : tl.frames[frame - 1]) {
        bool match = name.size() > 1 && name[0] == '#' ? p.character == std::atoi(name.c_str() + 1) : p.name == name;
        if (!match) continue;
        t = VectorTransform{};
        t.base = p.matrix;
        t.color = p.color;
        if (character) *character = p.character;
        return true;
    }
    return false;
}

VectorArt::Timeline& VectorArt::timeline(int sprite) {
    auto found = timelines.find(sprite);
    if (found != timelines.end()) return found->second;
    Timeline& t = timelines[sprite];
    const flash::Timeline& src = swf->sprites.at(sprite);
    const int count = std::max(1, static_cast<int>(src.frames.size()));

    // The pieces on each frame, as the timeline's place/move/remove tags
    // build them up.
    std::map<int, Piece> shown;
    for (int f = 1; f <= count; ++f) {
        if (f <= static_cast<int>(src.frames.size()))
            for (const flash::FrameOp& op : src.frames[f - 1]) {
                if (op.kind == flash::FrameOp::kRemove) {
                    shown.erase(op.depth);
                    continue;
                }
                const flash::PlaceOp& p = op.place;
                auto it = shown.find(p.depth);
                if (p.move && it != shown.end() && (p.character < 0 || p.character == it->second.character)) {
                    Piece& o = it->second;
                    if (p.hasMatrix) o.matrix = p.matrix;
                    if (p.hasColor) o.color = p.color;
                    if (p.hasRatio) o.ratio = p.ratio;
                    if (p.hasName) o.name = p.name;
                    if (p.hasClip) o.clipDepth = p.clipDepth;
                    if (!p.filters.empty()) o.filters = p.filters;
                    continue;
                }
                if (p.character < 0) continue;
                Piece o{p.depth, p.character, f};
                if (p.move && it != shown.end()) {
                    // Another character in its place (a mouth drawing swapped
                    // for the next): it keeps the place, tint and name the
                    // tag does not set again.
                    const Piece& old = it->second;
                    o.matrix = old.matrix;
                    o.color = old.color;
                    o.ratio = old.ratio;
                    o.name = old.name;
                    o.clipDepth = old.clipDepth;
                    o.filters = old.filters;
                }
                if (p.hasMatrix) o.matrix = p.matrix;
                if (p.hasColor) o.color = p.color;
                if (p.hasRatio) o.ratio = p.ratio;
                if (p.hasName) o.name = p.name;
                if (p.hasClip) o.clipDepth = p.clipDepth;
                if (!p.filters.empty()) o.filters = p.filters;
                shown[p.depth] = o;
            }
        std::vector<Piece> frame;
        for (auto& [d, p] : shown) frame.push_back(p);
        t.frames.push_back(std::move(frame));
    }
    // Pieces that stay from the first frame to the last keep animating
    // through the whole timeline (a looping background, a breathing body).
    for (Piece& p : t.frames[0]) {
        bool always = true;
        for (const auto& frame : t.frames)
            if (std::none_of(frame.begin(), frame.end(),
                             [&](const Piece& q) { return q.depth == p.depth && q.placedFrame == 1; })) {
                always = false;
                break;
            }
        if (!always) continue;
        for (auto& frame : t.frames)
            for (Piece& q : frame)
                if (q.depth == p.depth && q.placedFrame == 1) q.persistent = true;
    }

    t.main = simulate(sprite, 1, true, true);
    return t;
}

// The playhead from `start` (entered, its actions run), one frame a tick
// while playing, following the authored stops and gotos.
VectorArt::Run VectorArt::simulate(int sprite, int start, bool startPlaying, bool followRandom) {
    const flash::Timeline& src = swf->sprites.at(sprite);
    const int count = std::max(1, static_cast<int>(src.frames.size()));
    Run run;
    int frame = 1;
    bool playing = startPlaying;
    bool random = false;
    int repeats = 0;
    // Enters `target` and runs its frame actions (a goto enters its target
    // after the rest of the frame's actions, as Flash queues it).
    auto enter = [&](int target) {
        std::vector<int> queue{target};
        for (size_t q = 0; q < queue.size() && q < 8; ++q) {
            frame = std::clamp(queue[q], 1, count);
            if (frame > static_cast<int>(src.actions.size())) continue;
            for (const flash::FrameAction& a : src.actions[frame - 1]) {
                switch (a.kind) {
                case flash::FrameAction::kStop: playing = false; break;
                case flash::FrameAction::kPlay: playing = true; break;
                case flash::FrameAction::kGoto:
                    playing = false;
                    queue.push_back(a.frame);
                    break;
                case flash::FrameAction::kGotoLabel: {
                    std::string key = a.label;
                    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    auto l = src.labels.find(key);
                    if (l != src.labels.end()) {
                        playing = false;
                        queue.push_back(l->second);
                    }
                    break;
                }
                case flash::FrameAction::kGotoRandom:
                    if (followRandom && (a.count > 0 || !a.choices.empty())) {
                        run.randomCount = a.choices.empty() ? a.count : static_cast<int>(a.choices.size());
                        run.randomBase = a.frame;
                        run.randomChoices = a.choices;
                        run.randomPlay = a.play;
                        random = true;
                        return;
                    }
                    playing = false;  // a pick's own frame: stays
                    break;
                case flash::FrameAction::kNext:
                    playing = false;
                    queue.push_back(std::min(frame + 1, count));
                    break;
                case flash::FrameAction::kPrev:
                    playing = false;
                    queue.push_back(std::max(frame - 1, 1));
                    break;
                case flash::FrameAction::kRepeat:
                    if (++repeats < a.count) {
                        playing = true;
                        queue.push_back(a.frame);
                    } else {
                        playing = false;
                        queue.push_back(std::min(frame + 1, count));
                    }
                    break;
                }
            }
        }
    };
    std::map<std::tuple<int, bool, int>, int> seen;
    enter(start);
    for (int age = 0; age < 100000 && !random; ++age) {
        auto state = std::make_tuple(frame, playing, repeats);
        auto s = seen.find(state);
        if (s != seen.end()) {
            run.cycleFrom = s->second;
            break;
        }
        seen[state] = age;
        run.since.push_back(!run.heads.empty() && run.heads.back() == frame ? run.since.back() : age);
        run.heads.push_back(frame);
        if (!playing) break;
        enter(frame >= count ? 1 : frame + 1);
    }
    return run;
}

VectorArt::Head VectorArt::head(int sprite, long age, unsigned long long seed) {
    if (swf->sprites.find(sprite) == swf->sprites.end()) return {1, std::max(0L, age)};
    Timeline& t = timeline(sprite);
    const Run* run = &t.main;
    long a = std::max(0L, age);
    for (int pick = 0; pick < 4; ++pick) {
        long n = static_cast<long>(run->heads.size());
        if (a < n) return {run->heads[a], a - run->since[a]};
        if (run->randomCount > 0) {
            // This instance's pick, the same every time it is drawn.
            unsigned long long h = (seed + 0x9E3779B97F4A7C15ull * (pick + 1)) * 0xBF58476D1CE4E5B9ull;
            h ^= h >> 31;
            int index = static_cast<int>(h % static_cast<unsigned long long>(run->randomCount));
            int choice = run->randomChoices.empty() ? run->randomBase + index : run->randomChoices[index];
            auto it = t.picked.find(choice);
            if (it == t.picked.end()) it = t.picked.emplace(choice, simulate(sprite, choice, run->randomPlay, false)).first;
            a -= n;
            run = &it->second;
            continue;
        }
        if (n == 0) return {1, a};
        if (run->cycleFrom >= 0) {
            long period = n - run->cycleFrom;
            long i = run->cycleFrom + (a - run->cycleFrom) % period;
            return {run->heads[i], i - run->since[i]};
        }
        return {run->heads.back(), a - run->since.back()};
    }
    return {1, a};
}

int VectorArt::playhead(int sprite, long age, unsigned long long seed) { return head(sprite, age, seed).frame; }

bool VectorArt::stopsAfter(int sprite, long& frames) {
    if (swf->sprites.find(sprite) == swf->sprites.end()) {
        frames = 0;
        return true;
    }
    const Run& run = timeline(sprite).main;
    if (run.cycleFrom >= 0) return false;
    frames = static_cast<long>(run.heads.size()) - 1;
    return true;
}

// ------------------------------------------------------------- the walk

template <class Visit>
void VectorArt::walk(int character, int frame, long age, const flash::Matrix& world, const flash::ColorTransform& color,
                     const VectorLook* look, unsigned long long path, const std::string& names, Visit& visit,
                     long inFrameGiven, unsigned long long instance) {
    using Kind = flash::Swf::Kind;
    Kind kind = swf->kind(character);
    if (kind == Kind::kShape || kind == Kind::kMorph || kind == Kind::kText || kind == Kind::kEditText) {
        Leaf leaf{kind, character, 0, nullptr, world, color, path};
        leaf.parentWorld = world;
        leaf.parentPath = clipId(path ^ 0x5eedull, instance);
        for (auto f = filterStack.rbegin(); f != filterStack.rend(); ++f)
            leaf.filters.insert(leaf.filters.end(), (*f)->begin(), (*f)->end());
        visit.leaf(leaf);
        return;
    }
    // A sprite's pieces on the frame shown (a button: its up state).
    std::vector<Piece> buttonPieces;
    const std::vector<Piece>* pieces = nullptr;
    long inFrame = inFrameGiven >= 0 ? inFrameGiven : age;
    if (kind == Kind::kSprite) {
        Timeline& t = timeline(character);
        if (frame <= 0) {
            Head h = head(character, age, path);
            frame = h.frame;
            inFrame = h.inFrame;  // how long the playhead has been on this frame
        }
        frame = std::clamp(frame, 1, static_cast<int>(t.frames.size()));
        pieces = &t.frames[frame - 1];
        visit.shown(character, names, frame, age, inFrame);
    } else if (kind == Kind::kButton) {
        int states = visit.buttonStates(character, look);
        for (const flash::ButtonRecord& r : swf->buttons.at(character).records)
            if (r.states & states) {
                Piece p{r.depth, r.character, 1};
                p.matrix = r.matrix;
                p.color = r.color;
                p.persistent = true;
                buttonPieces.push_back(p);
            }
        std::sort(buttonPieces.begin(), buttonPieces.end(), [](const Piece& a, const Piece& b) { return a.depth < b.depth; });
        pieces = &buttonPieces;
    } else {
        return;
    }

    // A look's override for the piece at `names` (an instance path).
    auto lookup = [](const auto& map, const std::string& names) -> decltype(&map.begin()->second) {
        for (const auto& [key, value] : map)
            if (vectorPathMatches(key, names)) return &value;
        return nullptr;
    };
    std::vector<const Piece*> masks;
    auto drawPiece = [&](const Piece& p) {
        if (look && instance == 0 && (p.depth < look->fromDepth || p.depth > look->toDepth)) return;
        if (look && !look->hidden.empty() && !visit.includesHidden()) {
            std::string n = p.name.empty() ? std::string() : names.empty() ? p.name : names + "." + p.name;
            for (const std::string& h : look->hidden) {
                if (!h.empty() && h[0] == '#') {
                    if (std::atoi(h.c_str() + 1) == p.character) return;
                } else if (!n.empty()) {
                    if (vectorPathMatches(h, n)) return;
                } else {
                    UnnamedChild u(h);  // "a.b.#id": an unnamed clip in a.b
                    if (u.id == p.character && !u.prefix.empty() && vectorPathMatches(u.prefix, names)) return;
                }
            }
        }
        flash::Matrix w = world * p.matrix;
        flash::ColorTransform c = color * p.color;
        if (look && !look->alphas.empty() && !p.name.empty())
            if (const float* a = lookup(look->alphas, names.empty() ? p.name : names + "." + p.name)) c.mult[3] *= *a;
        unsigned long long sub = path * 1000003ull + static_cast<unsigned long long>(p.depth + 70000);
        // A Flash instance lasts while its depth keeps the placement that
        // made it (and its parent stays the same instance).
        unsigned long long id = ((instance + 1) * 0x9E3779B97F4A7C15ull) ^
                                (static_cast<unsigned long long>(p.depth) << 40) ^
                                (static_cast<unsigned long long>(p.placedFrame) << 20) ^
                                static_cast<unsigned long long>(p.character);
        std::string sub_names = p.name.empty() ? names : names.empty() ? p.name : names + "." + p.name;
        if (!p.name.empty()) visit.placed(sub_names, p.matrix, w, p.character, id);
        else if (swf->kind(p.character) == Kind::kSprite) visit.placedUnnamed(names, p.matrix, w, p.character, id);
        Kind k = swf->kind(p.character);
        if (k == Kind::kSprite || k == Kind::kButton) {
            int childFrame = 0;
            long childInFrame = -1;
            if (look && !look->instanceFrames.empty())
                if (auto f = look->instanceFrames.find(id); f != look->instanceFrames.end()) {
                    childFrame = f->second;
                    if (auto n = look->instanceInFrames.find(id); n != look->instanceInFrames.end()) childInFrame = n->second;
                }
            if (look && !childFrame && !p.name.empty())
                if (const int* f = lookup(look->frames, sub_names)) {
                    childFrame = *f;
                    if (!look->inFrames.empty())
                        if (const long* n = lookup(look->inFrames, sub_names)) childInFrame = *n;
                }
            if (look && !childFrame && !look->frames.empty()) {
                // An unnamed piece by its exact path ("/a.b.#id"); any piece
                // of that character by "#id".
                std::string id = "#" + std::to_string(p.character);
                auto f = p.name.empty() ? look->frames.find("/" + (names.empty() ? id : names + "." + id)) : look->frames.end();
                if (f != look->frames.end()) {
                    childFrame = f->second;
                    auto n = look->inFrames.find(f->first);
                    if (n != look->inFrames.end()) childInFrame = n->second;
                } else if ((f = look->frames.find(id)) != look->frames.end()) {
                    childFrame = f->second;
                }
            }
            if (!childFrame && !frameOverrides.empty()) {
                auto f = frameOverrides.find(p.character);
                if (f != frameOverrides.end()) childFrame = f->second;
            }
            long childAge = p.persistent ? age : inFrame + (frame - p.placedFrame);
            if (!childFrame && !triggers.empty() && !p.name.empty() && k == Kind::kSprite) {
                for (const Trigger& t : triggers) {
                    if (t.parent != character || t.child != p.name) continue;
                    long since;
                    childFrame = triggeredFrame(p.character, t.chance, sub, childAge, since);
                    if (since >= 0) childAge = since;
                }
            }
            if (k == Kind::kSprite && p.filters.empty() && visit.drawsStill() &&
                drewStill(p.character, childFrame, childAge, childInFrame, w, w, c, look, sub, sub_names, id, id, world,
                          clipId(path, instance), visit))
                return;
            // A faded clip with filters, as Flash drew it: its picture
            // filtered and faded as one (its overlapping pieces never show
            // through each other), when it is plain shapes.
            if (k == Kind::kSprite && !p.filters.empty() && c.mult[3] < 1 && !c.add[0] && !c.add[1] && !c.add[2] && !c.add[3] &&
                visit.drawsGroups() && (groupParentWorld = world, groupParentPath = clipId(path, instance), true) &&
                drewAsOne(p, childFrame, childAge, childInFrame, w, c, look, sub, sub_names, id, visit))
                return;
            visit.piece(p.depth, p.placedFrame);
            visit.enter(p.character, sub_names);
            if (!p.filters.empty()) filterStack.push_back(&p.filters);
            walk(p.character, childFrame, childAge, w, c, look, sub, sub_names, visit, childInFrame, id);
            if (!p.filters.empty()) filterStack.pop_back();
            visit.leave();
        } else {
            Leaf leaf{k, p.character, p.ratio, nullptr, w, c, sub};
            leaf.parentWorld = world;
            leaf.parentPath = clipId(path, instance);
            leaf.filters = p.filters;
            for (auto f = filterStack.rbegin(); f != filterStack.rend(); ++f)
                leaf.filters.insert(leaf.filters.end(), (*f)->begin(), (*f)->end());
            std::string shown;
            if (k == Kind::kEditText && look && !p.name.empty()) leaf.text = lookup(look->texts, sub_names);
            if (k == Kind::kEditText && !leaf.text && look) {
                // Fields are also known by the variable they show: in full
                // ("_root.dScore"), else its name ("dScore").
                const std::string& var = swf->editTexts.at(p.character).variable;
                auto full = look->texts.find(var);
                if (!var.empty() && full != look->texts.end()) {
                    leaf.text = &full->second;
                } else {
                    size_t dot = var.find_last_of(".:");
                    leaf.text = lookup(look->texts, dot == std::string::npos ? var : var.substr(dot + 1));
                }
                // Else the variable's value, from the game's scripts.
                if (!leaf.text && !var.empty() && look->fieldText && visit.showsTexts() && look->fieldText(names, var, shown))
                    leaf.text = &shown;
            }
            visit.leaf(leaf);
        }
    };
    auto closeMasks = [&](int depth) {
        while (!masks.empty() && masks.back()->clipDepth < depth) {
            if (visit.masking()) {
                visit.stencil(-1);
                drawPiece(*masks.back());
                visit.stencil(0);
                visit.maskLevel(-1);
            }
            masks.pop_back();
        }
    };
    // A look may move named pieces or raise them above the others.
    std::vector<Piece> rearranged;
    if (look && (!look->positions.empty() || !look->raised.empty())) {
        std::vector<std::pair<int, Piece>> above;
        for (const Piece& p : *pieces) {
            Piece q = p;
            if (!p.name.empty()) {
                std::string n = names.empty() ? p.name : names + "." + p.name;
                if (const VectorLook::Position* pos = lookup(look->positions, n)) {
                    q.matrix.tx = pos->x * 20;
                    q.matrix.ty = pos->y * 20;
                }
                if (const int* r = lookup(look->raised, n)) {
                    above.emplace_back(*r, q);
                    continue;
                }
            }
            rearranged.push_back(q);
        }
        std::stable_sort(above.begin(), above.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        int top = rearranged.empty() ? 0 : rearranged.back().depth;
        for (auto& [order, q] : above) {
            q.depth = ++top;
            rearranged.push_back(q);
        }
        pieces = &rearranged;
    }
    for (const Piece& p : *pieces) {
        closeMasks(p.depth);
        if (p.clipDepth) {
            if (visit.masking()) {
                visit.stencil(1);
                drawPiece(p);
                visit.stencil(0);
                visit.maskLevel(1);
            } else if (visit.walksMasks()) {
                drawPiece(p);
            }
            masks.push_back(&p);
            continue;
        }
        drawPiece(p);
    }
    closeMasks(INT32_MAX);
}

struct VectorArt::Collect : VisitBase {
    std::vector<flash::Raster::GroupPiece> pieces;
    bool plain = true;
    bool masking() const { return true; }
    void stencil(int mode) {
        if (mode) plain = false;
    }
    void leaf(const Leaf& l) {
        // Shapes and morph shapes, unfiltered, at most faded.
        const flash::ColorTransform& k = l.color;
        bool fadeOnly = k.mult[0] == 1 && k.mult[1] == 1 && k.mult[2] == 1 && !k.add[0] && !k.add[1] && !k.add[2] &&
                        !k.add[3];
        if ((l.kind != flash::Swf::Kind::kShape && l.kind != flash::Swf::Kind::kMorph) || !l.filters.empty() ||
            !fadeOnly) {
            plain = false;
            return;
        }
        if (k.mult[3] <= 0) return;  // (not drawn)
        pieces.push_back({l.kind, l.character, l.ratio, l.world, std::min(1.0f, k.mult[3])});
    }
};

template <class Visit>
bool VectorArt::drewAsOne(const Piece& p, int frame, long age, long inFrame, const flash::Matrix& world,
                          const flash::ColorTransform& color, const VectorLook* look, unsigned long long path,
                          const std::string& names, unsigned long long instance, Visit& visit) {
    // Its pieces in its own twips (the filters outside it set aside).
    Collect collect;
    std::vector<const std::vector<flash::Filter>*> outer;
    outer.swap(filterStack);
    walk(p.character, frame, age, flash::Matrix{}, flash::ColorTransform{}, look, path, names, collect, inFrame, instance);
    filterStack.swap(outer);
    if (!collect.plain || collect.pieces.empty()) return false;
    // (Not made yet, as when a layer holding it starts fading in: drawn
    // piece by piece meanwhile, not missing until it is.)
    return visit.group(p.character, collect.pieces, world, color, p.filters, path);
}

template <class Visit>
bool VectorArt::drewStill(int character, int frame, long age, long inFrame, const flash::Matrix& world,
                          const flash::Matrix& placed, const flash::ColorTransform& color, const VectorLook* look,
                          unsigned long long path, const std::string& names, unsigned long long key,
                          unsigned long long instance, const flash::Matrix& parentWorld,
                          unsigned long long parentPath, Visit& visit) {
    // A clip whose art has stayed the same for a while (an operation's
    // background: layers of big shapes over each other) is drawn as one
    // picture, wherever it moves as a whole: one layer of textures instead
    // of one per shape, made in the background meanwhile. Only plain
    // shapes, only opaque (faded, Flash showed its overlapping shapes
    // through each other), and only big ones (layers covering a good part
    // of the stage): small things whose art changes now and then (a cursor
    // showing the tool in hand, a button's states) stay drawn piece by
    // piece, never switching between the two.
    constexpr long kStillPasses = 10;
    constexpr float kBigShare = 0.25f;  // of the stage's area, its layers added up
    if (color.mult[3] < 1 || color.add[0] || color.add[1] || color.add[2] || color.add[3]) return false;
    Still& s = stills[key];
    s.seen = passes;
    if (s.notPlainUntil > passes) return false;
    Collect collect;
    std::vector<const std::vector<flash::Filter>*> outer;
    outer.swap(filterStack);
    // (A node's own symbol by its own instance, 0: only its slice of depths,
    // as the stage draws a node in slices around the clips made nodes in it.
    // Walked as instance `key`, the whole symbol was in every slice: AS1's
    // brain, flattened, drawn again above its cut line.)
    walk(character, frame, age, flash::Matrix{}, flash::ColorTransform{}, look, path, names, collect, inFrame, instance);
    filterStack.swap(outer);
    // Its layers' area on the stage (twips squared over 400: pixels).
    float area = 0;
    for (const flash::Raster::GroupPiece& p : collect.pieces) {
        flash::Rect b;
        if (p.kind == flash::Swf::Kind::kShape) {
            if (const flash::Shape* sh = swf->shape(p.id)) b = sh->bounds;
        } else if (auto m = swf->morphs.find(p.id); m != swf->morphs.end()) {
            const flash::Rect &a = m->second.first.bounds, &z = m->second.second.bounds;
            b = {std::min(a.xMin, z.xMin), std::min(a.yMin, z.yMin), std::max(a.xMax, z.xMax), std::max(a.yMax, z.yMax)};
        }
        flash::Matrix m = placed * p.matrix;
        area += std::fabs(m.a * m.d - m.b * m.c) * (b.xMax - b.xMin) * (b.yMax - b.yMin) / 400;
    }
    if (!collect.plain || collect.pieces.size() < 2 || area < kBigShare * swf->width * swf->height) {
        s.notPlainUntil = passes + 50;  // (looked at again a while later)
        return false;
    }
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(static_cast<uint64_t>(character));
    for (const flash::Raster::GroupPiece& p : collect.pieces) {
        mix(static_cast<uint64_t>(p.kind) << 32 | static_cast<uint32_t>(p.id));
        mix(static_cast<uint64_t>(std::llround(p.ratio * 1000)));
        mix(static_cast<uint64_t>(std::lround(p.alpha * 255)));
        for (float v : {p.matrix.a, p.matrix.b, p.matrix.c, p.matrix.d, p.matrix.tx, p.matrix.ty})
            mix(static_cast<uint64_t>(std::llround(v * 1000)));
    }
    if (h != s.hash) {
        s.hash = h;
        s.since = passes;
        return false;
    }
    if (passes - s.since < kStillPasses) return false;
    return visit.flat(character, collect.pieces, world, color, path, parentWorld, parentPath);
}

// Draws what walk() meets (or with `prefetch`, only queues it to be made).
struct VectorArt::Draw : VisitBase {
    Draw(VectorArt& art, float pixelScale, bool prefetch = false)
        : art(art), pixelScale(pixelScale), prefetch(prefetch) {}
    VectorArt& art;
    float pixelScale;
    bool prefetch;  // only queue the art for the background
    bool masking() const { return true; }
    bool showsTexts() const { return true; }
    void stencil(int mode) {
        if (!prefetch) platform::stencilMode(mode);
    }
    void maskLevel(int delta) {
        if (!prefetch) platform::maskLevel(delta);
    }
    void leaf(const Leaf& l) {
        const flash::Matrix& m = l.world;
        float scale = std::sqrt(std::fabs(m.a * m.d - m.b * m.c)) * pixelScale;
        flash::ColorTransform color = l.color;
        // An added colour cannot be drawn with the fixed pipeline: such
        // art is rasterised with its colour transform baked in. Its alpha
        // (a plain fade, mult only) is still the fixed-function pipeline's
        // job: baking it in too would rebake the whole texture on every
        // step of a fade over an otherwise fixed added tint (a damage
        // flash's added colour, fading out frame by frame).
        bool adds = color.add[0] != 0 || color.add[1] != 0 || color.add[2] != 0 || color.add[3] != 0;
        bool bakeAlpha = color.add[3] != 0;
        flash::ColorTransform bakeColor = color;
        if (adds && !bakeAlpha) {
            bakeColor.mult[3] = 1;
            bakeColor.add[3] = 0;
        }
        const flash::ColorTransform* baked = adds ? &bakeColor : nullptr;
        // Fully transparent (a body layer not shown yet, alpha 0): nothing
        // to draw, so no texture is asked for (it was uploaded at once on
        // the frame an operation opened: two hidden layers' art). Its
        // prefetch still queues it, for when it fades in.
        if (!prefetch && color.mult[3] + color.add[3] / 255 <= 0) return;
        // Shapes, morphs, texts and text fields as triangles the GPU draws
        // at any size (flash/VectorMesh); filtered ones rasterised.
        if (art.meshes && l.filters.empty() && l.kind != flash::Swf::Kind::kNone) {
            flash::MeshCache& meshes = *art.meshes;
            std::shared_ptr<const platform::VectorMesh> mesh =
                l.kind == flash::Swf::Kind::kShape   ? meshes.shape(l.character, scale)
                : l.kind == flash::Swf::Kind::kMorph ? meshes.morph(l.character, l.ratio, scale)
                : l.kind == flash::Swf::Kind::kText  ? meshes.text(l.character, scale)
                : meshes.editText(l.character, l.text ? *l.text : art.swf->editTexts.at(l.character).text, scale);
            if (!mesh) return;
            if (!prefetch) vector(std::move(mesh), l);
            return;
        }
        flash::Raster& r = *art.raster;
        flash::Effects fx{l.filters.empty() ? nullptr : &l.filters, pixelScale};
        const flash::Effects* effects = l.filters.empty() ? nullptr : &fx;
        flash::Rect visible;
        const flash::Rect* shown = visibleIn(m, visible) ? &visible : nullptr;
        textures.clear();
        switch (l.kind) {
        case flash::Swf::Kind::kShape: r.shape(l.character, scale, baked, effects, shown, textures); break;
        case flash::Swf::Kind::kMorph: r.morph(l.character, l.ratio, scale, baked, effects, shown, textures); break;
        case flash::Swf::Kind::kText: r.text(l.character, scale, baked, effects, shown, textures); break;
        case flash::Swf::Kind::kEditText:
            if (const flash::Texture* t = r.editText(
                    l.character, l.text ? *l.text : art.swf->editTexts.at(l.character).text, scale, baked, effects))
                textures.push_back(t);
            break;
        default: break;
        }
        if (textures.empty() || prefetch) return;
        if (adds) {
            // Its rgb (and its alpha too, if that was baked) is already in
            // the texture; only a fade left unbaked is still to apply.
            color.mult[0] = color.mult[1] = color.mult[2] = 1;
            color.add[0] = color.add[1] = color.add[2] = 0;
            if (bakeAlpha) {
                color.mult[3] = 1;
                color.add[3] = 0;
            }
        }
        for (const flash::Texture* t : textures)
            quad(*t, m, color, l.owner, l.parentWorld, l.parentPath, static_cast<unsigned long long>(l.character));
    }
    // The mesh where the piece's matrix puts it, by its colour transform.
    void vector(std::shared_ptr<const platform::VectorMesh> mesh, const Leaf& l) {
        const flash::Matrix &m = l.world, &parent = l.parentWorld;
        const float* b = mesh->bounds;
        platform::ArtPiece piece{{m.a / 20, m.b / 20, m.c / 20, m.d / 20, m.tx / 20, m.ty / 20},
                                 {b[0], b[1], b[2], b[3]},
                                 {parent.a / 20, parent.b / 20, parent.c / 20, parent.d / 20, parent.tx / 20, parent.ty / 20},
                                 l.parentPath,
                                 static_cast<unsigned long long>(l.character)};
        platform::artPiece(piece);
        const float add[4] = {l.color.add[0] / 255, l.color.add[1] / 255, l.color.add[2] / 255, l.color.add[3] / 255};
        platform::drawVector(l.owner, std::move(mesh), l.color.mult, add);
    }
    std::vector<const flash::Texture*> textures;
    // The screen (and a twentieth of it around, for what moves) in the art's
    // own twips, where `m` draws it: the part of big art to make.
    bool screenKnown = false, screenValid = false;
    float sx0 = 0, sy0 = 0, sx1 = 0, sy1 = 0;  // in drawing coordinates
    bool visibleIn(const flash::Matrix& m, flash::Rect& out) {
        if (!screenKnown) {
            screenKnown = true;
            screenValid = platform::screenInDrawing(0.05f, sx0, sy0, sx1, sy1);
        }
        float det = m.a * m.d - m.b * m.c;
        if (!screenValid || std::fabs(det) < 1e-12f) return false;
        flash::Matrix inv = m.inverse();
        out = {1e30f, 1e30f, -1e30f, -1e30f};
        for (float x : {sx0, sx1})
            for (float y : {sy0, sy1}) {
                float tx, ty;
                inv.apply(x * 20, y * 20, tx, ty);
                out = {std::min(out.xMin, tx), std::min(out.yMin, ty), std::max(out.xMax, tx), std::max(out.yMax, ty)};
            }
        return true;
    }
    bool drawsGroups() const { return true; }
    bool group(int clip, const std::vector<flash::Raster::GroupPiece>& pieces, const flash::Matrix& m,
               const flash::ColorTransform& color, const std::vector<flash::Filter>& filters, unsigned long long owner) {
        if (!prefetch && color.mult[3] + color.add[3] / 255 <= 0) return true;  // (transparent: leaf())
        float scale = std::sqrt(std::fabs(m.a * m.d - m.b * m.c)) * pixelScale;
        flash::Effects fx{&filters, pixelScale};
        flash::Rect visible;
        const flash::Rect* shown = visibleIn(m, visible) ? &visible : nullptr;
        textures.clear();
        art.raster->group(clip, pieces, scale, &fx, shown, textures);
        if (prefetch) return true;
        if (textures.empty()) return false;
        for (const flash::Texture* t : textures)
            quad(*t, m, color, owner, art.groupParentWorld, art.groupParentPath,
                 static_cast<unsigned long long>(clip) | (1ull << 40));
        return true;
    }
    // A still clip as one picture (VectorArt::drawUnder): false (drawn piece
    // by piece meanwhile) until all of it is made, in the background.
    // (Not with meshes: the GPU draws a clip's shapes in one go anyway.)
    bool drawsStill() const { return !prefetch && !art.meshes; }
    bool flat(int symbol, const std::vector<flash::Raster::GroupPiece>& pieces, const flash::Matrix& m,
              const flash::ColorTransform& color, unsigned long long owner, const flash::Matrix& parentWorld,
              unsigned long long parentPath) {
        float scale = std::sqrt(std::fabs(m.a * m.d - m.b * m.c)) * pixelScale;
        static const std::vector<flash::Filter> none;
        flash::Effects fx{&none, pixelScale};
        flash::Rect visible;
        const flash::Rect* shown = visibleIn(m, visible) ? &visible : nullptr;
        textures.clear();
        bool complete = false;
        art.raster->group(symbol, pieces, scale, &fx, shown, textures, &complete);
        if (!complete) return false;
        for (const flash::Texture* t : textures)
            quad(*t, m, color, owner, parentWorld, parentPath, static_cast<unsigned long long>(symbol) | (1ull << 40));
        return true;
    }
    // The texture where `m` puts it, its colour (multipliers) and fade
    // applied.
    void quad(const flash::Texture& tex, const flash::Matrix& m, const flash::ColorTransform& color, unsigned long long owner,
              const flash::Matrix& parent, unsigned long long parentId, unsigned long long drawing) {
        float a = std::clamp(color.mult[3] + color.add[3] / 255, 0.0f, 1.0f);
        if (a <= 0) return;
        float x0 = tex.x0, y0 = tex.y0, x1 = tex.x1, y1 = tex.y1;
        float xy[8];
        m.apply(x0, y0, xy[0], xy[1]);
        m.apply(x1, y0, xy[2], xy[3]);
        m.apply(x0, y1, xy[4], xy[5]);
        m.apply(x1, y1, xy[6], xy[7]);
        for (float& v : xy) v /= 20;
        const float uv[8] = {tex.u0, tex.v0, tex.u1, tex.v0, tex.u0, tex.v1, tex.u1, tex.v1};
        // Premultiplied texels: the colour multipliers scaled by alpha.
        float rgba[4] = {std::clamp(color.mult[0], 0.0f, 1.0f) * a, std::clamp(color.mult[1], 0.0f, 1.0f) * a,
                         std::clamp(color.mult[2], 0.0f, 1.0f) * a, a};
        drawing |= static_cast<unsigned long long>(tex.tile) << 41;  // (each tile its own piece)
        // Its place for smooth motion (twips -> pixels).
        platform::ArtPiece piece{{m.a / 20, m.b / 20, m.c / 20, m.d / 20, m.tx / 20, m.ty / 20},
                                 {x0, y0, x1, y1},
                                 {parent.a / 20, parent.b / 20, parent.c / 20, parent.d / 20, parent.tx / 20, parent.ty / 20},
                                 parentId,
                                 drawing};
        platform::artPiece(piece);
        platform::drawQuad(owner, tex.id, xy, uv, rgba, GL_ONE);
    }
};

void VectorArt::drawText(int editText, const std::string& text, float x, float y, float scale,
                         unsigned long long owner, float boxWidth) {
    auto it = swf->editTexts.find(editText);
    if (it == swf->editTexts.end() || prefetchOnly) return;
    if (boxWidth > 0) {
        // A copy of the field with that box (kept, by an id no character
        // has), grown away from its alignment. (The fields are looked up
        // on this thread only; the rasterising workers keep pointers to
        // theirs, which an insertion leaves valid.)
        int wide = 0x40000000 | (editText & 0xffff) << 12 | (static_cast<int>(boxWidth) & 0xfff);
        auto w = swf->editTexts.find(wide);
        if (w == swf->editTexts.end()) {
            flash::EditText f = it->second;
            const float twips = boxWidth * 20;
            if (f.align == 1) f.bounds.xMin = f.bounds.xMax - twips;
            else if (f.align == 2) {
                float c = (f.bounds.xMin + f.bounds.xMax) / 2;
                f.bounds.xMin = c - twips / 2;
                f.bounds.xMax = c + twips / 2;
            } else f.bounds.xMax = f.bounds.xMin + twips;
            w = swf->editTexts.emplace(wide, f).first;
        }
        editText = wide;
        it = w;
    }
    const flash::Rect& b = it->second.bounds;
    flash::Matrix m;
    m.a = m.d = scale;
    m.tx = x - (b.xMin + b.xMax) / 2 / 20 * scale;
    m.ty = y - b.yMin / 20 * scale;
    float gm[16] = {m.a, m.b, 0, 0, m.c, m.d, 0, 0, 0, 0, 1, 0, m.tx, m.ty, 0, 1};
    glPushMatrix();
    glMultMatrixf(gm);
    {
        platform::KeyframeGroup group(owner);
        Draw visit{*this, glScale() * platform::displayScale() * platform::game().artScale};
        visit.leaf(Leaf{flash::Swf::Kind::kEditText, editText, 0, &text, flash::Matrix{}, flash::ColorTransform{}, owner});
    }
    glPopMatrix();
}

void VectorArt::draw(int symbol, int frame, long age, const VectorTransform& t, unsigned long long owner,
                     const VectorLook* look) {
    draw(symbol, frame, age, -1, t, owner, look);
}

void VectorArt::draw(const VectorPlayhead& p, const VectorTransform& t, unsigned long long owner, const VectorLook* look) {
    draw(p.symbol, p.frame, p.age, p.inFrame, t, owner, look);
}

void VectorArt::draw(int symbol, int frame, long age, long inFrame, const VectorTransform& t, unsigned long long owner,
                     const VectorLook* look) {
    if (symbol < 0) return;
    // The game's transform goes through the GL matrix and the art is one
    // keyframe group: smooth motion moves what the game moves, while the
    // art's own frames (its authored animation) change exactly at their pass.
    glPushMatrix();
    glTranslatef(t.x, t.y, 0);
    glRotatef(t.rotation, 0, 0, 1);
    glScalef(t.scaleX, t.scaleY, 1);
    if (prefetchOnly) {
        raster->prefetching = true;
        Draw visit{*this, glScale() * platform::displayScale() * platform::game().artScale, true};
        walk(symbol, frame, age, t.base, colorOf(t), look, owner, std::string(), visit, inFrame);
        raster->prefetching = false;
    } else {
        std::optional<platform::KeyframeGroup> group;
        if (!smoothTweens) group.emplace(owner);
        Draw visit{*this, glScale() * platform::displayScale() * platform::game().artScale};
        walk(symbol, frame, age, t.base, colorOf(t), look, owner, std::string(), visit, inFrame);
    }
    glPopMatrix();
}

void VectorArt::prefetch(int symbol, int frame, long age, const VectorTransform& t, unsigned long long owner,
                         const VectorLook* look, long keepFrames) {
    prefetch(symbol, frame, age, -1, t, owner, look, keepFrames);
}

void VectorArt::prefetch(int symbol, int frame, long age, long inFrame, const VectorTransform& t,
                         unsigned long long owner, const VectorLook* look, long keepFrames) {
    if (symbol < 0) return;
    // As draw() would draw it (under no GL transform: the game's own scale
    // is in the walk's matrix), but only queued.
    raster->prefetching = true;
    raster->prefetchKeep = keepFrames;
    Draw visit{*this, platform::displayScale() * platform::game().artScale, true};
    walk(symbol, frame, age, matrixOf(t), colorOf(t), look, owner, std::string(), visit, inFrame);
    raster->prefetching = false;
    raster->prefetchKeep = 0;
}

int VectorArt::buttonAt(int symbol, int frame, long age, long inFrame, const VectorTransform& t, float x, float y,
                        const VectorLook* look) {
    std::string clipPath;
    return buttonAt(symbol, frame, age, inFrame, t, x, y, look, clipPath);
}

int VectorArt::buttonAt(int symbol, int frame, long age, long inFrame, const VectorTransform& t, float x, float y,
                        const VectorLook* look, std::string& clipPath, int* topDepth) {
    clipPath.clear();
    if (symbol < 0) return -1;
    // The last button drawn whose hit area covers the point (the topmost).
    struct Pick : VisitBase {
        Pick(VectorArt& art, float x, float y) : art(art), x(x), y(y) {}
        VectorArt& art;
        float x, y;
        int found = -1;
        std::string foundPath;
        int foundTop = 0;
        struct Entered {
            int button;        // the button entered, or -1
            std::string clip;  // the clip path inside it
            int top;           // the depth of the symbol's own piece it is in
        };
        std::vector<Entered> stack{{-1, std::string(), 0}};
        std::string pending;  // the path of the sprite about to be entered
        int pendingTop = 0;
        void piece(int d, int) {
            if (stack.size() == 1) pendingTop = d;
        }
        void placed(const std::string& names, const flash::Matrix&, const flash::Matrix&, int, unsigned long long) {
            pending = names;
        }
        void placedUnnamed(const std::string& parent, const flash::Matrix&, const flash::Matrix&, int character,
                           unsigned long long) {
            pending = (parent.empty() ? "#" : parent + ".#") + std::to_string(character);
        }
        void enter(int character, const std::string&) {
            bool button = art.swf->kind(character) == flash::Swf::Kind::kButton;
            stack.push_back({button ? character : -1, button ? stack.back().clip : pending,
                             stack.size() == 1 ? pendingTop : stack.back().top});
        }
        void leave() { stack.pop_back(); }
        int buttonStates(int, const VectorLook*) const { return 8; }
        void leaf(const Leaf& l) {
            int button = -1, top = 0;
            std::string path;
            for (const Entered& e : stack)
                if (e.button >= 0) {
                    button = e.button;
                    path = e.clip;
                    top = e.top;
                }
            if (button < 0) return;
            float lx, ly;
            l.world.inverse().apply(x, y, lx, ly);
            bool hit = false;
            if (l.kind == flash::Swf::Kind::kShape) {
                hit = art.raster->shapeContains(l.character, lx, ly);
            } else {
                flash::Rect b = art.localBounds(l);
                hit = !b.empty() && lx >= b.xMin && lx <= b.xMax && ly >= b.yMin && ly <= b.yMax;
            }
            if (hit) {
                found = button;
                foundPath = path;
                foundTop = top;
            }
        }
    } visit{*this, x * 20, y * 20};
    walk(symbol, frame, age, matrixOf(t), colorOf(t), look, 0, std::string(), visit, inFrame);
    clipPath = visit.foundPath;
    if (topDepth) *topDepth = visit.foundTop;
    return visit.found;
}

int VectorArt::buttonAt(const VectorPlayhead& p, const VectorTransform& t, float x, float y) {
    return buttonAt(p.symbol, p.frame, p.age, p.inFrame, t, x, y);
}

bool VectorArt::hitTest(int symbol, int frame, long age, const VectorTransform& t, float x, float y, int only) {
    if (symbol < 0) return false;
    struct Hit : VisitBase {
        Hit(VectorArt& art, float x, float y, int only) : art(art), x(x), y(y), only(only) {}
        VectorArt& art;
        float x, y;  // twips
        int only;
        bool hit = false;
        std::vector<int> stack;  // inner characters entered
        void enter(int character, const std::string&) { stack.push_back(character); }
        void leave() { stack.pop_back(); }
        int buttonStates(int, const VectorLook*) const { return 8; }  // the hit area
        void leaf(const Leaf& l) {
            if (hit || l.color.mult[3] <= 0) return;
            if (only >= 0 && std::find(stack.begin(), stack.end(), only) == stack.end()) return;
            float lx, ly;
            l.world.inverse().apply(x, y, lx, ly);
            if (l.kind == flash::Swf::Kind::kShape) {
                hit = art.raster->shapeContains(l.character, lx, ly);
                return;
            }
            flash::Rect b = art.localBounds(l);
            hit = !b.empty() && lx >= b.xMin && lx <= b.xMax && ly >= b.yMin && ly <= b.yMax;
        }
    } visit{*this, x * 20, y * 20, only};
    walk(symbol, frame, age, matrixOf(t), colorOf(t), nullptr, 0, std::string(), visit);
    return visit.hit;
}

bool vectorPathMatches(const std::string& key, const std::string& names) {
    // "/a.b": exactly that path from the symbol drawn (Flash's a.b), not
    // any piece whose path ends so.
    if (!key.empty() && key[0] == '/') return names.compare(0, std::string::npos, key, 1, std::string::npos) == 0;
    return names.size() >= key.size() && names.compare(names.size() - key.size(), key.size(), key) == 0 &&
           (names.size() == key.size() || names[names.size() - key.size() - 1] == '.');
}

namespace {

// Leaves under the inner piece at a path (all of them for an empty path).
struct PartFilter {
    const std::string& part;
    UnnamedChild unnamed;  // a path ending ".#<id>": that unnamed clip
    struct Entered {
        std::string names;
        bool isPrefix, inside;
    };
    std::vector<Entered> stack;
    explicit PartFilter(const std::string& part) : part(part), unnamed(part) {
        stack.push_back({std::string(), unnamed.id >= 0 && unnamed.prefix.empty(), false});
    }
    void enter(int character, const std::string& names) {
        const Entered& parent = stack.back();
        bool in = parent.inside;
        if (!in && unnamed.id >= 0) in = parent.isPrefix && character == unnamed.id && names == parent.names;
        else if (!in) in = !part.empty() && vectorPathMatches(part, names);
        bool isPrefix = unnamed.id >= 0 && !unnamed.prefix.empty() && names != parent.names &&
                        vectorPathMatches(unnamed.prefix, names);
        stack.push_back({names, isPrefix, in});
    }
    void leave() { stack.pop_back(); }
    bool on() const { return part.empty() || stack.back().inside; }
};

}  // namespace

bool VectorArt::hitTestPart(int symbol, int frame, long age, long inFrame, const VectorTransform& t,
                            const VectorLook* look, const std::string& part, float x, float y, bool shape) {
    if (symbol < 0) return false;
    struct Hit : VisitBase {
        Hit(VectorArt& art, float x, float y, const std::string& part, bool shape)
            : art(art), x(x), y(y), filter(part), shape(shape) {}
        VectorArt& art;
        float x, y;  // twips
        PartFilter filter;
        bool shape;
        bool hit = false;
        flash::Rect box{1, 1, 0, 0};
        bool includesHidden() const { return !shape; }
        void enter(int c, const std::string& names) { filter.enter(c, names); }
        void leave() { filter.leave(); }
        void leaf(const Leaf& l) {
            if (hit || !filter.on() || l.color.mult[3] <= 0) return;
            if (!shape) {
                flash::Rect b = transformRect(l.world, art.localBounds(l));
                if (!b.empty()) box = box.empty() ? b : flash::Rect{std::min(box.xMin, b.xMin), std::min(box.yMin, b.yMin),
                                                                     std::max(box.xMax, b.xMax), std::max(box.yMax, b.yMax)};
                return;
            }
            float lx, ly;
            l.world.inverse().apply(x, y, lx, ly);
            if (l.kind == flash::Swf::Kind::kShape) {
                hit = art.raster->shapeContains(l.character, lx, ly);
                return;
            }
            flash::Rect b = art.localBounds(l);
            hit = !b.empty() && lx >= b.xMin && lx <= b.xMax && ly >= b.yMin && ly <= b.yMax;
        }
    } visit{*this, x * 20, y * 20, part, shape};
    walk(symbol, frame, age, matrixOf(t), colorOf(t), look, 0, std::string(), visit, inFrame);
    if (!shape) {
        const flash::Rect& b = visit.box;
        return !b.empty() && x * 20 >= b.xMin && x * 20 <= b.xMax && y * 20 >= b.yMin && y * 20 <= b.yMax;
    }
    return visit.hit;
}

flash::Rect VectorArt::partBounds(int symbol, int frame, long age, long inFrame, const VectorTransform& t,
                                  const VectorLook* look, const std::string& part) {
    struct Bounds : VisitBase {
        Bounds(VectorArt& art, const std::string& part) : art(art), filter(part) {}
        VectorArt& art;
        PartFilter filter;
        flash::Rect r{1, 1, 0, 0};
        bool includesHidden() const { return true; }
        void enter(int c, const std::string& names) { filter.enter(c, names); }
        void leave() { filter.leave(); }
        void leaf(const Leaf& l) {
            if (!filter.on()) return;
            flash::Rect b = transformRect(l.world, art.localBounds(l));
            if (b.empty()) return;
            r = r.empty() ? b : flash::Rect{std::min(r.xMin, b.xMin), std::min(r.yMin, b.yMin), std::max(r.xMax, b.xMax),
                                            std::max(r.yMax, b.yMax)};
        }
    } visit{*this, part};
    if (symbol >= 0) walk(symbol, frame, age, matrixOf(t), colorOf(t), look, 0, std::string(), visit, inFrame);
    flash::Rect r = visit.r;
    if (!r.empty()) r = {r.xMin / 20, r.yMin / 20, r.xMax / 20, r.yMax / 20};
    return r;
}

std::vector<flash::Rect> VectorArt::childBounds(int symbol, int frame, long age, long inFrame, const VectorTransform& t,
                                                const VectorLook* look, const std::string& part) {
    struct Bounds : VisitBase {
        Bounds(VectorArt& art, const std::string& part) : art(art), filter(part), stack{{part.empty(), -1}} {}
        VectorArt& art;
        PartFilter filter;
        // Each level entered: whether it is the part itself, and the box
        // (index) its leaves go to (-1: none).
        struct Entered {
            bool isPart;
            int box;
        };
        std::vector<Entered> stack;  // (from the symbol: itself the part for "")
        std::vector<flash::Rect> boxes;
        bool includesHidden() const { return true; }
        void enter(int c, const std::string& names) {
            const bool parentIsPart = stack.back().isPart;
            filter.enter(c, names);
            const bool isPart = !filter.part.empty() && filter.on() && !parentIsPart && stack.back().box < 0 && !inPart();
            int box = stack.back().box;
            if (parentIsPart && art.swf->kind(c) == flash::Swf::Kind::kSprite) {
                box = static_cast<int>(boxes.size());
                boxes.push_back({1, 1, 0, 0});
            }
            stack.push_back({isPart, box});
        }
        bool inPart() const {
            for (const Entered& e : stack)
                if (e.isPart) return true;
            return false;
        }
        void leave() {
            filter.leave();
            stack.pop_back();
        }
        void leaf(const Leaf& l) {
            const int k = stack.back().box;
            if (k < 0) return;
            flash::Rect b = transformRect(l.world, art.localBounds(l));
            if (b.empty()) return;
            flash::Rect& r = boxes[static_cast<size_t>(k)];
            r = r.empty() ? b : flash::Rect{std::min(r.xMin, b.xMin), std::min(r.yMin, b.yMin), std::max(r.xMax, b.xMax),
                                            std::max(r.yMax, b.yMax)};
        }
    } visit{*this, part};
    if (symbol >= 0) walk(symbol, frame, age, matrixOf(t), colorOf(t), look, 0, std::string(), visit, inFrame);
    std::vector<flash::Rect> out;
    for (const flash::Rect& r : visit.boxes)
        if (!r.empty()) out.push_back({r.xMin / 20, r.yMin / 20, r.xMax / 20, r.yMax / 20});
    return out;
}

int VectorArt::innerFrame(int symbol, int frame, long age, const std::string& path, const VectorLook* look, long inFrame,
                          long* shownAge, long* shownInFrame) {
    if (symbol < 0) return 0;
    struct Find : VisitBase {
        bool walksMasks() const { return true; }
        explicit Find(const std::string& path) : path(path) {}
        const std::string& path;
        int found = 0;
        long age = 0, inFrame = 0;
        void shown(int, const std::string& names, int f, long a, long n) {
            // (the clip itself, met before the unnamed clips inside it,
            // which have its path too)
            if (names != path || found) return;
            found = f;
            age = a;
            inFrame = n;
        }
        void leaf(const Leaf&) {}
    } visit{path};
    walk(symbol, frame, age, flash::Matrix{}, flash::ColorTransform{}, look, 0, std::string(), visit, inFrame);
    if (shownAge) *shownAge = visit.age;
    if (shownInFrame) *shownInFrame = visit.inFrame;
    return visit.found;
}

bool VectorArt::partInfo(int symbol, int frame, long age, long inFrame, const VectorLook* look, const std::string& path,
                         flash::Matrix& local, flash::Matrix& inSymbol, int& character, unsigned long long* instance) {
    if (symbol < 0) return false;
    struct Find : VisitBase {
        bool walksMasks() const { return true; }
        explicit Find(const std::string& path) : path(path) {}
        const std::string& path;
        bool found = false;
        flash::Matrix local, world;
        int character = -1;
        unsigned long long instance = 0;
        bool includesHidden() const { return true; }
        void placedUnnamed(const std::string& parent, const flash::Matrix& l, const flash::Matrix& w, int c,
                           unsigned long long id) {
            UnnamedChild u(path);
            if (!found && u.id == c && (u.prefix.empty() ? parent.empty() : vectorPathMatches(u.prefix, parent)))
                take(l, w, c, id);
        }
        void placed(const std::string& names, const flash::Matrix& l, const flash::Matrix& w, int c,
                    unsigned long long id) {
            if (!found && vectorPathMatches(path, names)) take(l, w, c, id);
        }
        void take(const flash::Matrix& l, const flash::Matrix& w, int c, unsigned long long id) {
            found = true;
            local = l;
            world = w;
            character = c;
            instance = id;
        }
        void leaf(const Leaf&) {}
    } visit{path};
    walk(symbol, frame, age, flash::Matrix{}, flash::ColorTransform{}, look, 0, std::string(), visit, inFrame);
    if (!visit.found) return false;
    local = visit.local;
    inSymbol = visit.world;
    character = visit.character;
    if (instance) *instance = visit.instance;
    return true;
}

int VectorArt::innerSymbol(int symbol, int frame, long age, const std::string& path) {
    if (symbol < 0) return -1;
    struct Find : VisitBase {
        bool walksMasks() const { return true; }
        explicit Find(const std::string& path) : path(path) {}
        const std::string& path;
        int found = -1;
        void shown(int sprite, const std::string& names, int, long, long) {
            if (found < 0 && vectorPathMatches(path, names)) found = sprite;
        }
        void leaf(const Leaf&) {}
    } visit{path};
    walk(symbol, frame, age, flash::Matrix{}, flash::ColorTransform{}, nullptr, 0, std::string(), visit);
    return visit.found;
}

void VectorArt::drawUnder(const flash::Matrix& world, const flash::ColorTransform& color, int symbol, int frame, long age,
                          long inFrame, unsigned long long owner, const VectorLook* look) {
    if (symbol < 0) return;
    float m[16] = {world.a, world.b, 0, 0, world.c, world.d, 0, 0, 0, 0, 1, 0, world.tx, world.ty, 0, 1};
    glPushMatrix();
    glMultMatrixf(m);
    if (prefetchOnly) {
        raster->prefetching = true;
        Draw visit{*this, glScale() * platform::displayScale() * platform::game().artScale, true};
        walk(symbol, frame, age, flash::Matrix{}, color, look, owner, std::string(), visit, inFrame);
        raster->prefetching = false;
    } else {
        std::optional<platform::KeyframeGroup> group;
        if (!smoothTweens) group.emplace(owner);
        Draw visit{*this, glScale() * platform::displayScale() * platform::game().artScale};
        // (Keyed by the node and its slice of depths: the stage is drawn in slices.)
        unsigned long long key = owner ^ (static_cast<unsigned long long>(look ? look->fromDepth : 0) << 32);
        if (meshes || !drewStill(symbol, frame, age, inFrame, flash::Matrix{}, world, color, look, owner, std::string(), key, 0,
                                 flash::Matrix{}, owner ^ 0x5eedull, visit))
            walk(symbol, frame, age, flash::Matrix{}, color, look, owner, std::string(), visit, inFrame);
    }
    glPopMatrix();
}


std::vector<std::string> VectorArt::childClips(int symbol, int frame, long age, long inFrame, const VectorLook* look,
                                               const std::string& path) {
    std::vector<std::string> out;
    if (symbol < 0) return out;
    struct Find : VisitBase {
        bool walksMasks() const { return true; }
        Find(VectorArt& art, const std::string& path, std::vector<std::string>& out) : art(art), path(path), out(out) {}
        VectorArt& art;
        const std::string& path;
        std::vector<std::string>& out;
        struct Entered {
            std::string names;
            bool isPart;
        };
        std::vector<Entered> stack{{std::string(), path.empty()}};
        struct Child {
            std::string name;
            int placedFrame, depth;
        };
        std::vector<Child> found;
        int depth = 0, placedFrame = 0;
        bool includesHidden() const { return true; }
        void piece(int d, int f) {
            depth = d;
            placedFrame = f;
        }
        void enter(int character, const std::string& names) {
            const Entered& parent = stack.back();
            if (parent.isPart && art.swf->kind(character) == flash::Swf::Kind::kSprite) {
                std::string own = names.size() <= parent.names.size()
                                      ? "#" + std::to_string(character)
                                      : names.substr(parent.names.empty() ? 0 : parent.names.size() + 1);
                if (std::none_of(found.begin(), found.end(), [&](const Child& c) { return c.name == own; }))
                    found.push_back({own, placedFrame, depth});
            }
            stack.push_back({names, !path.empty() && names != parent.names && vectorPathMatches(path, names)});
        }
        void leave() { stack.pop_back(); }
        void leaf(const Leaf&) {}
    } visit{*this, path, out};
    walk(symbol, frame, age, flash::Matrix{}, flash::ColorTransform{}, look, 0, std::string(), visit, inFrame);
    // Flash's for..in meets a clip's children newest first: those placed on
    // a later frame, then (one frame's tags go up in depth) the deeper.
    std::stable_sort(visit.found.begin(), visit.found.end(), [](const auto& a, const auto& b) {
        return a.placedFrame != b.placedFrame ? a.placedFrame > b.placedFrame : a.depth > b.depth;
    });
    for (const auto& c : visit.found) out.push_back(c.name);
    return out;
}

void VectorArt::clips(int symbol, int frame, long age, long inFrame, const VectorLook* look,
                      std::vector<ClipVisit>& out) {
    if (symbol < 0) return;
    struct Find : VisitBase {
        bool walksMasks() const { return true; }
        explicit Find(std::vector<ClipVisit>& out) : out(out) {}
        std::vector<ClipVisit>& out;
        ClipVisit pending{std::string(), -1, 0, 0};
        std::vector<unsigned long long> inside;  // the clips entered, outermost first
        bool includesHidden() const { return true; }
        void enter(int character, const std::string&) {
            inside.push_back(pending.character == character ? pending.instance : 0);
        }
        void leave() {
            if (!inside.empty()) inside.pop_back();
        }
        void placed(const std::string& names, const flash::Matrix&, const flash::Matrix&, int character,
                    unsigned long long id) {
            pending = {names, character, 0, id};
        }
        void placedUnnamed(const std::string& parent, const flash::Matrix&, const flash::Matrix&, int character,
                           unsigned long long id) {
            pending = {(parent.empty() ? "#" : parent + ".#") + std::to_string(character), character, 0, id};
        }
        void shown(int sprite, const std::string&, int f, long, long) {
            if (pending.character != sprite) return;
            pending.frame = f;
            // (shown from inside its own walk: entered already)
            pending.parent = inside.size() >= 2 ? inside[inside.size() - 2] : 0;
            out.push_back(pending);
            pending.character = -1;
        }
        void leaf(const Leaf&) {}
    } visit{out};
    walk(symbol, frame, age, flash::Matrix{}, flash::ColorTransform{}, look, 0, std::string(), visit, inFrame);
}

std::vector<VectorArt::TopClip> VectorArt::topClips(int symbol, int frame) {
    std::vector<TopClip> out;
    if (swf->kind(symbol) != flash::Swf::Kind::kSprite) return out;
    Timeline& t = timeline(symbol);
    if (t.frames.empty()) return out;
    frame = std::clamp(frame, 1, static_cast<int>(t.frames.size()));
    for (const Piece& p : t.frames[frame - 1]) {
        if (swf->kind(p.character) != flash::Swf::Kind::kSprite) continue;
        // (the same instance id as walk() gives it under a root)
        unsigned long long id = (1 * 0x9E3779B97F4A7C15ull) ^ (static_cast<unsigned long long>(p.depth) << 40) ^
                                (static_cast<unsigned long long>(p.placedFrame) << 20) ^
                                static_cast<unsigned long long>(p.character);
        out.push_back({p.name.empty() ? "#" + std::to_string(p.character) : p.name, p.character, p.depth, id, p.matrix, p.color});
    }
    return out;
}

void VectorArt::setRandomStart(int sprite, int frame, std::vector<int> choices, bool play) {
    auto it = swf->sprites.find(sprite);
    if (it == swf->sprites.end() || frame < 1) return;
    auto& actions = it->second.actions;
    if (static_cast<int>(actions.size()) < frame) actions.resize(frame);
    flash::FrameAction a{flash::FrameAction::kGotoRandom};
    a.frame = choices.empty() ? 1 : choices[0];
    a.count = static_cast<int>(choices.size());
    a.choices = std::move(choices);
    a.play = play;
    actions[frame - 1] = {a};
    timelines.erase(sprite);
}

void VectorArt::setRepeat(int sprite, int frame, int from, int times) {
    auto it = swf->sprites.find(sprite);
    if (it == swf->sprites.end() || frame < 1) return;
    auto& actions = it->second.actions;
    if (static_cast<int>(actions.size()) < frame) actions.resize(frame);
    flash::FrameAction a{flash::FrameAction::kRepeat};
    a.frame = from;
    a.count = times;
    actions[frame - 1] = {a};
    timelines.erase(sprite);
}

void VectorArt::addStop(int sprite, int frame) {
    auto it = swf->sprites.find(sprite);
    if (it == swf->sprites.end() || frame < 1) return;
    auto& actions = it->second.actions;
    if (static_cast<int>(actions.size()) < frame) actions.resize(frame);
    for (const flash::FrameAction& a : actions[frame - 1])
        if (a.kind == flash::FrameAction::kStop) return;
    actions[frame - 1].push_back(flash::FrameAction{flash::FrameAction::kStop});
    timelines.erase(sprite);
}

void VectorArt::addRandomTrigger(int parent, const std::string& child, int chance) {
    triggers.push_back({parent, child, std::max(2, chance)});
}

int VectorArt::triggeredFrame(int sprite, int chance, unsigned long long seed, long age, long& since) {
    // How the clip plays once started: from frame 2 on, until it stops.
    Timeline& tl = timeline(sprite);
    auto run = tl.picked.find(-2);
    if (run == tl.picked.end()) run = tl.picked.emplace(-2, simulate(sprite, 2, true, false)).first;
    const Run& r = run->second;
    long length = r.cycleFrom >= 0 ? (1L << 30) : static_cast<long>(r.heads.size()) + 1;
    TriggerState& s = triggerStates[seed * 31 + static_cast<unsigned long long>(sprite)];
    if (age < s.simulated) s = TriggerState{};
    for (long t = s.simulated + 1; t <= age; ++t) {
        if (t < s.busyUntil) continue;
        unsigned long long h = (seed ^ (static_cast<unsigned long long>(t) * 0x9E3779B97F4A7C15ull)) * 0xBF58476D1CE4E5B9ull;
        h ^= h >> 29;
        if (h % static_cast<unsigned long long>(chance) == 1) {
            s.started = t;
            s.busyUntil = t + length;
        }
    }
    s.simulated = std::max(s.simulated, age);
    since = -1;
    if (s.started < 0 || age >= s.busyUntil) return 1;
    since = age - s.started;
    if (since == 0 || r.heads.empty()) return 1;
    long i = since - 1;
    if (i < static_cast<long>(r.heads.size())) return r.heads[i];
    long period = static_cast<long>(r.heads.size()) - r.cycleFrom;
    return r.heads[r.cycleFrom + (i - r.cycleFrom) % period];
}

void VectorArt::setFrameOverride(int character, int frame) {
    if (frame > 0) frameOverrides[character] = frame;
    else frameOverrides.erase(character);
}

void VectorArt::dumpTree(int symbol, int frame, long age) {
    struct Dump : VisitBase {
        int depth = 0;
        void enter(int, const std::string&) { ++depth; }
        void leave() { --depth; }
        void shown(int sprite, const std::string& names, int f, long a, long in) {
            std::fprintf(stderr, "%*s#%d '%s' frame %d age %ld (on frame %ld)\n", depth * 2, "", sprite, names.c_str(), f, a, in);
        }
        void leaf(const Leaf&) {}
    } visit;
    walk(symbol, frame, age, flash::Matrix{}, flash::ColorTransform{}, nullptr, 0, std::string(), visit);
}

flash::Rect VectorArt::bounds(int symbol, int frame, long age, const VectorTransform& t) {
    struct Bounds : VisitBase {
        explicit Bounds(VectorArt& art) : art(art) {}
        VectorArt& art;
        flash::Rect r{1, 1, 0, 0};
        void leaf(const Leaf& l) {
            flash::Rect b = transformRect(l.world, art.localBounds(l));
            if (b.empty()) return;
            if (r.empty()) r = b;
            else r = {std::min(r.xMin, b.xMin), std::min(r.yMin, b.yMin), std::max(r.xMax, b.xMax), std::max(r.yMax, b.yMax)};
        }
    } visit{*this};
    if (symbol >= 0) walk(symbol, frame, age, matrixOf(t), colorOf(t), nullptr, 0, std::string(), visit);
    flash::Rect r = visit.r;
    if (!r.empty()) r = {r.xMin / 20, r.yMin / 20, r.xMax / 20, r.yMax / 20};
    return r;
}

flash::Rect VectorArt::localBounds(const Leaf& l) const {
    using Kind = flash::Swf::Kind;
    switch (l.kind) {
    case Kind::kShape: return swf->shape(l.character)->bounds;
    case Kind::kMorph: {
        const auto& m = swf->morphs.at(l.character);
        const flash::Rect &a = m.first.bounds, &b = m.second.bounds;
        return {a.xMin + (b.xMin - a.xMin) * l.ratio, a.yMin + (b.yMin - a.yMin) * l.ratio,
                a.xMax + (b.xMax - a.xMax) * l.ratio, a.yMax + (b.yMax - a.yMax) * l.ratio};
    }
    case Kind::kText: return swf->texts.at(l.character).bounds;
    case Kind::kEditText: return swf->editTexts.at(l.character).bounds;
    default: return {1, 1, 0, 0};
    }
}

// ------------------------------------------------------------- sounds

const VectorArt::Pcm* VectorArt::sound(const std::string& linkage) {
    auto found = sounds.find(linkage);
    if (found != sounds.end()) return found->second.samples ? &found->second : nullptr;
    Pcm& pcm = sounds[linkage];
    auto e = swf->exports.find(linkage);
    auto s = e == swf->exports.end() ? swf->sounds.end() : swf->sounds.find(e->second);
    if (s == swf->sounds.end()) return nullptr;
    const flash::Sound& snd = s->second;
    pcm.channels = snd.stereo ? 2 : 1;
    pcm.rate = snd.rate;
    if (snd.format == 2) {  // MP3
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, 0, 0);
        ma_uint64 frames = 0;
        void* out = nullptr;
        if (ma_decode_memory(snd.data.data(), snd.data.size(), &cfg, &frames, &out) == MA_SUCCESS) {
            // As Flash played it: from after the encoder's delay (the seek
            // samples), for the sound's sample count: no silence at either
            // end, so a loop is seamless.
            const int16_t* f = static_cast<const int16_t*>(out);
            ma_uint64 skip = std::min<ma_uint64>(static_cast<ma_uint64>(std::max(0, snd.seekSamples)), frames);
            ma_uint64 count = frames - skip;
            // (Unless the count is plainly wrong: AS1's tongs grab claims 154
            // samples, 14 ms, of an effect five times as long, which the
            // original played whole.)
            if (snd.samples > 0 && snd.samples >= count / 2) count = std::min<ma_uint64>(count, snd.samples);
            pcm.samples = std::make_shared<const std::vector<int16_t>>(f + skip * cfg.channels,
                                                                       f + (skip + count) * cfg.channels);
            pcm.channels = static_cast<int>(cfg.channels);
            pcm.rate = static_cast<int>(cfg.sampleRate);
            ma_free(out, nullptr);
        }
    } else if (snd.format == 0 || snd.format == 3) {  // raw PCM
        size_t n = snd.is16 ? snd.data.size() / 2 : snd.data.size();
        std::vector<int16_t> samples(n);
        for (size_t i = 0; i < n; ++i)
            samples[i] = snd.is16 ? static_cast<int16_t>(snd.data[2 * i] | (snd.data[2 * i + 1] << 8))
                                  : static_cast<int16_t>((snd.data[i] - 128) * 256);
        pcm.samples = std::make_shared<const std::vector<int16_t>>(std::move(samples));
    }
    if (pcm.samples && pcm.samples->empty()) pcm.samples.reset();
    return pcm.samples ? &pcm : nullptr;
}

// ------------------------------------------------------------- playheads

VectorPlayhead::VectorPlayhead(VectorArt& art, int symbol) : art(&art), symbol(symbol) { enter(1); }

void VectorPlayhead::enter(int target) {
    if (!art) return;
    const flash::Swf& swf = art->movie();
    auto it = swf.sprites.find(symbol);
    int count = art->frameCount(symbol);
    std::vector<int> queue{target};
    for (size_t q = 0; q < queue.size() && q < 8; ++q) {
        int f = std::clamp(queue[q], 1, count);
        if (f != frame) inFrame = 0;
        frame = f;
        if (it == swf.sprites.end() || frame > static_cast<int>(it->second.actions.size())) continue;
        for (const flash::FrameAction& a : it->second.actions[frame - 1]) {
            switch (a.kind) {
            case flash::FrameAction::kStop: playing = false; break;
            case flash::FrameAction::kPlay: playing = true; break;
            case flash::FrameAction::kGoto:
                playing = false;
                queue.push_back(a.frame);
                break;
            case flash::FrameAction::kGotoLabel:
                if (int l = art->frameOf(symbol, a.label)) {
                    playing = false;
                    queue.push_back(l);
                }
                break;
            case flash::FrameAction::kGotoRandom:
                playing = a.play;
                if (!a.choices.empty()) queue.push_back(a.choices[std::rand() % a.choices.size()]);
                else queue.push_back(a.frame + (a.count > 0 ? std::rand() % a.count : 0));
                break;
            case flash::FrameAction::kNext:
                playing = false;
                queue.push_back(std::min(frame + 1, count));
                break;
            case flash::FrameAction::kPrev:
                playing = false;
                queue.push_back(std::max(frame - 1, 1));
                break;
            case flash::FrameAction::kRepeat:
                if (++repeats < a.count) {
                    playing = true;
                    queue.push_back(a.frame);
                } else {
                    playing = false;
                    queue.push_back(std::min(frame + 1, count));
                }
                break;
            }
        }
    }
}

void VectorPlayhead::update() {
    ++age;
    ++inFrame;
    if (playing && art) enter(frame >= art->frameCount(symbol) ? 1 : frame + 1);
}

void VectorPlayhead::gotoAndStop(int f) {
    playing = false;
    enter(f);
}

void VectorPlayhead::gotoAndPlay(int f) {
    playing = true;
    enter(f);
}

int VectorPlayhead::labelFrame(const std::string& label) const { return art ? art->frameOf(symbol, label) : 0; }
