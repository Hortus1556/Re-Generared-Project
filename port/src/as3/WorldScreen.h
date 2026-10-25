// mt::WorldScreen (made by 0x112530 and 0x1126c8; 0x41bc bytes; vtable
// 0x416e28): the world map, the areas (six; seven with Android 1.43's
// Hospital) as pages to swipe across (HorizontalScrollLayer, shrinking away
// from the middle), with the top bar (its main layout: coins and stars), the
// upgrades and tag partner buttons, and a dot for each page (the layout's
// six, and the port's seventh).
//
// An area is open once the area before it has all its patients treated (the
// Bunker's bonus surgery hubris2 aside) and the stars won reach its unlock
// cost (area_unlocks.json). An open area shows its stars won out of five a
// surgery (hubris2 counted once the game is finished), and sparkles while
// none is won yet; a shut one is greyed with its cost. Picking an open area
// goes to its map (AreaScreen); a shut one says why (AreaLockedScreen, over
// this screen). Made with an area (1 the first), the map slides to that page.
// The Hospital (Android 1.43, re/as3android: loadAssets 0x887fb8,
// onSelectedPage 0x88bef8) is the first page: it and the Bunker need no area
// before them, it opens at 9 stars, and its intro story plays when it is
// picked while none of its stars is won. (Android also had a locked "coming
// soon" page at each end: left out, they led nowhere.)
//
// Each second the top bar (GlobalHud) shows the blood packs and the slot's
// are recharged (BloodPacks.h). Removed (docs/as3.md): the ads (shown and
// hidden here).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "as3/GlobalHud.h"
#include "as3/Scene.h"

namespace cocos2d {
class CCSprite;
}

namespace mt {

class HorizontalScrollLayer;

class WorldScreen : public Scene {
public:
    static WorldScreen* create() { return create(-1); }  // 0x112530: no area to slide to
    static WorldScreen* create(int area);                // 0x1126c8
    void load() override;       // vf87, 0x1127bc
    void onShown() override;    // vf91, 0x115850
    void onRemoved() override;  // vf92, 0x115894

private:
    WorldScreen();  // 0x11c9f8
    void onButton(cocos2d::CCObject* sender);     // 0x114a88
    void onPressed(cocos2d::CCNode* page);        // 0x114f10: the icon lit
    void onReleased(cocos2d::CCNode* page);       // 0x114fc0
    void onSelected(cocos2d::CCNode* page);       // 0x115068
    void onPageChanged(cocos2d::CCNode* page);    // 0x1155d8
    void updateDots();                            // 0x1155dc
    void tick(float dt);                          // 0x115828: the top bar, each second

    UIDefinition m_def;                         // +0x420
    GlobalHud m_hud;                            // +0x212c
    HorizontalScrollLayer* m_scroll = nullptr;  // +0x418c
    std::vector<cocos2d::CCSprite*> m_icons;    // +0x4190: each area's icon
    std::map<int, bool> m_locked;               // +0x419c: by area
    std::vector<cocos2d::CCSprite*> m_extraDots;  // (the port's) the dots past the layout's six
    int m_area = -1;                            // +0x41b4: the page to slide to
    bool m_busy = false;                        // +0x41b8: a shut area's popup is up
};

// The picture of a tag partner on the maps' partner button: its name in the
// art ("Giblets", "Procrastibot"...), "" for none (0x113900, 0x20e40).
std::string partnerButtonPicture(const std::string& partnerId);

}  // namespace mt
