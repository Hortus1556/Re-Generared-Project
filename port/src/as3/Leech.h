// mt::Leech (vtable 0x418540; base class 11; 0xfdfc bytes): a leech sucking
// at the patient (Sucking), growing (from 0.6 of its size, 0.00075 a frame)
// until at 1.25 it bursts: a PoisonArea, poison particles and 5 damage. A
// cutting tool on it (stapler, cutter, tazer, chainsaw) bursts it at once.
// The injection shrinks it (by an eighth of what the syringe gives, to 0.2
// at least). The lighter burns it ("burnt": it lets go of its teeth); the
// tongs on its head stretch it away from the patient, and pulled 200 points
// times its size it comes off (unburnt, leaving its teeth, LeechTeeth),
// dangling in the tongs, to be dropped on the tray.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class ParticleSystem;

class Leech : public InjuryState {
public:
    Leech();                       // the factory's case 11 (0x998f8)
    ~Leech() override;             // 0x17cb38
    void start() override;         // vf2 0x17bb90
    void vf3() override;           // vf3 0x17bc48
    void vf4(float dt) override;   // vf4 0x17bcf0
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x17ccc0
    HitResult injectionHitTest(const cocos2d::CCPoint& touch, int mode, float fill) override;  // vf9 0x17d77c
    void completeStage() override;  // vf18 0x17ccac
    void onFinished() override;     // vf20 0x17da54
    void toolStopped(int tool) override;  // vf23 0x17d85c
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x17cc08

    // it bursts (0x17c624)
    void burst();
    // its teeth left in the patient (0x17c28c)
    void leaveTeeth();
    // back to sucking, let go of (0x17d87c)
    void backToSucking();
    // its rect and its head's for its size (vf2, vf4)
    void sizeRects();

    float scale = 0.6f;          // +0xfda8
    float growth = 0.00075f;     // +0xfdac: a frame
    float unused0 = 0.4f, unused1 = 0.6f, unused2 = 0.8f;  // +0xfdb0..+0xfdb8
    float burstScale = 1.25f;    // +0xfdbc
    float minScale = 0.2f;       // +0xfdc0: what the injection leaves of it
    float injectionDivisor = 8;  // +0xfdc4
    float lastFill = -1;         // +0xfdc8
    int stretchFrames = 18;      // +0xfdcc: of "stretch"
    int pullDistance = 0;        // +0xfdd0: cocos points (200 canvas points) at its full size
    float pullPerFrame = 0;      // +0xfdd4
    bool stretching = false;     // +0xfdd8
    bool dangling = false;       // +0xfdd9
    cocos2d::CCRect headRect;    // +0xfddc: where the tongs take it
    int rotation0 = 0;           // +0xfdec
    bool hasTeeth = true;        // +0xfdf0: its teeth not yet left
    ParticleSystem* poison = nullptr;  // +0xfdf4
    float stretchUnit = 0;       // +0xfdf8: 15 canvas points
    bool burnt = false;          // +0xfda5
    bool bursting = false;       // +6
};

}  // namespace mt
