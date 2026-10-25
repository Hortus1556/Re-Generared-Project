// mt::IceAnt (vtable 0x417bb0; base class 27; 0xfe14 bytes): an ice ant
// scurrying about the play area (IceAntWalking), turning a little now and
// then and back towards the middle at the area's sides; every 13 seconds or
// so it bites (IceAntBite: a FrostBite where it is and 1 damage). The tazer
// kills it (IceAntTazer, then IceAntDead), and dead it is taken out with the
// tongs. Its own state (+0x3c) runs it: its file's first stage
// ("IceAntWalk") is no state the game knows.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class IceAnt : public InjuryState {
public:
    IceAnt();                      // the factory's case 27 (0x98ca4)
    void start() override;         // vf2 0x144178
    void vf3() override;           // vf3 0x144478
    void vf4(float dt) override;   // vf4 0x144d40
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x145364
    void completeStage() override;  // vf18 0x14530c
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x1452ac

    // turned towards a random place of the middle box (0x1444d0)
    void turnToMiddle();
    // its bite (0x1445dc)
    void bite();
    // a new way to turn: either way at first, then -1, 0 or 1
    void newTurn();

    float biteEvery = 13;      // +0xfda8
    float unused0 = 4;         // +0xfdac
    float step = 3;            // +0xfdb0: cocos points a frame
    float boxWidth = 400, boxHeight = 200;  // +0xfdb4, +0xfdb8: the middle it heads back to
    float unused1 = 0, unused2 = 1, unused3 = 8;  // +0xfdbc..+0xfdc4
    float biteDamage = 1;      // +0xfdc8
    float turnEvery = 3;       // +0xfdcc
    float turnSpeed = 1.25f;   // +0xfdd0: degrees a frame
    bool dead = false;         // +0xfdd4
    cocos2d::CCRect bounds;    // +0xfddc: the play area, cocos points
    cocos2d::CCRect box;       // +0xfdec: the middle box
    float turnTimer = 0;       // +0xfdfc
    float turnDir = 0;         // +0xfe00
    float clock0 = 0, clock1 = 0;  // +0xfe04, +0xfe08 (counted, not used)
    float biteTimer = 0;       // +0xfe0c
    float unusedTimer = 0;     // +0xfe10
};

}  // namespace mt
