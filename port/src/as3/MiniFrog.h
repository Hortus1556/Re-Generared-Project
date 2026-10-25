// mt::MiniFrog (vtable 0x417d90; base class 13; 0xfe18 bytes): a frog's egg
// on the patient (MiniFrogEggClosed) that hatches after about 7 seconds, or
// at once at any tool but the clamp. A clamp on it holds it (MiniFrogEggClamped,
// 4 seconds to tighten it fully, or it hatches anyway); clamped shut, it is
// burnt or sawn away (5 of the tool's power). Hatched, the frog hops about
// (MiniFrogNormal); tazered it turns angry (MiniFrogAngry: faster, longer
// hops, flashing) and dies of the lighter (5 of its power); dead
// (MiniFrogDead), the tongs take it out.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class MiniFrog : public InjuryState {
public:
    MiniFrog();                    // the factory's case 13 (0x9889c)
    void start() override;         // vf2 0x1586c0
    void vf3() override;           // vf3 0x15873c
    void vf4(float dt) override;   // vf4 0x1587a0
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x159210
    void completeStage() override;  // vf18 0x159134
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x15918c

    void flash(float dt);          // 0x158e7c
    void hatch();                  // 0x158f58
    float hatchTime();             // 0x1590e4
    void heading();                // 0x159598: towards its target, its turn
    // done with outright (0x83624)
    void finishNow();

    float hatchBase = 7, hatchVariance = 1.5f;  // +0xfda8, +0xfdac
    float afterHatch = 2;      // +0xfdb0
    float unused0 = 5;         // +0xfdb4
    float clampTime = 4;       // +0xfdb8
    int hop = 45, angryHop = 100;           // +0xfdbc, +0xfdc0: canvas points
    float speed = 125, angrySpeed = 250;    // +0xfdc4, +0xfdc8: canvas points a second
    float hopEvery = 1, angryHopEvery = 0.5f;  // +0xfdcc, +0xfdd0
    int angryHops = 5;         // +0xfdd4
    float flashTime = 1;       // +0xfdd8
    float flashEvery = 0.1f;   // +0xfddc
    int hops = 0;              // +0xfde0
    bool hatched = false;      // +0xfde4
    float health = 5;          // +0xfde8: the clamped egg's, the angry frog's
    float timer = 0;           // +0xfdec
    bool clampDone = false;    // +0xfdf0
    float lastClamp = 0;       // +0xfdf4
    cocos2d::CCPoint dir;      // +0xfdf8
    cocos2d::CCPoint target;   // +0xfe00
    float angle = 0;           // +0xfe08
    float flashLeft = 0;       // +0xfe0c
    float flashTick = 0;       // +0xfe10
    bool flashOn = false;      // +0xfe14
    bool red = false;          // +0xfe15
};

}  // namespace mt
