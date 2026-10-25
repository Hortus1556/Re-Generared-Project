// Insertion: a pin to put in. The cutter opens the skin along its line;
// the pin then waits on the tongs' tray (the tongs' trayHolding), and taken
// from there and let go where it goes (73 points down the cut) it is pushed
// in.
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Insertion : public InjuryGraphic {
public:
    Insertion(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
    // How much of the pin shows past its picture's length (points; less
    // when pushed in).
    void setInsertionLength(float length);
};

}  // namespace as2
