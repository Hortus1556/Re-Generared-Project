// mt::SaveSlotScreen (made by 0x2e0730; 0x216c bytes; vtable 0x41d6f8): the
// save slots, where a returning player picks their game or starts a new one.
//
// Each slot is a page of a HorizontalScrollLayer: a picture of the area the
// player has got to (the area of the first surgery not yet won), the slot's
// stars, coins and when it was last played; after them a "new game" page.
// The current slot is the page shown first. Picking a slot makes it the
// current one; "new game" adds a slot (the others are kept). Then, as the
// title screen does: the intro if the first surgery is not won yet, the
// Bunker's map if the second is not, else the world map.
#pragma once

#include <string>
#include <vector>

#include "as3/SaveData.h"
#include "as3/Scene.h"

namespace mt {

class HorizontalScrollLayer;

class SaveSlotScreen : public Scene {
public:
    static SaveSlotScreen* create();  // 0x2e0730
    void load() override;             // vf87, 0x2e08c0

private:
    SaveSlotScreen();  // 0x2e8ba4
    void onButton(cocos2d::CCObject* sender) {}      // 0x2e183c: the layout's own (nothing)
    void onPressed(cocos2d::CCNode* page) {}         // 0x2e1840
    void onReleased(cocos2d::CCNode* page) {}        // 0x2e1844
    void onSelected(cocos2d::CCNode* page);          // 0x2e1848
    void onPageChanged(cocos2d::CCNode* page) {}     // 0x2e1be0

    UIDefinition m_def;                          // +0x420
    HorizontalScrollLayer* m_scroll = nullptr;   // +0x212c
    std::vector<cocos2d::CCNode*> m_buttons;     // +0x2130: each page's picture
    std::vector<cocos2d::CCNode*> m_pages;       // +0x213c
    bool m_busy = false;                         // +0x2164: picking ignored while set (the original never set it; the port once a slot is picked)
    std::string m_sheet = "savescreen_ui_sheet";  // +0x2168
};

// When a slot was last played, in words ("Just now", "5 Minutes ago"...;
// 0xe8d74).
std::string lastPlayedText(const SavedGame& slot);

}  // namespace mt
