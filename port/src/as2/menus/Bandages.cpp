#include "as2/menus/Bandages.h"

#include <algorithm>

#include "as2/menus/FlashStage.h"
#include "platform/Platform.h"

namespace as2::bandages {

namespace {

constexpr unsigned long long kOwner = 0x42414E4400000000ull;  // "BAND"
constexpr int kCovered = 25;  // the frame the screen changes under it

struct State {
    int symbol = -2, frames = 0;
    int frame = 0;  // 0: not playing
    double last = 0, carry = 0;
    std::function<void()> covered;
};

State& state() {
    static State s;
    return s;
}

// The clip, looked up once.
State& clip() {
    State& s = state();
    if (s.symbol == -2) {
        VectorArt& art = flashstage::art("flash-shell");
        s.symbol = art.symbol("bandages");
        s.frames = s.symbol >= 0 ? art.frameCount(s.symbol) : 0;
    }
    return s;
}

}  // namespace

bool playing() { return state().frame > 0; }

void prefetch() {
    State& s = clip();
    // (Kept 40 seconds of drawn frames; the screens ask again every 20.)
    for (int f = 1; f <= s.frames; ++f)
        flashstage::art("flash-shell").prefetch(s.symbol, f, f, flashstage::at(), kOwner, nullptr, 40 * 60);
}

void play(std::function<void()> covered) {
    State& s = clip();
    if (s.frame > 0) return;
    if (s.symbol < 0) {
        // (No art: the change at once.)
        if (covered) covered();
        return;
    }
    s.frame = 1;
    s.last = platform::passTime();
    s.carry = 0;
    s.covered = std::move(covered);
    platform::debugTrace("bandages");
}

void draw() {
    State& s = state();
    if (s.frame <= 0) return;
    // Its frames at 30 a second (Controller.checkTransition: nextFrame each
    // ENTER_FRAME), the change on frame 25, gone after the last.
    double now = platform::passTime();
    s.carry = std::min(s.carry + (now - s.last) * flashstage::kFps, 4.0);
    s.last = now;
    while (s.carry >= 1 && s.frame > 0) {
        s.carry -= 1;
        if (++s.frame == kCovered && s.covered) {
            std::function<void()> f = std::move(s.covered);
            s.covered = nullptr;
            f();
        }
        if (s.frame > s.frames) {
            s.frame = 0;
            platform::debugTrace("bandages done");
            return;
        }
    }
    flashstage::art("flash-shell").draw(s.symbol, s.frame, s.frame, flashstage::at(), kOwner);
}

}  // namespace as2::bandages
