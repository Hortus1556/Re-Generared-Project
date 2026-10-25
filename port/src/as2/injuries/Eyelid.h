// Eyelid (mission 17): an eye sewn shut, its lids drawn over it and its
// pupil (which follows the finger once it has moved) peeking through. Pulled
// open with the tongs: held on the lid and drawn away from the eye (within
// its angle buffer of the lid's turn) the lid opens over 150 points of pull,
// its box following the finger; fully open it is treated. Let go before, it
// shuts again.
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Eyelid : public InjuryGraphic {
public:
    Eyelid(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
    // Held: its box grows 16 points each way.
    void extendHitBox() override;
    // The tongs drew it `dist` points further open: its box follows, its
    // picture opens; true when it is fully open (then treated).
    bool pull(float dist);

    float angleBuffer = 35;  // degrees off its turn a pull may be

private:
    float opened = 0;  // points pulled
    float mx = 0, my = 0;  // the finger when it came
    bool movedYet = false;
};

}  // namespace as2
