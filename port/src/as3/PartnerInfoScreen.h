// mt::PartnerInfoScreen (made by 0x20afac; 0x419c bytes; vtable 0x41a0a8):
// a tag partner's file over the partner select screen: name, photo, tag
// power, bio and stats, with the top bar. On sale, a buy button with its
// price in coins; owned, a button to take it as the tag partner (which
// closes the file). It drops in from below and goes back down.
//
// Removed (docs/as3.md): without the coins, and from the top bar's button,
// the shop of coins sold for money opened; the buy is only refused (the
// button hidden: GlobalHud.h). The analytics.
#pragma once

#include <string>

#include "as3/GlobalHud.h"
#include "as3/Scene.h"

namespace mt {

class PartnerSelectScreen;

class PartnerInfoScreen : public Scene {
public:
    static PartnerInfoScreen* create(const std::string& partner, PartnerSelectScreen* select);  // 0x20afac
    void load() override;    // vf87, 0x20b15c
    void unload() override;  // vf88, 0x20d600

private:
    void onButton(cocos2d::CCObject* sender);  // 0x20c7c8
    void tick(float dt);                       // 0x20d5a8: the top bar, each second
    void close(cocos2d::CCNode* node);         // 0x20d5d0

    UIDefinition m_def;                       // +0x420
    GlobalHud m_hud;                          // +0x212c
    PartnerSelectScreen* m_select = nullptr;  // +0x418c
    std::string m_partner;                    // +0x4190: its id
    cocos2d::CCNode* m_root = nullptr;        // +0x4194: the file, moved
    std::string m_sheet = "partner_popup_sheet";  // +0x4198
    // Not in the original: going already. A second tap on back while the
    // file went down popped the screen under it too.
    bool m_closing = false;
};

}  // namespace mt
