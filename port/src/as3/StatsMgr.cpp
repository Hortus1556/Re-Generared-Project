#include "as3/StatsMgr.h"

#include <algorithm>

#include "as3/AchievementsScreen.h"
#include "as3/Content.h"
#include "as3/Log.h"
#include "as3/SaveData.h"
#include "as3/UpgradeInfoUtils.h"

namespace mt {

const char* const kStatNames[StatTypes::kCount] = {
    "Achieve3StarsOnSurgery", "ClampMaggot",     "CollectBlueStar",    "CollectRedStar",
    "CollectYellowStar",      "CompleteBonusChallenge", "CompleteSuddenDeathSurgery", "CureSuperPoisonPatch",
    "ExtractBatMutagen",      "ExtractItem",     "KillPatient",        "RemoveLeechNotLeavingTeeth",
    "ReplaceLimb",            "ReplaceOrgan",    "RevivePatient",      "UnlockLadySaturday",
    "UnlockReverend",         "UpgradeTech",     "Use4ClampsAtOnce",   "UseTagPower",
    "ZapFlea",                "TaggedGiblets",   "TaggedFlow",         "TaggedReverand",
    "TaggedBot",              "TaggedSaturday",  "TaggedBeetle",       "UsedBloodpack",
    "TaggedNowe",             "TaggedCrumpet",   "TaggedBrutality",
};

namespace {

// The port's titles, descriptions and icons (none ever shipped: Game
// Center's servers held them). The titles follow the ids; the icons are
// frames of upgrade_screen_sheet and preop_common, the sheets the list loads
// (an upgrade icon's "-grey" twin shows it not yet earned).
struct AchievementText {
    const char* id;
    const char* title;
    const char* description;
    const char* icon;
};
const AchievementText kAchievementTexts[] = {
    {"as3ach_high5", "High Five!", "Use a tag partner's power.", "ui_tagpartner-Giblets"},
    {"as3ach_tagyourit", "Tag, You're It!", "Use tag partner powers 25 times.", "ui_tagpartner-Flowmaster"},
    {"as3ach_ladysaturday", "Lady Saturday", "Win Lady Saturday over as a tag partner.", "ui_tagpartner-Saturday"},
    {"as3ach_reverend", "The Reverend", "Win the Reverend over as a tag partner.", "ui_tagpartner-Reverand"},
    {"as3ach_kleptomaniac", "Kleptomaniac", "Pull 5 objects out of your patients.", "upgrades_icons_green04"},
    {"as3ach_10bluestars", "Blue Ribbon", "Collect 10 blue stars from first challenges.", "star_panel_bigstar_blue"},
    {"as3ach_10redstars", "Seeing Red", "Collect 10 red stars from second challenges.", "star_panel_bigstar_red"},
    {"as3ach_contender", "Contender", "Win a bonus challenge.", "preop_star_bonus1"},
    {"as3ach_holybatjuice", "Holy Bat Juice!", "Draw the mutagen out of a bat before it hatches.", "upgrades_icons_red01"},
    {"as3ach_holdstillmaggot", "Hold Still, Maggot!", "Clamp down a fire maggot.", "upgrades_icons_orange06"},
    {"as3ach_clampetyclampclamp", "Clampety Clamp Clamp", "Have 4 clamps on at once.", "upgrades_icons_orange02"},
    {"as3ach_3stars", "Gold Standard", "Win a surgery with 3 stars.", "starpanel_star_on_yellow"},
    {"as3ach_fleacircus", "Flea Circus", "Zap 50 fleas.", "upgrades_icons_blue05"},
    {"as3ach_freshmeat", "Fresh Meat", "Put in 100 new organs.", "upgrades_icons_red04"},
    {"as3ach_allthestars", "All the Stars", "Collect 69 yellow stars.", "preop_star_main"},
    {"as3ach_technicalwizard", "Technical Wizard", "Buy every upgrade in the Upgrades Machine.", "upgrades_icons_purple03"},
    {"as3ach_necromancer", "Necromancer", "Bring 5 patients back from the dead.", "upgrades_icons_red06"},
    {"as3ach_acleanbreak", "A Clean Break", "Burn a leech, then pull it off without leaving its teeth.", "upgrades_icons_orange01"},
    {"as3ach_unflappable", "Unflappable", "Win 5 sudden death challenges.", "preop_star_bonus2"},
    {"as3ach_expertvirologist", "Expert Virologist", "Cure 5 super poison patches.", "upgrades_icons_red05"},
    {"as3ach_lifeandlimb", "Life and Limb", "Put on 10 new limbs.", "upgrades_icons_orange05"},
    {"as3ach_massmurderer", "Mass Murderer", "Lose 50 patients. Oops.", "upgrades_icons_blue04"},
};

int statByName(const std::string& name) {
    for (int i = 0; i < StatTypes::kCount; ++i)
        if (name == kStatNames[i]) return i;
    return -1;
}

}  // namespace

StatsMgr* StatsMgr::sharedInstance() {
    static StatsMgr* s = new StatsMgr;
    return s;
}

StatsMgr::StatsMgr() { loadAchievementDefinitions(); }

void StatsMgr::loadAchievementDefinitions() {
    // (0x2f4eb0) Data/achievements.json, in its order
    json::Value root;
    if (!readJson("Data/achievements.json", root)) return;
    for (const json::Value& a : root.items) {
        AchievementDef d;
        d.id = a["achievementId"].str();
        d.statType = statByName(a["statType"].str());
        d.countToUnlock = static_cast<int>(a["countToUnlock"].integer());
        if (d.statType < 0) {
            logError("Achievement %s: unknown stat %s", d.id.c_str(), a["statType"].str().c_str());
            continue;
        }
        // Technical Wizard asks 30 upgrades: the original's tech tree had
        // 30, the port's 29 (it leaves out the dead "Bleed Buddies",
        // docs/as3-unused.md): every upgrade on sale, then
        if (d.statType == StatTypes::kUpgradeTech) {
            const int onSale = static_cast<int>(UpgradeInfoUtils::shared()->upgrades.size());
            if (onSale > 0) d.countToUnlock = std::min(d.countToUnlock, onSale);
        }
        for (const AchievementText& t : kAchievementTexts)
            if (d.id == t.id) {
                d.title = t.title;
                d.description = t.description;
                d.icon = t.icon;
            }
        if (d.title.empty()) d.title = d.id;
        m_defs.push_back(d);
    }
}

int StatsMgr::getStatValue(StatTypes::Enum stat) const {
    if (stat < 0 || stat >= StatTypes::kCount || SaveData::shared().savedGames.empty()) return 0;
    const auto& m = SaveData::shared().currentSlot().statProgress;
    auto it = m.find(kStatNames[stat]);
    return it == m.end() ? 0 : static_cast<int>(it->second);
}

void StatsMgr::setStatValue(StatTypes::Enum stat, int value) {
    // (0x2f56b8) the slot's counter; each achievement of that stat whose
    // count it now reaches, and didn't before, is earned (the original told
    // Game Center, whose banner showed it). The save follows with the
    // caller's own, as the original's.
    if (stat < 0 || stat >= StatTypes::kCount) return;
    SavedGame& slot = SaveData::shared().currentSlot();
    const int before = getStatValue(stat);
    slot.statProgress[kStatNames[stat]] = static_cast<unsigned int>(std::max(0, value));
    for (const AchievementDef& a : m_defs)
        if (a.statType == stat && before < a.countToUnlock && a.countToUnlock <= value) {
            log("achievement earned %s (%s %d/%d)", a.id.c_str(), kStatNames[stat], value, a.countToUnlock);
            AchievementPopup::show(a);
        }
}

void StatsMgr::incrementStat(StatTypes::Enum stat, int amount) {
    // (0x2f56ac)
    if (amount == 0) return;
    setStatValue(stat, getStatValue(stat) + amount);
}

void StatsMgr::incrementStat(const std::string& name, int amount) {
    int stat = statByName(name);
    if (stat >= 0) incrementStat(static_cast<StatTypes::Enum>(stat), amount);
}

bool StatsMgr::earned(const AchievementDef& a) const {
    return getStatValue(static_cast<StatTypes::Enum>(a.statType)) >= a.countToUnlock;
}

int StatsMgr::earnedCount() const {
    int n = 0;
    for (const AchievementDef& a : m_defs)
        if (earned(a)) ++n;
    return n;
}

void StatsMgr::catchUp() {
    if (!SaveData::shared().savedGames.empty()) catchUp(SaveData::shared().currentSlot());
}

void StatsMgr::catchUp(SavedGame& slot) const {
    // what the slot's stars, partners and upgrades say was done already
    int yellow = 0, blue = 0, red = 0, three = 0;
    for (const auto& s : slot.stars) {
        yellow += s.second[0];
        blue += s.second[1] != 0;
        red += s.second[2] != 0;
        three += s.second[0] >= 3;
    }
    int upgrades = 0;
    for (const UpgradeDef& u : UpgradeInfoUtils::shared()->upgrades)
        if (slot.owns(SavedGame::kItemUpgrade, upgradeIndex(u.id))) ++upgrades;
    auto raise = [&slot](StatTypes::Enum stat, int v) {
        unsigned int& n = slot.statProgress[kStatNames[stat]];
        if (static_cast<int>(n) < v) n = static_cast<unsigned int>(v);
    };
    raise(StatTypes::kCollectYellowStar, yellow);
    raise(StatTypes::kCollectBlueStar, blue);
    raise(StatTypes::kCollectRedStar, red);
    raise(StatTypes::kAchieve3StarsOnSurgery, three);
    raise(StatTypes::kUpgradeTech, upgrades);
    // the two partners the story gives (deceasedkeith, reverendsorgan)
    if (slot.starsOf("surgery_mumbojungle_deceasedkeith", 0) > 0) raise(StatTypes::kUnlockLadySaturday, 1);
    if (slot.starsOf("surgery_robotopia_reverendsorgan", 0) > 0) raise(StatTypes::kUnlockReverend, 1);
}

}  // namespace mt
