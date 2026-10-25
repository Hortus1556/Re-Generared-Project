// Crab (the Flash AS2's, its logic and art: Crab.as): walks along its row
// across the body (round again at the sides), shying from the pointer when it
// comes near (40 stage pixels: away, faster), now and then nipping a small
// cut (3 health). The chainsaw (held on it a while: its boxes are tougher
// than Flash's) cracks its shell (stage 1: its hit plays, the
// shell flies off as a piece to take out, and it walks on bare and faster),
// then kills it (stage 2: its death plays as it bleeds blue).
#pragma once

#include <memory>

#include "as2/FlashCreature.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Crab : public InjuryGraphic {
public:
    Crab(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
    void special() override;
    // Sawn: blue blood once the shell is off.
    void createEffect(int count, int velocity) override;

private:
    Shape* body() { return mSpriteArray[0]->currentShape(); }
    // Its box under it.
    void boxFollows();
    void showFlash();
    // The hit (shell or bare) played out.
    bool hitDone() const;

    std::unique_ptr<FlashCreature> flash;
    // Crab.as's numbers, on the stage (converted with flashstage).
    static constexpr float kMinX = 0, kMaxX = 600, kRunRadius = 40;
    static constexpr float kSlowMulti = 0.99f, kFastMulti = 1.02f;
    // Each box's health against the saw (1 a frame, 3 at its best): the
    // iPad's 40, over a second of sawing a stage at the first level (Flash's
    // 4 went in a few frames, both shell and crab in a single stroke).
    static constexpr int kBoxHealth = 40;
    // A row above this (the world) is the top one, by the cards.
    static constexpr float kTopRow = 140;
    float minSpeed = 3, maxSpeed = 5;
    float x = 0;          // along its row, on the stage
    float speed = 0;
    bool left = false;    // walking left
    bool hitting = false; // its hit (shell cracking, or dying) playing
    int hitFrames = 0;    // passes of it
};

}  // namespace as2
