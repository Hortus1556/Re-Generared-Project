// mt::GiantWorm (vtable 0x41b9a0; base class 38; 0xfe28 bytes, made in the
// factory itself): the toxic worm, the last surgery's boss
// (surgery_bunker_hubris2's ninth stage). It lives under the skin
// (GiantWormUnderSkin) and every 12 seconds comes up through the patient in
// a row of loops (GiantWormMoving): its body four times its animation
// (common/anim_creature_toxic_worm_anim), each coming out of one hole and
// into the next ("burrow_start", then "burrow_loop"), placed one after the
// other across a box of the canvas, top to bottom or bottom to top at
// random, each hole leaving a WormCut. The last is its head: it looks around
// (GiantWormLookingAround), then spits at the patient
// (GiantWormSpittingAtPatient: a PoisonArea and a Fire, or a SuperPoisonArea
// at its last health) and goes back under, its loops sinking from the tail
// (GiantWormGoingUnder). The chainsaw on its head while it looks around
// hurts it (GiantWormReact): it spits at the screen (GiantWormSpittingAtScreen:
// the tools frozen and poisoned 5 s, a green wash), spits at the patient and
// goes under. At its third cut it dies (GiantWormDead): where it lay, a
// GiantWormIncision at its head (its front comes out with the tongs) and a
// GiantWormLoopIncision at each loop (cut open, a mini worm crawls out of
// each).
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Animation;
class ParticleSystem;

class GiantWorm : public InjuryState {
public:
    GiantWorm();                    // the factory's case 38 (in 0x908c4)
    ~GiantWorm() override;          // 0x26859c
    void start() override;          // vf2 0x2681c8
    void vf3() override;            // vf3 0x26843c
    void vf4(float dt) override;    // vf4 0x26908c
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x26b778
    void completeStage() override;  // vf18 0x26b3a0
    int toolFor(const cocos2d::CCPoint&) override { return kChainsaw; }  // vf27 0x26b39c
    // (vf23 0x269088: nothing; vf19 0x268698, its place: its current loop's,
    // which vf4 keeps in `position`)

    // its rect about its place (vf31 0x26903c)
    void sizeRects();
    // its loop i (0 the tail end, `segments` its head), or nullptr
    Animation* loop(int i);
    // the loops past the stage's one, each a toxic worm animation at its
    // place (0x268290)
    void addLoops(int count);
    // every loop turned round (0x268ebc)
    void flipAll();
    // where loop k comes up: along the box from one corner, its z order by
    // its turn (0x268f08)
    cocos2d::CCPoint placeFor(Animation* node, int k);
    // a WormCut where a loop goes in (1) or comes out (2) (0x269ac8)
    void leaveCut(Animation* node, int kind);
    // the spit drawn back in, at its mouth (0x269ed0)
    void spitIn();
    // its spit at the patient (0x269fc0)
    void spitAtPatient();
    // its spit at the screen (0x26ac10)
    void spitAtScreen();
    // what it leaves dead: its incisions (0x2686e0)
    void leaveRemains();
    // its death, the head unturned (vf8, vf18)
    void die(Animation* head);
    // (the port's, debug) "creature #<id> GiantWorm state <n>" as it changes
    void traceState();

    ParticleSystem* spitInFx = nullptr;   // +0xfda8: particle_wormspitinFX
    ParticleSystem* spitOutFx = nullptr;  // +0xfdac: particle_wormspitoutFX
    cocos2d::CCRect area;      // +0xfdb0: the play area (vf2), then the box its loops come up in (vf3, canvas points)
    int flipY = 1;             // +0xfdd0: 1 from the top, -1 from the bottom
    int flipX = -1;            // +0xfdd4 (always -1: the original's second random is not used)
    int topZ = 0;              // +0xfdd8: its top layer's z order
    int current = 1;           // +0xfddc: the loop it is at, 1-based
    int segments = 3;          // +0xfde0: the loops before its head
    int unused0 = 3;           // +0xfde4
    int spitAtPatientFrame = 12;  // +0xfde8
    bool spatAtPatient = false;   // +0xfdec
    float underTime = 12;      // +0xfdf0
    float underTimer = 0;      // +0xfdf4
    float hitTime = 1;         // +0xfdf8: copied to +0xfe0c, not used
    float maxHealth = 3;       // +0xfdfc: chainsaw cuts
    int spitInFrame = 8;       // +0xfe00
    bool spatIn = false;       // +0xfe04
    int health = 3;            // +0xfe08
    float hitTimer = 0;        // +0xfe0c
    bool reacted = false;      // +0xfe10
    int spitAtScreenFrame = 12;  // +0xfe14
    bool spatAtScreen = false;   // +0xfe18
    int exitCutFrame = 11;     // +0xfe1c: a loop's way out (its "burrow_start")
    int exitCutFrame2 = 23;    // +0xfe20: the head's way out (its "spit_at_patient")
    bool exitCut = false;      // +0xfe24
    bool dead = false;         // +0xfe25
    // +5: not to be pointed at (the tutorial's and the partners' pick):
    // only while its head is up (looking around, spitting)
    bool hiddenFromPick = true;
    int tracedState = -1;
};

}  // namespace mt
