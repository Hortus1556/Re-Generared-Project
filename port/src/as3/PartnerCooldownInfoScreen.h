// mt::PartnerCooldownInfoScreen (made by 0x2c0fb4; 0x216c bytes; vtable
// 0x41ce88): under a tag partner's portrait on the partner select screen
// (a screen of its own in the original, used there as a plain node), its
// cooldown: "Ready in..." and the time left, a bar filling up, and a button
// to end it now for the partner's end_cooldown_cost in coins, which also
// takes the partner.
//
// Removed (docs/as3.md): without the coins, the shop of coins sold for money
// opened; the button is only refused.
#pragma once

#include <string>

#include "as3/PartnerInfoUtils.h"
#include "as3/Scene.h"

namespace mt {

class PartnerSelectScreen;

class PartnerCooldownInfoScreen : public Scene {
public:
    static PartnerCooldownInfoScreen* create(const std::string& partner, PartnerSelectScreen* select);  // 0x2c0fb4
    void load() override;  // vf87, 0x2c11fc
    // the time left (seconds) and the price to end it (0x2c2090)
    void setTime(int left, int cost);

private:
    using Scene::init;
    bool init(const std::string& partner, PartnerSelectScreen* select);  // 0x2c0dac
    void onButton(cocos2d::CCObject* sender);  // 0x2c1bb4

    cocos2d::CCNode* m_root = nullptr;  // +0x420: the layout's parent
    UIDefinition m_def;                 // +0x424
    PartnerSelectScreen* m_select = nullptr;  // +0x2130
    PartnerInfo m_partner;              // +0x2138
};

}  // namespace mt
