#include "christmas/StageExtras.h"

#include "surgeon/SurgeonGame.h"

LevelsBack::LevelsBack(StageScreen& s, std::string p) : stage(s), path(std::move(p)) {
    // On the card's snow, bottom right.
    flash::Rect b = stage.stage->art.bounds(kBack, 1, 0, VectorTransform{});
    at.x = 495 - (b.xMin + b.xMax) / 2;
    at.y = 428 - (b.yMin + b.yMax) / 2;
}

bool LevelsBack::cardOpen() const {
    // The patient card (popup) rests on its first frame, off the screen.
    int f = stage.stage->partFrame(path + ".popup");
    return f > 1;
}

bool LevelsBack::hits(float x, float y) {
    return !gone && !cardOpen() && stage.stage->art.hitTest(kBack, 1, 0, at, x, y);
}

void LevelsBack::draw() {
    VectorLook look;
    if (over && !cardOpen()) look.buttons[kBack] = down ? 4 : 2;
    stage.stage->art.draw(kBack, 1, 0, at, serial(), &look);
}

void LevelsBack::touchDown(float x, float y) {
    over = hits(x, y);
    down = over;
}

void LevelsBack::touchMove(float x, float y) { over = hits(x, y); }

void LevelsBack::touchUp(float x, float y) {
    over = hits(x, y);
    if (down && over) back();
    down = false;
}

void LevelsBack::escapeKey() {
    if (!cardOpen()) back();
    else if (auto popup = stage.scene.clip(stage.stage, path + ".popup")) popup->gotoFrame(script::Value("moveoff"), true);
}

void LevelsBack::back() {
    if (gone) return;
    gone = true;
    stage.runner.callMethod(script::Value(script::ClipRef(stage.root)), "ftb", {script::Value("start")});
}

// ------------------------------------------------------------- engine

WorkshopEngine::WorkshopEngine(StageScreen& s, std::string path)
    : stage(s), bg(path + ".dialogueHolder.bg") {
    VectorArt& art = stage.stage->art;
    constexpr int kBackdrop = 3921;
    firstWorkshop = art.frameOf(kBackdrop, "workshop");
    engineSymbol = art.innerSymbol(kBackdrop, firstWorkshop, 0, "inner.engine");
}

void WorkshopEngine::update() {
    ArtNode& node = *stage.stage;
    if (engineSymbol < 0 || node.partFrame(bg) == firstWorkshop) return;
    // (the later backdrops' engine: unnamed, in an unnamed clip of bg)
    std::string copy = bg + ".#" + std::to_string(engineSymbol);
    if (node.partSymbol(copy) != engineSymbol) return;
    int landed = node.art.frameCount(engineSymbol);
    if (node.partFrame(copy) != landed) node.partGotoAndStop(copy, landed);
}
