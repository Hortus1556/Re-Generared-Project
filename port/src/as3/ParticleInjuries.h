// The injuries made of particles, or that leave things where their particles
// land (mt::ParticleSystem keeps the particles that die for them):
// - Fire (vtable 0x415610; base 24): sucked out with the vacuum; it spreads
//   to its four sides now and then, and its embers land as burns;
// - PassiveParticle (0x418a20; base 14) and SoakableParticle (0x418ab0; base
//   19): a drop of poison where a poison's particle landed, sucked up with
//   the vacuum; a soakable one soaks in after a while, making what it was
//   given (more poison);
// - PoisonArea (0x418970; base 15) and SuperPoisonArea (0x4188e0; base 33):
//   a pool of poison, cut through with the pizza cutter (it bursts into a
//   cut, spilling drops) or drawn out with the syringe;
// - SuperPoisonScab (0x418850; base 34), CementDrop (0x418d30; base 6) and
//   SpikeBase (0x419760; base 36): chipped away with the chainsaw, their
//   chips flying (the cement's landing as cuts).
#pragma once

#include <string>
#include <vector>

#include "as3/InjuryState.h"
#include "as3/ParticleSystem.h"

namespace mt {

class Fire : public InjuryState {
public:
    Fire();  // 0x99210
    ~Fire() override;  // 0x8bc2c
    void start() override;  // vf2 0x8bd94: its flames and embers
    void vf3() override;    // vf3 0x8bea4: fire_loop, its systems shown
    void vf4(float dt) override;  // vf4 0x8c428: smaller as it is sucked out; spreads; embers burn
    void onFinished() override;   // vf20 0x8be9c: its sound stopped
    void pause() override;        // vf28 0x8bd44
    void resume() override;       // vf29 0x8bd6c

    int emberLimit = 45;          // +0xfda8: embers landed for a burn
    int emberCount = 0;           // +0xfdac
    float spreadTimer = 0;        // +0xfdb0
    float spreadEvery = 45;       // +0xfdb4: seconds
    ParticleSystem* flames = nullptr;  // +0xfdb8: particle_fireFX
    ParticleSystem* embers = nullptr;  // +0xfdbc: particle_emberFX (its dead kept)
    float minScale = 0.3f;        // +0xfdc0
    int spreadDistance = 0;       // +0xfdc4: 75 canvas points
    std::vector<int> directions;  // +0xfdc8: the sides not spread to yet (1 right, 2 left, 3 down, 4 up)

private:
    void spread();  // 0x8c010
};

class PassiveParticle : public InjuryState {
public:
    PassiveParticle();
    void start() override;  // vf2 0x19f608
    void vf4(float dt) override;  // vf4 0x19f630: drawn toward the vacuum
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x19f75c
    void toolStopped(int) override { pull = cocos2d::CCPointZero; }  // vf23 0x19f8ac
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x19f6f8

    float speed = 0.015f;   // +0xfda8
    float radius = 70;      // +0xfdac: squared canvas points the vacuum takes it at
    cocos2d::CCPoint pull;  // +0xfdb0: toward the vacuum (a unit vector)
    float reachSq = 0;      // +0xfdb8: half its width, squared
    float distanceSq = 0;   // +0xfdbc: to the vacuum
};

class SoakableParticle : public PassiveParticle {
public:
    SoakableParticle();
    void vf4(float dt) override;  // vf4 0x19f8f0: soaks in when its time is up
    // its time (0x19f8bc: t .. 1.4 t)
    void setSoakTime(float t);

    std::vector<std::string> makes;  // +0xfdc0: made where it soaks in
    float soakTime = 0;              // +0xfdcc
};

class PoisonArea : public InjuryState {
public:
    PoisonArea();
    ~PoisonArea() override;  // 0x19dd34
    void start() override;   // vf2 0x19d920: its systems
    void vf3() override;     // vf3 0x19de78
    void vf4(float dt) override;  // vf4 0x19df78: its drops; done when they all fell
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x19e788
    HitResult injectionHitTest(const cocos2d::CCPoint& touch, int mode, float fill) override;  // vf9 0x19f160
    void completeStage() override;  // vf18 0x19ee10
    void toolStopped(int) override;  // vf23 0x19f148
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x19e730

