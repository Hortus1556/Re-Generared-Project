// The tag partners (a singleton, 0x209978; 0x1a8 bytes; vtable 0x41a088):
// up to 8 from the downloaded content's partners.json, in their
// partner_order (Mr Giblets first), each with its texts, its price, its
// tag power's tutorial and its cooldown.
//
// iOS 1.45 adds a free partner a day: free_daily_partner_list.json names nine
// partners in turn, one a day (days since 1970, local time, modulo nine); that
// one comes first in the list and can be taken without being bought (its
// cooldown still holds), with an "employee of the day" badge.
//
// A partner is known to the save by its index in the game's list of ids (the
// map every unit built from that list, here 0x43f174): giblets 0, saturday 1,
// reverend 2, flow 3, bot 4, nowe 5, crumpet 6, brutality 7. Bought, it is a
// transaction of item type 2 with that index (SavedGame::owns). Its
// cooldown is the time its power was last used, in the slot's tag_cooldowns.
//
// The port's change (the user's, docs/amateur-surgeon-3.md): every cooldown
// is the original's divided by kPartnerCooldownDivisor.
#pragma once

#include <string>
#include <vector>

namespace mt {

// The port's change, not the original's: the partners' cooldowns are 20
// times shorter than partners.json gives (Mr Giblets' 25 minutes become 75
// seconds, Procrastibot's 23 h 20 min become 70 minutes). Applied once, as
// the partners are read, so every use (the time left, the bars) follows.
constexpr int kPartnerCooldownDivisor = 20;

// The partners' ids by index (0x43f18c...), and the names their art and
// dialogues use (0x43f1ac...: "Reverand" as the art spells it).
extern const char* const kPartnerIds[8];
extern const char* const kPartnerNames[8];
// An id's index (-1 if none), its name ("" if none; 0x1ff378).
int partnerIndex(const std::string& id);
std::string partnerName(const std::string& id);

struct PartnerInfo {  // 0x34 bytes
    int order = 0;                // +0: partner_order
    std::string id;               // +4
    std::string name;             // +8: partner_name
    std::string bio;              // +0xc
    std::string skills;           // +0x10
    std::string stats;            // +0x14
    // +0x18: not partner_unlocked_ingame: bought with coins; else given by
    // the story (Mr Giblets, Lady Saturday, the Reverend), shown locked till then
    bool purchasable = false;
    int cost = 0;                 // +0x1c: partner_cost, coins
    std::string tutorialTitle;    // +0x20
    std::string tutorialDesc;     // +0x24
    int actionsToUnlock = 0;      // +0x28
    int cooldownDuration = 0;     // +0x2c: seconds (the original's / kPartnerCooldownDivisor)
    int endCooldownCost = 0;      // +0x30: coins to end a cooldown at once
    // (1.45 +0x34) today's free partner: taken without being bought
    // (LoadPartners 0x2d2744; the list: SettingsInfoUtils::LoadFreePartnerList 0x3ca708)
    bool freeToday = false;
};

class PartnerInfoUtils {
public:
    static PartnerInfoUtils* shared();  // 0x209978
    std::vector<PartnerInfo> partners;  // +4: 8 at most; their count +0x1a4

    // a partner by id (0x20a948); null if none (the original read on past
    // its list)
    const PartnerInfo* partner(const std::string& id) const;
    // today's free partner's id ("" if none)
    std::string freeToday;

private:
    PartnerInfoUtils() = default;
    void load();  // 0x2099ec
};

// Among the save's functions in the original:
// the seconds left of a partner's cooldown, 0 if none (0xe9314): its last
// use (tag_cooldowns) plus its cooldown, less now
int partnerCooldownLeft(const std::string& id);
// a partner's tag_cooldowns set, and the game saved (0xe9494)
void setPartnerCooldown(const std::string& id, unsigned int value);
// a partner's cooldown begun now, its power used: tag_cooldowns = now, and
// the game saved (0xe9468, called by the surgery)
void startPartnerCooldown(const std::string& id);
// a cooldown ended for coins: a transaction of item type 5 if the coins
// allow (0xe84d8)
void payPartnerCooldown(int index, int cost);

}  // namespace mt
