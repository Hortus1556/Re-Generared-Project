// Poison: a green blob on the liver, pulsing (its scale between 0.9 and
// 1.1). The injection bursts it into three PoisonSplatters, which the vacuum
// must clear.
//
// Its render does not call InjuryGraphic's, so it neither hurts the patient
// nor runs its special (a new blob 20-degree steps round it, kept clear of
// the others): the original's, dead on the iPad and left so.
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Poison : public InjuryGraphic {
public:
    Poison(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // Injected: three splatters where it was, and it is gone.
    void updateInjury() override;
    // `count` splatters at the finger.
    void createEffect(int count, int velocity) override;

private:
    float scaleDirection = 0.0125f;
};

}  // namespace as2
