// The makeover's injuries (mission 14), each one box at the injury:
//
// Fat: a lump of fat under the skin, found with the scanner, cut open (it
// shows, throwing fat), then burnt with the lighter: it bursts, fat flying,
// a fire left where it was (the heart hurt).
// Lip: a lip to plump up with the injection; the full lip then shows (and
// stays, with the stage's art). Its place on the mouth by its linkage.
// Unibrow: a monobrow burnt off with the lighter, which sets a fire there
// (the heart hurt).
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Fat : public InjuryGraphic {
public:
    Fat(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
};

class Lip : public InjuryGraphic {
public:
    Lip(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
};

class Unibrow : public InjuryGraphic {
public:
    Unibrow(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
};

}  // namespace as2