    int swipeSq = 500;        // +0xfda8: squared canvas points a cut must go
    cocos2d::CCPoint swipeStart;  // +0xfdac: on the canvas
    ParticleSystem* bubbles = nullptr;   // +0xfdb4: bubble_fx_ref
    ParticleSystem* soakable = nullptr;  // +0xfdb8: soakable_fx_ref (dead kept)
    ParticleSystem* passive = nullptr;   // +0xfdbc: passive_fx_ref (dead kept)
    bool swiping = false;     // +0xfdc0
    float scale = 1;          // +0xfdc4: drawn out by the syringe
    float fdc8 = 1;           // +0xfdc8
    float minScale = 0.3f;    // +0xfdcc
    float injectionDivisor = 1.7f;  // +0xfdd0
    float lastFill = -1;      // +0xfdd4
    float soakableDrops = 2, passiveDrops = 4;  // +0xfdd8, +0xfddc: at full size
    // the bubbles' settings, scaled with it (+0xfde0..+0xfdf4)
    cocos2d::CCPoint bubblePosVar;
    float bubbleTotal = 0, bubbleLifeVar = 0, bubbleStartSize = 0, bubbleEndSize = 0;

private:
    void burst(float cutRotation);  // vf8's and vf18's
    void landDrops();
};

class SuperPoisonArea : public InjuryState {
public:
    SuperPoisonArea();  // 0x987bc
    ~SuperPoisonArea() override;  // 0x19a9c0
    void start() override;   // vf2 0x19abc0
    void vf3() override;     // vf3 0x19b070
    void vf4(float dt) override;  // vf4 0x19b524
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x19cde4
    HitResult injectionHitTest(const cocos2d::CCPoint& touch, int mode, float fill) override;  // vf9 0x19d3c8
    void completeStage() override;  // vf18 0x19caac
    void onFinished() override;     // vf20 (1.45 SuperPoisonArea::onHealed 0x2303d8): its stat
    void dragged(int, const cocos2d::CCPoint&) override {}  // vf22 0x19b06c
    void toolStopped(int) override;  // vf23 0x19d3b0
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x19ca54
    void pause() override;   // vf28 0x19ab38
    void resume() override;  // vf29 0x19ab7c

    int swipeSq = 500;           // +0xfda8
    cocos2d::CCPoint swipeStart; // +0xfdac
    ParticleSystem* soak1 = nullptr;     // +0xfdb4: soakable, a puff at its place
    ParticleSystem* passive1 = nullptr;  // +0xfdb8
    ParticleSystem* soak2 = nullptr;     // +0xfdbc: soakable, for ever while it lasts
    ParticleSystem* passive2 = nullptr;  // +0xfdc0
    ParticleSystem* soak3 = nullptr;     // +0xfdc4: its burst's
    ParticleSystem* passive3 = nullptr;  // +0xfdc8
    ParticleSystem* bubbles = nullptr;   // +0xfdcc
    bool swiping = false;        // +0xfdd0
    float scale = 1, fullScale = 1, minScale = 0.1f;  // +0xfdd4, +0xfdd8, +0xfddc
    float injectionDivisor = 3;  // +0xfde0
    float lastFill = -1;         // +0xfde4
    cocos2d::CCPoint bubblePosVar;  // +0xfde8
    float bubbleTotal = 0, bubbleLifeVar = 0, bubbleStartSize = 0, bubbleEndSize = 0;  // +0xfdf0..
    float burstAmount = 0.6f;    // +0xfe00
    float fe04 = 0.2f;           // +0xfe04
    float puffAmount = 0.2f;     // +0xfe08
    float soakRate = 0.2f, passiveRate = 0.6f;  // +0xfe0c, +0xfe10: its for-ever systems' emission
    float shrunkScale = 0.3f;    // +0xfe14: its picture once its veins are gone
    float veinFrames = 24;       // +0xfe18
    float veinLimit = 10;        // +0xfe1c
    float soakDrops = 4, passiveDrops = 8;  // +0xfe20, +0xfe24

private:
    void puff(float amount, const cocos2d::CCPoint& at);   // 0x19b1c4
    void burst(float amount, const cocos2d::CCPoint& at);  // 0x19b2b0
    void cutOpen(float cutRotation);  // vf8's and vf18's
    void landDrops(ParticleSystem* p, bool soakable, float delay);
};

// The ones chipped away with the chainsaw (their vf8, vf17 and vf4 are the
// same pattern): its points worn, its chips flying where touched.
class SuperPoisonScab : public InjuryState {
public:
    SuperPoisonScab();
    ~SuperPoisonScab() override;  // 0x19a8cc
    void start() override;  // vf2 0x19a3d8
    void vf4(float dt) override;  // vf4 0x19a434: its chips stop when not touched
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x19a454
    void onTouch(const cocos2d::CCPoint& touch) override;  // vf17 0x19a6fc

