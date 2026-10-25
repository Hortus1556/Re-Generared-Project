// mt::MiniWorm (vtable 0x41b8c0; base class 37; 0xfe38 bytes): a little
// worm crawling about the patient (MiniWormMove), turning a little now and
// then and back towards the middle at the play area's sides; every 4 to 8
// seconds it turns to the middle and bites (MiniWormBite: a SmallCut a third
// of its length ahead of it and 1 damage). Two ways to take it out with the
// tongs:
// - the tazer shocks it for 4 s (MiniWormShocked), and meanwhile the tongs
//   on its body take it (MiniWormDangling), to be dropped on the tray;
// - the tongs on its tail make it lunge and bite deeper (MiniWormLatched):
//   pulled away it stretches, and 100 canvas points out its head tears
//   free, leaving a WormCut and 1 damage, dangling from the tongs.
// Let go of, it crawls on. Its own state (+0x3c) runs it: its file's stage
// ("MiniWormAlive") is no state it uses.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Animation;

class MiniWorm : public InjuryState {
public:
    MiniWorm();                     // the factory's case 37 (0x9857c)
    void start() override;          // vf2 0x25da18
    void vf3() override;            // vf3 0x25dd30
    void vf4(float dt) override;    // vf4 0x25f4b8
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x25fa30
    void completeStage() override;  // vf18 0x25dbe8
    void toolStopped(int tool) override;  // vf23 0x25f160
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x25f984

    // its rect and its tail's (vf31 0x25f088: its own slot, not the cuts'
    // onStaple)
    void sizeRects();
    // pulled off the patient: a WormCut where its head bit, 1 damage
    // (vf32 0x25df00)
    void tearOff();
    // turned towards a random place of the middle box (0x25ddf0)
    void turnToBox();
    // its bite: a SmallCut ahead of it, 1 damage (0x25e7d8)
    void bite();
    // let go of: it crawls on (0x25f180)
    void release();
    // the tongs on its tail: it lunges and bites deeper (0x25f368)
    void lunge();
    // a new way to turn: either way at first, then -1, 0 or 1
    void newTurn();
    // taken by the tongs, dangling by its tail (vf8's two ways)
    void takeInTongs(HitResult& r);
    // (the port's) its animation back out of the tongs' sprite (grabPiece)
    // once they let go of it, where they left it
    void unwrap();
    // (the port's, debug) "creature #<id> MiniWorm state <n>" as it changes
    void traceState();

    float biteEvery = 8;        // +0xfda8
    float unused0 = 2.5f;       // +0xfdac: a timer's span (+0xfe34), not used
    float step = 1.25f;         // +0xfdb0: cocos points a frame
    float boxWidth = 400, boxHeight = 200;  // +0xfdb4, +0xfdb8: the middle it heads back to
    float unused1 = 0;          // +0xfdbc
    float biteDamage = 1;       // +0xfdc0 (the bite's and the tear's)
    float turnEvery = 2.5f;     // +0xfdc4
    float turnSpeed = 1.75f;    // +0xfdc8: degrees a frame
    float shockTime = 4;        // +0xfdcc
    bool latched = false;       // +0xfdd0: held by its tail, biting on
    float stretchUnit = 0;      // +0xfdd4: 15 canvas points
    int rotation0 = 0;          // +0xfdd8
    cocos2d::CCRect tailRect;   // +0xfddc: where the tongs take it by the tail (its head goes first)
    int stretchFrames = 12;     // +0xfdec: of "mini_worm_streched_end"
    int pullDistance = 0;       // +0xfdf0: cocos points (100 canvas points)
    float pullPerFrame = 0;     // +0xfdf4
    bool dead = false;          // +0xfdf8 (never set: the original's checks kept)
    cocos2d::CCRect bounds;     // +0xfe00: the play area, cocos points
    cocos2d::CCRect box;        // +0xfe10: the middle box
    bool shocked = false;       // +0xfe20: the tongs can take it by the body
    bool lunging = false;       // +0xfe21
    float turnTimer = 0;        // +0xfe24
    float turnDir = 0;          // +0xfe28
    float shockTimer = 0;       // +0xfe2c
    float biteTimer = 0;        // +0xfe30
    float unusedTimer = 0;      // +0xfe34
    int tracedState = -1;
};

}  // namespace mt
