// The mouse over the buttons of drawn art, as Flash ran its buttons: the
// button under the mouse shows its over state, the one pressed its down
// state while the mouse stays on it, and a release on the button pressed is
// a click. Buttons are the art's button characters (ids), found under the
// mouse by the screen (VectorArt::buttonAt).
#pragma once

#include "engine/VectorArt.h"

class ButtonTracker {
public:
    // The mouse moved; `under` is the button under it (-1: none).
    void move(int under) { hover = under; }
    void down(int under) {
        hover = under;
        pressed = under;
    }
    // Returns the button clicked (-1: none).
    int up(int under) {
        hover = under;
        int clicked = pressed >= 0 && pressed == under ? pressed : -1;
        pressed = -1;
        return clicked;
    }
    void reset() { hover = pressed = -1; }
    // The states to draw the buttons in.
    void apply(VectorLook& look) const {
        look.buttons.clear();
        if (hover >= 0) look.buttons[hover] = pressed == hover ? 4 : pressed >= 0 ? 1 : 2;
    }

    int hover = -1, pressed = -1;
};
