#include "surgeon/StageScreen.h"

#include <algorithm>
#include <cstdlib>

#include "platform/Platform.h"
#include "platform/WinShell.h"
#include "surgeon/ClipWatch.h"
#include "surgeon/SurgeonGame.h"

using script::ClipRef;
using script::Value;

StageScreen::StageScreen(SurgeonGame& game)
    : Screen(game), runner(game.scripts()), scene(runner, game.art()), classes(runner, game.art()) {
    runner.traceUnknown = platform::traceEnabled();
    if (platform::traceEnabled()) watch = std::make_unique<ClipWatch>();
    if (const char* vars = platform::debugMode() ? std::getenv("AS_TRACE_VARS") : nullptr) runner.traceVars = vars;
    stage = new ArtNode(game.art(), 0);
    stage->partClocks = true;
    root = scene.clip(stage);
    runner.root = root;
    stage->setFieldText([this](const std::string& c, const std::string& v, std::string& t) { return fieldText(c, v, t); });
}

StageScreen::~StageScreen() {
    for (Running& r : running) releaseRef(r.module);
    releaseRef(stage);
}

// ------------------------------------------------------------- per frame

void StageScreen::update() {
    scene.mouseX = game.mouseX;
    scene.mouseY = game.mouseY;
    // (the first pass shows frame 1, as the movie starts on it)
    if (started) stage->update();
    started = true;
    scene.update(stage);
    if (stage->frame() != tracedFrame && runner.traceUnknown) {
        // Debug (AS_TRACE): where the main timeline is, for automated tests.
        tracedFrame = stage->frame();
        std::string label = std::to_string(tracedFrame);
        for (const auto& [name, f] : game.art().movie().sprites.at(0).labels)
            if (f == tracedFrame) label = name;
        platform::debugTrace("stage %s", label.c_str());
    }
    classes.update();
    findPointer();
    // Debug (AS_TRACE set, not -d alone): what is alive, to find leaks. (Its
    // walk of the heaps holds every other thread's allocations for a few
    // milliseconds: a hitch every ten seconds in a -d play session.)
    static const bool census = platform::debugMode() && std::getenv("AS_TRACE");
    if (census && ++censusPasses % 250 == 0) {
        flash::Raster& raster = game.art().rasterCache();
        const flash::MeshCache* meshes = game.art().meshCache();
        size_t waiting, made, madeBytes, heap, otherHeaps;
        raster.backlog(waiting, made, madeBytes);
        int bigW, bigH;
        size_t texBytes = raster.textureBytes(bigW, bigH);
        platform::heapUse(heap, otherHeaps);
        platform::debugTrace("census pass %ld: heap %zu MB, other heaps %zu MB, objects %ld, scene handles %zu, "
                             "handlers %zu, sounds %zu, textures %zu (%zu MB, largest %dx%d), art waiting %zu, "
                             "made %zu (%zu MB); meshes %zu (%zu KB); %s; most %s",
                             censusPasses, heap >> 20, otherHeaps >> 20, Object::live(), scene.entryCount(),
                             runner.handlerCount(), classes.soundCount(), raster.cachedCount(), texBytes >> 20, bigW,
                             bigH, waiting, made, madeBytes >> 20, meshes ? meshes->count() : 0,
                             (meshes ? meshes->bytes() : 0) >> 10, raster.churn().c_str(), (raster.topArt(4) + "; " + platform::memoryMap()).c_str());
    }
    syncModules();
    prefetch();
    std::vector<StageModule*> now;
    for (Running& r : running) now.push_back(retainRef(r.module));
    for (StageModule* m : now) {
        m->update();
        releaseRef(m);
    }
    if (afterUpdate) afterUpdate();
    if (watch) watch->update(stage, censusPasses, game.mouseX, game.mouseY);
}

void StageScreen::syncModules() {
    std::vector<VectorArt::TopClip> top = game.art().topClips(0, stage->frame());
    // Modules whose clip went.
    for (size_t i = running.size(); i-- > 0;) {
        Running& r = running[i];
        bool there = std::any_of(top.begin(), top.end(), [&](const auto& c) { return c.instance == r.instance; });
        if (there) continue;
        if (r.module->replacesClip()) {
            stage->hidePart(r.path, false);
            scene.hand(stage, r.path, nullptr);
        }
        if (hoverModule == r.module) hoverModule = nullptr;
        if (pointerModule == r.module) {
            releaseRef(pointerModule);
            pointerModule = nullptr;
        }
        releaseRef(r.module);
        running.erase(running.begin() + static_cast<long>(i));
    }
    // New clips a module takes.
    for (const VectorArt::TopClip& c : top) {
        auto f = factories.find(c.character);
        if (f == factories.end()) continue;
        if (std::any_of(running.begin(), running.end(), [&](const Running& r) { return r.instance == c.instance; }))
            continue;
        StageModule* m = f->second(*this, c.path);
        if (!m) continue;
        if (m->replacesClip()) {
            stage->hidePart(c.path);
            ClipRef handle = m->clip();
            scene.hand(stage, c.path, handle ? handle : ClipRef(std::make_shared<VarClip>()));
        }
        running.push_back({c.path, c.instance, c.depth, m});
        std::sort(running.begin(), running.end(), [](const Running& a, const Running& b) { return a.depth < b.depth; });
    }
}

