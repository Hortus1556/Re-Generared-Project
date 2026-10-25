// mt::Bat (vtable 0x4185e0; base class 2; 0xfe00 bytes): a bat hanging on
// the patient (BatHanging), shuddering at any tool but the lighter; burnt, it
// takes flight (BatFlying) round the screen on a wavy path, cutting the
// patient (a SmallCut, a quarter of the times a LargeCut) every 1.5 seconds;
// tazered three times it dies (BatDied), and seven mini bats (MiniBat) are
// born from it as its "dead" animation goes on, unless an injection got it
// first; dead, it is taken out with the tongs.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Bat : public InjuryState {
public:
    Bat();                    // the factory's case 2 (0x99c2c)
    void start() override;    // vf2 0x183348
    void vf3() override;      // vf3 0x1833e0: its pictures are built
    void vf4(float dt) override;  // vf4 0x183418: its flight
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x1843f4
    HitResult injectionHitTest(const cocos2d::CCPoint& touch, int mode, float fill) override;  // vf9 0x184a58
    void completeStage() override;  // vf18 0x1849e8
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x183f5c

    // burnt: it flies (0x183fc0)
    void takeFlight();

    int tazerHits = 0;         // +0xfda8
    int tazerLoops = 3;        // +0xfdac: the "tazer" animation's loops
    int hitsToKill = 3;        // +0xfdb0
    int slowSpeed = 25;        // +0xfdb4: its path's pace
    int fastSpeed = 35;        // +0xfdb8: tazered
    float fastTime = 3;        // +0xfdbc
    float cutEvery = 3;        // +0xfdc0: the first cut (half of it after)
    int miniBats = 7;          // +0xfdc4
    // its path: x = sin(t xFreq) xAmp, y = cos(t yFreq) yAmp + cos(t yFreq2) yAmp2
    // about the canvas' middle
    float yAmp = 100, yFreq = 0.08f;   // +0xfdc8, +0xfdcc
    float xAmp = 140, xFreq = 0.12f;   // +0xfdd0, +0xfdd4
    float yFreq2 = 0.7f, yAmp2 = 20;   // +0xfdd8, +0xfddc
    cocos2d::CCPoint dir;      // +0xfde0: towards a side of the play area (not used by the flight)
    cocos2d::CCPoint canvas;   // +0xfde8: where it flies, canvas points
    int lastSide = -1;         // +0xfdf0
    float fastLeft = 0;        // +0xfdf4
    float cutTimer = 0;        // +0xfdf8
    float pathTime = 0;        // +0xfdfc
    bool dying = false;        // +6: the mini bats to come
    bool spawnsMiniBats = true;  // +0xfda5
};

}  // namespace mt
