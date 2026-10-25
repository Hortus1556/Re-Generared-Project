#include "as3/ComboMeter.h"

#include <cmath>
#include <string>

#include "as3/SaveData.h"
#include "as3/SoundMgr.h"
#include "as3/UpgradeInfoUtils.h"

namespace mt {

namespace {

bool hasUpgrade(const char* id) {
    SavedGame& slot = SaveData::shared().currentSlot();
    return slot.owns(SavedGame::kItemUpgrade, upgradeIndex(id)) || slot.allUpgrades;
}

}  // namespace

void ComboMeter::update(float dt) {
    sinceLast += dt;
    if (barTimer <= 0 || bars < 1 || level > 4) return;
    barTimer -= dt;
    if (barTimer > 0) return;
    --bars;
    if (bars > 0) {
        barTimer = 3.0f;
        if (hasUpgrade("score_timer_upgrade")) barTimer += 1.5f;
        // and 1.5 s more in Flow Master's power (+0x1c: the conditional
        // vaddne at 0x9aada, which Ghidra's listing dropped; Android's
        // ScoringMgr::update 0x6cda5c reads it too). Fixed: the port added
        // it always, so every bar lasted 1.5 s longer than the original's.
        if (flag1c) barTimer += 1.5f;
    }
    dirty = true;
}

int ComboMeter::action() {
    bool doubleCombo = hasUpgrade("score_double_combo_upgrade");
    quickChain = doubleCombo && sinceLast <= 1.5f ? quickChain + 1 : 0;
    sinceLast = 0;
    int mult = level << doubled;
    score += mult * 10;
    if (level < 5) {
        bars += doubleCombo && quickChain > 2 ? 2 : 1;
        barTimer = 3.0f;
        if (bars > 9) {
            ++level;
            SoundMgr::sharedSoundMgr()->playSound((doubled ? "d_score_" : "score_") + std::to_string(level), false);
            if (level < 5) bars = 0;
        }
    }
    dirty = true;
    wrongInjury = nullptr;
    wrongCount = 0;
    return mult * 10;
}

void ComboMeter::breakCombo() {
    if (hasUpgrade("score_combo_saver_upgrade")) {
        int t = bars + level * 10 - 10;
        level = t / 20 + 1;
        bars = (t / 2) % 10;
    } else {
        level = 1;
        bars = 0;
    }
    barTimer = 0;
    dirty = true;
}

int ComboMeter::coinsFor(int score, float rate, int shift) {
    return static_cast<int>(std::floor(static_cast<float>(score / 50) * rate)) << shift;
}

void ComboMeter::wrongTarget(const void* injury) {
    if (wrongInjury && wrongInjury == injury) {
        ++wrongCount;
        return;
    }
    wrongInjury = injury;
    wrongCount = 1;
}

}  // namespace mt
