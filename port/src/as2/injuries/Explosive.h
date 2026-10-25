// What bursts when burnt (missions 17, 18).
//
// ExplosiveExtraction: a cyst, burnt with the lighter: it bursts into a fire
// and burning splatter (the heart hurt), and is gone. (Like the eggs, the
// tongs could hold it; its stage asks for the lighter.)
// ExplosiveExtractionBomb: a little bomb, burnt with the lighter: a fire,
// burning splatter and a poison blob where it was (the heart hurt).
#pragma once

#include "as2/injuries/Extraction.h"

namespace as2 {

class ExplosiveExtraction : public PictureExtraction {
public:
    ExplosiveExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void updateInjury() override;
};

class ExplosiveExtractionBomb : public InjuryGraphic {
public:
    ExplosiveExtractionBomb(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
};

}  // namespace as2
