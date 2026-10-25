#include "surgeon/SurgeonGame.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "platform/DrawList.h"
#include "platform/GL.h"

SurgeonGame::SurgeonGame(const std::string& artName, std::vector<SoundBank::Sound> sounds)
    : art_(&VectorArt::library(artName)), sounds_(*art_, std::move(sounds)), debugMenu_(new DebugMenu(*this)) {}

SurgeonGame::~SurgeonGame() {
    releaseRef(debugMenu_);
    releaseRef(screen);
    releaseRef(next);
}

void SurgeonGame::show(Screen* s) {
    releaseRef(next);
    next = s;
}

Screen* SurgeonGame::inputTarget() const {
    if (debugMenu_->capturesInput()) return debugMenu_;
    return screen;
}

namespace {

double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// Debug: AS_PROFILE=1 logs each pass's times to stderr.
bool profiling() {
    static bool on = platform::debugMode() && std::getenv("AS_PROFILE");
    return on;
}

}  // namespace

void SurgeonGame::cycle() {
    double t0 = nowMs();
    if (next) {
        releaseRef(screen);
        screen = next;
        next = nullptr;
    }
    if (screen && !debugMenu_->isOpen()) screen->update();
    {
        Screen* t = inputTarget();
        int b = t ? t->hoveredButton() : -1;
        platform::setHandCursor(b >= 0 && std::find(noHandCursor.begin(), noHandCursor.end(), b) == noHandCursor.end());
        platform::setSystemCursorVisible(!(screen && screen->drawsPointer()) || debugMenu_->isOpen());
    }
    sounds_.update();
    double t1 = nowMs();

    auto draw = [&] {
        const uint8_t* bg = art_->movie().background;
        platform::clearScreen(bg[0] / 255.0f, bg[1] / 255.0f, bg[2] / 255.0f, 1);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, platform::screenWidth(), platform::screenHeight(), 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        if (screen) screen->draw();
        debugMenu_->draw();
    };
    // Art the pass shows for the first time (a screen coming in) is made by
    // all the raster workers side by side, then the pass is drawn again with
    // it: made one piece after another here, a transition's passes took up
    // to a tenth of a second each.
    flash::Raster& raster = art_->rasterCache();
    raster.deferring = true;
    draw();
    raster.deferring = false;
    if (raster.deferred()) {
        raster.finishDeferred();
        draw();
    }
    double t2 = nowMs();
    // While the screen prepares what it will show soon (a transition hiding
    // it), the art made in the background goes up to the GPU faster.
    art_->endFrame(screen && screen->preparing() ? 20 : 4);
    double t3 = nowMs();
    platform::endFrame();
    int n = 0, un = 0;
    double ms = 0, ums = 0;
    art_->rasterCache().takeSyncStats(n, ms);
    art_->rasterCache().takeUploadStats(un, ums);
    static long pass = 0;
    ++pass;
    if (profiling()) {
        if (pass == 1) std::fprintf(stderr, "display scale %.2f\n", platform::displayScale());
        std::fprintf(stderr, "pass %ld: update %.1f draw %.1f (raster %d in %.1f, uploads %d in %.1f) swap-in %.1f at %.4f\n",
                     pass, t1 - t0, t2 - t1, n, ms, un, ums, t3 - t2, platform::now());
    }
    // Debug mode: a pass long enough to be seen (a stall, a hitch at a
    // stage's change) is in the player's debug.log, with where it went.
    if (platform::traceEnabled() && t3 - t0 > 80)
        platform::debugTrace("slow pass %ld (%.0f ms): update %.0f, draw %.0f (art made at once %d in %.0f ms, uploaded at "
                             "once %d in %.0f ms), swap-in %.0f; display scale %.2f",
                             pass, t3 - t0, t1 - t0, t2 - t1, n, ms, un, ums, t3 - t2, platform::displayScale());
}

void SurgeonGame::touchBegan(float x, float y) {
    mouseX = x;
    mouseY = y;
    mouseDown = true;
    if (Screen* s = inputTarget()) s->touchDown(x, y);
}

void SurgeonGame::touchMoved(float x, float y, float, float) {
    mouseX = x;
    mouseY = y;
    if (Screen* s = inputTarget()) s->touchMove(x, y);
}

void SurgeonGame::touchEnded(float x, float y, float, float) {
    mouseX = x;
    mouseY = y;
    mouseDown = false;
    if (Screen* s = inputTarget()) s->touchUp(x, y);
}

void SurgeonGame::debugMenuKey() { debugMenu_->toggle(); }

void SurgeonGame::numberKey(int n) {
    if (Screen* s = inputTarget()) s->numberKey(n);
}

void SurgeonGame::escapeKey() {
    if (Screen* s = inputTarget()) s->escapeKey();
}

void SurgeonGame::mouseMoved(float x, float y) {
    mouseX = x;
    mouseY = y;
    if (Screen* s = inputTarget()) s->touchMove(x, y);
}
