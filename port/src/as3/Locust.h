// mt::Locust (iOS 1.45; base class Locust, 1.45's 0xc; 0x10298 bytes): Le
// Flairground Fantastique's locust (Chartreuse Flair's "Plague of Locust"),
// the flea's twin: 1.45's code is the flea's, line for line, with its own
// states (LocustJumping, LocustDead), sounds and piece. It jumps about the
// patient, 130 canvas points a jump in a direction that keeps it in the play
// area, and every 3 to 5 jumps bites where it lands (a SmallCut); the tazer
// kills it, and dead the tongs take it out.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Locust : public InjuryState {
public:
    // its states (kStateNames' order: the Flairground's, appended,
    // SurgeryData.cpp; 1.45's own 0x19, 0x1a)
    static constexpr int kJumping = 92, kDead = 93;

    Locust();                      // the factory's case 0xc (1.45 0xb8838)
    void start() override;         // vf2: 1.45 onCreated 0x4b7cdc
    void vf3() override;           // vf3: 1.45 onAddedToScene 0x4b7d78
    void vf4(float dt) override;   // vf4: 1.45 runLogic 0x4b7de8
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8: 1.45 runInteraction 0x4b85fc
    void completeStage() override;  // vf18: 1.45 autoCompleteStage 0x4b887c
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27: 1.45 getCurrentLegalTool 0x4b859c

    // a jump (1.45 setupJumpAnimation 0x4b8324) to a place picked by
    // 1.45 getNewPositionToGo 0x4b8a00
    void jump();
    cocos2d::CCPoint pickLanding();

    int jumpLength = 130;      // +0x10258: canvas points (+0x10270 in cocos points)
    int turnRange = 10;        // +0x1025c: degrees a jump turns it, either way
    float jumpEvery = 0.9f;    // +0x10260: from a jump's start to its landing
    int minJumps = 3, maxJumps = 5;  // +0x10264, +0x10268: between bites
    float startDelayMax = 1.5f;  // +0x1026c
    int jumpDistance = 0;      // +0x10270
    float jumpTimer = 0;       // +0x10278
    cocos2d::CCPoint landing;  // +0x1027c
    float startDelay = 0;      // +0x10290
    int jumpsLeft = 0;         // +0x10294: before it bites
};

}  // namespace mt
