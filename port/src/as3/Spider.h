// mt::Spider (Android 1.43; vtable 0xee86e8; base class Spider; 0x10080
// bytes): the Hospital's spider, the ice ant's close relative. It scurries
// about the play area (SpiderWalking), turning now and then and back towards
// the middle at the area's sides; every 18 seconds or so it bites
// (SpiderBite: a DarkPoisonArea and a SmallCut where it is, and 1 damage).
// The lighter burns it: held on it, each frame wears the lighter's power off
// its 5 lives, then it dies (SpiderDead) and is taken out with the tongs. The
// lighter used anywhere else scares it: it runs from the flame, faster for a
// moment (or back to the middle if it is far out and the flame is close).
// Its own state (+0x3c) runs it: its file's first stage ("SpiderWalk") is no
// state the game knows. It has no scurrying sound (its file lists the ants').
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Spider : public InjuryState {
public:
    Spider();                      // 0x560564 (the factory's tuning)
    void start() override;         // vf2 onCreated 0x71057c
    void vf3() override;           // vf3 onAddedToScene 0x7107bc
    void vf4(float dt) override;   // vf4 runLogic 0x711128
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 runInteraction 0x7119e4
    void completeStage() override;  // vf18 autoCompleteStage 0x71192c
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 getCurrentLegalTool 0x711810

    // turned towards a random place of the middle box (lookAtMiddle 0x7108b0)
    void lookAtMiddle();
    // turned away from the flame, or to the middle (fleeFromFire 0x710ac8)
    void fleeFromFire();
    // its bite (bite 0x710ed8)
    void bite();
    // the lighter used elsewhere: it runs (reactToPlayer 0x711df8)
    void reactToPlayer(const cocos2d::CCPoint& touch);

    float biteEvery = 18;      // +0x10004
    float unused0 = 2;         // +0x10008 (its timer is counted from it, not used)
    float step = 8;            // +0x1000c: cocos points a frame
    float fleeStep = 12;       // +0x10010: the same while it runs from the flame
    float boxWidth = 200, boxHeight = 100;  // +0x10014, +0x10018: the middle it heads back to
    float unused1 = 0;         // +0x1001c
    float biteDamage = 1;      // +0x10020
    float turnEvery = 1;       // +0x10024
    float turnSpeed = 2.25f;   // +0x10028: degrees a frame
    float fleeTime = 0.2f;     // +0x1002c: how long it runs faster
    float farFromMiddle = 150; // +0x10030: further than this from the middle...
    float fireNear = 120;      // +0x10034: ...and the flame nearer than this: to the middle
    bool dead = false;         // +0x10038
    cocos2d::CCRect bounds;    // +0x10040: the play area, cocos points
    cocos2d::CCRect box;       // +0x10050: the middle box
    float lives = 5;           // +0x10060: worn by the lighter's power a frame
    float turnTimer = 0;       // +0x10064
    float turnDir = 0;         // +0x10068
    float biteTimer = 0;       // +0x1006c
    float unusedTimer = 0;     // +0x10070
    cocos2d::CCPoint fire;     // +0x10074: where the flame was
    float fleeTimer = 0;       // +0x1007c
};

}  // namespace mt
