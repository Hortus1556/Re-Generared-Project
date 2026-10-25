#include "surgeon/Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_set>

#include "platform/Platform.h"

using script::ClipRef;
using script::Value;

namespace {

std::string lastSegment(const std::string& path) {
    size_t dot = path.rfind('.');
    return dot == std::string::npos ? path : path.substr(dot + 1);
}

std::string parentPath(const std::string& path) {
    size_t dot = path.rfind('.');
    return dot == std::string::npos ? std::string() : path.substr(0, dot);
}

}  // namespace

// --------------------------------------------------------------- the clip

SceneClip::SceneClip(Scene& scene, ArtNode* node, std::string path, unsigned long long instance)
    : node(retainRef(node)), part(std::move(path)), instance(instance), scene(scene) {}

SceneClip::~SceneClip() { releaseRef(node); }

void SceneClip::rebind(ArtNode* to, std::string path, unsigned long long id) {
    retainRef(to);
    releaseRef(node);
    node = to;
    part = std::move(path);
    instance = id;
}

ArtNode* SceneClip::asNode() { return part.empty() || scene.promote(*this) ? node : nullptr; }

std::string SceneClip::child(const std::string& name) const {
    // An unnamed clip ("a.#id") adds nothing to the instance paths of what
    // is in it: its pieces are "a.<name>" (AS1's popup text holder, unnamed,
    // stopping its `great` and hiding its `comboBox` by name).
    std::string base = part;
    if (!base.empty() && lastSegment(base)[0] == '#') base = parentPath(base);
    return base.empty() ? name : base + "." + name;
}

Value SceneClip::var(const std::string& name) const {
    for (const auto& [k, v] : vars)
        if (k == name) return v;
    return Value();
}

bool SceneClip::hasVar(const std::string& name) const {
    return std::any_of(vars.begin(), vars.end(), [&](const auto& p) { return p.first == name; });
}

void SceneClip::setVar(const std::string& name, const Value& v) {
    for (auto& [k, old] : vars)
        if (k == name) {
            old = v;
            return;
        }
    vars.emplace_back(name, v);
}

bool SceneClip::gone() const {
    for (const ArtNode* n = node; n; n = n->parent)
        if (n->removed) return true;
    return false;
}

Value SceneClip::get(const std::string& name) {
    if (gone()) return Value();
    if (part.empty()) {
        auto b = scene.bindings.find({node->serial(), name});
        if (b != scene.bindings.end()) return b->second.get();
    }
    if (!name.empty() && name[0] == '_') {
        bool own = part.empty();
        if (name == "_x") return Value(own ? node->x : node->partX(part));
        if (name == "_y") return Value(own ? node->y : node->partY(part));
        if (name == "_visible") return Value(own ? node->visible : !node->partHidden(part));
        if (name == "_currentframe") return Value(currentFrame());
        if (name == "_totalframes") return Value(own ? node->frameCount() : scene.art.frameCount(sprite()));
        if (name == "_name") return Value(own ? node->name : lastSegment(part));
        if (name == "_alpha") return Value(own ? node->opacity() * 100 : 100.0f);
        if (name == "_rotation" || name == "_xscale" || name == "_yscale") {
            if (own) return Value(name == "_rotation" ? node->rotation : name == "_xscale" ? node->xscale : node->yscale);
            // (as ArtNode::setMatrix reads a matrix)
            flash::Matrix m = node->partMatrix(part);
            if (name == "_rotation") return Value(std::atan2(m.b, m.a) * 180 / 3.14159265358979);
            if (name == "_xscale") return Value(std::sqrt(m.a * m.a + m.b * m.b) * 100);
            return Value(std::sqrt(m.c * m.c + m.d * m.d) * 100 * (m.a * m.d - m.b * m.c < 0 ? -1 : 1));
        }
        if (name == "_width") return Value(own ? static_cast<double>(node->bounds().xMax - node->bounds().xMin) : node->partWidth(part));
        if (name == "_height") return Value(own ? static_cast<double>(node->bounds().yMax - node->bounds().yMin) : node->partHeight(part));
        if (name == "_xmouse" || name == "_ymouse") {
            float x = scene.mouseX, y = scene.mouseY;
            if (own) node->globalToLocal(x, y);
            else node->stageToPart(part, scene.mouseX, scene.mouseY, x, y);
            return Value(name == "_xmouse" ? x : y);
        }
        if (name == "_parent") return Value(parent());
        if (name == "_target") return Value(path());
    }
    for (const auto& [k, v] : vars)
        if (k == name) return v;
    // A child clip: attached by the game, or in the symbol.
    if (part.empty())
        if (ArtNode* c = node->child(name)) return Value(ClipRef(scene.clip(c)));
    auto h = scene.handed.find({node->serial(), child(name)});
    if (h != scene.handed.end()) return Value(h->second);
    if (auto c = part.empty() ? scene.clip(node, child(name)) : scene.clipWithin(node, child(name), instance))
        return Value(ClipRef(c));
    return Value();
}

