// Transistor (mission 29): a transistor to fit into the brain. The cutter
// opens a line on one side, then the other (each leaves a scar, closed for
// now); the transistor then waits on the tongs' tray and, let go between
// the cuts, goes in. Then the scars are stitched and burnt while it is held
// steady: a gauge swings across it (while both scars are open; with one
// left it stops) and its needle follows the tilt (the device's, or on a PC
// the arrow keys'). Off the gauge by 6 or more the patient is hurt every 5
// frames and, every 30, cut. Both scars healed, it buzzes right and stays
// until the rest is done. With two, one is operated at a time.
#pragma once

#include <string>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Transistor : public InjuryGraphic {
public:
    Transistor(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
    // In place (or waiting its turn): "wait".
    std::string toolNow() const override;

private:
    // Another transistor still to be fitted (one at a time).
    bool otherWaiting() const;
    // The balance: the gauge swinging, the needle on the tilt.
    void balance();
    Shape* shape(int k) { return mSpriteArray[k]->currentShape(); }

    InjuryGraphic* scar1 = nullptr;  // not retained (looked for among the injuries)
    InjuryGraphic* scar2 = nullptr;
    float gaugeOffset = 0, gaugeDirection = 1;
    int cutTimer = 0;
    bool stillActive = true;
    int tracedGauge = 1000, tracedNeedle = 1000;
};

}  // namespace as2
