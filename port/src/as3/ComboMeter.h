// The surgery's score and combo meter (0x28 bytes at the screen's +0x2e60;
// made by 0x9a954; the port's name). Each completed action scores 10 times
// the multiplier and fills a bar; ten bars raise the multiplier (levels 1 to
// 5: bronze, silver, gold, diamond, rainbow). A bar empties when its timer
// runs out (3 s; once one is lost, the next lasts 1.5 s more with the timer
// upgrade and 1.5 s more in Flow Master's power); a mistake breaks the
// combo (the combo saver upgrade keeps half). The double combo upgrade adds a bar for quick chains; a tag partner can
// double the multiplier.
#pragma once

namespace mt {

class ComboMeter {
public:
    void update(float dt);        // 0x9a974
    // a completed action: what it scored (0x9abb4)
    int action();
    void breakCombo();            // 0x9af5c
    int multiplier() const { return level << doubled; }  // 0x9aed8
    int levelOf() const { return level; }               // 0x9af58
    // the coins of a score (0x9aee0)
    static int coinsFor(int score, float rate, int shift);
    // a tool used on an injury it can't treat: the same injury three times
    // over is a repeated mistake (0x9b11c, 0x9b134)
    void wrongTarget(const void* injury);
    bool repeatedWrong() const { return wrongCount > 2; }
    void setFlag1c(bool b) { flag1c = b; dirty = true; }  // 0x9aba4
    void setDoubled(bool b) { doubled = b ? 1 : 0; dirty = true; }  // 0x9abac

    int score = 0;          // +0
    int level = 1;          // +4
    int bars = 0;           // +8
    float barTimer = 0;     // +0xc
    bool dirty = true;      // +0x10: the HUD to refresh
    float sinceLast = 0;    // +0x14
    int quickChain = 0;     // +0x18
    bool flag1c = false;    // +0x1c: Flow Master's longer bars (Android's increaseTimeEnabled)
    int doubled = 0;        // +0x1d
    const void* wrongInjury = nullptr;  // +0x20
    int wrongCount = 0;     // +0x24
};

}  // namespace mt
