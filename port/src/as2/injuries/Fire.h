// Fire: flames on the body (a missed word of the type-n-tell, mission 16's
// fireflies): put out with the vacuum while it holds blood it has sucked up;
// out, it leaves a burn. Its render does not call InjuryGraphic's, so it
// neither hurts the patient nor runs its special (a fire splatter): the
// original's, kept so.
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Fire : public InjuryGraphic {
public:
    Fire(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;

private:
    int currentFrame = 0;
};

}  // namespace as2