unsigned long long SceneClip::own() const {
    if (part.empty()) return 0;
    return shared || lastSegment(part)[0] == '#' ? instance : 0;
}

void SceneClip::set(const std::string& name, const Value& v) {
    if (gone()) return;
    if (part.empty()) {
        auto b = scene.bindings.find({node->serial(), name});
        if (b != scene.bindings.end()) {
            if (b->second.set) b->second.set(v);
            return;
        }
    }
    if (!name.empty() && name[0] == '_') {
        bool own = part.empty();
        if (name == "_visible") {
            if (own) node->visible = v.toBool();
            else node->hidePart(part, !v.toBool());
            return;
        }
        if (name == "_x" || name == "_y") {
            double d = v.toNumber();
            if (std::isnan(d)) return;  // (Flash ignores it)
            if (own) {
                (name == "_x" ? node->x : node->y) = static_cast<float>(d);
                node->scripted = true;
            } else if (name == "_x") node->setPartPosition(part, static_cast<float>(d), node->partY(part));
            else node->setPartPosition(part, node->partX(part), static_cast<float>(d));
            return;
        }
        if (name == "_alpha" || name == "_rotation" || name == "_xscale" || name == "_yscale") {
            double d = v.toNumber();
            ArtNode* n = std::isnan(d) ? nullptr : asNode();
            if (!n) return;
            float& f = name == "_alpha" ? n->alpha : name == "_rotation" ? n->rotation
                       : name == "_xscale"                             ? n->xscale
                                                                       : n->yscale;
            f = static_cast<float>(d);
            if (name != "_alpha") n->scripted = true;
            else n->alphaSet = true;
            return;
        }
    }
    setVar(name, v);
}

bool SceneClip::remove(const std::string& name) {
    for (auto it = vars.begin(); it != vars.end(); ++it)
        if (it->first == name) {
            vars.erase(it);
            return true;
        }
    return false;
}

namespace {

// The unnamed sprite of `ancestor`'s frame `frame` (directly, or in more
// unnamed clips) that holds the clip `last` (its name, or "#id" for an
// unnamed one) directly: 0 if it is placed right in `ancestor`, -1 if not
// found.
int holderOf(VectorArt& art, ArtNode* node, const std::string& at, int ancestor, int frame, const std::string& last,
             int depth = 0) {
    std::vector<VectorArt::TopClip> pieces = art.topClips(ancestor, frame);
    bool unnamed = last[0] == '#';
    int id = unnamed ? std::atoi(last.c_str() + 1) : 0;
    for (const auto& p : pieces)
        if (unnamed ? p.character == id && p.path[0] == '#' : p.path == last) return 0;
    if (depth > 4) return -1;
    for (const auto& p : pieces) {
        if (p.path[0] != '#') continue;  // (a named clip starts a path of its own)
        std::string inner = at.empty() ? p.path : at + "." + p.path;
        int f = node->partFrame(inner);
        int found = holderOf(art, node, at, p.character, f > 0 ? f : 1, last, depth + 1);
        if (found == 0) return p.character;
        if (found > 0) return found;
    }
    return -1;
}

}  // namespace