void StageScreen::prefetch() {
    // What plays on by itself, a second ahead (an intro's next scenes).
    constexpr long kAhead = 25;
    stage->prefetchAhead(kAhead);
    // The frame a transition is going to.
    int target = 0;
    if (!transitionTarget.empty()) {
        Value t = root->get(transitionTarget);
        target = t.type == Value::kString ? game.art().frameOf(0, t.string) : t.toInt();
    }
    if (target <= 0 || target == stage->frame()) {
        preparingFrame = 0;
        return;
    }
    if (target != preparingFrame) {
        preparingFrame = target;
        preparedAges = 0;
    }
    if (preparedAges < kAhead) game.art().prefetch(0, target, preparedAges, preparedAges, VectorTransform{}, stage->serial(), nullptr);
    ++preparedAges;
}

void StageScreen::draw() {
    int from = -0x7fffffff;
    for (Running& r : running) {
        int below = r.module->replacesClip() ? r.depth - 1 : r.depth;
        if (below >= from) stage->drawSlice(from, below);
        r.module->draw();
        from = below + 1;
        if (r.module->replacesClip()) from = r.depth + 1;
    }
    stage->drawSlice(from, 0x7fffffff);
    stage->drawAttached();  // what scripts attached to _root
}

bool StageScreen::drawsPointer() const {
    if (pointerNode && pointerNode->shown()) return true;
    return std::any_of(running.begin(), running.end(), [](const Running& r) { return r.module->drawsPointer(); });
}

void StageScreen::findPointer() {
    pointerNode = nullptr;
    if (pointerClip.empty()) return;
    std::string path = pointerClip;
    ArtNode* in = stage->through(path);
    if (!in) return;
    // (Once a node: the scripts' moving it each pass made it one.)
    ArtNode* n = path.empty() ? in : nullptr;
    if (n && n != stage) {
        n->pointer = true;
        pointerNode = n;
        if (!pointerHit.empty() && platform::touchDevice() && pointerHitScaled != n->serial()) {
            if (ClipRef hit = ClipRef(scene.clip(n, pointerHit))) {
                hit->set("_xscale", Value(300.0));
                hit->set("_yscale", Value(300.0));
                pointerHitScaled = n->serial();
            }
        }
    }
}

StageModule* StageScreen::hitModule(float x, float y) const {
    for (auto it = running.rbegin(); it != running.rend(); ++it)
        if (it->module->hits(x, y)) return it->module;
    return nullptr;
}

StageModule* StageScreen::inputModule() const {
    for (auto it = running.rbegin(); it != running.rend(); ++it)
        if (it->module->takesInput()) return it->module;
    return nullptr;
}

// ------------------------------------------------------------- buttons

StageScreen::Hit StageScreen::hitAt(float x, float y) {
    Hit h;
    int top = 0;
    h.button = stage->buttonAt(x, y, h.node, h.clip, top);
    // A module drawn over the timeline's pieces below it hides their buttons.
    if (h.button >= 0)
        for (const Running& r : running)
            if (r.module->takesInput() && top < r.depth) return Hit{};
    return h;
}

bool StageScreen::onStage(const ArtNode* n) const {
    std::vector<const ArtNode*> todo{stage};
    while (!todo.empty()) {
        const ArtNode* at = todo.back();
        todo.pop_back();
        if (at == n) return true;
        for (auto& [d, c] : at->children) todo.push_back(c);
    }
    return false;
}

void StageScreen::event(const Hit& h, const char* name) {
    if (h.button < 0 || !onStage(h.node)) return;  // (its clip may have gone meanwhile)
    ClipRef c = ClipRef(scene.clip(h.node, h.clip));
    if (!c) return;
    bool ran = runner.runButton(c, h.button, name);
    if (ran && runner.traceUnknown)  // Debug (AS_TRACE): the handlers run, for automated tests
        platform::debugTrace("button %d %s on %s", h.button, name,
                             h.node == stage && h.clip.empty() ? "_root" : c->path().substr(sizeof("_level0")).c_str());
}

void StageScreen::showButtons() {
    stage->clearButtonStates();
    if (hover.button >= 0) {
        int state = pressed.button < 0 ? 2 : (pressed == hover ? 4 : 1);
        stage->setButtonState(hover.button, state);
    }
}

