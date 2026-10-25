// The fireflies (mission 16).
//
// FireFlyEgg: an egg laid on the patient, lifted away with the tongs; the
// Flash AS2's art, glowing up as it nears hatching (its `anim` at the share
// of the time gone). Left too long it hatches, bursting into flames (the
// heart hurt): a firefly, the broken shell (to lift away too), a fire and
// burning splatter. Hatching in the tongs' grip, it is dropped.
// ExtractionFireFlyEggBroken: the hatched shell (Flash's), lifted away.
// FireFly (the Flash AS2's, its logic and art: FireFly.as): a firefly
// darting from place to place over the body (a pause at each), now and then
// spitting a fireball (a fire and burning splatter where it is, the heart
// hurt). One shock of the battery kills it: green blood flies, its death
// plays and bursts into a fire.
#pragma once

#include <memory>

#include "as2/FlashCreature.h"
#include "as2/injuries/Extraction.h"

namespace as2 {

class FireFlyEgg : public PictureExtraction {
public:
    FireFlyEgg(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void render() override;
    // Hatching.
    void special() override;

private:
    std::unique_ptr<FlashCreature> flash;
};

class ExtractionFireFlyEggBroken : public PictureExtraction {
public:
    ExtractionFireFlyEggBroken(InjuryData* d, Injury* i, bool playerCreated, bool force);

private:
    std::unique_ptr<FlashCreature> flash;
};

class FireFly : public InjuryGraphic {
public:
    FireFly(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // The fireball.
    void special() override;
    // Shocked with the battery.
    void interact() override;
    void updateInjury() override;

private:
    // A new place to fly to, and the way there.
    void newTarget();
    void place();  // its picture, boxes and injury where it is
    void showFlash();

    std::unique_ptr<FlashCreature> flash;
    // FireFly.as's numbers, on the stage (converted with flashstage).
    static constexpr float kMinX = 75, kMaxX = 525, kMinY = 125, kMaxY = 360, kSpeed = 18;
    static constexpr int kMaxAngleTimer = 30;
    Point at, target;   // on the stage
    float angle = 0;    // Flash's: 0 up, clockwise
    float speed = 0;
    float angleTimer = 0;
    int lives = 1;
    bool burst = false;  // its death's fire made
};

}  // namespace as2
