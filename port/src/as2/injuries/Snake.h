// Snake (mission 22): a snake in a hole in the patient. Now and then (or
// when the device is shaken) it rears out (a hiss), writhes a while, and
// slips back, leaving poison splatter. While it is out the chainsaw saws at
// it (it writhes in pain); sawn through, a poison blob is left, it bleeds a
// while, and its body is taken out with the tongs and dropped on the tray.
// The hole is then stitched (across it), burnt and creamed.
#pragma once

#include <string>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Snake : public InjuryGraphic {
public:
    Snake(InjuryData* d, Injury* i, bool playerCreated, bool force);
    ~Snake() override;
    void render() override;
    void setup() override;
    // Its time to rear out.
    void special() override;
    // Sawn at: it writhes in pain.
    void interact() override;
    void updateInjury() override;
    // While it hides, nothing ("wait").
    std::string toolNow() const override;

private:
    void rearOut();

    enum Label { kHidden, kRearing, kOut, kHiding, kBleeding, kHurt, kDead };
    int currentLabel = kHidden;
    int currentFrame = 0;
    int interactCount = 0;
};

}  // namespace as2
