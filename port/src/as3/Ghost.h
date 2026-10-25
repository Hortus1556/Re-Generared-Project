// mt::Ghost (Android 1.43: base class Ghost, the Hospital's; 0x10064 bytes):
// a ghost drifting over the patient (GhostWalking) in a straight line, 5
// canvas points a frame, turned towards a random place of the middle box
// every 1.5 s and at the play area's sides. The vacuum catches it
// (GhostStruggle: "struggle" once, standing still); held on it, the vacuum
// drains its strength (20, the tool's power a frame), the ghost fading with
// it. Its strength out, it loses a life: with lives left it splits, hiding
// itself (GhostInvisible, 3.5 s, drifting on unseen) and making another
// ghost with the same lives (event 2 "Ghost", its +0x28 the lives: the
// screen hides the new one too, SurgeryScreen::createInjury); both come back
// smaller (scale 0.2 a life + 0.4). The last life lost, it dies (GhostDying:
// swelling to 5 times its size as it fades, 0.6 s), leaving a FrostBite
// where it was. Its own state (+0x3c) runs it: its file's first stage
// ("GhostWalk") is no state the game knows.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Ghost : public InjuryState {
public:
    // its states (kStateNames' order: the Hospital's, appended, SurgeryData.cpp;
    // Android's 0x3b..0x3e)
    static constexpr int kWalking = 85, kInvisible = 86, kStruggle = 87, kDying = 88;

    Ghost();                       // Android 0x55e86c (the factory's case 0x1d)
    void start() override;         // vf2: Android onCreated 0x4e15dc
    void vf3() override;           // vf3: Android onAddedToScene 0x4e17dc
    void vf4(float dt) override;   // vf4: Android runLogic 0x4e1b20
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8: Android runInteraction 0x4e25b8
    void completeStage() override;  // vf18: Android autoCompleteStage 0x4e2428
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27: Android getCurrentLegalTool 0x4e22d8

    // turned towards a random place of the middle box (Android 0x4e18b0)
    void lookAtMiddle();
    // its lives set, hidden for a while with no strength (Android 0x4e1a80;
    // the screen calls it on a ghost made with lives, event 2's +0x28)
    void setLivesAndHide(float lives);

    float hideTime = 3.5f;         // +0x10004: hidden this long
    float lookEvery = 1.5f;        // +0x10008: turns to the middle this often
    float dyingTime = 0.6f;        // +0x1000c
    float unused0 = 1;             // +0x10010
    float step = 5;                // +0x10014: cocos points a frame (5 canvas points)
    float boxWidth = 400, boxHeight = 200;  // +0x10018, +0x1001c: the middle it heads for
    float unused1 = 0;             // +0x10020
    float maxStrength = 20;        // +0x10024
    bool dead = false;             // +0x10028
    int lives = 3;                 // +0x1002c
    float strength = 20;           // +0x10030: the vacuum's power a frame drains it
    cocos2d::CCRect bounds;        // +0x10034: the play area, cocos points
    cocos2d::CCRect box;           // +0x10044: the middle box
    float heading = 0;             // +0x10054: degrees, anticlockwise from the right (cocos)
    float hideTimer = 0;           // +0x10058
    float dyingTimer = 0;          // +0x1005c
    float lookTimer = 0;           // +0x10060

private:
    // its death: a FrostBite where it is, and it is healed (Android
    // runLogic 0x4e2100.., autoCompleteStage)
    void die();
    void traceState();             // (the port's: for test scripts)
    int tracedState = -1;
    int tracedLives = -1;
};

}  // namespace mt
