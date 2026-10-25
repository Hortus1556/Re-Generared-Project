// mt::Pygmy (iOS 1.45; base class Pygmy, 1.45's 0x27; 0x102b0 bytes): Le
// Flairground Fantastique's pygmy, the little islander living on the whale
// (Baby AWOOOOGAH). It wanders slowly about the play area, swaying up and
// down, and every 7.5 s, or at the area's sides, heads for a random place of
// the 400x200 middle box. It takes eight tools and is hurt by none for good:
// each makes it react once and walk on ("twitch"; the lighter sets it on
// fire and the tazer shocks it, after which it walks on burnt; the stapler
// leaves a staple in it). The cutter or the chainsaw slices it in two: it
// walks on as its left half, a PygmyHalf (its `extra_injuries`, 1.45's child
// 0x2a, the port's "PygmyHalf") as the right, both turning slowly; a half
// takes only the tongs. Whole, burnt or halved, the tongs take it to the
// tray, which finishes it (its advanceStage). Its own state (+0x3c) runs it:
// its file's stages (PygmyAlive, PygmyBurnt, the halves, PygmyInjured)
// only list the tools, it stays on its first.
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Animation;

class Pygmy : public InjuryState {
public:
    // its states (kStateNames' order: the Flairground's, appended,
    // SurgeryData.cpp; 1.45's own 0x5e..0x62, the same numbers)
    static constexpr int kAlive = 94, kLeftHalf = 95, kRightHalf = 96, kBurnt = 97, kInjured = 98;

    Pygmy();                       // 1.45 0xc37a8 (the factory's case 0x27, 0xb8838)
    void vf3() override;           // vf3: 1.45 onAddedToScene 0x4cae3c
    void vf4(float dt) override;   // vf4: 1.45 runLogic 0x4cb088
    void finishStage() override;   // vf6: 1.45 advanceStage 0x4cc560 (it is taken out)
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8: 1.45 runInteraction 0x4cb9c0
    void onTouch(const cocos2d::CCPoint& touch) override;  // vf17: 1.45 onHit 0x4cc564
    void completeStage() override;  // vf18: 1.45 autoCompleteStage 0x4cc0e4
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27: 1.45 getCurrentLegalTool 0x4cc574
    // (its vf2, 1.45 onCreated 0x4cae38, only jumps to the base's)

    // turned towards a random place of the middle box (1.45 lookAtMiddle 0x4cb8c8)
    void lookAtMiddle();

    bool touchedNow = false;     // +0x10255: a touch this frame (set by vf17, cleared each frame; read by nothing)
    bool burnt = false;          // +0x10256: the lighter or the tazer had it: it walks on burnt
    bool cut = false;            // +0x10257: sliced (or made a right half)
    bool rightHalf = false;      // +0x10258
    bool splitPending = false;   // +0x10259: its right half to make once the cut has played
    float boxWidth = 400, boxHeight = 200;  // +0x1025c, +0x10260: the middle box (× the canvas scale)
    float step = 0.2f;           // +0x10264: canvas points a frame
    float turnEvery = 7.5f;      // +0x10268: seconds between heading for the middle
    float sway = 0.2f;           // +0x1026c: canvas points a frame up or down at most
    float swayTime = 1.5f;       // +0x10270: seconds from one end of the sway to the middle
    float tilt = 10;             // +0x10274: degrees either way it stands turned
    float spin = 0.08f;          // +0x10278: degrees a frame a half turns at most
    float swayClock = 1;         // +0x1027c: -swayTime..swayTime
    bool swayingUp = true;       // +0x10280
    float heading = 0;           // +0x10284: degrees, 0 right, anticlockwise
    cocos2d::CCRect bounds;      // +0x10288: the play area, cocos points
    cocos2d::CCRect box;         // +0x10298: the middle box
    float turnTimer = 0;         // +0x102a8
    float spinSpeed = 0;         // +0x102ac: a half's turn a frame

private:
    // what moves it: its animation, or (the port's) the sprite the tongs
    // put it in (creature::grabPiece) once they have had it
    cocos2d::CCNode* mover(Animation* a);
    // carried by the tongs (the port's: the original walked on under the
    // finger, the screen putting it back there each frame)
    bool carried() const;
    // the cut played: the right half made beside it (the event 2 of 1.45's
    // runLogic and autoCompleteStage)
    void split(Animation* a);
    void setState(int s);
};

}  // namespace mt
