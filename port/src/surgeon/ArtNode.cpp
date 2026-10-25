#include "surgeon/ArtNode.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "platform/DrawList.h"
#include "platform/Platform.h"

namespace {

constexpr float kPi = 3.14159265358979f;

// A part's path as the art matches it: anchored at the node's symbol (Flash's
// clip.a.b names exactly that child of a child), or "#<id>" as it is.
std::string at(const std::string& path) { return path.empty() || path[0] == '#' ? path : "/" + path; }

flash::Matrix pixels(flash::Matrix m) {
    m.tx /= 20;
    m.ty /= 20;
    return m;
}

}  // namespace

ArtNode::ArtNode(VectorArt& art, int symbol) : art(art) {
    if (symbol >= 0) clip = VectorPlayhead(art, symbol);
}

ArtNode::~ArtNode() {
    for (auto& [d, c] : children) {
        c->parent = nullptr;
        c->removed = true;  // (gone with it)
        releaseRef(c);
    }
}

ArtNode* ArtNode::attach(ArtNode* c, const std::string& n, int d) {
    auto it = children.find(d);
    if (it != children.end()) {
        it->second->parent = nullptr;
        it->second->removed = true;
        releaseRef(it->second);
        children.erase(it);
    }
    c->parent = this;
    c->removed = false;
    c->name = n;
    c->depth = d;
    children[d] = c;
    // Its text fields show the scripts' variables as the parent's do (the
    // path to its clips through it).
    if (look.fieldText && !c->look.fieldText)
        c->look.fieldText = [f = look.fieldText, n](const std::string& clipPath, const std::string& v, std::string& t) {
            return f(clipPath.empty() ? n : n + "." + clipPath, v, t);
        };
    return c;
}

ArtNode* ArtNode::attach(const std::string& linkage, const std::string& n, int d) {
    int s = art.symbol(linkage);
    if (s < 0) return nullptr;
    return attach(new ArtNode(art, s), n, d);
}

void ArtNode::remove() {
    if (!parent) return;
    auto it = parent->children.find(depth);
    if (it != parent->children.end() && it->second == this) {
        parent->children.erase(it);
        parent = nullptr;
        removed = true;
        release();
    }
}

ArtNode* ArtNode::child(const std::string& n) const {
    for (auto& [d, c] : children)
        if (c->name == n) return c;
    return nullptr;
}

void ArtNode::swapDepths(int d) {
    if (!parent || d == depth) return;
    timeline = false;
    auto& siblings = parent->children;
    siblings.erase(depth);
    auto other = siblings.find(d);
    if (other != siblings.end()) {
        ArtNode* o = other->second;
        siblings.erase(other);
        o->depth = depth;
        siblings[depth] = o;
    }
    depth = d;
    siblings[d] = this;
}

void ArtNode::gotoAndStop(int f) {
    int before = clip.frame;
    clip.gotoAndStop(f);
    if (clip.frame != before) dropReplacedParts();
}

void ArtNode::gotoAndPlay(int f) {
    int before = clip.frame;
    clip.gotoAndPlay(f);
    if (clip.frame != before) dropReplacedParts();
}

bool ArtNode::partGotoAndStop(const std::string& path, int f, unsigned long long instance) {
    VectorPlayhead* p = part(path, instance);
    if (!p) return false;
    int before = p->frame;
    p->gotoAndStop(f);
    if (p->frame != before) dropReplacedParts();
    return true;
}

bool ArtNode::partGotoAndStop(const std::string& path, const std::string& label, unsigned long long instance) {
    VectorPlayhead* p = part(path, instance);
    int f = p ? p->labelFrame(label) : 0;
    return f && partGotoAndStop(path, f, instance);
}

bool ArtNode::partGotoAndPlay(const std::string& path, int f, unsigned long long instance) {
    VectorPlayhead* p = part(path, instance);
    if (!p) return false;
    int before = p->frame;
    p->gotoAndPlay(f);
    if (p->frame != before) dropReplacedParts();
    return true;
}

