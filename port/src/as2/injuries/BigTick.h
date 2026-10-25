// BigTick (mission 30): the overlord tick, under the skin. Cut open it
// shows (the scene changes, blood bursts, a large cut and poison where it
// sucks); a chainsaw line round it then frees it, and it flies about,
// biting a small cut every 20 frames. Shaking the device (on a PC, the
// pointer shaken) stuns it a while (3 seconds): then one shock of the
// battery hurts it, and it brings two ticks, lands where it is (another
// large cut and poison) and the chainsaw again... Three shocks kill it
// (a great burst of blood, and its two last ticks). Left flying, or
// stunned and not shocked, it lands where it is and must be sawn free
// again.
#pragma once

#include <string>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class BigTick : public InjuryGraphic {
public:
    BigTick(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // Shocked with the battery (while stunned).
    void interact() override;
    void updateInjury() override;
    // Sucking: the stage's tool; flying: a shake; stunned: the battery.
    std::string toolNow() const override;

private:
    enum State { kFlying, kSucking, kHurt, kStunned };
    // A new place to fly to from here, and the way there.
    void newTarget();
    // Down where it is: sucking again (a large cut and poison), the
    // chainsaw's stage again.
    void returnHome();
    // A large cut and poison where it sucks (playerCreated: the later ones).
    void bite(bool playerCreated);
    Shape* body() { return mSpriteArray[0]->currentShape(); }

    int minX = 90, maxX = 390, minY = 80, maxY = 250;
    State currentState = kSucking;
    int currentFrame = 0;
    int lives = 3;
    int flyTimer = 0;
    int playerCreatedCounter = 3;
    float targetX = 0, targetY = 0, origX = 0, origY = 0;
    float angle = 0;  // degrees: 0 down the screen, 90 to the right
    float angleChangeValue = 2;
    int angleChangeTimer = 0;
    float speed = 0;
};

}  // namespace as2
