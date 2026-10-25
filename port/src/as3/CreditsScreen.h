// mt::CreditsScreen (made by 0x2aa5d0; 0x2150 bytes; vtable 0x41c708): the
// credits, the team's eighteen portraits as pages to swipe across
// (HorizontalScrollLayer, shrinking away from the middle), over the
// credits' background; back returns to the title. The music is the title's.
//
// Removed (online, docs/rules.md §2): the legal button, which opened the
// attributions' web page; it is hidden. The layout's two arrows have no art
// in the game (nothing to press, as in the original); the pages are swiped.
// Fixed: the back button had no art from the title (CreditsScreen.cpp).
#pragma once

#include <string>
#include <vector>

#include "as3/Scene.h"

namespace mt {

class HorizontalScrollLayer;

class CreditsScreen : public Scene {
public:
    static CreditsScreen* create();  // 0x2aa5d0
    void load() override;            // vf87, 0x2aa768

private:
    CreditsScreen() = default;  // 0x2b0da8
    void onButton(cocos2d::CCObject* sender);      // 0x2aad44
    void onPage(cocos2d::CCNode* page) {}           // 0x2aaff8..0x2ab004: the scroller's four (nothing)

    UIDefinition m_def;                          // +0x420
    HorizontalScrollLayer* m_scroll = nullptr;   // +0x212c
    std::vector<cocos2d::CCNode*> m_portraits;   // +0x2130
    std::vector<cocos2d::CCNode*> m_pages;       // +0x213c
    std::string m_sheet = "credits_sheet";       // +0x214c
};

}  // namespace mt