std::vector<std::string> ArtNode::partChildren(const std::string& path) {
    if (clip.symbol < 0) return {};
    applyParts();
    return art.childClips(clip.symbol, clip.frame, clip.age, clip.inFrame, &look, at(path));
}

void ArtNode::setCharacterFrame(int character, int f) {
    std::string key = "#" + std::to_string(character);
    if (f > 0) look.frames[key] = f;
    else look.frames.erase(key);
}

int ArtNode::characterFrame(int character) const {
    auto it = look.frames.find("#" + std::to_string(character));
    return it == look.frames.end() ? 0 : it->second;
}

int ArtNode::partSymbol(const std::string& path) {
    flash::Matrix l, w;
    if (clip.symbol < 0) return -1;
    applyParts();
    int character;
    return art.partInfo(clip.symbol, clip.frame, clip.age, clip.inFrame, &look, at(path), l, w, character) ? character : -1;
}

void ArtNode::innerClips(std::vector<VectorArt::ClipVisit>& out) {
    if (clip.symbol < 0) return;
    applyParts();
    art.clips(clip.symbol, clip.frame, clip.age, clip.inFrame, &look, out);
}

unsigned long long ArtNode::partInstance(const std::string& path) {
    flash::Matrix l, w;
    if (clip.symbol < 0) return 0;
    applyParts();
    int character;
    unsigned long long id = 0;
    return art.partInfo(clip.symbol, clip.frame, clip.age, clip.inFrame, &look, at(path), l, w, character, &id) ? id : 0;
}

void ArtNode::dropReplacedParts() {
    // A part whose clip is no longer the one its playhead was made for is
    // a new instance: it starts over, as do the looks set on it and below.
    for (bool again = true; again;) {
        again = false;
        for (auto it = parts.begin(); it != parts.end(); ++it) {
            if (partInstance(it->first) == partIds[it->first]) continue;
            std::string gone = it->first;
            parts.erase(it);
            partIds.erase(gone);
            auto under = [&](const std::string& key) {
                return key == gone || (key.size() > gone.size() && key.compare(0, gone.size(), gone) == 0 &&
                                       key[gone.size()] == '.');
            };
            auto lookUnder = [&](const std::string& key) { return !key.empty() && key[0] == '/' && under(key.substr(1)); };
            for (auto f = look.frames.begin(); f != look.frames.end();) f = lookUnder(f->first) ? look.frames.erase(f) : ++f;
            for (auto f = look.inFrames.begin(); f != look.inFrames.end();)
                f = lookUnder(f->first) ? look.inFrames.erase(f) : ++f;
            for (auto p = look.positions.begin(); p != look.positions.end();)
                p = lookUnder(p->first) ? look.positions.erase(p) : ++p;
            look.hidden.erase(std::remove_if(look.hidden.begin(), look.hidden.end(), lookUnder), look.hidden.end());
            for (auto p = parts.begin(); p != parts.end();) {
                if (!under(p->first)) {
                    ++p;
                    continue;
                }
                partIds.erase(p->first);
                p = parts.erase(p);
            }
            again = true;
            break;
        }
    }
    followTimeline();
    // Hidden or moved clips that were replaced (another frame placed a new
    // instance there, or none): a new instance starts shown, where its
    // frame puts it, as in Flash.
    for (auto it = lookInstances.begin(); it != lookInstances.end();) {
        if (partInstance(it->first) == it->second) {
            ++it;
            continue;
        }
        const std::string gone = it->first;
        look.hidden.erase(std::remove(look.hidden.begin(), look.hidden.end(), at(gone)), look.hidden.end());
        look.positions.erase(at(gone));
        it = lookInstances.erase(it);
    }
}

int ArtNode::frameCount() const { return clip.symbol >= 0 ? art.frameCount(clip.symbol) : 1; }