ClipRef SceneClip::parent() {
    if (part.empty()) {
        if (node->parent) return scene.clip(node->parent);
        return scene.parentOf(node);
    }
    std::string up = parentPath(part);
    // A clip ("a.b", or unnamed "a.#id") may sit in unnamed clips inside a,
    // which instance paths do not show: its parent is the innermost of those
    // (AS1's popup combo box, whose text is `_parent._parent.com`).
    std::string last = lastSegment(part);
    int sprite = up.empty() ? node->symbol() : node->partSymbol(up);
    int frame = up.empty() ? node->frame() : node->partFrame(up);
    int holder = sprite >= 0 ? holderOf(scene.art, node, up, sprite, frame > 0 ? frame : 1, last) : -1;
    // Where the holder is unnamed or not found by path, the clip it is in is
    // the walk's (VectorArt::ClipVisit::parent): several copies of an
    // unnamed holder share its path (AS1's eight bandage strips, each
    // re-placing its stain every frame: the stain keeps its strip's pick in
    // `_parent.rando`), and a holder empty on its first frame is not found
    // by its art (a strip's stain comes from its frame 2).
    if (holder != 0 && instance) {
        std::vector<VectorArt::ClipVisit> shown;
        node->innerClips(shown);
        unsigned long long in = 0;
        for (const VectorArt::ClipVisit& v : shown)
            if (v.instance == instance) in = v.parent;
        int copies = 0;
        const VectorArt::ClipVisit* pick = nullptr;
        for (const VectorArt::ClipVisit& v : shown)
            if (in && v.instance == in) pick = &v;
        if (pick) {
            for (const VectorArt::ClipVisit& v : shown) copies += v.path == pick->path;
            Scene::Entry& e = scene.entries[{node->serial(), pick->instance}];
            if (!e.clip) e.clip = std::make_shared<SceneClip>(scene, node, pick->path, pick->instance);
            if (copies > 1) e.clip->shared = true;
            return e.clip;
        }
    }
    if (holder > 0) return scene.clip(node, (up.empty() ? "#" : up + ".#") + std::to_string(holder));
    return scene.clip(node, up);
}

void SceneClip::gotoFrame(const Value& frame, bool playing) {
    int f = 0;
    if (frame.type == Value::kString) {
        f = scene.art.frameOf(sprite(), frame.string);
        if (!f) {
            char* end = nullptr;
            long n = std::strtol(frame.string.c_str(), &end, 10);
            if (end && !*end && !frame.string.empty()) f = static_cast<int>(n);
        }
    } else {
        f = frame.toInt();
    }
    if (platform::debugMode() && std::getenv("AS_TRACE_GOTO"))
        platform::debugTrace("gotoFrame part='%s' sprite=%d arg=%s -> f=%d playing=%d", part.c_str(), sprite(),
                             frame.type == Value::kString ? frame.string.c_str() : std::to_string(frame.toNumber()).c_str(),
                             f, playing);
    if (f < 1) {
        // Flash's gotoAndStop/gotoAndPlay always takes effect (stops or
        // plays the clip) even when the frame/label it was given does not
        // resolve to anything (undefined, a label the symbol lacks, NaN):
        // it just does not move. Without this a clip meant to be held in
        // place by such a call is left to play on by itself instead.
        f = currentFrame();
        if (f < 1) return;
    }
    if (part.empty()) {
        if (playing) node->gotoAndPlay(f);
        else node->gotoAndStop(f);
    } else if (playing) {
        node->partGotoAndPlay(part, f, own());
    } else {
        node->partGotoAndStop(part, f, own());
    }
}

void SceneClip::play() {
    if (part.empty()) node->play();
    else if (VectorPlayhead* p = node->part(part, own())) p->play();
}

