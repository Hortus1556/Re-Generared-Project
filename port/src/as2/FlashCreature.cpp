#include "as2/FlashCreature.h"

#include <algorithm>

#include "as2/menus/FlashStage.h"
#include "platform/Platform.h"

namespace as2 {

FlashCreature::FlashCreature(const std::string& name) : art(flashstage::art("flash")) {
    symbol = art.symbol(name);
    since = platform::passTime();
}

FlashCreature::~FlashCreature() {
    if (attached) attached->art = nullptr;
}

void FlashCreature::attach(Shape* shape) {
    if (!ready() || !shape) return;
    attached = shape;
    shape->art = [this](const float color[4]) { draw(color); };
}

void FlashCreature::show(const std::string& label) {
    if (label == shown || !ready()) return;
    shown = label;
    frame = std::max(1, art.frameOf(symbol, label));
    since = platform::passTime();
}

int FlashCreature::frameIn(const std::string& onLabel, const std::string& path, const std::string& label) {
    if (!ready()) return 1;
    int clip = art.innerSymbol(symbol, std::max(1, art.frameOf(symbol, onLabel)), 0, "/" + path);
    return clip >= 0 ? std::max(1, art.frameOf(clip, label)) : 1;
}

int FlashCreature::lengthOf(const std::string& onLabel, const std::string& path) {
    if (!ready()) return 1;
    int clip = art.innerSymbol(symbol, std::max(1, art.frameOf(symbol, onLabel)), 0, "/" + path);
    return clip >= 0 ? art.frameCount(clip) : 1;
}

flash::Rect FlashCreature::boundsOf(const std::string& onLabel, const std::string& part, const VectorLook* with) {
    if (!ready()) return flash::Rect{1, 1, 0, 0};
    VectorLook all;  // (a hit area is drawn hidden: its box all the same)
    return art.partBounds(symbol, std::max(1, art.frameOf(symbol, onLabel)), 0, 0, VectorTransform{}, with ? with : &all,
                          part);
}

std::vector<flash::Rect> FlashCreature::hitAreas(const std::string& onLabel, const std::string& group) {
    if (!ready()) return {};
    VectorLook all;
    return art.childBounds(symbol, std::max(1, art.frameOf(symbol, onLabel)), 0, 0, VectorTransform{}, &all, group);
}

int FlashCreature::innerFrameOf(const std::string& clip, const std::string& label) {
    if (!ready()) return 1;
    int inner = art.innerSymbol(symbol, 1, 0, "/" + clip);
    return inner >= 0 ? std::max(1, art.frameOf(inner, label)) : 1;
}

int FlashCreature::innerLength(const std::string& clip, const std::string& label, const std::string& child) {
    if (!ready()) return 1;
    int inner = art.innerSymbol(symbol, 1, 0, "/" + clip);
    if (inner < 0) return 1;
    int sub = art.innerSymbol(inner, std::max(1, art.frameOf(inner, label)), 0, "/" + child);
    return sub >= 0 ? art.frameCount(sub) : 1;
}

flash::Rect FlashCreature::boundsNow(const std::string& part) {
    if (!ready()) return flash::Rect{1, 1, 0, 0};
    long n = age();
    return art.partBounds(symbol, frame, n, n, VectorTransform{}, &look, part);
}

long FlashCreature::age() const {
    if (still) return 0;
    return static_cast<long>((platform::passTime() - since) * flashstage::kFps + 1e-6);
}

void FlashCreature::draw(const float color[4]) {
    VectorTransform t;
    t.scaleX = t.scaleY = flashstage::scale() * scale;
    t.rotation = turn;
    // The scene's tint (a fade, the red flash) on the clip.
    const float a = std::max(color[3], 1e-4f);
    flash::ColorTransform scene;
    for (int i = 0; i < 3; ++i) scene.mult[i] = color[i] / a;
    scene.mult[3] = color[3];
    t.color = scene * tint;
    long n = age();
    art.draw(symbol, frame, n, n, t, attached ? attached->id : 0, &look);
}

}  // namespace as2
