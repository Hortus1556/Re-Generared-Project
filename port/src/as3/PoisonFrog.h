// mt::PoisonFrog (vtable 0x4187b0; base class 25; 0xfe40 bytes): a poison
// dart frog walking about the play area (PoisonFrogWalking), turning now and
// then and back to the middle at the sides; every 20 seconds or so it looks
// left and right (PoisonFrogLooking) and spits a dart (PoisonFrogFiring: a
// PoisonDart injury). Touched with anything but the tazer while it is not
// stunned, it puffs poison (a blue cloud, the screen's wobble, every 8 s a
// blue screen) and the tool is put down. Tazered (PoisonFrogShocked) it runs
// (PoisonFrogAngry, flashing) for 10 seconds, when the lighter hurts it (15
// of the lighter's power a life, PoisonFrogHurt); three lives and it dies,
// to be taken out with the tongs. Its own state (+0x3c) runs it: its file's
// first stage ("PoisonFrogNormal") is no state the game knows.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class PoisonFrog : public InjuryState {
public:
    PoisonFrog();                  // the factory's case 25 (0x98fd4)
    void start() override;         // vf2 0x1912c8
    void vf3() override;           // vf3 0x191860
    void vf4(float dt) override;   // vf4 0x191c68
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x193304
    void completeStage() override;  // vf18 0x1930a4
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x19303c

    void turnToMiddle();           // 0x1919f8
    void spit();                   // 0x191b04
    void puff();                   // 0x193f28: the gas cloud and the wobble
    void blueScreen();             // 0x194714
    // walking or running, by its own step: turned back at the sides
    void walk(float step);
    void startWalking();
    void startRunning();

    float lookEvery = 20;      // +0xfda8
    float turnEvery = 4;       // +0xfdac
    float walkStep = 2, runStep = 7;  // +0xfdb0, +0xfdb4: cocos points a frame
    float boxWidth = 300, boxHeight = 175;  // +0xfdb8, +0xfdbc
    float dartDistance = 75;   // +0xfdc0
    int dartFrame = 15;        // +0xfdc4
    float runTurnRate = 0;     // +0xfdc8
    float runTime = 10;        // +0xfdcc
    float puffEvery = 1;       // +0xfdd0
    float blueEvery = 8;       // +0xfdd4
    float flashEvery = 0.4f;   // +0xfdd8
    float burnHealth = 15;     // +0xfddc
    int lives0 = 3;            // +0xfde0
    bool dead = false;         // +0xfde4
    int health = 15;           // +0xfde8
    int lives = 3;             // +0xfdec
    bool dartFired = false;    // +0xfdf0
    bool hurtFlash = false, redOn = false, greenOn = false;  // +0xfdf1..+0xfdf3
    bool stunned = false;      // +0xfdf4: tazered, running
    cocos2d::CCRect bounds;    // +0xfdf8
    cocos2d::CCRect box;       // +0xfe08
    bool hopSounded = false;   // +0xfe18
    float hopTimer = 0;        // +0xfe1c
    float slowHop = 0.65f, fastHop = 0.35f;  // +0xfe20, +0xfe24: a hop's sound's place
    float flashTimer = 0;      // +0xfe28
    float puffTimer = 0;       // +0xfe2c
    float blueTimer = 0;       // +0xfe30
    float lookTimer = 0;       // +0xfe34
    float turnTimer = 0;       // +0xfe38
    float runTimer = 0;        // +0xfe3c
    bool comboGiven = false;   // +0xfd9c
};

}  // namespace mt
