// The game's tunable settings (a singleton, 1.02 0x2b8a28, 0x48 bytes,
// vtable 0x41ce58; 1.45 SettingsInfoUtils, getSetting 0x3cab90): the id/value
// pairs from the downloaded content's settings.json whose id is one of
// kSettingIds (1.45's SettingNames, in its order).
//
// The port acts on the blood packs' (max_bloodpacks, start_bloodpacks,
// bloodpack_recharge_secs: BloodPacks.h); the rest are about what the port
// removes (the coin, blood pack and revive packs sold for money, the
// server's throttle, Facebook's reward) and are read as the original did.
#pragma once

#include <string>
#include <vector>

namespace mt {

// The settings by index (1.45's SettingNames::Enum; 1.02 had 8: max_bloodpacks,
// bloodpack_recharge_secs, the coin packs, throttle).
enum SettingId {
    kSettingMaxBloodpacks = 0,
    kSettingStartBloodpacks = 1,        // (1.45) a new slot's packs
    kSettingBloodpackRechargeSecs = 2,
    kSettingCoinPack1 = 3,              // ... to kSettingCoinPack1 + 4: the five coin packs
    kSettingBloodPack1 = 8,             // ... + 2: the blood packs sold (their counts)
    kSettingRevivePack1 = 11,           // ... + 2: the revives sold
    kSettingThrottle = 14,
    kSettingFacebookConnectReward = 15,
    kSettingCount
};
extern const char* const kSettingIds[kSettingCount];

class SettingsInfoUtils {
public:
    static SettingsInfoUtils* shared();  // 0x2b8a28
    // a setting's value, -1 if it is not in the file (1.02 0x2b9080, 1.45 0x3cab90)
    int value(int setting) const;

    struct Setting {  // 8 bytes (1.45: 12)
        std::string id;
        int value = 0;
    };
    std::vector<Setting> settings;  // +4: 24 at most (1.45 +0x124 their count)

private:
    SettingsInfoUtils() = default;
    void load();  // 0x2b8a8c
};

}  // namespace mt
