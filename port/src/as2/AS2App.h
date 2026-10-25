// Amateur Surgeon 2 HD on the PC: the app delegate's start-up, the display
// link's loop on the platform's passes, and full-screen movies.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "platform/Platform.h"

namespace ui {
class DebugPanel;
struct DebugItem;
}

namespace as2 {

// MPMoviePlayerController, full screen over everything: plays the bundle
// movie, then calls `finished` (also when the player skips it with a
// double tap, as on the iPad, or Escape).
void playMovie(const std::string& file, std::function<void()> finished);

class AS2App : public platform::TouchSink {
public:
    AS2App();
    ~AS2App() override;

    // One platform pass.
    void cycle();

    void touchBegan(float x, float y) override;
    void touchMoved(float x, float y, float prevX, float prevY) override;
    void touchEnded(float x, float y, float prevX, float prevY) override;
    void escapeKey() override;
    void numberKey(int n) override;
    void arrowKey(int dx, int dy, bool down) override;
    // AS_GOTO: straight to a mission's stage (debug mode, for screenshots).
    void gotoStage();
    void mouseMoved(float x, float y) override;
    // F1 (debug mode): the debug menu.
    void debugMenuKey() override;
    // Typed text: a type-n-tell's letters (PC only).
    void charKey(char c) override;

private:
    // The debug menu's items: the missions (from the menus, straight into
    // the operation), in a mission win now, next stage, heal, +60 s, lose
    // and god mode; smooth motion; every mission unlocked (in the save in
    // use).
    std::vector<ui::DebugItem> debugItems();
    int framesSinceUpdate = 0;
    ui::DebugPanel* debugPanel = nullptr;
};

}  // namespace as2
