// mt::GeneralTipPopupScreen (made by 0x2f883c and 0x2f8948; 0x2170 bytes;
// vtable 0x41dcc8): a tip popup pushed over a screen, dropped in with a
// bounce over the darkened screen. Its kind:
// - "upgrades_unlocked": a new row of upgrades is open; "upgrades": upgrades
//   can be bought (both: to the upgrades machine, or not now);
// - "shop": the shop could help (its button does nothing, as in the
//   original; not now closes);
// - "partner": a new tag partner (the slot's selectedPartnerIndex: Mr
//   Giblets, Lady Saturday or the Reverend), to take as the partner;
// - "hospital_unlocked" (Android 1.43): the stars reach the Hospital's
//   cost (to the world map on its page, or not now);
// - "quit_warning", "retry_warning" (from the pause screen): go back, or quit
//   or restart the surgery (the pause screen's own functions).
// Its buttons close it (SceneMgr::popScene) and act.
//
// - "bloodpacks_out": out of blood packs, the time to the next counted down
//   (closing by itself when one is back: the port's); "bloodpacks_first":
//   the blood packs explained (BloodPacks.h). Their buy button is hidden:
//   the packs are free on the PC.
//
// Removed (docs/as3.md): buying blood packs, the analytics.
#pragma once

#include <string>

#include "as3/Scene.h"

namespace mt {

class GeneralTipPopupScreen : public Scene {
public:
    static GeneralTipPopupScreen* create(const std::string& type);  // 0x2f883c
    // from the pause screen: its quit (0x1200cc) and retry (0x120350) are
    // called on `pause` (0x2f8948: the original kept the PauseScreen)
    static GeneralTipPopupScreen* create(const std::string& type, cocos2d::CCObject* pause,
                                         cocos2d::SEL_CallFunc quit, cocos2d::SEL_CallFunc retry);
    void load() override;    // vf87, 0x2f8b00
    void unload() override;  // vf88, 0x2fcf9c

private:
    GeneralTipPopupScreen() = default;  // 0x303adc
    void build(const char* layout);
    void onButton(cocos2d::CCObject* sender);  // 0x2fbf68
    void close();                              // 0x2fced4
    void closeNode(cocos2d::CCNode* node) { close(); }
    void closeNow(float dt) { close(); }
    void updateBloodPacks(float dt);  // 0x42acf0: "bloodpacks_out"'s time to the next

    cocos2d::CCNode* m_shade = nullptr;          // +0x420
    cocos2d::CCObject* m_pause = nullptr;        // +0x444
    cocos2d::SEL_CallFunc m_quit = nullptr, m_retry = nullptr;
    UIDefinition m_def;                          // +0x448
    std::string m_type;                          // +0x2154
    cocos2d::CCNode* m_popup = nullptr;          // +0x2158
    int m_partner = 0;                           // +0x215c: the partner shown (1 Saturday, 2 the Reverend)
    std::string m_sheet = "end_of_surgery_popups_sheet";  // +0x216c
};

}  // namespace mt
