// Squid (mission 13; the Flash AS2's, its logic and art: Squid.as): a squid
// clamped on the pirate's chest, its five arms about it. It rests, then
// opens its mouth and sucks (shorter at each stage), then sprays ink (black
// splats, the heart hurt) and rests again. The battery on its tonsils while
// its mouth is open stuns it (its arms' boxes shown yellow): an arm can be
// sawn off (each box 6), and it goes into a craze a while (the screen red,
// the heart hurt). With every arm off, stunned once more, its body is sawn
// through and it dies, ink bursting from it as its death plays.
#pragma once

#include <memory>
#include <string>

#include "as2/FlashCreature.h"

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Squid : public InjuryGraphic {
public:
    enum State { kDying = 1, kResting = 2, kAngry = 4, kSucking = 5, kSpitting = 6, kStunned = 7, kGone = 9 };

    Squid(InjuryData* d, Injury* i, bool playerCreated, bool force);
    ~Squid() override;
    void render() override;
    void setup() override;
    void special() override;
    // Shocked (the battery in its mouth): stunned.
    void interact() override;
    void updateInjury() override;
    // Sawing: ink where the saw is.
    void createEffect(int count, int velocity) override;
    std::string toolNow() const override;
    void traceBoxes() const override;

    // The battery's box: its mouth while it sucks, else none.
    HitBox* mouthBox() { return state == kSucking ? groups[0].hits : nullptr; }
    // A group of boxes the chainsaw saws (1-5 the tentacles, 6 the body),
    // only while it is stunned; its count.
    HitBox* groupBoxes(int group) { return state == kStunned ? groups[group].hits : nullptr; }
    int groupCount(int group) const { return groups[group].count; }
    bool allTentaclesRemoved() const;
    // A group sawn through: its boxes gone (a tentacle's picture drawn back).
    void removeHit(int group);

private:
    struct Group {
        HitBox* hits = nullptr;
        int count = 0;
        bool complete = false;
    };
    void freeGroup(Group& g);
    // Its head on its state's frame (its inner clips from the start).
    void showHead();
    Point at() const { return flashPlace(); }

    Group groups[7];
    int state = kResting;
    int inkTimer = 0;
    int stateFrames = 0;  // passes in its state
    bool stunned = false;
    std::unique_ptr<FlashCreature> flash;
    std::unique_ptr<FlashCreature> arms[5];
    Point armAt[5];  // each arm's place about it (Flash's arm_positions, sawn ones drawn back)
    int dyingFrame = 0, dyingFrames = 1;
};

}  // namespace as2