VectorPlayhead* ArtNode::part(const std::string& path, unsigned long long instance) {
    if (byInstance(path, instance)) {
        auto it = instanceParts.find(instance);
        if (it != instanceParts.end()) return &it->second;
        // Made on the frame that instance shows now.
        std::vector<VectorArt::ClipVisit> shown;
        innerClips(shown);
        for (const VectorArt::ClipVisit& v : shown) {
            if (v.instance != instance) continue;
            if (art.movie().kind(v.character) != flash::Swf::Kind::kSprite) return nullptr;
            VectorPlayhead p(art, v.character);
            p.gotoAndStop(std::max(1, v.frame));
            return &instanceParts.emplace(instance, p).first->second;
        }
        return nullptr;
    }
    auto it = parts.find(path);
    if (it != parts.end()) return &it->second;
    int s = partSymbol(path);
    if (s < 0 || art.movie().kind(s) != flash::Swf::Kind::kSprite) return nullptr;
    VectorPlayhead p(art, s);
    p.gotoAndStop(std::max(1, art.innerFrame(clip.symbol, clip.frame, clip.age, path, &look, clip.inFrame)));
    partIds[path] = partInstance(path);
    return &parts.emplace(path, p).first->second;
}

void ArtNode::hidePart(const std::string& path, bool hidden) {
    auto it = std::find(look.hidden.begin(), look.hidden.end(), at(path));
    if (hidden && it == look.hidden.end()) {
        look.hidden.push_back(at(path));
        noteInstance(path);
    }
    if (!hidden && it != look.hidden.end()) look.hidden.erase(it);
}

void ArtNode::noteInstance(const std::string& path) {
    // A look set on a named clip belongs to that instance of it.
    if (path.empty() || path[0] == '#' || clip.symbol < 0) return;
    lookInstances[path] = partInstance(path);
}

bool ArtNode::partHidden(const std::string& path) const {
    return std::find(look.hidden.begin(), look.hidden.end(), at(path)) != look.hidden.end();
}

bool ArtNode::partMatrices(const std::string& path, flash::Matrix& local, flash::Matrix& inSymbol) {
    if (clip.symbol < 0) return false;
    applyParts();
    int character;
    return art.partInfo(clip.symbol, clip.frame, clip.age, clip.inFrame, &look, at(path), local, inSymbol, character);
}

bool ArtNode::hasPart(const std::string& path) {
    flash::Matrix l, w;
    return partMatrices(path, l, w);
}

void ArtNode::keepInstanceParts(const std::unordered_set<unsigned long long>& shown) {
    for (auto it = instanceParts.begin(); it != instanceParts.end();) {
        if (shown.count(it->first)) {
            ++it;
            continue;
        }
        look.instanceFrames.erase(it->first);
        look.instanceInFrames.erase(it->first);
        it = instanceParts.erase(it);
    }
}

int ArtNode::partFrame(const std::string& path, unsigned long long instance) {
    if (byInstance(path, instance))
        if (auto i = instanceParts.find(instance); i != instanceParts.end()) return i->second.frame;
    auto it = parts.find(path);
    if (it != parts.end()) return it->second.frame;
    if (clip.symbol < 0) return 0;
    applyParts();
    return art.innerFrame(clip.symbol, clip.frame, clip.age, path, &look, clip.inFrame);
}

float ArtNode::partX(const std::string& path) {
    flash::Matrix l, w;
    return partMatrices(path, l, w) ? l.tx / 20 : 0;
}

float ArtNode::partY(const std::string& path) {
    flash::Matrix l, w;
    return partMatrices(path, l, w) ? l.ty / 20 : 0;
}

void ArtNode::setPartPosition(const std::string& path, float px, float py) {
    look.positions[at(path)] = {px, py};
    noteInstance(path);
}

void ArtNode::stageToPart(const std::string& path, float sx, float sy, float& px, float& py) {
    flash::Matrix l, w;
    px = sx;
    py = sy;
    if (!partMatrices(path, l, w)) return;
    w.tx /= 20;
    w.ty /= 20;
    (world() * w).inverse().apply(sx, sy, px, py);
}

void ArtNode::partToStage(const std::string& path, float px, float py, float& sx, float& sy) {
    flash::Matrix l, w;
    sx = px;
    sy = py;
    if (!partMatrices(path, l, w)) return;
    w.tx /= 20;
    w.ty /= 20;
    (world() * w).apply(px, py, sx, sy);
}

