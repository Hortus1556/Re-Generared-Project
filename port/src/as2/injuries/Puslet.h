// Mission 28's pus and poison.
//
// Puslet: a pustule on the patient, lanced with the cutter: it bursts
// (yellow pus flying, to be vacuumed, and a small cut where it was) and
// fades away.
// PoisonParticle: poison loose in the body, three drops of poison splatter
// as the stage starts (to be vacuumed: while they lie they sap the heart).
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Puslet : public InjuryGraphic {
public:
    Puslet(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
};

class PoisonParticle : public InjuryGraphic {
public:
    PoisonParticle(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void updateInjury() override {}
};

}  // namespace as2
