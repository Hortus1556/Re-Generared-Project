// One screen of a surgeon game (title, story, dialogue, operation...): it
// updates once per movie frame and draws in the movie's stage pixels.
#pragma once

#include "core/Object.h"

class SurgeonGame;

class Screen : public Object {
public:
    explicit Screen(SurgeonGame& game) : game(game) {}

    virtual void update() {}
    virtual void draw() = 0;

    // A panel over the screen (the debug menu) that takes the pointer
    // input instead of the screen beneath, now.
    virtual bool capturesInput() const { return false; }

    // Pointer input in stage pixels.
    virtual void touchDown(float x, float y) {}
    virtual void touchMove(float x, float y) {}  // with or without a button down
    virtual void touchUp(float x, float y) {}
    // Keys: 1..9 and 10 for 0 (an operation's tools), Escape.
    virtual void numberKey(int /*n*/) {}
    virtual void escapeKey() {}
    // The button (character id) under the mouse, -1 if none (for the hand
    // cursor).
    virtual int hoveredButton() const { return -1; }
    // Whether it draws its own pointer now (the system's is hidden then).
    virtual bool drawsPointer() const { return false; }
    // Whether art it will show soon is being made now (more of it goes up
    // to the GPU each pass meanwhile).
    virtual bool preparing() const { return false; }

protected:
    SurgeonGame& game;
};
