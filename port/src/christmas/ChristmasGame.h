// Amateur Surgeon Christmas: the original Flash game run by its own scripts
// on the generic stage, its operation included, as AS1 is
// (surgeon/FlashSurgeonGame.h; the package games/christmas), with the
// port's additions (StageExtras.h). docs/christmas.md.
#pragma once

#include "surgeon/FlashSurgeonGame.h"

class ChristmasGame : public FlashSurgeonGame {
public:
    ChristmasGame();
    static constexpr int kMissions = 5;  // missionArray1..5
};
