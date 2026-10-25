// TypeAndTell (mission 12): the vocal cords replaced by a type-'n'-tell.
// The cords come out as an organ of three parts (BoneTransplant's routine),
// the device goes in from the tray, the cut is burnt and creamed; then the
// battery wakes the device (its bomb shows), and words scroll across its
// screen: each is typed on its keys before it is gone. A wrong letter or a
// word let go sets the patient on fire (a fire and burning splatter) and
// costs the heart; five words typed and it is done.
//
// PC: the keyboard types too (typeKey), as a tap on the key would.
#pragma once

#include <string>
#include <vector>

#include "as2/injuries/Transplant.h"

namespace as2 {

class TypeAndTell : public BoneTransplant {
public:
    TypeAndTell(InjuryData* d, Injury* i, bool playerCreated, bool force);
    ~TypeAndTell() override;
    void setup() override;
    void render() override;
    // Stage 5: woken by the battery. Stage 6: a key tapped.
    void interact() override;
    std::string toolNow() const override;
    void traceBoxes() const override;
    // A letter typed (a key, or on the PC the keyboard), while it asks.
    void typeKey(char c);
    bool typing() const { return currentStage == 6 && alive; }

private:
    void chooseWord();
    void clearWord();
    void checkWord(char c);
    // Too slow, or a wrong letter: fire, and the heart hurt; a new word.
    void burnt();
    void ownBoxes(int count);

    std::vector<Sprite*> letters;  // the word's, retained
    std::string word, typed;
    int correctWords = 0;
    float otx = 136;  // the word's start from the device's middle
    bool ownHits = false;  // hitboxes are the device's own (stages 5, 6)
};

}  // namespace as2