flash::Rect ArtNode::partLocalBounds(const std::string& path) {
    flash::Matrix l, w;
    if (!partMatrices(path, l, w)) return {1, 1, 0, 0};
    // The part's parent clip in the symbol: its matrix without its own.
    flash::Matrix parentInSymbol = w * l.inverse();
    VectorTransform t;
    t.base = parentInSymbol.inverse();
    return art.partBounds(clip.symbol, clip.frame, clip.age, clip.inFrame, t, &look, at(path));
}

float ArtNode::partWidth(const std::string& path) {
    flash::Rect r = partLocalBounds(path);
    return r.empty() ? 0 : r.xMax - r.xMin;
}

float ArtNode::partHeight(const std::string& path) {
    flash::Rect r = partLocalBounds(path);
    return r.empty() ? 0 : r.yMax - r.yMin;
}

flash::Matrix ArtNode::partMatrix(const std::string& path) {
    flash::Matrix l, w;
    return partMatrices(path, l, w) ? pixels(l) : flash::Matrix{};
}

ArtNode* ArtNode::promote(const std::string& n) {
    if (ArtNode* c = child(n)) return c;
    if (clip.symbol < 0 || n.empty() || n[0] == '#' || n.find('.') != std::string::npos) return nullptr;
    applyParts();
    std::vector<VectorArt::TopClip> tops = art.topClips(clip.symbol, clip.frame);
    auto top = std::find_if(tops.begin(), tops.end(), [&](const VectorArt::TopClip& t) { return t.path == n; });
    if (top == tops.end()) return nullptr;
    auto* c = new ArtNode(art, top->character);
    c->timeline = true;
    c->placement = top->instance;
    c->partClocks = partClocks;
    // Its playhead: the part's own, or its timeline as the art shows it now.
    auto own = parts.find(n);
    if (own != parts.end()) {
        c->clip = own->second;
    } else {
        long shownAge = 0, shownInFrame = 0;
        c->clip.frame = std::max(1, art.innerFrame(clip.symbol, clip.frame, clip.age, n, &look, clip.inFrame, &shownAge,
                                                   &shownInFrame));
        c->clip.age = shownAge;
        c->clip.inFrame = shownInFrame;
        // (resting on a frame that stops it: stopped)
        const auto& sprites = art.movie().sprites;
        auto s = sprites.find(top->character);
        if (s != sprites.end() && c->clip.frame <= static_cast<int>(s->second.actions.size()))
            for (const flash::FrameAction& a : s->second.actions[c->clip.frame - 1])
                if (a.kind == flash::FrameAction::kStop) c->clip.playing = false;
    }
    parts.erase(n);
    partIds.erase(n);
    // Where the timeline places it, and what scripts set on it so far.
    c->setMatrix(pixels(top->matrix));
    c->tint = top->color;
    auto pos = look.positions.find(at(n));
    if (pos != look.positions.end()) {
        c->x = pos->second.x;
        c->y = pos->second.y;
        c->scripted = true;
        look.positions.erase(pos);
    }
    c->visible = !partHidden(n);
    // Its parts' playheads and looks are its own now.
    const std::string prefix = n + ".";
    auto under = [&](const std::string& key) { return key.compare(0, prefix.size(), prefix) == 0; };
    auto lookUnder = [&](const std::string& key) { return !key.empty() && key[0] == '/' && under(key.substr(1)); };
    auto inner = [&](const std::string& key) { return "/" + key.substr(1 + prefix.size()); };
    for (auto it = parts.begin(); it != parts.end();) {
        if (!under(it->first)) {
            ++it;
            continue;
        }
        c->parts.emplace(it->first.substr(prefix.size()), it->second);
        partIds.erase(it->first);
        it = parts.erase(it);
    }
    for (auto it = look.frames.begin(); it != look.frames.end();) {
        bool mine = lookUnder(it->first);
        if (mine) c->look.frames[inner(it->first)] = it->second;
        it = mine || it->first == at(n) ? look.frames.erase(it) : std::next(it);
    }
    for (auto it = look.inFrames.begin(); it != look.inFrames.end();) {
        bool mine = lookUnder(it->first);
        if (mine) c->look.inFrames[inner(it->first)] = it->second;
        it = mine || it->first == at(n) ? look.inFrames.erase(it) : std::next(it);
    }
    for (auto it = look.positions.begin(); it != look.positions.end();) {
        if (!lookUnder(it->first)) {
            ++it;
            continue;
        }
        c->look.positions[inner(it->first)] = it->second;
        it = look.positions.erase(it);
    }
    for (auto it = look.texts.begin(); it != look.texts.end();) {
        if (!under(it->first)) {
            ++it;
            continue;
        }
        c->look.texts[it->first.substr(prefix.size())] = it->second;
        it = look.texts.erase(it);
    }
    std::vector<std::string> keptHidden;
    for (const std::string& h : look.hidden) {
        if (lookUnder(h)) c->look.hidden.push_back(inner(h));
        else if (h != at(n)) keptHidden.push_back(h);
    }
    look.hidden = std::move(keptHidden);
    std::vector<std::string> noted;
    for (auto it = lookInstances.begin(); it != lookInstances.end();) {
        if (it->first != n && !under(it->first)) {
            ++it;
            continue;
        }
        if (it->first != n) noted.push_back(it->first.substr(prefix.size()));
        it = lookInstances.erase(it);
    }
    c->look.buttons = look.buttons;
    for (auto& [path, p] : c->parts) c->partIds[path] = c->partInstance(path);
    for (const std::string& path : noted) c->noteInstance(path);
    attach(c, n, top->depth + kTimelineDepth);
    hidePart(n);  // (drawn as the node now)
    return c;
}

