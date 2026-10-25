// The broken limbs and the bleeding vessels:
// - BrokenBone (vtable 0x418b50; base 22): a bone broken in two halves,
//   each turned back into line with the tongs (Broken), then burnt and
//   creamed;
// - BrokenVein (vtable 0x418bf0; base 23): a vein broken in two, its halves
//   swaying and spurting until clamped at their ends (the clamps follow
//   them), then turned into line with the tongs;
// - Artery (vtable 0x418c90; base 1): spurting until clamped shut (the
//   clamp's tightness stems it), then burnt and creamed; a clamp taken off
//   before it is healed opens it again.
// The clamps are the clamps tool's (ClampsController.h): InjuryState::clamps,
// HitPointState::clamp, removeClamp (vf24).
#pragma once

#include "as3/InjuryState.h"
#include "as3/ParticleSystem.h"

namespace mt {

class BrokenBone : public InjuryState {
public:
    BrokenBone();  // (0x908c4 case 0x16)
    void start() override;  // vf2 0x1a5520 (nothing)
    void vf3() override;    // vf3 0x1a5524: its halves
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x1a59c0
    void completeStage() override;  // vf18 0x1a5874
    void toolStopped(int) override { fdb8 = -1; }  // vf23 0x1a5864
    void placeNodes() override;  // its halves' node at its place

    float target1 = 0, target2 = 180;  // +0xfda8, +0xfdac: the halves' turns in line
    float tolerance = 2;      // +0xfdb0
    float scatter = 25;       // +0xfdb4: how far off line they start (and can be turned)
    float fdb8 = -1;          // +0xfdb8
    cocos2d::CCNode* halves = nullptr;  // +0xfdbc: the two halves (sprites 0 and 1), turned with it
};

class BrokenVein : public InjuryState {
public:
    BrokenVein();  // 0x9972c
    ~BrokenVein() override;  // 0x1ab944
    void start() override;  // vf2 0x1aba3c: its two spurts
    void vf3() override;    // vf3 0x1abeb0: its halves, the spurts at their ends
    void vf4(float dt) override;  // vf4 0x1ac5a8: the clamps stem them; the halves sway
    void update(float dt) override;  // vf5 0x1ac4c8: its damage, less the tighter the clamps
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x1add98
    void completeStage() override;  // vf18 0x1abca8
    void toolStopped(int) override;  // vf23 0x1ac580
    void removeClamp(struct Clamp* clamp) override;  // vf24 0x1abb58
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x1ada58
    void pause() override;   // vf28 0x1ac478
    void resume() override;  // vf29 0x1ac4a0
    void placeNodes() override;

    float emission = 5000, total = 180, speed = 700, minSpeed = 350;  // +0xfdac..+0xfdb8: a spurt's when open
    ParticleSystem* spurt1 = nullptr;  // +0xfdbc: passive_fx_ref
    ParticleSystem* spurt2 = nullptr;  // +0xfdc0
    float open1 = -1, open2 = -1;      // +0xfdc4, +0xfdc8: last frame's (1 open .. 0 clamped shut)
    float target1 = 0, target2 = 180;  // +0xfdcc, +0xfdd0
    bool swayDir1 = false, swayDir2 = false;  // +0xfdd4, +0xfdd5
    float swayTimer1 = 0.08f;  // +0xfdd8
    float swayEvery = 0.08f;   // +0xfddc
    float swayTimer2 = 0.08f;  // +0xfde0
    float fde4 = 0;            // +0xfde4
    bool held1 = false, held2 = false;  // +0xfde8, +0xfde9: in the tongs this frame
    float fdec = 10;           // +0xfdec
    float tolerance = 3;       // +0xfdf0
    float range = 15;          // +0xfdf4: how far off line they start (and can be turned)
    float lastAngle = -1;      // +0xfdf8
    cocos2d::CCNode* halves = nullptr;  // +0xfdfc
    struct Clamp* clamp1 = nullptr;  // +0xfe00
    struct Clamp* clamp2 = nullptr;  // +0xfe04
    bool bothClamped = false;  // +0xfe08
    int hurt = 2;              // +0xfe0c: a clamp taken off
    bool popup1 = false, popup2 = false;  // +0xfe10, +0xfe11

private:
    // a half's spurt at its end, and its clamp point there (vf4)
    void stem(ParticleSystem* p, float open);
    float sway(cocos2d::CCSprite* half, bool closed, bool held, float& timer, bool& dir, float open, float target,
               float dt);
};

class Artery : public InjuryState {
public:
    Artery();  // (0x908c4 case 1)
    ~Artery() override;  // 0x1b3ea8
    void start() override;  // vf2 0x1b404c: its spurt
    void vf3() override;    // vf3 0x1b40ec
    void vf4(float dt) override;  // vf4 0x1b4374: the clamp stems it
    void update(float dt) override;  // vf5 0x1b3c94: its damage, less the tighter the clamp
    void completeStage() override;  // vf18 0x1b3d34
    void onFinished() override;  // vf20 0x1b4344: healed
    void removeClamp(struct Clamp* clamp) override;  // vf24 0x1b3f78
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x1b411c
    void pause() override;   // vf28 0x1b3d14
    void resume() override;  // vf29 0x1b3d24

    float emission = 8000;   // +0xfdac
    float total = 1055;      // +0xfdb0
    float fdb4 = 1100, fdb8 = 350;  // +0xfdb4, +0xfdb8
    float speed = 1300;      // +0xfdbc
    float minSpeed = 125;    // +0xfdc0
    float fdc4 = 3;          // +0xfdc4
    ParticleSystem* spurt = nullptr;  // +0xfdc8: particle_arteryFX
    int hurt = 2;            // +0xfdcc: a clamp taken off too soon
    bool popupGiven = false; // +0xfd9c
};

}  // namespace mt
