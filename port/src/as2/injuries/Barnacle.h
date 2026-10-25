// Barnacle (mission 13; the Flash AS2's, its logic and art: Barnicle.as):
// stuck on the pirate, its eye on the pointer, now and then hurting him;
// burnt off with the lighter (its hit areas 5 each), it dies: it bleeds
// sea-green (BarnacleSplatter) while its death plays, and is gone.
#pragma once

#include <memory>

#include "as2/FlashCreature.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Barnacle : public InjuryGraphic {
public:
    Barnacle(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;

private:
    Point at() const { return flashPlace(); }
    Point picturePlace() const override { return at(); }
    void showFlash();
    std::unique_ptr<FlashCreature> flash;
};

}  // namespace as2
