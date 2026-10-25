// Amateur Surgeon (AS1): the original Flash game (Adult Swim / Mediatonic,
// 2008) run by its own scripts on the generic stage, its operation included
// (surgeon/FlashSurgeonGame.h; the package games/as1). docs/as1.md.
#pragma once

#include "surgeon/FlashSurgeonGame.h"

class As1Game : public FlashSurgeonGame {
public:
    As1Game();
    static constexpr int kPatients = 31;  // missionArray1..31 (31: the bonus patient)
};
