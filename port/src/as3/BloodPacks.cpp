#include "as3/BloodPacks.h"

#include <cstdint>
#include <cstdio>
#include <ctime>

#include "as3/Log.h"
#include "as3/SaveData.h"
#include "as3/SettingsInfoUtils.h"

namespace mt {
namespace GameStateUtils {

namespace {

int64_t now() { return static_cast<int64_t>(std::time(nullptr)); }

}  // namespace

// (the settings as the bundle's default_data.json has them: 3, 3, 1800; a
// value missing or broken falls back on those, where the original's -1 made
// no sense)
int MaxBloodPacks() {
    int v = SettingsInfoUtils::shared()->value(kSettingMaxBloodpacks);
    return v > 0 ? v : 3;
}

int StartBloodPacks() {
    int v = SettingsInfoUtils::shared()->value(kSettingStartBloodpacks);
    return v >= 0 ? v : MaxBloodPacks();
}

int RechargeSecs() {
    int v = SettingsInfoUtils::shared()->value(kSettingBloodpackRechargeSecs);
    return v > 0 ? v : 1800;
}

bool UpdateBloodPacksAndTime(SavedGame& slot) {
    // (1.45 0x1388e4)
    if (slot.numBloodPacks == -1) {
        slot.numBloodPacks = StartBloodPacks();
        log("blood packs: a new slot's %d", slot.numBloodPacks);
        return true;
    }
    const int most = MaxBloodPacks();
    if (slot.numBloodPacks >= most) return false;
    const int64_t t = now();
    int64_t last = static_cast<int64_t>(slot.lastBloodPackTimestamp);
    bool changed = false;
    // the clock put back: counted from now
    if (last > t) {
        last = t;
        changed = true;
    }
    // one back for every whole recharge since the timestamp, which moves on
    // by as much each time, up to the most
    const int64_t secs = RechargeSecs();
    const int before = slot.numBloodPacks;
    while (slot.numBloodPacks < most && last <= t - secs) {
        last += secs;
        ++slot.numBloodPacks;
        changed = true;
    }
    if (slot.numBloodPacks != before) log("blood packs: %d back, %d now", slot.numBloodPacks - before, slot.numBloodPacks);
    slot.lastBloodPackTimestamp = static_cast<uint64_t>(last);
    return changed;
}

std::string GetTimeUntilNextBloodpack() {
    // (1.45 0x138d7c)
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (UpdateBloodPacksAndTime(slot)) save.saveGame();
    if (slot.numBloodPacks >= MaxBloodPacks()) return "Full";
    long long left = RechargeSecs() + (static_cast<long long>(slot.lastBloodPackTimestamp) - now());
    if (left < 0) left = 0;
    char b[32];
    // Fixed: the original wrote the last minute "00:%d" ("00:5" five seconds
    // before); it is zero-padded as the minutes before it ("%d:%02d")
    if (left < 60) std::snprintf(b, sizeof b, "00:%02lld", left);
    else std::snprintf(b, sizeof b, "%lld:%02lld", left / 60, left % 60);
    return b;
}

void UseBloodPack(SavedGame& slot) {
    // (1.45 0x138eec) from a full slot, the count to the next starts now
    // (the original asked == the most; >=, so a slot above it, which only a
    // purchase could make, counts from when it drops below)
    if (slot.numBloodPacks >= MaxBloodPacks()) slot.lastBloodPackTimestamp = static_cast<uint64_t>(now());
    if (slot.numBloodPacks > 0) --slot.numBloodPacks;
}

void AddBloodPacks(SavedGame& slot, int n) {
    // (1.45 0x138f20 added them as they came; 1.02's win, 0xe92d0, stopped at
    // the most, as here: nothing in the port gives more than the most)
    UpdateBloodPacksAndTime(slot);
    const int most = MaxBloodPacks();
    slot.numBloodPacks += n;
    if (slot.numBloodPacks > most) slot.numBloodPacks = most;
    if (slot.numBloodPacks < 0) slot.numBloodPacks = 0;
}

void RefillBloodPacks() {
    // (1.45 0x138f2c)
    SaveData& save = SaveData::shared();
    save.currentSlot().numBloodPacks = MaxBloodPacks();
    save.saveGame();
}

void UpdateBloodPacksAndSave() {
    SaveData& save = SaveData::shared();
    if (UpdateBloodPacksAndTime(save.currentSlot())) save.saveGame();
}

bool BloodPacksInUse(const SavedGame& slot) {
    return slot.starsOf("surgery_bunker_mrgiblets", 0) != 0 && slot.starsOf("surgery_bunker_prxy4", 0) != 0;
}

}  // namespace GameStateUtils
}  // namespace mt
