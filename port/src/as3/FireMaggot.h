// mt::FireMaggot (vtable 0x417b10; base class 7; 0xfe10 bytes): a fire
// maggot crawling about (FireMaggotCalmed), turning towards its next place
// (58 canvas points away) at 90 degrees a second; every 6 to 8 seconds it
// breathes fire for a second (a Fire injury in front of it). The vacuum
// sucks its flame out for 5 seconds; then a clamp holds it (Clamped: 5
// seconds to tighten, 8 more once tight). The tazer makes it angry
// (FireMaggotAngry: 5 seconds of fire breath, then it burrows, leaving a
// SnakeCut, and goes about under the skin for 13 seconds hurting the patient
// where it stops: Hidden; the cutter brings it up). Three tazerings kill it
// (FireMaggotDead), to be taken out with the tongs. Its breath, tail and
// fire are particle systems on its animation's control points.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class ParticleSystem;

class FireMaggot : public InjuryState {
public:
    FireMaggot();                  // the factory's case 7 (0x98e60)
    ~FireMaggot() override;        // 0x13b7a4
    void start() override;         // vf2 0x13b8c4
    void vf3() override;           // vf3 0x13b9d4
    void vf4(float dt) override;   // vf4 0x13bc98
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x13e3c8
    void completeStage() override;  // vf18 0x13e15c
    void setRotation(float degrees) override;  // vf21 0x13e88c
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x13e328

    void newTarget();              // 0x13bb98
    void resurface();              // 0x13d45c
    void hurtFromUnder();          // 0x13da64
    void tazered(bool clampTight);  // 0x13e570
    void die();
    void crawl(float dt);          // vf4's end: turning, then moving
    float breathDelay();
    // the fire in front of it
    void postFire();

    int unused0 = 3;           // +0xfda8
    float angryTime = 5;       // +0xfdac
    float underTime = 13;      // +0xfdb0
    float clampTime = 5;       // +0xfdb4
    float clampHold = 8;       // +0xfdb8
    float breathEvery = 7, breathVariance = 1;  // +0xfdbc, +0xfdc0
    float breathTime = 1;      // +0xfdc4
    float vacuumTime = 5;      // +0xfdc8
    int lives = 3;             // +0xfdcc
    cocos2d::CCPoint dir;      // +0xfdd0
    cocos2d::CCPoint target;   // +0xfdd8
    float angle = 0;           // +0xfde0
    float timer = 0;           // +0xfde4
    float clampTimer = 0;      // +0xfde8
    float breathTimer = 0;     // +0xfdec
    float vacuumTimer = 0;     // +0xfdf0
    bool clampDone = false;    // +0xfdf4
    ParticleSystem* breath = nullptr;  // +0xfdf8 (particle_maggotFX, at its mouth)
    ParticleSystem* tail = nullptr;    // +0xfdfc (particle_maggotailFX)
    ParticleSystem* fire = nullptr;    // +0xfe00 (particle_maggotFX, its breath of fire)
    float breathRate = 0, tailRate = 0;  // +0xfe04, +0xfe08: their emission rates
    bool vacuumed = false;     // +0xfe0c
    bool breathing = false;    // +0xfe0d
};

}  // namespace mt
