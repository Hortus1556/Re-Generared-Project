// Dove (the Flash AS2's, its logic and art: Dove.as): in from off one side
// low across the body to off the other (nipping now and then: a small cut,
// 3 more health), waits 4 seconds, crosses back high up (only its shadow:
// out of reach, dropping droppings; slower), and so on. The battery shocks
// it on a low pass (feathers fly, its zap plays, it flies on, out of reach
// until it next comes in low); the third shock kills it: it bursts, and
// leaves its body (DeadDove, Remains.h) where it fell.
// Shocking its shadow shocks the patient (Battery).
//
// Droplet: a dropping falling from the dove's shadow, landing as white
// splatter (DoveSplatter) that only the vacuum clears.
//
// InsectBoss: a wasp (mission 9; the Flash AS2's, its logic and art:
// InsectBoss.as). It darts across the body and off the side (stinging all
// the way: a small cut and 3 health now and then), rests off it, darts back,
// then lands on the body and rests a moment (45 frames, wings still): only
// then can the battery shock it. A shock leaves poison and sends it off the
// side; the third kills it: it oozes poison as it dies, and leaves a blob.
#pragma once

#include <memory>

#include "as2/FlashCreature.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Dove : public InjuryGraphic {
public:
    Dove(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
    void special() override;
    // Shocked.
    void interact() override;
    bool showShadow() const { return high; }

private:
    Shape* body() { return mSpriteArray[0]->currentShape(); }
    // Off to the other side (a new target off the body, a height at random),
    // turned to it.
    void headAcross();
    void boxFollows();
    void showFlash();

    std::unique_ptr<FlashCreature> flash;
    // Dove.as's numbers, on the stage (converted with flashstage).
    static constexpr float kMinX = 75, kMaxX = 525, kMinY = 125, kMaxY = 385;
    static constexpr float kOff = 250, kSpeed = 10, kSettle = 25;
    static constexpr int kMaxAngleTimer = 120, kMaxCrossed = 1;
    Point at;            // where it is, on the stage
    Point target;        // where it heads
    float angle = 0;     // Flash's: 0 up, clockwise
    float speed = 0;     // stage pixels a frame
    int angleTimer = 0;
    int crossed = 0;
    int lives = 3;
    bool right = true;   // heading off the right side
    bool high = false;   // the shadow showing (up high)
    bool shocked = false;
    int numIs = 1;       // which of a pair (they come in from opposite sides)
};

class Droplet : public InjuryGraphic {
public:
    Droplet(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override {}
    void updateInjury() override;

private:
    int mCurrentFrame = 0;
};

class InsectBoss : public InjuryGraphic {
public:
    InsectBoss(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
    void special() override;
    // Shocked.
    void interact() override;

private:
    Shape* body() { return mSpriteArray[0]->currentShape(); }
    void boxFollows();
    // Toward a stage point, turned to it.
    void headFor(float x, float y);
    void showFlash();

    std::unique_ptr<FlashCreature> flash;
    // InsectBoss.as's numbers, on the stage (converted with flashstage).
    static constexpr float kMinX = 75, kMaxX = 525, kMinY = 125, kMaxY = 385;
    static constexpr float kOff = 250, kFlee = 150, kSpeed = 70, kSettle = 50;
    static constexpr int kMaxAngleTimer = 270, kMaxCrossed = 2;
    Point at, target;    // on the stage
    float angle = 0;     // Flash's: 0 up, clockwise
    float speed = 0;
    float angleTimer = 0;
    int crossed = 0;
    int lives = 3;
    int bloodTimer = 0;
    bool right = true;   // heading off the right side
    bool shocked = false;
};

}  // namespace as2