ArtNode* ArtNode::through(std::string& path) {
    ArtNode* n = this;
    while (!path.empty() && !n->children.empty()) {
        size_t dot = path.find('.');
        ArtNode* c = n->child(path.substr(0, dot));
        if (!c) break;
        n = c;
        path = dot == std::string::npos ? std::string() : path.substr(dot + 1);
    }
    return n;
}

void ArtNode::followTimeline() {
    std::vector<ArtNode*> placed;
    for (auto& [d, c] : children)
        if (c->timeline) placed.push_back(c);
    if (placed.empty() || clip.symbol < 0) return;
    std::vector<VectorArt::TopClip> tops = art.topClips(clip.symbol, clip.frame);
    for (ArtNode* c : placed) {
        auto t = std::find_if(tops.begin(), tops.end(),
                              [&](const VectorArt::TopClip& t) { return t.instance == c->placement; });
        if (t == tops.end()) {
            // Its timeline removed it (placed again, it is a new instance).
            hidePart(c->name, false);
            c->remove();
            continue;
        }
        if (!c->scripted) c->setMatrix(pixels(t->matrix));
        c->tint = t->color;
    }
}

void ArtNode::setMatrix(const flash::Matrix& m) {
    x = m.tx;
    y = m.ty;
    xscale = std::sqrt(m.a * m.a + m.b * m.b) * 100;
    yscale = std::sqrt(m.c * m.c + m.d * m.d) * 100;
    if (m.a * m.d - m.b * m.c < 0) yscale = -yscale;
    rotation = std::atan2(m.b, m.a) * 180 / kPi;
}

flash::Matrix ArtNode::matrix() const {
    float r = rotation * kPi / 180, c = std::cos(r), s = std::sin(r);
    flash::Matrix m;
    m.a = c * xscale / 100;
    m.b = s * xscale / 100;
    m.c = -s * yscale / 100;
    m.d = c * yscale / 100;
    m.tx = x;
    m.ty = y;
    return m;
}

flash::Matrix ArtNode::world() const { return parent ? parent->world() * matrix() : matrix(); }

void ArtNode::localToGlobal(float& px, float& py) const {
    float ox, oy;
    world().apply(px, py, ox, oy);
    px = ox;
    py = oy;
}

