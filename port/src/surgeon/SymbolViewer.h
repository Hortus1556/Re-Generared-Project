// Debug: one symbol of the game's art on an empty stage, to check the
// vector art against other renderers. Debug mode only, from
//   AS_VECTOR_SYMBOL=<linkage or id>  the symbol
//   AS_VECTOR_FRAME=<n>               hold that frame (default: play it)
//   AS_VECTOR_AT=<x>,<y>              its origin on the stage (default 300,230)
//   AS_VECTOR_SCALE=<s>               its scale (default 1)
#pragma once

#include "surgeon/Screen.h"

class SymbolViewer : public Screen {
public:
    // The viewer the environment asks for, or null (+1).
    static SymbolViewer* fromEnvironment(SurgeonGame& game);

    void update() override { ++age; }
    void draw() override;

private:
    using Screen::Screen;
    int symbol = -1, frame = 0;
    float x = 300, y = 230, scale = 1;
    long age = 0;
};
