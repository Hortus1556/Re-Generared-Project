// Tick (missions 27 and 30): a tick sucking on the patient (a small cut and
// poison where it bit), out of reach of every tool. Shaking the device (on
// a PC, the pointer shaken) knocks it off: it flies about on a wobbling
// course, a little while (shaking keeps it in the air longer), then bites
// somewhere else, a new cut and poison. In the air one shock of the
// battery kills it: it fizzes, and bursts in a spray of blood.
// With a big tick about, the ticks it brings start in the air. In the
// final mission the first one also hides the patient's eyes.
#pragma once

#include <string>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Tick : public InjuryGraphic {
public:
    Tick(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // Shocked with the battery.
    void interact() override;
    void updateInjury() override;
    // In the air, the battery; sucking, a shake.
    std::string toolNow() const override;

private:
    enum State { kFlying, kSucking, kDying };
    // A new place to fly to from here, and the way there.
    void newTarget();
    // Biting where it is: a small cut and poison.
    void bite();
    void takeOff();
    Shape* body() { return mSpriteArray[0]->currentShape(); }

    int minX = 90, maxX = 420, minY = 80, maxY = 280;
    State currentState = kFlying;
    int currentFrame = 0;
    int lives = 1;
    int flyTimer = 0;
    float targetX = 0, targetY = 0, origX = 0, origY = 0;
    float angle = 0;  // degrees: 0 down the screen, 90 to the right
    float angleChangeValue = 2;  // its wobble, degrees a frame
    int angleChangeTimer = 0;
    float speed = 0;
};

}  // namespace as2
