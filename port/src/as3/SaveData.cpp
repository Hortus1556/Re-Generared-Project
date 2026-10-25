#include "as3/SaveData.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "as3/Cheats.h"
#include "as3/Log.h"
#include "core/Json.h"
#include "engine/cocos/CCFileUtils.h"
#include "platform/Platform.h"

namespace mt {

namespace {

const char* kFile = "state.json";  // 0x42f768
// the tag partners every new slot has a cooldown for (0x42f920)
const char* kPartners[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};

std::string path() { return cocos2d::CCFileUtils::sharedFileUtils()->getWritablePath() + kFile; }

uint64_t now() { return static_cast<uint64_t>(std::time(nullptr)); }

// the u64s are written as decimal strings ("%lld") and read with strtoull
uint64_t u64(const json::Value& v) { return v.type == json::Value::kString ? std::strtoull(v.str().c_str(), nullptr, 10) : 0; }
int asInt(const json::Value& v) {
    if (v.type == json::Value::kNumber) return static_cast<int>(v.number);
    if (v.type == json::Value::kBool) return v.boolean ? 1 : 0;
    return 0;
}
unsigned int asUInt(const json::Value& v) {
    return v.type == json::Value::kNumber ? static_cast<unsigned int>(v.number) : 0;
}
std::string decimal(uint64_t v) {
    char b[32];
    std::snprintf(b, sizeof b, "%lld", static_cast<long long>(v));
    return b;
}

}  // namespace

int SavedGame::starsOf(const std::string& surgery, int mode) const {
    auto it = stars.find(surgery);
    return it == stars.end() || mode < 0 || mode > 2 ? 0 : it->second[mode];
}

int SavedGame::totalStars() const {
    // (debug: cheat 18 reads 250, 1.45 GameStateUtils::GetStarsBalance
    // 0x138878 / GetStarsBalanceFromState 0x1388b0; Cheats.h)
    if (cheat(Cheats::kStars250)) return 250;
    int n = 0;
    for (const auto& s : stars) n += s.second[0] + s.second[1] + s.second[2];
    return n;
}

int SavedGame::coins() const {
    int n = 0;
    for (const Transaction& t : transactions)
        if (t.currency == 0) n += t.currencyInc;
    return n;
}

bool SavedGame::owns(int itemType, int item) const {
    for (const Transaction& t : transactions)
        if (t.itemType == itemType && t.itemSubtype == item) return true;
    return false;
}

void SavedGame::addTransaction(int currencyInc, int itemType, int itemSubtype, int itemInc) {
    Transaction t;
    // (the original's device id was its analytics' id for the device)
    t.deviceId = "PC";
    t.timestamp = now();
    t.currency = 0;
    t.currencyInc = currencyInc;
    t.itemType = itemType;
    t.itemSubtype = itemSubtype;
    t.itemInc = itemInc;
    transactions.push_back(t);
}

void SavedGame::choosePartner(int index, int cost) {
    selectedPartnerIndex = index;
    if (!owns(kItemPartner, index) && cost <= coins()) addTransaction(-cost, kItemPartner, index, 1);
}

SaveData& SaveData::shared() {
    static SaveData* s = [] {
        SaveData* d = new SaveData;
        d->load();
        return d;
    }();
    return *s;
}

void SaveData::load() {
    json::Value root;
    std::string text;
    size_t n = 0;
    std::string file = path();
    // (a file there that can't be read at once, held by another program a
    // moment: tried again before the save is taken for missing)
    bool there = SDL_GetPathInfo(file.c_str(), nullptr);
    for (int tries = 0; tries < (there ? 10 : 1); ++tries) {
        if (void* raw = SDL_LoadFile(file.c_str(), &n)) {  // UTF-8 path safe, unlike fopen on Windows
            text.assign(static_cast<const char*>(raw), n);
            SDL_free(raw);
            break;
        }
        if (there) SDL_Delay(100);
    }
    bool readable = !text.empty() && json::parse(text, root) && root.type == json::Value::kObject;
    if (there && !readable) {
        // Not in the original (0xe6640 reads the file as it is): a save that
        // can't be read is kept aside before the game starts afresh, as the
        // next save would write over it. The player's save is never lost.
        std::string kept = file + ".unreadable-" + decimal(now());
        if (SDL_CopyFile(file.c_str(), kept.c_str()))
            logError("The save %s can't be read: kept as %s, a new game starts", file.c_str(), kept.c_str());
        else
            logError("The save %s can't be read nor kept (%s), a new game starts", file.c_str(), SDL_GetError());
    }
    if (!readable) {
        // none yet: the defaults (0xe40c4), one new slot
        formatVersion = 1;
        monetized = false;
        newSlot();
    } else {
        formatVersion = asInt(root["formatVersion"]);
        lastLoaded = asUInt(root["lastLoaded"]);
        last1DayLocalNotificationTimestamp = u64(root["last1DayLocalNotificationTimestamp"]);
        last3DayLocalNotificationTimestamp = u64(root["last3DayLocalNotificationTimestamp"]);
        nextAppReviewPopupTimestamp = u64(root["nextAppReviewPopupTimestamp"]);
        monetized = root["monetized"].flag();
        hasLoggedIntoFacebook = root["hasLoggedIntoFacebook"].flag();  // (1.45 0x13610c)
        for (const json::Value& g : root["savedGames"].items) {
            SavedGame s;
            s.uniqueId = asUInt(g["uniqueId"]);
            s.lastSaveTimestamp = u64(g["lastSaveTimestamp"]);
            const char* modes[3] = {"starsMain", "starsChallenge1", "starsChallenge2"};
            for (int m = 0; m < 3; ++m)
                for (const auto& kv : g[modes[m]].members) s.stars[kv.first][m] = static_cast<unsigned char>(asInt(kv.second));
            for (const json::Value& t : g["completedTutorials"].items) s.completedTutorials.push_back(t.str());
            for (const auto& kv : g["statProgress"].members) s.statProgress[kv.first] = asUInt(kv.second);
            for (const auto& kv : g["highScores"].members) s.highScores[kv.first] = asUInt(kv.second);
            for (const json::Value& a : g["areasUnlocked"].items) s.areasUnlocked.push_back(asInt(a));  // (1.45)
            for (const json::Value& t : g["transactions"].items) {
                Transaction x;
                x.formatVersion = asInt(t["formatVersion"]);
                x.deviceId = t["deviceId"].str();
                x.timestamp = u64(t["timestamp"]);
                x.currency = asInt(t["currency"]);
                x.currencyInc = asInt(t["currencyInc"]);
                x.itemType = asInt(t["itemType"]);
                x.itemSubtype = asInt(t["itemSubtype"]);
                x.itemInc = asInt(t["itemInc"]);
                s.transactions.push_back(x);
            }
            s.selectedTagPartner = g["selectedTagPartner"].str();
            s.lastSelectedArea = g["lastSelectedArea"].str();
            s.sfxOn = asInt(g["sfxOn"]) != 0;
            s.musicOn = asInt(g["musicOn"]) != 0;
            // (1.45 read it into a byte, so the -1 of a slot saved before its
            // first recharge read back as 255 packs. Fixed: read as the new
            // slot it was, given its start packs at its first recharge.)
            s.numBloodPacks = asInt(g["numBloodPacks"]);
            if (s.numBloodPacks < 0 || s.numBloodPacks >= 255) s.numBloodPacks = -1;
            s.lastBloodPackTimestamp = u64(g["lastBloodPackTimestamp"]);
            for (const auto& kv : g["tag_cooldowns"].members) s.tagCooldowns[kv.first] = asUInt(kv.second);
            s.lastFailedSurgery = g["lastFailedSurgery"].str();
            s.lastFailedSurgeryMode = asInt(g["lastFailedSurgeryMode"]);
            s.lastFailedSurgeryStage = asInt(g["lastFailedSurgeryStage"]);
            s.seenHospitalUnlockedPopup = asInt(g["seenHospitalUnlockedPopup"]) != 0;  // (Android 1.43, iOS 1.45)
            s.seenPocketgodsUnlockedPopup = asInt(g["seenPocketgodsUnlockedPopup"]) != 0;  // (1.45)
            s.directorsCut = asInt(g["directorsCut"]) != 0;  // (the port's; RestoredContent.h)
            savedGames.push_back(s);
        }
    }
    for (SavedGame& s : savedGames) s.loadingTip = -1;
}

SavedGame& SaveData::currentSlot() {
    for (SavedGame& s : savedGames)
        if (s.uniqueId == lastLoaded) return s;
    if (savedGames.empty()) return newSlot();
    return savedGames.front();
}

SavedGame& SaveData::newSlot() {
    SavedGame s;
    // a random id no slot has
    auto taken = [this](unsigned int id) {
        for (const SavedGame& g : savedGames)
            if (g.uniqueId == id) return true;
        return false;
    };
    do {
        s.uniqueId = static_cast<unsigned int>(std::rand()) & 0x7fffffff;
    } while (taken(s.uniqueId));
    s.lastSaveTimestamp = now();
    s.numBloodPacks = -1;  // (1.45 AddNewGameStateToGroup 0x133510) start_bloodpacks at its first recharge
    s.lastBloodPackTimestamp = now();
    s.sfxOn = s.musicOn = true;
    for (const char* p : kPartners) s.tagCooldowns[p] = 0;
    savedGames.push_back(s);
    lastLoaded = s.uniqueId;
    return savedGames.back();
}

void SaveData::saveGame() {
    log("Save game state");
    currentSlot().lastSaveTimestamp = now();
    save();
}

void SaveData::save() {
    json::Writer w(true);
    w.beginObject();
    w.field("formatVersion", formatVersion);
    w.key("lastLoaded").value(static_cast<long long>(lastLoaded));
    w.field("last1DayLocalNotificationTimestamp", decimal(last1DayLocalNotificationTimestamp));
    w.field("last3DayLocalNotificationTimestamp", decimal(last3DayLocalNotificationTimestamp));
    w.field("nextAppReviewPopupTimestamp", decimal(nextAppReviewPopupTimestamp));
    w.field("monetized", monetized);
    w.field("hasLoggedIntoFacebook", hasLoggedIntoFacebook);  // (1.45: GameStateGroupAsString 0x133f0c)
    w.key("savedGames").beginArray();
    for (const SavedGame& s : savedGames) {
        w.beginObject();
        w.key("uniqueId").value(static_cast<long long>(s.uniqueId));
        w.field("lastSaveTimestamp", decimal(s.lastSaveTimestamp));
        const char* modes[3] = {"starsMain", "starsChallenge1", "starsChallenge2"};
        for (int m = 0; m < 3; ++m) {
            w.key(modes[m]).beginObject();
            for (const auto& kv : s.stars) w.field(kv.first, static_cast<int>(kv.second[m]));
            w.endObject();
            if (m == 0) {
                // written by the original, never read back
                w.key("upgradesMain").beginObject().endObject();
                w.key("partnersMain").beginObject().endObject();
            }
        }
        w.key("completedTutorials").beginArray();
        for (const std::string& t : s.completedTutorials) w.value(t);
        w.endArray();
        w.key("statProgress").beginObject();
        for (const auto& kv : s.statProgress) w.key(kv.first).value(static_cast<long long>(kv.second));
        w.endObject();
        w.key("highScores").beginObject();
        for (const auto& kv : s.highScores) w.key(kv.first).value(static_cast<long long>(kv.second));
        w.endObject();
        w.key("areasUnlocked").beginArray();
        for (int a : s.areasUnlocked) w.value(static_cast<long long>(a));
        w.endArray();
        w.key("transactions").beginArray();
        for (const Transaction& t : s.transactions) {
            w.beginObject();
            w.field("formatVersion", t.formatVersion);
            w.field("deviceId", t.deviceId);
            w.field("timestamp", decimal(t.timestamp));
            w.field("currency", t.currency);
            w.field("currencyInc", t.currencyInc);
            w.field("itemType", t.itemType);
            w.field("itemSubtype", t.itemSubtype);
            w.field("itemInc", t.itemInc);
            w.endObject();
        }
        w.endArray();
        w.field("selectedTagPartner", s.selectedTagPartner);
        w.field("lastSelectedArea", s.lastSelectedArea);
        w.field("sfxOn", s.sfxOn);
        w.field("musicOn", s.musicOn);
        w.field("numBloodPacks", s.numBloodPacks);
        w.field("lastBloodPackTimestamp", decimal(s.lastBloodPackTimestamp));
        w.key("tag_cooldowns").beginObject();
        for (const auto& kv : s.tagCooldowns) w.key(kv.first).value(static_cast<long long>(kv.second));
        w.endObject();
        w.field("lastFailedSurgery", s.lastFailedSurgery);
        w.field("lastFailedSurgeryMode", s.lastFailedSurgeryMode);
        w.field("lastFailedSurgeryStage", s.lastFailedSurgeryStage);
        w.field("seenHospitalUnlockedPopup", s.seenHospitalUnlockedPopup);  // (1.45 0x133f0c; Android 1.43 0x4b21dc)
        w.field("seenPocketgodsUnlockedPopup", s.seenPocketgodsUnlockedPopup);
        w.field("directorsCut", s.directorsCut);  // (the port's)
        w.endObject();
    }
    w.endArray();
    w.endObject();
    // a temporary file renamed over the save: a crash mid-write leaves the old one whole
    std::string file = path(), tmp = file + ".tmp";
    if (!SDL_SaveFile(tmp.c_str(), w.text().data(), w.text().size()) || !SDL_RenamePath(tmp.c_str(), file.c_str()))
        logError("Cannot write the save %s: %s", file.c_str(), SDL_GetError());
}

}  // namespace mt
