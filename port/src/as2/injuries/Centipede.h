// The centipedes (mission 19).
//
// Centipede (the Flash AS2's, its logic and art: Centipede.as): a
// centipede crawling over the body (a head, four body segments, a tail, each
// a step behind the one before), now and then biting (a small cut, the heart
// hurt). Its last segment is tinted red; sawn through with the chainsaw
// (its box 5) it comes off (poison splatter flies; with the head, the last,
// a poison blob too) and the next is tinted, until the head is off: dead.
//
// BigCentipede: the boss, under the skin. While anything else is left to
// treat it waits; then the body rumbles and it bursts out of a hole and
// attacks (the heart hurt every frame; no tool can be used) until the
// finger is rubbed fast over it: stunned, stars about its head, its three
// eyes open to the injection. An eye injected, it goes into a craze (the
// body shaken, the heart hurt), shrinks back into its hole and a centipede
// crawls out; then it waits again, and must be rubbed longer to be stunned.
// All three eyes injected it dies, bursting into poison and three
// centipedes.
#pragma once

#include <string>
#include <vector>

#include <memory>

#include "as2/FlashCreature.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Centipede : public InjuryGraphic {
public:
    Centipede(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // The bite.
    void special() override;
    // Its tinted segment sawn through.
    void updateInjury() override;
    void createEffect(int count, int velocity) override;

private:
    // The head's next step (turning now and then, back from the edges),
    // each segment's to the one before, the tail's to the last left.
    void tweenBodyParts();
    void removeHit();
    // Its pictures, their turns and tints, and the box on the tinted
    // segment, where they are now.
    void place();

    static constexpr int kSegments = 6;  // head, four body segments, tail
    static constexpr int kTail = kSegments - 1;
    // Centipede.as's numbers, on the stage (converted with flashstage).
    static constexpr float kMinX = 75, kMaxX = 525, kMinY = 125, kMaxY = 385, kMaxMoveDist = 30;
    static constexpr int kMaxDirTimer = 5, kStepFrames = 6;  // (TweenLite's 0.2 s)
    Point from[kSegments], to[kSegments];  // each segment's step, on the stage
    float turn[kSegments] = {};            // Flash's: 0 up, clockwise
    std::unique_ptr<FlashCreature> art[kSegments];
    flash::Rect hitArea;  // the tinted segment's box about it (stage pixels)
    int left = 5;         // segments still on (head first)
    int dirTimer = 0, stepFrame = 0;
    int ranRot = 0;
};

class BigCentipede : public InjuryGraphic {
public:
    enum State { kHidden, kAppear, kAttack, kStunned, kCraze, kVanish, kDying, kRetract };

    BigCentipede(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void special() override;
    void updateInjury() override;
    void createEffect(int count, int velocity) override;
    bool isAttacking() const override { return state == kAttack; }
    std::string toolNow() const override;
    void traceBoxes() const override;
    bool stunned() const { return isStunned; }
    // The injection's close-up was opened over eye `eye` (or none, -1);
    // given, that eye is hit.
    void updateEye(int eye);

private:
    void removeHit(int eye);
    void setState(State s);
    // Its clip on the state's frame, its eyes as they are; the eyes' boxes
    // on them.
    void showFlash();
    const char* label() const;
    Point at() const { return flashPlace(); }
    Point picturePlace() const override { return at(); }
    // The state's clip played to its end.
    bool played() const;

    std::unique_ptr<FlashCreature> flash;
    State state = kHidden;
    bool isStunned = false;
    bool hinted = false, hintedHide = false;  // the port's hints shown
    bool hits[3] = {false, false, false};
    int eyeHit = -1;
    int bloodTimer = 0, cutTimer = 0;
    int mouseShake = 0;
    float lastX = 0, lastY = 0;
    // BigCentipede.as's numbers.
    static constexpr int kOMaxMouseShake = 90, kBaseMaxShake = 15, kMaxCutTimer = 10;
    static constexpr float kNeededDist = 10;  // stage pixels a frame
    int eyesLeft() const { return !hits[0] + !hits[1] + !hits[2]; }
};

}  // namespace as2
