// Goo (mission 23): a lump of goo stuck to the patient with a magician's
// prop in it (a wand, a honker, juggling pins, a snake flute: by its
// linkage). Cut down its middle with the cutter it bursts (yellow goo
// flying, to be vacuumed) and fades away; the prop stays (with the stage's
// art).
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Goo : public InjuryGraphic {
public:
    Goo(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
};

}  // namespace as2
