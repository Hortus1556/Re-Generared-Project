// mt::CooldownInfoScreen (made by 0x2e8ee0; 0x2170 bytes; vtable 0x41d8e8):
// the popup over the partner select screen when a partner still cooling
// down is tapped: "<name> is Recharging", the time left counting down, and
// two buttons: ok, or recharge now for the partner's end_cooldown_cost in
// coins (which also takes the partner). It drops in from below and goes
// back down; it closes by itself when the cooldown is over.
//
// Removed (docs/as3.md): without the coins, the shop of coins sold for money
// opened; the button is only refused.
#pragma once

#include <string>

#include "as3/PartnerInfoUtils.h"
#include "as3/Scene.h"

namespace mt {

class PartnerSelectScreen;

class CooldownInfoScreen : public Scene {
public:
    static CooldownInfoScreen* create(const std::string& partner, PartnerSelectScreen* select);  // 0x2e8ee0
    void load() override;  // vf87, 0x2e9324

private:
    using Scene::init;
    bool init(const std::string& partner);  // 0x2e9110
    void onButton(cocos2d::CCObject* sender);  // 0x2e9a50
    void tick(float dt);                       // 0x2e9ec0: the time left, every frame
    void close();                              // 0x2ea268: down, then off
    void closed(cocos2d::CCNode* node);        // 0x2ea248

    UIDefinition m_def;                       // +0x420
    PartnerSelectScreen* m_select = nullptr;  // +0x212c
    cocos2d::CCNode* m_root = nullptr;        // +0x2130: the popup, moved
    PartnerInfo m_partner;                    // +0x2134
    std::string m_sheet = "end_of_surgery_popups_sheet";  // +0x216c
    // Not in the original: going already. A second tap while it went down,
    // or its time running out then, popped the screen under it too.
    bool m_closing = false;
};

}  // namespace mt