    ParticleSystem* chips = nullptr;  // +0xfda8: particle_skabFX
    int fdac = 75, fdb0 = 37;         // +0xfdac, +0xfdb0 (unused here)
    float minOpacity = 0.3f;          // +0xfdb4
    bool chipping = false;            // +0xfda5
};

// iOS 1.45's CementDrop (1.45 addresses after the 1.02 ones) also runs Le
// Flairground Fantastique's Barnacle (child 0x18: particle_barnacleFX, else
// as cement) and Brimstone (child 0x19: particle_brimstoneFX). A brimstone
// rains fire: a Fire where it is 6 s after it is made, then every 12 s, until
// its last (creamed) stage. Its chips don't make cuts where they land: every
// third frame one lands it leaves ash (a PassiveParticle drawn as its
// passive_sprite_ref, particleSprite_ash.png), and the chainsaw wears its
// points without ending a stage: past 30 landing frames it goes on to its
// next stage itself (the count starting again).
class CementDrop : public InjuryState {
public:
    CementDrop();            // (1.45 the factory's case 6, 0xb8838)
    ~CementDrop() override;  // 0x1baa1c (1.45 0x25b090)
    void start() override;   // vf2 0x1ba404 (1.45 onCreated 0x25a24c)
    void vf3() override;     // vf3 0x1ba4c0 (1.45 onAddedToScene 0x25a36c)
    // vf4 0x1ba4f0 (1.45 runLogic 0x25a3a8): its cement landing outside it
    // makes cuts; a brimstone's fire and ash
    void vf4(float dt) override;
    // vf6 0x1bb59c (1.45 advanceStage 0x25b620): the cement off; its new
    // stage's picture (1.45: moved here from vf8)
    void finishStage() override;
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x1bab14 (1.45 runInteraction 0x25b198)
    void onTouch(const cocos2d::CCPoint& touch) override;  // vf17 0x1bb3e8 (1.45 onHit 0x25b460)
    int toolFor(const cocos2d::CCPoint& touch) override;   // vf27: 1.45 getCurrentLegalTool 0x25be40
    // (the child classes 1.45 tells apart)
    bool isBrimstone() const;
    bool isBarnacle() const;

    ParticleSystem* dust = nullptr;    // +0xfda8 (1.45 +0x10258): particle_dustFX
    ParticleSystem* cement = nullptr;  // +0xfdac (1.45 +0x1025c): particle_cementFX (dead kept)
    int landingsForCut = 75;           // +0xfdb0 (1.45 +0x10260)
    int landings = 37;                 // +0xfdb4 (1.45 +0x10264)
    float minOpacity = 0.5f;           // +0xfdb8 (1.45 +0x10268)
    bool chipping = false;             // +0xfda5 (1.45 +0x10255)
    // (1.45) a brimstone's fire: its timer, its interval (0 until made)
    float fireTimer = 0;               // +0x1026c
    float fireEvery = 0;               // +0x10270
    int landingFrames = 0;             // +0x10274: frames its chips landed in, this stage
    float fireInterval = 12;           // +0x10278
    float firstFireEarly = 6;          // +0x1027c: the first fire this much sooner
    int framesForStage = 30;           // +0x10280: landing frames past which it goes on
    int ashEvery = 3;                  // +0x10284: landing frames an ash
};

class SpikeBase : public InjuryState {
public:
    SpikeBase();
    ~SpikeBase() override;  // 0x1dffc8
    void start() override;  // vf2 0x1dff2c
    void vf3() override;    // vf3 0x1dff90
    void vf4(float dt) override;  // vf4 0x1e0098
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x1e0584
    void onTouch(const cocos2d::CCPoint& touch) override;  // vf17 0x1e0e08

    ParticleSystem* chips = nullptr;  // +0xfda8: particle_cementFX (dead kept)
    int landingsForCut = 75;          // +0xfdac
    int landings = 37;                // +0xfdb0
    float fdb4 = 0.3f;                // +0xfdb4 (unused)
    bool chipping = false;            // +0xfda5
};

}  // namespace mt
