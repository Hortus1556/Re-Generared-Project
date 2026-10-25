// The game's save (the global at 0x42f9dc; re/as3/notes/save.md): state.json
// in the save folder, in the original's own format, so the iOS game's file
// reads as it is. Its names are the port's (the binary has none), the JSON
// keys the original's.
//
// The save holds slots (savedGames); the current one is the last loaded.
// A slot's coins and purchases are its transactions: each reward or
// purchase is one entry (currencyInc, itemInc), the sum being what it has.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mt {

struct Transaction {  // 0x24 bytes
    int formatVersion = 1;
    std::string deviceId;
    uint64_t timestamp = 0;
    int currency = 0;
    int currencyInc = 0;
    int itemType = 0;
    int itemSubtype = 0;
    int itemInc = 0;
};

struct SavedGame {  // 0xc8 bytes
    unsigned int uniqueId = 0;                                   // +0
    uint64_t lastSaveTimestamp = 0;                              // +8
    std::map<std::string, std::array<unsigned char, 3>> stars;   // +0x14: main, challenge 1, challenge 2
    std::vector<std::string> completedTutorials;
    std::map<std::string, unsigned int> statProgress;
    std::map<std::string, unsigned int> highScores;
    std::vector<Transaction> transactions;
    std::string selectedTagPartner;
    std::string lastSelectedArea;
    bool sfxOn = true;                                           // +0x70
    bool musicOn = true;                                         // +0x71
    // the blood packs (BloodPacks.h; 1.45 +0x80, +0x88): -1 in a new slot
    // until its first recharge gives it start_bloodpacks
    int numBloodPacks = -1;
    uint64_t lastBloodPackTimestamp = 0;  // whence the next pack is counted
    std::map<std::string, unsigned int> tagCooldowns;
    std::string lastFailedSurgery;
    int lastFailedSurgeryMode = 0;
    int lastFailedSurgeryStage = 0;
    // (1.45 GameState +0xd8, Android 1.43's +0xcc) the "Hospital Unlocked"
    // popup was shown (SurgeryEndScreen::showTipsLogic 1.45 0xf6b8c); false in
    // an older save
    bool seenHospitalUnlockedPopup = false;
    // (1.45 +0xd9) the "Flairground Unlocked" popup was shown; false in an older save
    bool seenPocketgodsUnlockedPopup = false;
    // (1.45 "areasUnlocked": the areas opened with money, isAreaUnlockedByPurchase;
    // an array of ints) kept as read, never added to: purchases are removed
    std::vector<int> areasUnlocked;
    // (the port's) the Director's cut: the cut content restored in the
    // surgeries (RestoredContent.h); false in an older save
    bool directorsCut = false;
    // +0xb0: the loading tip last shown (LoadingScreen 0x233ea0); -1 at every
    // load: the first one random
    int loadingTip = -1;
    // not saved: the surgery being played and its mode, 1 + (0 main, 1 and 2
    // the challenges)
    std::string selectedSurgery;                                 // +0xb4
    int selectedMode = 0;                                        // +0xb8
    int failures = 0;  // +0xbc: not saved; surgeries lost in a row
    bool allUpgrades = false;  // +0xc0: not saved; every upgrade for this surgery (Crumpet's power)
    int selectedPartnerIndex = 0;                                // +0xc4: not saved

    // the stars of a surgery in a mode (0 main, 1 and 2 its challenges); 0 if none
    int starsOf(const std::string& surgery, int mode) const;
    // every star of every surgery (0xe8c30)
    int totalStars() const;
    // the coins: the sum of the transactions in currency 0 (0xe61c0)
    int coins() const;
    // bought: a tag partner (item type 2, 0xe83b0), an upgrade (type 3, 0xe8b08)
    enum ItemType { kItemPartner = 2, kItemUpgrade = 3 };
    bool owns(int itemType, int item) const;
    // a transaction now, in currency 0 (0xe62a0)
    void addTransaction(int currencyInc, int itemType, int itemSubtype, int itemInc);
    // the tag partner chosen (its index), bought first if not yet and the coins allow (0xe8248)
    void choosePartner(int index, int cost);
};

class SaveData {
public:
    static SaveData& shared();  // 0xe3608: read on first use
    SavedGame& currentSlot();   // 0xe4054
    SavedGame& newSlot();       // 0xe3a18: a slot with a fresh id, made current
    void save();                // written to state.json (0xe43c0)
    void saveGame();            // 0xe419c: the current slot's save time, then save()

    int formatVersion = 1;
    unsigned int lastLoaded = 0;  // the current slot's id
    uint64_t last1DayLocalNotificationTimestamp = 0;
    uint64_t last3DayLocalNotificationTimestamp = 0;
    uint64_t nextAppReviewPopupTimestamp = 0;
    bool monetized = false;
    bool hasLoggedIntoFacebook = false;  // (1.45 +0x3a) kept as read: Facebook is removed
    std::vector<SavedGame> savedGames;

private:
    SaveData() = default;
    void load();
};

}  // namespace mt
