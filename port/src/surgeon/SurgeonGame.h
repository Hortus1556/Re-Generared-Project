// The game canvas of the surgeon games rebuilt from their Flash originals:
// the movie's stage is the game's screen (stage pixels are game units), its
// symbols are the art (VectorArt), and one game pass is one movie frame.
// It runs the current Screen (the game's StageScreen: the movie itself, its
// own transitions included), the sounds and the debug menu.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/VectorArt.h"
#include "platform/Platform.h"
#include "surgeon/DebugMenu.h"
#include "surgeon/Screen.h"
#include "surgeon/Script.h"
#include "surgeon/SoundBank.h"

class SurgeonGame : public platform::TouchSink {
public:
    // `artName`: the game's art (a package's library.json, or a SWF).
    SurgeonGame(const std::string& artName, std::vector<SoundBank::Sound> sounds);
    ~SurgeonGame() override;

    VectorArt& art() { return *art_; }
    SoundBank& sounds() { return sounds_; }
    // The package's script index (scripts/index.json; Script.h).
    const script::ScriptIndex& scripts() const { return scripts_; }

    // Shows `screen` (+1, taken over) from the next pass.
    void show(Screen* screen);

    void cycle();  // one pass

    void touchBegan(float x, float y) override;
    void touchMoved(float x, float y, float prevX, float prevY) override;
    void touchEnded(float x, float y, float prevX, float prevY) override;
    void mouseMoved(float x, float y) override;
    void numberKey(int n) override;
    void escapeKey() override;
    void debugMenuKey() override;

    // The F1 debug menu's buttons (debug mode).
    virtual std::vector<DebugMenu::Item> debugItems() { return {}; }
    DebugMenu& debugMenu() { return *debugMenu_; }
    Screen* currentScreen() const { return screen; }

    float mouseX = 0, mouseY = 0;
    bool mouseDown = false;
    // Buttons that keep the arrow (Flash's useHandCursor = false).
    std::vector<int> noHandCursor;

private:
    VectorArt* art_;
    SoundBank sounds_;
    script::ScriptIndex scripts_{"scripts/index.json"};
    DebugMenu* debugMenu_;
    Screen* screen = nullptr;
    Screen* next = nullptr;
    Screen* inputTarget() const;
};