void SceneClip::stop() {
    if (part.empty()) node->stop();
    else if (VectorPlayhead* p = node->part(part, own())) p->stop();
}

void SceneClip::nextFrame() {
    int f = currentFrame() + 1;
    if (f <= (part.empty() ? node->frameCount() : scene.art.frameCount(sprite()))) gotoFrame(Value(f), false);
}

void SceneClip::prevFrame() {
    if (currentFrame() > 1) gotoFrame(Value(currentFrame() - 1), false);
}

bool SceneClip::call(const std::string& method, const std::vector<Value>& args, Value& result) {
    if (gone()) return true;  // (nothing there to do it)
    auto num = [&](size_t i) { return i < args.size() ? args[i].toNumber() : 0.0; };
    if (method == "getBytesLoaded" || method == "getBytesTotal") {
        result = Value(100);  // (all of it is there)
        return true;
    }
    if (method == "getDepth") {
        result = Value(part.empty() ? node->depth : 0);
        return true;
    }
    if (method == "removeMovieClip") {
        // (a clip the timeline placed stays, as in Flash, unless its depth
        // was swapped)
        if (part.empty() && node->parent && !node->timeline) node->remove();
        return true;
    }
    if (method == "swapDepths") {
        int depth = 0;
        if (!args.empty() && args[0].type == Value::kNumber) {
            depth = args[0].toInt();
        } else if (!args.empty() && args[0].type == Value::kClip) {
            auto* other = dynamic_cast<SceneClip*>(args[0].clip.get());
            ArtNode* o = other ? other->asNode() : nullptr;
            if (!o || o->parent != (part.empty() ? node->parent : node)) return true;
            depth = o->depth;
        } else {
            return true;
        }
        if (ArtNode* n = asNode()) n->swapDepths(depth);
        return true;
    }
    if (method == "attachMovie" || method == "createEmptyMovieClip") {
        bool empty = method == "createEmptyMovieClip";
        ArtNode* n = args.size() >= (empty ? 2u : 3u) ? asNode() : nullptr;
        ArtNode* c = !n      ? nullptr
                     : empty ? n->attach(new ArtNode(scene.art, -1), args[0].toString(), args[1].toInt())
                             : n->attach(args[0].toString(), args[1].toString(), args[2].toInt());
        if (c) result = Value(ClipRef(scene.clip(c)));
        return true;
    }
    if (method == "getBounds") {
        // Its box in the space of the clip given (its own if none).
        flash::Rect r = part.empty() ? node->bounds() : node->bounds(part);
        auto o = std::make_shared<script::Object>();
        if (r.empty()) {
            for (const char* k : {"xMin", "xMax", "yMin", "yMax"}) o->set(k, Value(6710886.35));
            result = Value(o);
            return true;
        }
        auto* space = this;
        if (!args.empty() && args[0].type == Value::kClip)
            if (auto* s = dynamic_cast<SceneClip*>(args[0].clip.get())) space = s;
        float xMin = 1e30f, yMin = 1e30f, xMax = -1e30f, yMax = -1e30f;
        for (int i = 0; i < 4; ++i) {
            float sx = i & 1 ? r.xMax : r.xMin, sy = i & 2 ? r.yMax : r.yMin, x = sx, y = sy;
            if (space->part.empty()) space->node->globalToLocal(x, y);
            else space->node->stageToPart(space->part, sx, sy, x, y);
            xMin = std::min(xMin, x);
            xMax = std::max(xMax, x);
            yMin = std::min(yMin, y);
            yMax = std::max(yMax, y);
        }
        o->set("xMin", Value(xMin));
        o->set("xMax", Value(xMax));
        o->set("yMin", Value(yMin));
        o->set("yMax", Value(yMax));
        result = Value(o);
        return true;
    }
    if (method == "hitTest") {
        if (args.size() >= 2) {
            result = Value(node->hitTest(static_cast<float>(num(0)), static_cast<float>(num(1)),
                                         args.size() > 2 && args[2].toBool(), part));
        } else if (!args.empty() && args[0].type == Value::kClip) {
            auto* other = dynamic_cast<SceneClip*>(args[0].clip.get());
            result = Value(other && node->hitTest(*other->node, part, other->part));
        }
        return true;
    }
    return false;
}

