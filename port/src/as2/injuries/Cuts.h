// The cuts: SmallCut (burn it shut, then cream the burn), Incision (the
// yellow line to open with the cutter: the stage under it follows) and
// LargeCut (stitch, burn, cream). Each treated stage pops "correct"; healed,
// it fades out.
#pragma once

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class SmallCut : public InjuryGraphic {
public:
    SmallCut(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void updateInjury() override;
    void setup() override;
};

class Incision : public InjuryGraphic {
public:
    Incision(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void updateInjury() override;
    void setup() override;

private:
    float mx = 0, my = 0;  // the finger when it was made
    bool bMovedYet = false;
};

class LargeCut : public InjuryGraphic {
public:
    LargeCut(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void updateInjury() override;
    void setup() override;

private:
    float mx = 0, my = 0;
    bool bMovedYet = false;
};

// LargeCutScar: a large cut's scar, stitched and burnt (no "correct", no
// score; gone at once when done).
class LargeCutScar : public InjuryGraphic {
public:
    LargeCutScar(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void updateInjury() override;
    void setup() override;
    // Closed for now (a transistor's, until it is in): "wait".
    std::string toolNow() const override { return invincible ? "wait" : InjuryGraphic::toolNow(); }
};

// ClownNose: the clown's red nose over a line to cut: opened, it pops off
// (a burst of blood) and leaves a cut to stitch, burn and cream. The clown's
// eyes follow the finger.
class ClownNose : public InjuryGraphic {
public:
    ClownNose(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
    // Cut or sawn: blood, eight drops a time.
    void createEffect(int count, int velocity) override;

private:
    // An eye's pupil from its middle toward the finger (an eighth of the
    // way, at most 30 points).
    void eyeFollows(Sprite* eye, float x, float y);
    float mx = 0, my = 0;
    bool bMovedYet = false;
};

// BloodSmallCut, BloodLargeCut (mission 10, the vampire): a cut whose
// blood is sucked up with the vacuum along its boxes, one stage; it fades out
// when done.
class BloodCut : public InjuryGraphic {
public:
    BloodCut(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void updateInjury() override;
    void setup() override;

private:
    int boxes = 4;
    const char* guide = "hitbox_20x16.png";
};

// Burn: a burn mark (a lighter or a battery used wrong): creamed away, with
// no "correct" or score; it fades out. BloodBurn (the vampire's) is the same
// mark, vacuumed instead (its own stages).
class Burn : public InjuryGraphic {
public:
    Burn(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
};

}  // namespace as2
