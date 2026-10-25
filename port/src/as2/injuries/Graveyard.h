// The graveyard (mission 20).
//
// Dirt: a patch of grave dirt on the coffin (blood is not shed here), made
// but never shown (as the original).
// IncisionCoffin: a line down the coffin's lid, sawn with the chainsaw; the
// lid's cut shows.
// Bleed: the patient raised from the dead. Each treatment, on the body,
// adds to the heart rate (each up to 10, fading as it wears off): the
// injection, the battery, the lighter, the cream (its mouth), the stitch
// (here a phial: its tears, on its heart) and a tap on its thumb (the
// button by its hand); with the heart at 30 or more for 15 seconds (the
// HUD's countdown) it lives. The lighter, cream and phial are held on
// it: each held frame counts down (`held`), and at 0 the treatment is given
// and the count starts again.
#pragma once

#include <string>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Dirt : public InjuryGraphic {
public:
    Dirt(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void updateInjury() override;
};

class IncisionCoffin : public InjuryGraphic {
public:
    IncisionCoffin(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
    void createEffect(int, int) override {}
};

class Bleed : public InjuryGraphic {
public:
    Bleed(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // A treatment given: by the tool in hand.
    void updateInjury() override;
    // A held tool's frame on it: the lighter anywhere on it, the cream on
    // its mouth, the tears on its heart.
    enum Part { kBody, kMouth, kHeart };
    void held(Part part);
    void interact() override {}
    void special() override {}
    // A new touch: on the thumb's button, it is pressed.
    void checkThumbClick() override;
    // The treatment that has worn off most ("tap": the thumb).
    std::string toolNow() const override;
    void traceBoxes() const override;
    // The counter's ticks left (the HUD shows its seconds), 0 while the heart
    // is under 30.
    int aliveLeft() const { return aliveTimer; }

private:
    // The tool in hand's treatment given (with "correct" shown or not).
    void treat(bool popup);

    float injectionTimer = 0, batteryTimer = 0, syrupTimer = 0, tearsTimer = 0, lighterTimer = 0, thumbTimer = 0;
    float maxInjectionTimer = 900, maxBatteryTimer = 500, maxSyrupTimer = 600, maxTearsTimer = 600,
          maxLighterTimer = 750, maxThumbTimer = 500;
    int aliveTimer = 0, maxAliveTimer = 450;
    int mouthHealth = 15, heartHealth = 15, bodyHealth = 25;
    // The held tools: the frame each part was last held on, and whether
    // this hold has shown its "correct" yet (one a hold: the original showed
    // one at every refill while held, two a second).
    int frame = 0;
    int heldFrame[3] = {-2, -2, -2};
    bool heldShown[3] = {false, false, false};
};

}  // namespace as2
