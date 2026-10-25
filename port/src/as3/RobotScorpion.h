// mt::RobotScorpion (vtable 0x417e30; base class 16; 0xfe44 bytes): a robot
// scorpion that comes up out of the patient at a random place of the play
// area (RobotScorpionEmerging, leaving a LargeCutRobot), dashes across it
// three times (Dashing: a MoveTo of a quarter second, the "dash" animation),
// turning towards each next place in between (Turning, 400 degrees a
// second), then charges its tail and zaps the patient (Firing: the
// electricity, a shock, 6 damage and a Rust where it stands), and dashes on.
// The tazer stuns it (Tazered, 3 seconds; each zap again gives 3 more):
// let go, it burrows (Burying, another LargeCutRobot) and goes about under
// the skin (Hidden: a SmallCutRobot and the damage effect where it was every
// 5.5 seconds); the cutter brings it up again. Stunned, a clamp on its tail
// holds it (Clamped, 6 seconds): tightened fully, the chainsaw cuts it (150
// frames of sawing) and it explodes (the explosion, three RoborpionShards
// about it to take out with the tongs), finished at once (Dead). Its own
// state (+0x3c) runs it; the stages of its file (Emerging, Hidden, Tazered,
// Dead) give the tools.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class ParticleSystem;

class RobotScorpion : public InjuryState {
public:
    RobotScorpion();                // the factory's case 16 (0x994ec)
    ~RobotScorpion() override;      // 0x15ef6c
    void start() override;          // vf2 0x15f4f4
    void vf3() override;            // vf3 0x15f5e0
    void vf4(float dt) override;    // vf4 0x15f874
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x16210c
    void completeStage() override;  // vf18 0x161980
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x1618c0

    // the play area less the margin on each side, cocos points (the
    // arithmetic of 0x1611b0, 0x1617b0, 0x161c80)
    cocos2d::CCRect playArea() const;
    // a random place in it (0x1617b0)
    cocos2d::CCPoint randomPlace() const;
    // its next place from where it is: from the bottom left quarter
    // anywhere in the top half, from the top left the right half, from the
    // right the bottom left quarter; its heading and the turn's time
    // (0x1611b0)
    cocos2d::CCPoint nextTarget();
    // somewhere else: put at a random place, turned towards its next one
    // (0x15f670)
    void relocate();
    // it comes up: its sound, turned to its next place, its hole, "emerge",
    // the dirt (0x15f6d0)
    void emerge();
    // the dirt and the shards of its burrowing (0x1613f4)
    void startBurrowFX();
    // its systems at its animation's origin and turn (the port's: see vf3)
    void placeFX();
    // a LargeCutRobot `distance` in front of it (0x16143c)
    void postHole(float distance);
    // the damage effect and the scream, from under the skin (0x15f090)
    void hurtFromUnder();
    // a RoborpionShard<n> 50 canvas points off on a diagonal, turned ±30°,
    // if that is in the play area (0x161c80)
    void postShard(int n);
    // stunned by the tazer (vf8's)
    void tazered(HitResult& r);
    // it burrows (vf4's Tazered and Clamped ends)
    void burrow();
    // blown apart (vf18's body): the explosion, its shards, Dead, finished
    void explode();
    // the clamp on its tail (its clamp hit area's), nullptr if none
    struct Clamp* tailClamp();

    struct Clamp* clamp = nullptr;  // +0xfda8: the clamp on its tail
    float marginBase = 40;     // +0xfdac
    float emergeHoleBase = 0;  // +0xfdb0
    float burrowHoleBase = 18; // +0xfdb4
    float margin = 0;          // +0xfdb8: × the canvas scale (start)
    float emergeHole = 0;      // +0xfdbc
    float burrowHole = 0;      // +0xfdc0
    float tazerTime = 3;       // +0xfdc4: stunned
    float clampTime = 6;       // +0xfdc8: held by a clamp
    float chargeTime = 1.3f;   // +0xfdcc: charging before the zap
    float dashTime = 0.25f;    // +0xfdd0
    float turnSpeed = 400;     // +0xfdd4: degrees a second
    float dashes = 4;          // +0xfdd8: dashes left after a zap: this less one
    float zapDamage = 6;       // +0xfddc
    float hiddenTime = 5.5f;   // +0xfde0: under the skin between its cuts
    float hurtEvery = 1;       // +0xfde4
    float hurtAmount = 0.7f;   // +0xfde8 (whole points in the event: none)
    cocos2d::CCPoint home;     // +0xfdec: where it was put last
    cocos2d::CCPoint target;   // +0xfdf4: its next place
    float hiddenTimer = 0;     // +0xfdfc
    float hurtTimer = 0;       // +0xfe00
    float holdTimer = 0;       // +0xfe04: stunned or clamped
    float chargeTimer = 0;     // +0xfe08
    float openness = 0;        // +0xfe18: the clamp's, last frame (1 open)
    float heading = 0;         // +0xfe1c: towards its next place
    float turnTimer = 0;       // +0xfe20
    int dashesLeft = 0;        // +0xfe28
    int sawHits = 150;         // +0xfe2c: frames of the chainsaw on it, tightly clamped
    int sawPopupEvery = 15;    // +0xfe30
    int zapPhase = 0;          // +0xfe34: 1 charging, 2 zapped
    ParticleSystem* chargeFX = nullptr;  // +0xfe38 (particle_scorpionChargeFX)
    ParticleSystem* burrowFX = nullptr;  // +0xfe3c (particle_scorpionBurrowFX)
    ParticleSystem* shardsFX = nullptr;  // +0xfe40 (particle_scorpionShardsFX)
    bool dashing = false;      // +5: not to be pointed at
    // (the port's) the unused "swoosh" of its animation file played over it
    // as it dashes: a second animation of the same file, beside its own
    class Animation* swoosh = nullptr;
    void showSwoosh();
};

}  // namespace mt
