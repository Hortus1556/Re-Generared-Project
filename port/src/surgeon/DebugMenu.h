// The F1 debug menu of the surgeon games (debug mode only): a panel of
// buttons over the game, given by the game (debugItems()); a click runs
// one and closes the panel, unless it is a switch.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "surgeon/Screen.h"

class DebugMenu : public Screen {
public:
    struct Item {
        std::string label;
        std::function<void()> action;
        bool enabled = true;
        bool on = false;         // a switch shown on
        bool keepOpen = false;   // switches leave the menu up
        bool newRow = false;     // starts a new row
    };

    explicit DebugMenu(SurgeonGame& game) : Screen(game) {}

    bool isOpen() const { return open; }
    void toggle() { open = !open; }

    void draw() override;
    bool capturesInput() const override { return open; }
    void touchUp(float x, float y) override;

    // The text field drawn for labels (a character id of the movie).
    int labelText = -1;

private:
    struct Placed {
        Item item;
        float x, y, w, h;
    };
    std::vector<Placed> layout();
    bool open = false;
};
