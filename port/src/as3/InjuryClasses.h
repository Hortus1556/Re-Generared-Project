// The factory's cases for the injuries that are not creatures (0x908c4;
// the creatures': Creature.h): the subclass of a base class, tuned as the
// original's case does it (their constructors carry the tuning). Bases with
// no class of their own (SmallCut 17, Burn 5: the base's vtable, no tuning;
// the robot cuts and Rust are their classes' data on those bases) and the
// unknown ones give nullptr (the base InjuryState runs them).
#pragma once

namespace mt {

class InjuryState;

namespace injuries {
InjuryState* make(int base);
}

}  // namespace mt
