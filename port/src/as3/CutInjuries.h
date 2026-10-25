// The cuts and wounds (re/as3/notes/surgery.md, "The injuries"). Most of
// them are the base InjuryState as it is: their stages, the tools each takes
// and the pictures are their definitions' (SmallCut and its robot and worm
// cuts, Burn and Rust, IncisionRobot's and LargeCutRobot's own classes are
// plain injuries: the factory 0x908c4 makes them with the base's vtable and
// no tuning). Three have code of their own:
// - Incision (vtable 0x4157e0; base 9): drawn over the other injuries (z
//   101), its wound made when it starts, the cut's sound when it is done;
// - LargeCut (vtable 0x4156a0; base 10) and SnakeCut (vtable 0x415740; base
//   32): an open cut is closed with the stapler, a staple shown where each
//   press lands on the cut's line (vf31).
#pragma once

#include "as3/InjuryState.h"

namespace mt {

class Incision : public InjuryState {
public:
    // (0x908c4 case 9) z order 101, no graphics event
    Incision();
    // vf2 0x86be0: only its stage's injury_to_create_at_start (the wound
    // under it), allowed over a locked area, not counted toward the cap; the
    // base's graphics event and hit points' health are not done
    void start() override;
    // vf20 0x86ab0: one of incision_1..3
    void onFinished() override;
};

// The staple a press of the stapler leaves on a cut (LargeCut vf31 0x862c0,
// SnakeCut vf31 0x864f8: the same code): while the cut is open (its stage's
// state Open), a "tool_staple.png" on the cut's line through its picture's
// turn, at the press's distance from its middle on the press's side, turned
// with it by -10..10 degrees more, in its top layer; the stapler counts the
// press (ToolState vf13).
void placeStaple(InjuryState& cut, const cocos2d::CCPoint& touch, ToolState* tool);

class LargeCut : public InjuryState {
public:
    void onStaple(const cocos2d::CCPoint& touch, ToolState* tool) override { placeStaple(*this, touch, tool); }
};

class SnakeCut : public InjuryState {
public:
    void onStaple(const cocos2d::CCPoint& touch, ToolState* tool) override { placeStaple(*this, touch, tool); }
};

}  // namespace mt
