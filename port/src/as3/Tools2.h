// The surgery tools with small classes of their own (ToolState.h has the
// base): the pizza cutter, the chainsaw, the vacuum, the tazer and the
// scanner. Each is made by ToolState::create (0x65318), which tunes it by the
// upgrades owned. The injection (Injection.h) and the clamps
// (ClampsController.h) have their own files.
#pragma once

#include "as3/ToolState.h"

namespace mt {

// The pizza cutter (vtable 0x415310; 0x520 bytes; ctor 0x75954): cuts along
// an injury's hit areas; its loop sounds while it is dragged.
class Cutter : public ToolState {
public:
    Cutter() { zOrder = 3; }
    void start() override { active = true; }               // vf9 0x66940
    void moved(const cocos2d::CCPoint&) override;           // vf3 0x66948: "pizza_cutter_loop"
    void stop() override;                                   // vf10 0x66a80
    void misuse(const cocos2d::CCPoint& touch) override;    // vf8 0x66aa0: a damage effect
    void setEfficiency(float e) override;                   // vf11 0x66d50
};

// The chainsaw (vtable 0x4154a0; 0x560 bytes; ctor 0x755d4). Moved too fast
// it gets away (lose control): the patient is hurt, it is let go and slides
// on by itself for a second, its trail following it (the screen's cursor
// follows the tool while `followTool`).
class Chainsaw : public ToolState {
public:
    Chainsaw();
    void begin(const cocos2d::CCPoint& touch) override;     // vf2 0x6ad88 (no cooldown)
    void end(const cocos2d::CCPoint&) override { stop(); }  // vf4 0x6aee4
    void update(float dt) override;                         // vf5 0x6aeec: the slide
    void frame(float dt) override;                          // vf6 0x6af50: too fast?
    void setPosition(const cocos2d::CCPoint& p) override;   // vf7 0x6ae58
    void misuse(const cocos2d::CCPoint& touch) override;    // vf8 0x6b7bc: a damage effect
    void start() override;                                  // vf9 0x6aa54
    void stop() override;                                   // vf10 0x6ac38
    void setEfficiency(float e) override;                   // vf11 0x6ae34

    float maxSpeed0 = 7000;       // +0x520: canvas points² a second (squared distance over time)
    float f524 = 0.8f, f528 = 0.9f, f52c = 1.0f;  // +0x524..+0x52c (unused)
    float warmUp0 = 0.25f;        // +0x530
    float warmUpTime = 0.25f;     // +0x534: held this long before it can get away
    bool slipping = false;        // +0x538
    cocos2d::CCPoint slideStep;   // +0x53c: a frame's slide
    cocos2d::CCPoint last;        // +0x544: the last position, on the canvas
    float slideTime = 0;          // +0x54c
    float slideDuration = 1;      // +0x550
    float warmUp = 0.25f;         // +0x554
    float maxSpeed = 7000;        // +0x558: 60% without the chainsaw upgrade
    float heldTime = 0;           // +0x55c

private:
    void loseControl();  // 0x6b080
};

// The vacuum (vtable 0x4154f0; 0x524 bytes; ctor 0x754ec): sucks up the
// particles and creatures that take it (their classes read its suction).
class Vacuum : public ToolState {
public:
    Vacuum() { zOrder = 0; }
    void start() override;                                  // vf9 0x6de58
    void stop() override;                                   // vf10 0x6e03c
    void setEfficiency(float e) override { suction *= e; }  // vf11 0x6e188
    float suction = 4000;  // +0x520 (half without the vacuum upgrade)
};

// The tazer (vtable 0x4153b0; 0x520 bytes; ctor 0x75874): a zap a press (it
// lets go on its first frame), the electricity shown wherever it is pressed.
class Tazer : public ToolState {
public:
    Tazer() { zOrder = 6; }
    void begin(const cocos2d::CCPoint& touch) override;     // vf2 0x67f8c
    void frame(float) override { stop(); }                  // vf6 0x68268
    void misuse(const cocos2d::CCPoint& touch) override;    // vf8 0x68270: a burn
    void start() override;                                  // vf9 0x67e58
    void setEfficiency(float e) override;                   // vf11 0x68244
};

// The scanner (vtable 0x4151a0; 0x53c bytes; ctor 0x6c1a4). Held, every
// `interval` the screen (0x39104, SurgeryScreen::scan) shows on its effect
// (ScannerEffect) how near the hidden injuries (state Hidden) are; with the
// arrow upgrade an arrow on the tool points at the nearest, with the global
// upgrade each one is shown.
class Scanner : public ToolState {
public:
    Scanner();
    void frame(float) override {}                           // vf6 0x6c2dc
    void misuse(const cocos2d::CCPoint&) override {}        // vf8 0x6c304
    void stop() override;                                   // vf10 0x6c334
    void setEfficiency(float e) override;                   // vf11 0x6c2e0

    // a blip's colour for a squared distance (0x6bee4: ScannerEffect's)
    int colourFor(float distSq) const;
    // its sound (0x6bf18)
    const char* soundFor(float distSq) const;
    // the arrow (made the first time) turned to `target` from `from` (0x6c390)
    void pointArrow(const cocos2d::CCPoint& from, const cocos2d::CCPoint& target);
    void removeArrow();  // 0x6c36c

    float range = 0;         // +0x520: 75 canvas points
    float rangeSq = 0;       // +0x524: beyond, red
    float nearSq = 0;        // +0x528: within (25 canvas points), green
    float interval = 0.9f;   // +0x52c: between scans (0.5 with the cooldown upgrade)
    float interval2 = 0.9f;  // +0x530
    cocos2d::CCSprite* arrow = nullptr;  // +0x538
};

// a misuse's damage effect at most every 3 whole seconds of time() (0x652ac);
// the tools' shared check
bool toolMisuseDue(ToolState* t);
// a damage effect (event 3, kind 0) on a tool's queue
void postToolDamageEffect(ToolState* t);
// a touch on the 640x384 canvas (the tools' sums: p / scale + the margin)
cocos2d::CCPoint toolCanvasPoint(const cocos2d::CCPoint& p);

}  // namespace mt
