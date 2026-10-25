// The brain's amp (mission 29): after the amp itself is transplanted (a
// BoneTransplant kind), its plugs and then a pill go in.
//
// AmpPlug: a plug (by its linkage: three sizes, or the cap) waiting on the
// tongs' tray; let go on its socket it goes in (its plugged picture).
// BrainPill: a pill waiting on the tray; let go over the brain it fizzes
// away (its fizz rising and fading), the brain's pink easing back to
// healthy.
// While either is there the background is tinted pink, as the amp's.
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class AmpPlug : public InjuryGraphic {
public:
    AmpPlug(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void updateInjury() override;
};

class BrainPill : public InjuryGraphic {
public:
    BrainPill(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void updateInjury() override;

private:
    int currentFrame = 0;
};

}  // namespace as2