void ArtNode::globalToLocal(float& px, float& py) const {
    float ox, oy;
    world().inverse().apply(px, py, ox, oy);
    px = ox;
    py = oy;
}

void ArtNode::update() {
    bool moved = false;
    if (clip.symbol >= 0) {
        int before = clip.frame;
        clip.update();
        moved = clip.frame != before;
    }
    for (auto& [path, p] : parts) {
        int before = p.frame;
        p.update();
        moved = moved || p.frame != before;
    }
    for (auto& [instance, p] : instanceParts) {
        int before = p.frame;
        p.update();
        moved = moved || p.frame != before;
    }
    if (moved) dropReplacedParts();
    std::vector<ArtNode*> kids;
    for (auto& [d, c] : children) kids.push_back(retainRef(c));
    for (ArtNode* c : kids) {
        c->update();
        releaseRef(c);
    }
}

void ArtNode::applyParts() {
    for (auto& [path, p] : parts) {
        look.frames[at(path)] = p.frame;
        if (partClocks) look.inFrames[at(path)] = p.inFrame;
    }
    for (auto& [instance, p] : instanceParts) {
        look.instanceFrames[instance] = p.frame;
        if (partClocks) look.instanceInFrames[instance] = p.inFrame;
    }
}

void ArtNode::draw() {
    if (!visible) return;
    // (No pointer on a touch device, the finger being it: the game's cursor,
    // the tool in hand, is not drawn. docs/rules.md §7)
    if (pointer && platform::touchDevice()) return;
    // (Placed at the mouse by the game this pass: shown where it is now.)
    std::optional<platform::PointerArt> atPointer;
    if (pointer) {
        flash::Matrix w = world();
        atPointer.emplace(w.tx, w.ty);
    }
    drawSlice(-0x7fffffff, 0x7fffffff);
    drawAttached();
}

bool ArtNode::shown() const {
    for (const ArtNode* n = this; n; n = n->parent)
        if (!n->visible || n->removed) return false;
    return true;
}

void ArtNode::drawAttached() {
    if (!visible) return;
    for (auto& [d, c] : children)
        if (!c->timeline) c->draw();
}

void ArtNode::setButtonState(int character, int state) {
    look.buttons[character] = state;
    for (auto& [d, c] : children) c->setButtonState(character, state);
}

void ArtNode::clearButtonStates() {
    look.buttons.clear();
    for (auto& [d, c] : children) c->clearButtonStates();
}

VectorTransform ArtNode::stageTransform() const {
    VectorTransform t;
    flash::Matrix w = world();
    t.base = w;
    t.base.tx *= 20;
    t.base.ty *= 20;
    return t;
}

bool ArtNode::hitTest(float sx, float sy, bool shape, const std::string& p) {
    // A clip's own art and what is under it (Flash's hitTest).
    if (p.empty() || clip.symbol < 0)
        for (auto& [d, c] : children)
            if (c->visible && c->hitTest(sx, sy, shape, p)) return true;
    if (clip.symbol < 0) return false;
    applyParts();
    return art.hitTestPart(clip.symbol, clip.frame, clip.age, clip.inFrame, stageTransform(), &look, at(p), sx, sy, shape);
}

flash::Rect ArtNode::bounds(const std::string& p) {
    flash::Rect r{1, 1, 0, 0};
    if (clip.symbol >= 0) {
        applyParts();
        r = art.partBounds(clip.symbol, clip.frame, clip.age, clip.inFrame, stageTransform(), &look, at(p));
    }
    if (p.empty())
        for (auto& [d, c] : children) {
            if (!c->visible) continue;
            flash::Rect b = c->bounds();
            if (b.empty()) continue;
            r = r.empty() ? b : flash::Rect{std::min(r.xMin, b.xMin), std::min(r.yMin, b.yMin), std::max(r.xMax, b.xMax),
                                            std::max(r.yMax, b.yMax)};
        }
    return r;
}

