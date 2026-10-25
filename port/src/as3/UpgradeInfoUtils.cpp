#include "as3/UpgradeInfoUtils.h"

#include "as3/Content.h"
#include "as3/SaveData.h"

namespace mt {

const char* const kUpgradeIds[30] = {
    "cutting_cutter_use_upgrade", "cutting_chainsaw_upgrade", "cutting_cutter_damage_upgrade",
    "cutting_chainsaw_damage_upgrade", "cutting_tazer_use_upgrade", "cutting_chainsaw_timer_boost",
    "binding_lighter_first_upgrade", "binding_lighter_use_upgrade", "binding_4_clamp_upgrade",
    "binding_heal_boost_upgrade", "binding_staple_upgrade", "binding_clamp_use_upgrade",
    "healing_injection_fluid_upgrade", "healing_gel_upgrade", "healing_injection_cooldown_upgrade",
    "healing_injection_speed_upgrade", "healing_injection_use_upgrade", "healing_death_shield_upgrade",
    "extraction_scanner_cooldown_upgrade", "extraction_vacuum_upgrade", "extraction_scanner_arrow_upgrade",
    "extraction_speed_upgrade", "extraction_scanner_global_upgrade", "extraction_heartrate_upgrade",
    "score_timer_upgrade", "score_double_combo_upgrade", "score_tag_team_upgrade",
    "score_reduced_tag_team_cooldown", "score_combo_saver_upgrade", "score_timeslow_upgrade",
};

int upgradeIndex(const std::string& id) {
    for (int i = 0; i < 30; ++i)
        if (id == kUpgradeIds[i]) return i;
    return -1;
}

UpgradeInfoUtils* UpgradeInfoUtils::shared() {
    static UpgradeInfoUtils* u = [] {
        UpgradeInfoUtils* x = new UpgradeInfoUtils;
        x->load();
        return x;
    }();
    return u;
}

void UpgradeInfoUtils::load() {
    json::Value list, unlocks;
    if (readContent("upgrades.json", list))
        for (const json::Value& u : list.items) {
            if (upgrades.size() >= 30) break;
            // "Bleed Buddies" (score_timeslow_upgrade: "quick combos slow down
            // the game timer") is read by no version of the game's code (1.02,
            // Android 1.43, 1.45): it could be bought and did nothing, so the
            // port leaves it out. Its index in kUpgradeIds stays, so saves that
            // bought it still read.
            if (u["id"].str() == "score_timeslow_upgrade") continue;
            UpgradeDef d;
            d.id = u["id"].str();
            d.name = u["name"].str();
            d.description = u["description"].str();
            d.column = u["column"].integer() & 0xff;
            d.row = u["row"].integer() & 0xff;
            d.cost = u["cost"].integer() & 0xffff;
            upgrades.push_back(d);
        }
    if (readContent("upgrade_unlocks.json", unlocks))
        for (const json::Value& u : unlocks.items) {
            const std::string& id = u["id"].str();
            for (int r = 0; r < 6; ++r)
                if (id == "row_" + std::to_string(r + 1) + "_stars_unlock") rowStars[r] = u["cost_to_unlock"].integer();
        }
}

int UpgradeInfoUtils::affordable(int stars, int coins) const {
    const SavedGame& slot = SaveData::shared().currentSlot();
    int n = 0;
    for (const UpgradeDef& u : upgrades) {
        bool below = true;  // the one under it in its column bought (row 1: none)
        if (u.row > 1) {
            below = false;
            for (const UpgradeDef& v : upgrades)
                if (v.row == u.row - 1 && v.column == u.column) {
                    below = slot.owns(SavedGame::kItemUpgrade, upgradeIndex(v.id));
                    break;
                }
        }
        int rowNeeds = u.row >= 1 && u.row <= 6 ? rowStars[u.row - 1] : 0;
        if (rowNeeds <= stars && u.cost <= coins && !slot.owns(SavedGame::kItemUpgrade, upgradeIndex(u.id)) && below) ++n;
    }
    return n;
}

}  // namespace mt