std::vector<std::string> SceneClip::keys() {
    std::vector<std::string> out;
    for (auto it = vars.rbegin(); it != vars.rend(); ++it) out.push_back(it->first);
    for (const std::string& c : node->partChildren(part)) out.push_back(c);
    if (part.empty())
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
            if (std::find(out.begin(), out.end(), it->second->name) == out.end()) out.push_back(it->second->name);
    return out;
}

bool SceneClip::same(const script::Clip& other) const {
    auto* o = dynamic_cast<const SceneClip*>(&other);
    return o && o->node == node && o->part == part && o->instance == instance;
}

int SceneClip::sprite() const { return part.empty() ? node->symbol() : node->partSymbol(part); }

int SceneClip::currentFrame() const { return part.empty() ? node->frame() : node->partFrame(part, own()); }

std::string SceneClip::path() const {
    std::string p;
    for (const ArtNode* n = node; n; n = n->parent)
        if (!n->name.empty()) p = n->name + (p.empty() ? "" : "." + p);
    if (!part.empty()) p += (p.empty() ? "" : ".") + part;
    return "_level0" + (p.empty() ? "" : "." + p);
}

// ------------------------------------------------------------ a var clip

Value VarClip::get(const std::string& name) {
    auto b = bindings.find(name);
    if (b != bindings.end()) return b->second.get();
    for (const auto& [k, v] : vars)
        if (k == name) return v;
    auto c = children.find(name);
    return c == children.end() ? Value() : Value(c->second);
}

void VarClip::set(const std::string& name, const Value& v) {
    auto b = bindings.find(name);
    if (b != bindings.end()) {
        if (b->second.set) b->second.set(v);
        return;
    }
    for (auto& [k, old] : vars)
        if (k == name) {
            old = v;
            return;
        }
    vars.emplace_back(name, v);
}

bool VarClip::remove(const std::string& name) {
    for (auto it = vars.begin(); it != vars.end(); ++it)
        if (it->first == name) {
            vars.erase(it);
            return true;
        }
    return false;
}

std::vector<std::string> VarClip::keys() {
    std::vector<std::string> out;
    for (auto it = vars.rbegin(); it != vars.rend(); ++it) out.push_back(it->first);
    for (const auto& [k, c] : children) out.push_back(k);
    return out;
}

// --------------------------------------------------------------- the scene

std::shared_ptr<SceneClip> Scene::clip(ArtNode* node, const std::string& at) {
    if (!node) return nullptr;
    // Through the clips made nodes (and the nodes attached) on the way.
    std::string path = at;
    node = node->through(path);
    unsigned long long instance = 0;
    if (!path.empty()) {
        instance = node->partInstance(path);
        if (!instance) return nullptr;
    }
    Entry& e = entries[{node->serial(), instance}];
    if (!e.clip) e.clip = std::make_shared<SceneClip>(*this, node, path, instance);
    return e.clip;
}

std::shared_ptr<SceneClip> Scene::clipWithin(ArtNode* node, const std::string& at, unsigned long long within) {
    if (!node || at.empty()) return clip(node, at);
    std::string path = at;
    ArtNode* n = node->through(path);
    if (n != node || path.empty()) return clip(node, at);
    std::vector<VectorArt::ClipVisit> shown;
    node->innerClips(shown);
    const VectorArt::ClipVisit* pick = nullptr;
    int copies = 0;
    for (const VectorArt::ClipVisit& v : shown) {
        if (v.path != path) continue;
        ++copies;
        if (!pick || v.parent == within) pick = &v;
    }
    if (copies < 2) return clip(node, at);
    Entry& e = entries[{node->serial(), pick->instance}];
    if (!e.clip) e.clip = std::make_shared<SceneClip>(*this, node, path, pick->instance);
    e.clip->shared = true;
    return e.clip;
}

