// The upgrades on sale (a singleton, 0x1344c4; 0x200 bytes): up to 30 from
// the downloaded content's upgrades.json, laid out in a grid (column, row),
// and the stars each row needs to open (upgrade_unlocks.json; 0x1346d8).
// An upgrade is known by its index in the game's list of ids (the map at
// 0x433a08); bought, it is a transaction of item type 3 with that index.
#pragma once

#include <string>
#include <vector>

namespace mt {

// The upgrades' ids by index (0x433c1c...).
extern const char* const kUpgradeIds[30];
// An id's index, or -1.
int upgradeIndex(const std::string& id);

struct UpgradeDef {  // 0x10 bytes
    std::string id, name, description;
    int column = 0, row = 0;  // bytes in the original
    int cost = 0;             // coins (u16)
};

class UpgradeInfoUtils {
public:
    static UpgradeInfoUtils* shared();  // 0x1344c4
    std::vector<UpgradeDef> upgrades;   // +4: 30 at most; their count +0x1e4
    int rowStars[6] = {};               // +0x1e8: stars to open rows 1..6

    // how many could be bought now with these stars and coins (0x13459c):
    // not yet bought, its row open, the one under it in its column bought,
    // and the coins enough
    int affordable(int stars, int coins) const;

private:
    UpgradeInfoUtils() = default;
    void load();  // 0x1346d8
};

}  // namespace mt
