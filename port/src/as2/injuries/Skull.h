// The skull (mission 30).
//
// SkullCut: the top of the skull (its cap) sawn round with the chainsaw and
// lifted away whole with the tongs to the tray (a BoneTransplant kind with
// no replacement). Meanwhile two masks cover the eyes; with the cap gone
// they go.
// ReplaceSkull: the cap waiting on the tongs' tray; let go in its place it
// goes back on, and its line is then burnt and creamed.
#pragma once

#include "as2/injuries/Transplant.h"

namespace as2 {

class SkullCut : public BoneTransplant {
public:
    SkullCut(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void updateInjury() override;
};

class ReplaceSkull : public InjuryGraphic {
public:
    ReplaceSkull(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
};

}  // namespace as2
