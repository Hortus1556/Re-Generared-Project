// The areas of the world map (a singleton, 0xf2d80; 0x428 bytes: six area
// definitions of 0xb0 bytes, then their count), read once from the
// downloaded content's areas.json, with each area's unlock cost (stars) from
// area_unlocks.json (0xf2e74).
// iOS 1.45 (LoadWorld 0x1457e0, an area 0xd8 bytes): eight areas at most,
// Le Flairground Fantastique (area_pocketgods) first and the Hospital
// (area_hospital) second wherever areas.json lists them, then the others in
// their order. Code that wants a given area looks it up by its id (byId).
#pragma once

#include <string>
#include <vector>

namespace mt {

struct AreaDef {  // 0xb0 bytes (made by 0x2bea4, copied by 0x2b368)
    std::string areaId;                    // +0
    std::string title;                     // +4
    std::string bgm;                       // +8
    std::string worldMapIcon;              // +0xc
    std::string bgSprite;                  // +0x10
    std::string mapUiDefinitionFile;       // +0x14
    std::string bgSpritesheet;             // +0x18
    std::string mapSpritesheet;            // +0x1c
    std::string bossSurgery;               // +0x20
    std::string leaderboardId;             // +0x24
    std::string completeDialogueFile;      // +0x28
    std::string newTitle;                  // +0x2c
    std::string newBody;                   // +0x30
    // (Android 1.43) the area's own intro story, shown on its first visit
    // (WorldScreen::onSelected), and its sheets; empty for the iOS areas
    std::string introDialogueFile;
    std::vector<std::string> introDialogueSpritesheets;
    int unlockCost = 0;                    // +0x34: stars
    float pointsMultiplier = 0;            // +0x38
    float injuryDamageMultiplier = 0;      // +0x3c
    std::vector<std::string> completeDialogueSpritesheets;  // +0x40: 6 at most
    std::vector<std::string> surgeries;                     // +0x5c: 8 at most
    std::vector<std::string> surgeryShortNames;             // +0x80: 8 at most
    // +0xa4: room for two only; the third of an area's three overwrote the
    // count (the port keeps them all)
    std::vector<int> surgeryBaseNumbers;
};

class AreasDefUtils {
public:
    static AreasDefUtils* shared();  // 0xf2d80
    std::vector<AreaDef> areas;      // +4: eight at most (1.02: six); their count 1.45 +0x6c4
    // the area of that id, or nullptr (Android's getAreaDef 0x323360)
    const AreaDef* byId(const std::string& id) const;

private:
    AreasDefUtils() = default;
    void load();  // 0xf2e74
};

}  // namespace mt