script::ClipRef Scene::parentOf(ArtNode* root) const {
    auto it = parents.find(root);
    return it == parents.end() ? nullptr : it->second;
}

bool Scene::scriptedBelow(int sprite) {
    auto it = subtreeScripted.find(sprite);
    if (it != subtreeScripted.end()) return it->second;
    subtreeScripted[sprite] = false;  // (cycles)
    bool found = runner.index.hasScripts(sprite);
    auto s = art.movie().sprites.find(sprite);
    if (!found && s != art.movie().sprites.end()) {
        for (const auto& frame : s->second.frames) {
            for (const flash::FrameOp& op : frame)
                if (op.kind == flash::FrameOp::kPlace && op.place.character >= 0 &&
                    art.movie().kind(op.place.character) == flash::Swf::Kind::kSprite &&
                    scriptedBelow(op.place.character)) {
                    found = true;
                    break;
                }
            if (found) break;
        }
    }
    return subtreeScripted[sprite] = found;
}

void Scene::runFrame(Entry& e, int frame) {
    e.frame = frame;
    ++scriptsRun;
    // The frame's plain stop/play/goto on the clip itself are already
    // played by the art (Swf's FrameAction markers).
    const auto& sprites = art.movie().sprites;
    auto s = sprites.find(e.clip->sprite());
    bool markers = s != sprites.end() && frame >= 1 && static_cast<size_t>(frame) <= s->second.actions.size() &&
                   !s->second.actions[frame - 1].empty();
    if (platform::debugMode() && std::getenv("AS_TRACE_FRAME"))
        platform::debugTrace("runFrame sprite %d frame %d path '%s'%s", e.clip->sprite(), frame, e.clip->path().c_str(),
                             markers ? " (has markers)" : "");
    runner.runFrame(e.clip, frame, markers);
}

void Scene::enter(Entry& e, int frame, bool allowCatchUp) {
    // A clip with no part of its own (ArtNode::part) has no playhead of its
    // own: its frame is computed fresh from its parent's age each time,
    // which (once the parent stops moving, or between two far-apart passes)
    // can jump several frames at once instead of advancing one at a time —
    // skipping a sound cue or a pick a frame in between made, or (the first
    // time anything asks for this clip at all) its own frame 1. In Flash
    // every one of those frames ran, one pass each, so this is caught up —
    // unlike a clip with a playhead of its own (ArtNode::clip, or a part
    // once anything has gone to a frame of it), where a jump is always an
    // explicit goto, deliberately skipping the frames between, never caught
    // up.
    constexpr int kCatchUpCap = 500;  // a clip stalled this long is jumped to, not replayed
    bool first = !e.seen;
    e.seen = true;
    if (e.frame == frame && !first) return;
    int from = std::max(1, e.frame + 1);
    if (allowCatchUp && frame > from && frame - from <= kCatchUpCap)
        for (int f = from; f < frame; ++f) runFrame(e, f);
    runFrame(e, frame);
}

void Scene::bind(ArtNode* node, const std::string& name, std::function<Value()> get,
                 std::function<void(const Value&)> set) {
    bindings[{node->serial(), name}] = Binding{std::move(get), std::move(set)};
}

void Scene::hand(ArtNode* node, const std::string& path, script::ClipRef to) {
    if (to) handed[{node->serial(), path}] = std::move(to);
    else handed.erase({node->serial(), path});
}

bool Scene::isHanded(ArtNode* node, const std::string& path) const {
    for (auto it = handed.lower_bound({node->serial(), std::string()}); it != handed.end() && it->first.first == node->serial(); ++it) {
        const std::string& h = it->first.second;
        if (path == h || (path.size() > h.size() && path.compare(0, h.size(), h) == 0 && path[h.size()] == '.')) return true;
    }
    return false;
}