void StageScreen::touchDown(float x, float y) {
    // A finger lands with no hover before: the clip the game keeps at the
    // mouse once a pass (AS1's cursor, whose `hit` the tools test on the
    // press) is put under it first, where a mouse hovering there left it.
    if (platform::touchDevice() && pointerNode) {
        scene.mouseX = x;
        scene.mouseY = y;
        if (auto clip = scene.clip(pointerNode))
            if (script::ClipRef parent = clip->parent()) {
                clip->set("_x", parent->get("_xmouse"));
                clip->set("_y", parent->get("_ymouse"));
            }
    }
    classes.mouseEvent("onMouseDown");
    if (StageModule* m = hitModule(x, y)) {
        pointerModule = retainRef(m);
        m->touchDown(x, y);
        return;
    }
    Hit h = hitAt(x, y);
    hover = h;
    if (h.button >= 0) {
        pressed = h;
        pressedOver = true;
        event(h, "press");
    } else if (StageModule* m = inputModule()) {
        pointerModule = retainRef(m);
        m->touchDown(x, y);
    }
    showButtons();
}

void StageScreen::touchMove(float x, float y) {
    classes.mouseEvent("onMouseMove");
    StageModule* over = hitModule(x, y);
    if (over != hoverModule) {
        if (hoverModule) hoverModule->touchMove(x, y);  // (it sees the pointer leave)
        hoverModule = over;
    }
    Hit h = over ? Hit{} : hitAt(x, y);
    if (pressed.button >= 0) {
        bool over = h == pressed;
        if (over != pressedOver) event(pressed, over ? "dragOver" : "dragOut");
        pressedOver = over;
        hover = over ? h : Hit{};
    } else if (!(h == hover)) {
        event(hover, "rollOut");
        hover = h;
        event(h, "rollOver");
    }
    if (pointerModule) pointerModule->touchMove(x, y);
    else if (over) over->touchMove(x, y);
    else if (StageModule* m = inputModule()) m->touchMove(x, y);
    showButtons();
}

void StageScreen::touchUp(float x, float y) {
    classes.mouseEvent("onMouseUp");
    if (pressed.button >= 0) {
        Hit h = hitAt(x, y);
        Hit was = pressed;
        pressed = Hit{};
        event(was, h == was ? "release" : "releaseOutside");
        hover = hitAt(x, y);
    } else if (pointerModule) {
        pointerModule->touchUp(x, y);
    }
    releaseRef(pointerModule);
    pointerModule = nullptr;
    // On a touch device the finger leaving the screen is the pointer
    // leaving: no button stays rolled over, no module keeps its hover look
    // (nothing that only follows a hovering pointer shows: docs/rules.md §7).
    if (platform::touchDevice()) {
        if (hoverModule) hoverModule->touchMove(-1e6f, -1e6f);  // (it sees the pointer leave)
        hoverModule = nullptr;
        if (hover.button >= 0) event(hover, "rollOut");
        hover = Hit{};
    }
    showButtons();
}

bool StageScreen::escapeTaken() {
    for (auto it = running.rbegin(); it != running.rend(); ++it)
        if (it->module->takesEscape()) {
            it->module->escapeKey();
            return true;
        }
    return false;
}

void StageScreen::numberKey(int n) {
    classes.keyEvent('0' + n % 10, true);
    classes.keyEvent('0' + n % 10, false);
    if (StageModule* m = inputModule()) m->numberKey(n);
}

void StageScreen::escapeKey() {
    classes.keyEvent(27, true);
    classes.keyEvent(27, false);
    if (escapeTaken()) return;
    if (StageModule* m = inputModule()) m->escapeKey();
}

// ------------------------------------------------------------- text fields

bool StageScreen::fieldText(const std::string& clip, const std::string& variable, std::string& text) {
    Value v = clip.empty() ? Value(ClipRef(root)) : Value(ClipRef(scene.clip(stage, clip)));
    if (v.type != Value::kClip) return false;
    // "a.b.c", "_root.x", Flash 4's "/a/b:x".
    std::string path = variable;
    for (char& ch : path)
        if (ch == ':' || ch == '/') ch = '.';
    if (!path.empty() && path[0] == '.') path = "_root" + path;
    size_t from = 0;
    while (from <= path.size()) {
        size_t dot = path.find('.', from);
        std::string seg = path.substr(from, dot == std::string::npos ? std::string::npos : dot - from);
        from = dot == std::string::npos ? path.size() + 1 : dot + 1;
        if (seg.empty()) continue;
        if (seg == "_root" || seg == "_level0") v = Value(ClipRef(root));
        else if (v.type == Value::kClip) v = seg == "_parent" ? Value(v.clip->parent()) : v.clip->get(seg);
        else if (v.type == Value::kObject) v = v.object->get(seg);
        else return false;
    }
    if (v.isUndefined()) return false;
    text = v.toString();
    return true;
}
