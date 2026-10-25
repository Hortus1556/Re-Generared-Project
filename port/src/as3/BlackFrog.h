// mt::BlackFrog (vtable 0x417cf0; base class 3; 0xfe50 bytes): the big black
// frog (BlackFrogNormal), hopping about (100 canvas points a hop), laying a
// frog's egg (MiniFrog) behind it and spitting poison (FrogPoisonArea) in
// front, more often as it weakens. Burnt with the lighter (80 of the
// lighter's power), it lays five eggs and leaps off the screen
// (BlackFrogAngry), comes back 5 seconds later somewhere else, and goes on
// one level weaker; the third burning kills it (BlackFrogDead), to be taken
// out with the tongs.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class BlackFrog : public InjuryState {
public:
    BlackFrog();                   // the factory's case 3 (0x98a54)
    void start() override;         // vf2 0x151350
    void vf3() override;           // vf3 0x1513b8
    void vf4(float dt) override;   // vf4 0x151418
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x1528cc
    void completeStage() override;  // vf18 0x152730
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x1526e0

    void flash(float dt);          // 0x152548
    void targetInArea();           // 0x1525ec: a place anywhere in the play area
    void targetOffScreen();        // 0x152c44: a screen's width from its middle
    void heading();                // 0x152d08
    void die();

    int levels = 3;            // +0xfda8
    float burnHealth = 80;     // +0xfdac
    float flashTime = 1, flashEvery = 0.1f;  // +0xfdb0, +0xfdb4
    float burntDelay = 0.1f;   // +0xfdb8
    float backDelay = 0.1f;    // +0xfdbc
    float speed = 400, angrySpeed = 500;  // +0xfdc0, +0xfdc4: canvas points a second
    float offTime = 5;         // +0xfdc8: off the screen
    // a level's wait between hops, and every how many hops it lays and spits
    // (+0xfdcc: the weakest first)
    struct Level {
        float wait;
        int eggEvery, spitEvery;
    } table[3] = {{0.5f, 1, 3}, {1.0f, 3, 5}, {2.0f, 5, 7}};
    // its eggs' places behind it: a turn off its own and a distance (+0xfdf0)
    float eggs[5][2] = {{0, 105}, {20, 70}, {20, 110}, {-20, 70}, {-20, 110}};
    int level = 3;             // +0xfe18
    int hops = 0;              // +0xfe1c
    float health = 80;         // +0xfe20
    float flashLeft = 0, flashTick = 0;  // +0xfe24, +0xfe28
    bool flashOn = false;      // +0xfe2c
    cocos2d::CCPoint dir;      // +0xfe30
    cocos2d::CCPoint target;   // +0xfe38
    float angle = 0;           // +0xfe40
    float timer = 0;           // +0xfe44
    bool off = false;          // +0xfe48: leapt off the screen
    bool layNext = false;      // +0xfe49
    bool spitNext = false;     // +0xfe4a
    bool laying = false;       // +0xfe4b
    bool spitting = false;     // +0xfe4c
    bool laid = false;         // +0xfe4d
    bool spat = false;         // +0xfe4e
    bool away = false;         // +5: off the screen (not to be pointed at)
};

}  // namespace mt
