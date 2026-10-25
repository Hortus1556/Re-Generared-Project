// mt::UpgradeScreen (made by 0x127f80; 0x41a8 bytes; vtable 0x4178f8): the
// upgrades machine, a tech tree of 30 upgrades in five columns (blue,
// orange, red, green, purple) and six rows, bought with coins.
//
// A row opens once the player has its stars (upgrade_unlocks.json; the rows
// still shut are chained, with their price in stars); an upgrade above the
// first row needs the one under it bought first. Tapping an icon selects it
// (a blue frame), shows its name, text and art, and its price with the buy
// button if it can be bought now, or a tick if bought. Bought upgrades get a
// gold frame. The back button returns where the screen came from: the world
// map ("world"), the patient's pre-op screen ("preop", the slot's selected
// surgery), else the area's map.
//
// Removed (docs/as3.md): the shop of coins sold for money, which the top
// bar's button and a buy without the coins opened (the button opens the
// port's shop of items for earned coins, GlobalHud.h; a buy without the
// coins is only refused), the ads and the analytics.
#pragma once

#include <string>

#include "as3/GlobalHud.h"
#include "as3/Scene.h"
#include "as3/SurgeryInfo.h"

namespace mt {

class UpgradeScreen : public Scene {
public:
    // `from`: "world", "preop" or another (the area's map) (0x127f80)
    static UpgradeScreen* create(const std::string& from);
    void load() override;       // vf87, 0x128138
    void onShown() override;    // vf91, 0x12cf8c
    void onRemoved() override;  // vf92, 0x12cfe4

private:
    void onButton(cocos2d::CCObject* sender);  // 0x128f18
    void refreshTree();                        // 0x12b900: rows, frames, icons, prices
    void tick(float dt);                       // 0x12cd0c: the top bar, each second
    void select(const std::string& icon);      // 0x12cd34: the column and row of an icon
    void goBack();                             // 0x128f18's back button

    UIDefinition m_def;   // +0x420
    GlobalHud m_hud;      // +0x212c
    int m_row = 0;        // +0x418c: the selected upgrade's (1 to 6, from the bottom)
    int m_column = 0;     // +0x4190: 1 blue ... 5 purple
    std::string m_from;   // +0x4194
    std::string m_sheet = "upgrade_screen_sheet";  // +0x4198
    bool m_bought = false;  // +0x419c: something bought here (for the ads)
};

}  // namespace mt