void Scene::enter(ArtNode* n) {
    entered.insert(n->serial());
    int sprite = n->symbol();
    if (sprite >= 0 && runner.index.hasScripts(sprite)) {
        Entry& own = entries[{n->serial(), 0}];
        if (!own.clip) own.clip = std::make_shared<SceneClip>(*this, n, std::string(), 0);
        // A clip a script attached ran its frame 1 as it was attached, even
        // when that script sent it straight on to another frame: AS1's
        // extraction wounds set the cut they leave (cutFrame) on frame 1,
        // and the blockage's own frame sets none (its hole closed as a plain
        // cut without it).
        if (!own.seen && n->placement == 0 && !n->timeline && n->frame() != 1) runFrame(own, 1);
        // Its own frame is a real playhead (natural play, or an explicit
        // goto): either way Flash never ran the frames between two
        // sightings of it, so a jump here is never caught up.
        enter(own, n->frame(), false);
    }
    if (sprite < 0 || !scriptedBelow(sprite)) return;
    visits.clear();
    n->innerClips(visits);
    std::vector<VectorArt::ClipVisit> found = visits;  // (a script may walk again)
    std::unordered_set<unsigned long long> shown;
    std::unordered_map<std::string, int> copies;  // instances by path
    for (const VectorArt::ClipVisit& v : found) {
        shown.insert(v.instance);
        ++copies[v.path];
    }
    n->keepInstanceParts(shown);
    for (const VectorArt::ClipVisit& v : found) {
        if (!runner.index.hasScripts(v.character)) continue;
        if (!handed.empty() && isHanded(n, v.path)) continue;
        if (!n->children.empty()) {
            ArtNode* c = n->child(v.path.substr(0, v.path.find('.')));
            if (c && c->placement) continue;  // (a node of its own now: entered as one)
        }
        met.insert({n->serial(), v.instance});
        Entry& e = entries[{n->serial(), v.instance}];
        if (!e.clip) e.clip = std::make_shared<SceneClip>(*this, n, v.path, v.instance);
        if (copies[v.path] > 1) e.clip->shared = true;
        // Before anything ever sends it to a frame of its own, its frame is
        // found fresh from its parent's age each time, which can jump
        // ahead several frames between two sightings instead of advancing
        // one at a time (catch up); once it has a playhead of its own, a
        // jump is as deliberate as the clip's own above (do not replay it).
        enter(e, v.frame, !n->hasPartPlayhead(v.path, e.clip->own()));
    }
}

bool Scene::promote(SceneClip& c) {
    while (!c.part.empty()) {
        size_t dot = c.part.find('.');
        std::string name = c.part.substr(0, dot);
        ArtNode* owner = c.node;
        ArtNode* n = owner->child(name);
        if (!n) n = owner->promote(name);
        if (!n) return false;
        rebindUnder(owner, name, n);
        if (c.node == owner) {  // (a handle the scene did not keep)
            std::string rest = dot == std::string::npos ? std::string() : c.part.substr(dot + 1);
            unsigned long long id = rest.empty() ? 0 : n->partInstance(rest);
            if (!rest.empty() && !id) return false;
            c.rebind(n, rest, id);
        }
    }
    return true;
}

void Scene::rebindUnder(ArtNode* owner, const std::string& name, ArtNode* to) {
    // The handles on the clip and what is in it, with their variables and
    // the frames their scripts ran: now on the node, by their paths in it.
    std::vector<Entry> moved;
    for (auto it = entries.lower_bound({owner->serial(), 0}); it != entries.end() && it->first.first == owner->serial();) {
        const SceneClip& c = *it->second.clip;
        bool under = c.node == owner && c.part.compare(0, name.size(), name) == 0 &&
                     (c.part.size() == name.size() || c.part[name.size()] == '.');
        if (!under) {
            ++it;
            continue;
        }
        moved.push_back(std::move(it->second));
        it = entries.erase(it);
    }
    for (Entry& e : moved) {
        std::string rest = e.clip->part.size() == name.size() ? std::string() : e.clip->part.substr(name.size() + 1);
        unsigned long long id = rest.empty() ? 0 : to->partInstance(rest);
        if (!rest.empty() && !id) {
            runner.dropHandlers(*e.clip);
            continue;
        }
        e.clip->rebind(to, rest, id);
        Key key{to->serial(), id};
        met.insert(key);
        entries[key] = std::move(e);
    }
}

