// mt::Flea (vtable 0x418710; base class 8; 65000 bytes): a flea jumping
// about the patient (FleaJumping), a jump of 130 canvas points in a
// direction that keeps it in the play area, landing every 3 to 5 jumps with
// a bite (a SmallCut where it lands); the tazer kills it (FleaDead), and
// dead it is taken out with the tongs.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Flea : public InjuryState {
public:
    Flea();                        // the factory's case 8
    void start() override;         // vf2 0x18aa64
    void vf3() override;           // vf3 0x18aaf8
    void vf4(float dt) override;   // vf4 0x18ab58
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x18b348
    void completeStage() override;  // vf18 0x18b5dc
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x18b2e8

    // a jump (0x18b07c) to a place picked by 0x18b760
    void jump();
    cocos2d::CCPoint pickLanding();

    int jumpLength = 130;      // +0xfda8: canvas points (+0xfdc0 in cocos points)
    int turnRange = 10;        // +0xfdac: degrees a jump turns it, either way
    float jumpEvery = 0.9f;    // +0xfdb0: from a jump's start to its landing
    int minJumps = 3, maxJumps = 5;  // +0xfdb4, +0xfdb8: between bites
    float startDelayMax = 1.5f;  // +0xfdbc
    int jumpDistance = 0;      // +0xfdc0
    float jumpTimer = 0;       // +0xfdc8
    cocos2d::CCPoint landing;  // +0xfdcc
    float startDelay = 0;      // +0xfde0
    int jumpsLeft = 0;         // +0xfde4: before it bites
};

}  // namespace mt
