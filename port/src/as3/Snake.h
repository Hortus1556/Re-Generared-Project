// mt::Snake (vtable 0x417c50; base class 18; 0xfddc bytes): a snake in a hole
// in the patient (SnakeInHole), coming out (SnakeOut) every 1.5 to 2.5
// seconds for a second, then back in. Out, the tazer stuns it (it stays out
// up to 4 seconds), and stunned the chainsaw held on it for 2 seconds cuts
// it: back into its hole, three times, and the third kills it (its fang,
// AngledExtractionSnake, left in the patient).
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Snake : public InjuryState {
public:
    Snake();                       // the factory's case 18 (0x995cc)
    void vf3() override;           // vf3 0x14aea0
    void vf4(float dt) override;   // vf4 0x14af38
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x14b4b8
    void completeStage() override;  // vf18 0x14b2cc
    void toolStopped(int tool) override;  // vf23 0x14ba3c
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x14b254
    // (the port's) its hole shows while it waits for others (Snake.cpp's vf4)
    bool shownWhileWaiting() const override { return true; }

    // back into its hole (the end of 0x14af38's SnakeOut, and of a cut)
    void goBackIn();

    int unused0 = 3;           // +0xfda8
    float sawTime = 2;         // +0xfdac: the chainsaw on it this long cuts it
    float inTime = 2;          // +0xfdb0: in its hole, give or take
    float inTimeVariance = 0.5f;  // +0xfdb4
    float outTime = 1;         // +0xfdb8
    float unused1 = 2;         // +0xfdbc
    float stunTime = 3;        // +0xfdc0: what the tazer adds to its time out
    float outLeft = 0;         // +0xfdc4
    int lives = 3;             // +0xfdc8
    bool sawing = false;       // +0xfdcc
    float sawLeft = 0;         // +0xfdd0
    float inLeft = 0;          // +0xfdd4
    bool stunned = false;      // +0xfdd8
    bool comboGiven = false;   // +0xfd9c
    bool inHole = false;       // +5: not to be pointed at (the tutorial's, the partners' pick)
};

}  // namespace mt
