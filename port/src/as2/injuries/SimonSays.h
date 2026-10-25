// SimonSays (mission 25): six sick muscles of a six-pack (three left,
// three right), played as Simon. Each round they twitch in a sequence (the
// first a sick one, then healthy ones), and the player shocks them in that
// order with the battery; done right, the first is healed (and the next
// round's sequence is longer, up to five). Wrong, or too slow (6 seconds),
// every muscle is burnt and the heart hurt. Six rounds, all healed: done.
#pragma once

#include <string>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class SimonSays : public InjuryGraphic {
public:
    SimonSays(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // Shocked: the muscle nearest the finger, the next in the sequence or
    // not.
    void interact() override;
    // While it is the player's turn, the battery on the next muscle.
    std::string toolNow() const override;
    void traceBoxes() const override;

private:
    enum State { kWaiting, kShowing, kPenalty, kStart, kPlayer, kRoundWon };
    void lightOn(int muscle);
    void resetMuscles();
    void createSeq();
    void addCutsOnMuscle(int muscle);
    int sequenceLength() const;
    void penalty(bool slow);
    Shape* shape(int muscle) { return mSpriteArray[muscle]->currentShape(); }

    State currentState = kStart;
    int currentRound = 1;
    int currentSeq = 0;
    int playersSeq = 0;
    int waitingTimer = 0;
    int hitTimer = 0;
    bool healed[6] = {};
    int lightSequence[6] = {-1, -1, -1, -1, -1, -1};
};

}  // namespace as2
