// mt::MiniBat (vtable 0x418670; base class 12; 0xfdc4 bytes): a little bat
// born of a dead Bat. It grows to its size in 0.7 s and flies straight
// across the play area, turning off its sides towards another, biting (its
// stage's damage) until tazered.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class MiniBat : public InjuryState {
public:
    MiniBat();                     // the factory's case 12 (0x99acc)
    void vf3() override;           // vf3 0x184b08
    void vf4(float dt) override;   // vf4 0x184d80
    void onFinished() override;    // vf20 0x185058

    // its next heading: towards a point on another side (0x184bdc)
    void newHeading();

    int speed = 200;           // +0xfda8: canvas points a second
    float growTime = 0.7f;     // +0xfdac
    cocos2d::CCPoint dir;      // +0xfdb0
    cocos2d::CCPoint canvas;   // +0xfdb8: where it is, canvas points
    int lastSide = -1;         // +0xfdc0
};

}  // namespace mt