int ArtNode::buttonAt(float sx, float sy, std::string& clipPath, int& topDepth) {
    clipPath.clear();
    if (clip.symbol < 0 || !visible) return -1;
    applyParts();
    return art.buttonAt(clip.symbol, clip.frame, clip.age, clip.inFrame, stageTransform(), sx, sy, &look, clipPath,
                        &topDepth);
}

int ArtNode::buttonAt(float sx, float sy, ArtNode*& in, std::string& clipPath, int& topDepth) {
    in = this;
    clipPath.clear();
    topDepth = 0;
    if (!visible) return -1;
    ArtNode* cin;
    std::string cpath;
    int ctop;
    // What is attached is above its timeline.
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (it->second->timeline) continue;
        int b = it->second->buttonAt(sx, sy, cin, cpath, ctop);
        if (b < 0) continue;
        in = cin;
        clipPath = cpath;
        topDepth = 0x7fffffff;
        return b;
    }
    int own = clip.symbol >= 0 ? buttonAt(sx, sy, clipPath, topDepth) : -1;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (!it->second->timeline) continue;
        int d = it->first - kTimelineDepth;
        if (own >= 0 && d < topDepth) break;
        int b = it->second->buttonAt(sx, sy, cin, cpath, ctop);
        if (b < 0) continue;
        in = cin;
        clipPath = cpath;
        topDepth = d;
        return b;
    }
    return own;
}

void ArtNode::drawSlice(int fromDepth, int toDepth) {
    if (!visible) return;
    int from = fromDepth;
    for (auto& [d, c] : children) {
        if (!c->timeline) continue;
        int own = d - kTimelineDepth;
        if (own < fromDepth || own > toDepth) continue;
        drawOwn(from, own - 1);
        c->draw();
        from = own + 1;
    }
    drawOwn(from, toDepth);
}

void ArtNode::drawOwn(int fromDepth, int toDepth) {
    if (clip.symbol < 0 || fromDepth > toDepth) return;
    flash::ColorTransform color;
    for (const ArtNode* n = this; n; n = n->parent) {
        flash::ColorTransform own = n->tint;
        own.mult[3] = n->opacity();
        color = own * color;
    }
    applyParts();
    look.fromDepth = fromDepth;
    look.toDepth = toDepth;
    art.drawUnder(world(), color, clip.symbol, clip.frame, clip.age, clip.inFrame, serial(), &look);
    look.fromDepth = -0x7fffffff;
    look.toDepth = 0x7fffffff;
}

void ArtNode::prefetchAhead(long frames) {
    if (!visible || clip.symbol < 0) return;
    applyParts();
    // Its playheads (its own, its parts' set playing by the game) played on
    // that far, as they will be.
    VectorPlayhead own = clip;
    for (long i = 0; i < frames && own.playing; ++i) own.update();
    VectorLook ahead = look;
    for (auto& [path, p] : parts) {
        if (!p.playing) continue;
        VectorPlayhead q = p;
        for (long i = 0; i < frames && q.playing; ++i) q.update();
        ahead.frames[at(path)] = q.frame;
        if (partClocks) ahead.inFrames[at(path)] = q.inFrame;
    }
    long inFrame = own.frame == clip.frame ? clip.inFrame + frames : own.inFrame;
    art.prefetch(clip.symbol, own.frame, clip.age + frames, inFrame, stageTransform(), serial(), &ahead);
    for (auto& [d, c] : children)
        if (c->timeline) c->prefetchAhead(frames);
}

int ArtNode::buttonAt(float sx, float sy) {
    if (clip.symbol < 0 || !visible) return -1;
    applyParts();
    return art.buttonAt(clip.symbol, clip.frame, clip.age, clip.inFrame, stageTransform(), sx, sy, &look);
}

bool ArtNode::hitTest(ArtNode& other, const std::string& p, const std::string& otherPart) {
    flash::Rect a = bounds(p), b = other.bounds(otherPart);
    return !a.empty() && !b.empty() && a.xMin <= b.xMax && b.xMin <= a.xMax && a.yMin <= b.yMax && b.yMin <= a.yMax;
}
