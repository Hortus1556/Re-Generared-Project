// mt::StatsMgr (1.45 0x2f4d80…0x2f5750; 1.02's names table 0x440ad4): the
// player's counters (StatTypes) and the 22 achievements of
// Data/achievements.json ({achievementId, statType, countToUnlock}, read by
// loadAchievementDefinitions 0x2f4eb0 into mt::AchievementDef, 12 bytes).
//
// The original kept the counters in memory, loaded once from the slot shown
// first (loadStatProgress 0x2f5354), wrote each change back to the slot's
// statProgress (setStatValue 0x2f56b8), and when a counter passed an
// achievement's count it reported it to Game Center
// (GamingSocialNetworkMgr::unlockAchievement 0x30be6c: percentComplete 100,
// showsCompletionBanner, so Game Center's own banner showed it). Titles,
// descriptions and pictures lived on Game Center's servers: none ships.
//
// The port keeps the achievements locally: the counters are the current
// slot's statProgress itself (so each slot has its own; fixed: the
// original's counters stayed those of the first slot loaded), an
// achievement is earned when its counter reaches its count, and the moment
// one is passed the AchievementPopup banner shows it (AchievementsScreen.h).
// The list is AchievementsScreen, from the title's top-left button (the
// original's Game Center button, which opened Game Center's achievements).
// Titles, descriptions and icons are the port's (kAchievementTexts).
#pragma once

#include <string>
#include <vector>

namespace mt {

struct SavedGame;

namespace StatTypes {
// (the names at 1.02 0x440ad4 / 1.45 0x675998, in this order)
enum Enum {
    kAchieve3StarsOnSurgery = 0,
    kClampMaggot,
    kCollectBlueStar,
    kCollectRedStar,
    kCollectYellowStar,
    kCompleteBonusChallenge,
    kCompleteSuddenDeathSurgery,
    kCureSuperPoisonPatch,
    kExtractBatMutagen,
    kExtractItem,
    kKillPatient,  // 10
    kRemoveLeechNotLeavingTeeth,
    kReplaceLimb,
    kReplaceOrgan,
    kRevivePatient,
    kUnlockLadySaturday,
    kUnlockReverend,
    kUpgradeTech,
    kUse4ClampsAtOnce,
    kUseTagPower,
    kZapFlea,  // 20
    kTaggedGiblets,
    kTaggedFlow,
    kTaggedReverand,
    kTaggedBot,
    kTaggedSaturday,
    kTaggedBeetle,  // the cut ninth partner's
    kUsedBloodpack,
    kTaggedNowe,
    kTaggedCrumpet,
    kTaggedBrutality,  // 30
    kCount
};
}  // namespace StatTypes

extern const char* const kStatNames[StatTypes::kCount];

// (1.45 mt::AchievementDef: the id, the stat, the count)
struct AchievementDef {
    std::string id;
    int statType = 0;
    int countToUnlock = 1;
    // the port's: what the list and the banner show
    std::string title, description, icon;
};

class StatsMgr {
public:
    static StatsMgr* sharedInstance();  // 0x2f53b8

    const std::vector<AchievementDef>& achievements() const { return m_defs; }
    // (0x2f56ac / 0x2f56b8) a counter of the current slot; an achievement
    // passed shows its banner
    void incrementStat(StatTypes::Enum stat, int amount = 1);
    void incrementStat(const std::string& name, int amount = 1);
    void setStatValue(StatTypes::Enum stat, int value);
    int getStatValue(StatTypes::Enum stat) const;  // 0x2f5748
    bool earned(const AchievementDef& a) const;
    int earnedCount() const;
    // The counters a slot's stars, partners and upgrades already show, for
    // slots saved before the port counted them (not the original's: it
    // counted from 1.0's first launch). Raised only, silently; called as a
    // screen that counts loads (the surgery, the upgrades, the list), before
    // anything it counts is added to the slot.
    void catchUp(SavedGame& slot) const;
    void catchUp();  // the current slot's

private:
    StatsMgr();
    void loadAchievementDefinitions();  // 0x2f4eb0
    std::vector<AchievementDef> m_defs;
};

}  // namespace mt