void Scene::prune(ArtNode* root) {
    std::unordered_map<unsigned long long, bool> live;
    std::vector<ArtNode*> todo{root};
    while (!todo.empty()) {
        ArtNode* n = todo.back();
        todo.pop_back();
        live[n->serial()] = true;
        for (auto& [d, c] : n->children) todo.push_back(c);
    }
    for (auto it = entries.begin(); it != entries.end();) {
        if (!live.count(it->first.first)) {
            runner.dropHandlers(*it->second.clip);
            it = entries.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = bindings.begin(); it != bindings.end();) it = live.count(it->first.first) ? std::next(it) : bindings.erase(it);
}

void Scene::update(ArtNode* root) {
    met.clear();
    entered.clear();
    // A script that moves a timeline places new clips at once, and Flash
    // runs their frame scripts right after it, before anything plays on:
    // so the scene is gone over again while scripts ran (a few times).
    for (int pass = 0; pass < 4; ++pass) {
        long ran = scriptsRun;
        std::vector<ArtNode*> nodes;
        std::vector<ArtNode*> todo{root};
        while (!todo.empty()) {
            ArtNode* n = todo.back();
            todo.pop_back();
            nodes.push_back(retainRef(n));
            for (auto& [d, c] : n->children) todo.push_back(c);
        }
        for (ArtNode* n : nodes)
            if (n == root || n->parent) enter(n);  // (not if a script removed it meanwhile)
        for (ArtNode* n : nodes) releaseRef(n);
        if (scriptsRun == ran) break;
    }
    prune(root);
    // An inner clip no longer on the stage is gone too: Flash destroyed a
    // clip whose depth was emptied, or whose timeline left the frames that
    // place it, and placed again it is a new instance, whose frame scripts
    // run again from frame 1. Its instance number is the same each time it
    // is placed so (its depth, frame and character), so its entry is
    // dropped here. (Kept, a screen gone back to (the level select, after a
    // patient) never ran its frame script again: its grade marks, placed
    // anew, played through all their frames.) Only here, where every clip on
    // the stage was just met: a scene not updated so (the operation's) keeps
    // its entries while their nodes live. Only clips with frame scripts are
    // met so; the others' handles (and the variables scripts set on them:
    // a hit piece's health) stay while their nodes live.
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->first.second != 0 && it->second.seen && !met.count(it->first)) {
            runner.dropHandlers(*it->second.clip);
            it = entries.erase(it);
        } else {
            ++it;
        }
    }
    runner.runEnterFrames();
    // What the frame handlers attached runs its frames now, before it is
    // drawn, as Flash ran a new clip's frame actions in the frame it was
    // made: a spark the chainsaw sprays on the car (AS1's blood particle,
    // turned to a spark by its frame 1 on patients 30 and 31) showed as
    // blood for a frame first.
    enterNew(root);
}

void Scene::enterNew(ArtNode* root) {
    for (int pass = 0; pass < 4; ++pass) {
        long ran = scriptsRun;
        std::vector<ArtNode*> nodes;
        std::vector<ArtNode*> todo{root};
        while (!todo.empty()) {
            ArtNode* n = todo.back();
            todo.pop_back();
            if (!entered.count(n->serial())) nodes.push_back(retainRef(n));
            for (auto& [d, c] : n->children) todo.push_back(c);
        }
        for (ArtNode* n : nodes)
            if (n == root || n->parent) enter(n);
        for (ArtNode* n : nodes) releaseRef(n);
        if (scriptsRun == ran) break;
    }
}
