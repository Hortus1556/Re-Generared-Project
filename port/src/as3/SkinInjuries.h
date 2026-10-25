// Two injuries of the skin with code of their own:
// - FrostBite (vtable 0x415b70; base 35): thawed with the lighter, fading as
//   its points thaw; touched with any other tool, the tools freeze (5 s);
// - Boil (vtable 0x415870; base 4): grows until it bursts by itself (a cut,
//   and poison around it, more the bigger it was, and damage), cut open with
//   the pizza cutter first or drawn out with the syringe.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class FrostBite : public InjuryState {
public:
    FrostBite();  // (0x908c4 case 0x23)
    void vf3() override;  // vf3 0x8b1cc: frostbite_spread, its animation
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x8b32c

    float minOpacity = 0.4f;  // +0xfda8
};

class Boil : public InjuryState {
public:
    Boil();  // (0x908c4 case 4)
    void start() override;  // vf2 0x898c8
    void vf3() override;    // vf3 0x89b68: growing
    void vf4(float dt) override;  // vf4 0x89bbc: grows; bursts; done when its burst is over
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x8ad30
    HitResult injectionHitTest(const cocos2d::CCPoint& touch, int mode, float fill) override;  // vf9 0x8ae4c
    void completeStage() override;  // vf18 0x8ad14
    void toolStopped(int) override { lastFill = -1; }  // vf23 0x89bac
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x8acbc

    float size = 0.3f;        // +0xfda8: its picture's scale
    float growth = 0.0002f;   // +0xfdac: a frame
    float poison2At = 0.4f, poison3At = 0.6f, poison4At = 0.8f;  // +0xfdb0..+0xfdb8: more poison the bigger it bursts
    float burstAt = 1.0f;     // +0xfdbc
    float poppedAt = 0.15f;   // +0xfdc0: drawn out to this, it is done
    float injectionDivisor = 4.0f;  // +0xfdc4
    float lastFill = -1;      // +0xfdc8
    bool bursting = false;    // +0x6700 (the original's dependency flag: no hit tests meanwhile)

private:
    void burst();  // 0x89f8c
};

}  // namespace mt
