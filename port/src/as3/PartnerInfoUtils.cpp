#include "as3/PartnerInfoUtils.h"

#include <ctime>

#include "as3/Content.h"
#include "as3/SaveData.h"

namespace mt {

const char* const kPartnerIds[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};
const char* const kPartnerNames[8] = {"Giblets", "Saturday", "Reverand", "Flow", "Bot", "Nowe", "Crumpet", "Brutality"};

namespace {

// a transaction ending a partner's cooldown for coins (0xe84d8)
constexpr int kItemEndCooldown = 5;

}  // namespace

int partnerIndex(const std::string& id) {
    for (int i = 0; i < 8; ++i)
        if (id == kPartnerIds[i]) return i;
    return -1;
}

std::string partnerName(const std::string& id) {
    int i = partnerIndex(id);
    return i < 0 ? std::string() : kPartnerNames[i];
}

PartnerInfoUtils* PartnerInfoUtils::shared() {
    static PartnerInfoUtils* p = [] {
        PartnerInfoUtils* x = new PartnerInfoUtils;
        x->load();
        return x;
    }();
    return p;
}

void PartnerInfoUtils::load() {
    json::Value list;
    if (!readContent("partners.json", list)) return;
    // every known partner as read, then put in their partner_order (0 to 8)
    std::vector<PartnerInfo> read;
    for (const json::Value& p : list.items) {
        if (partnerIndex(p["id"].str()) < 0 || read.size() >= 8) continue;
        PartnerInfo d;
        d.id = p["id"].str();
        d.order = p["partner_order"].integer();
        d.name = p["partner_name"].str();
        d.bio = p["partner_bio"].str();
        d.skills = p["partner_skills"].str();
        d.stats = p["partner_stats"].str();
        d.cost = p["partner_cost"].integer();
        d.purchasable = !p["partner_unlocked_ingame"].flag();
        d.tutorialTitle = p["tutorial_title"].str();
        d.tutorialDesc = p["tutorial_desc"].str();
        d.actionsToUnlock = p["actions_to_unlock"].integer();
        // the port's change: the cooldowns shortened (kPartnerCooldownDivisor)
        d.cooldownDuration = p["cooldown_duration"].integer() / kPartnerCooldownDivisor;
        d.endCooldownCost = p["end_cooldown_cost"].integer();
        read.push_back(d);
    }
    // (1.45 0x2d2744) today's free partner: the list's names ("tag_partners.<id>")
    // in turn by the days since 1970, local time (TimeUtils::LocalDeviceTime)
    json::Value free;
    if (readContent("free_daily_partner_list.json", free) && free.size()) {
        const json::Value& list = free[0]["free_partner_list"];
        if (list.size()) {
            std::time_t now = std::time(nullptr);
            std::tm utc = *std::gmtime(&now);
            utc.tm_isdst = -1;
            const long long offset = static_cast<long long>(now) - static_cast<long long>(std::mktime(&utc));
            long long day = (static_cast<long long>(now) + offset) / 86400;
            std::string name = list[static_cast<size_t>(day % static_cast<long long>(list.size()))].str();
            freeToday = name.size() > 13 ? name.substr(13) : name;  // (13: "tag_partners.")
        }
    }
    int freeOrder = -1;
    for (PartnerInfo& d : read)
        if (d.id == freeToday) {
            d.freeToday = true;
            freeOrder = d.order;
        }
    // (the original took, for each order from 0 to 8, the first partner of
    // it, and for an order no partner had, the one taken last again; the
    // content has each of 1 to 8 once. 1.45 takes the free one first, the
    // others after it in their order)
    auto place = [freeOrder](const PartnerInfo& d) {
        if (freeOrder < 0) return d.order;
        if (d.order == freeOrder) return 0;
        return d.order < freeOrder ? d.order : d.order - 1;
    };
    for (int order = 0; order <= 8; ++order)
        for (const PartnerInfo& d : read)
            if (place(d) == order) {
                partners.push_back(d);
                break;
            }
}

const PartnerInfo* PartnerInfoUtils::partner(const std::string& id) const {
    for (const PartnerInfo& p : partners)
        if (p.id == id) return &p;
    return nullptr;
}

int partnerCooldownLeft(const std::string& id) {
    SavedGame& slot = SaveData::shared().currentSlot();
    const PartnerInfo* p = PartnerInfoUtils::shared()->partner(id);
    long long now = static_cast<long long>(std::time(nullptr));
    long long left = static_cast<long long>(slot.tagCooldowns[id]) - now + (p ? p->cooldownDuration : 0);
    return left < 1 ? 0 : static_cast<int>(left);
}

void setPartnerCooldown(const std::string& id, unsigned int value) {
    SaveData& save = SaveData::shared();
    save.currentSlot().tagCooldowns[id] = value;
    save.saveGame();
}

void startPartnerCooldown(const std::string& id) {
    setPartnerCooldown(id, static_cast<unsigned int>(std::time(nullptr)));
}

void payPartnerCooldown(int index, int cost) {
    SavedGame& slot = SaveData::shared().currentSlot();
    if (cost <= slot.coins()) slot.addTransaction(-cost, kItemEndCooldown, index, 1);
}

}  // namespace mt
