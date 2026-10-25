// mt::Jellyfish (iOS 1.45; base class Jellyfish, 1.45's 0xd; 0x102b8
// bytes): the jellyfish of Baby AWOOOOGAH's Splash Tank (the whale, Le
// Flairground Fantastique's boss). It swims a Lissajous figure about the
// middle of the canvas (x 320 ± 140, y 192 ± 80), its own clock running 4
// a second (9 while it darts); every 13 s (the first within 6.5) it stings:
// "sting", the electricity and shock effects, 3 damage and a JellyfishSting
// (a Burn) where it is. The tazer zaps it: each zap (not while its "tazer"
// plays) takes one of its 4 lives and sends it darting for 3 s; the last
// kills it (JellyfishDied, jellyfish_death), and dead it bobs gently until
// the tongs take it out. JellyfishFiring, in the state table, is used by no
// code.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Jellyfish : public InjuryState {
public:
    // its states (kStateNames' order: the Flairground's, appended,
    // SurgeryData.cpp; 1.45's own 0xd..0xf)
    static constexpr int kSwimming = 89, kFiring = 90, kDied = 91;

    Jellyfish();                   // 1.45 0xc3994 (the factory's case 0xd)
    void start() override;         // vf2: 1.45 onCreated 0x4c0fac
    void vf3() override;           // vf3: 1.45 onAddedToScene 0x4c10b4
    void vf4(float dt) override;   // vf4: 1.45 runLogic 0x4c1190
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8: 1.45 runInteraction 0x4c262c
    void completeStage() override;  // vf18: 1.45 autoCompleteStage 0x4c2968
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27: 1.45 getCurrentLegalTool 0x4c242c

    // (1.45 startSwimming 0x4c10d4) its figure begun from where it is,
    // raised over the other injuries, swimming
    void startSwimming();
    // (1.45 setNewRandomPositionToGo 0x4c247c) a heading towards a random
    // place on a random side of the play area (kept; nothing reads it)
    void setNewRandomPositionToGo();
    // its sting (in 1.45's runLogic)
    void sting();

    bool dying = false;        // +6 (InjuryState's in 1.45: findInjuryToHit skips it)
    int hits = 0;              // +0x10258: tazer zaps taken
    int tazerLoops = 1;        // +0x1025c: "tazer" played this many times a zap
    int lives = 4;             // +0x10260: zaps that kill it
    int swimSpeed = 4;         // +0x10264: its clock's pace
    int dartSpeed = 9;         // +0x10268: the same while it darts
    float dartTime = 3;        // +0x1026c: darting after a zap
    float stingEvery = 13;     // +0x10270
    float swayY = 80;          // +0x10274: canvas points up and down
    float swayYRate = 0.3f;    // +0x10278
    float swayX = 140;         // +0x1027c: canvas points left and right
    float swayXRate = 0.1f;    // +0x10280
    float stingDamage = 3;     // +0x10284
    float deadDrift = 0.2f;    // +0x10288: canvas points a frame at most, dead
    float bobRange = 1.5f;     // +0x1028c
    float bob = 1;             // +0x10290: -bobRange..bobRange and back, a second a second
    bool bobUp = true;         // +0x10294
    cocos2d::CCPoint heading;  // +0x10298
    cocos2d::CCPoint centre;   // +0x102a0: canvas points
    int side = -1;             // +0x102a8
    float dartTimer = 0;       // +0x102ac
    float stingTimer = 0;      // +0x102b0
    float clock = 0;           // +0x102b4: its figure's time
};

}  // namespace mt
