// mt::SurgeryCurrencyScreen (made by 0x2291a8; 0x213c bytes; vtable
// 0x41aab8): the coins a won surgery earned, on a cheque that drops in with
// a bounce over its results (SurgeryEndScreen, which pushes it): the total,
// then the score's share and the new stars' bonus. With Filthy Nowe as the
// tag partner and his power used, the cheque is his ("filthy": his power
// doubled the coins; the two shares are shown as they were before). OK
// slides it down and away; the results then stop the coins' sound, show
// their buttons and take their shade off.
//
// Not a shop: nothing is bought here (the coins were added by the surgery
// screen). The layout's coin doubler icon has no art in the game.
#pragma once

#include "as3/Scene.h"

namespace mt {

class SurgeryEndScreen;

class SurgeryCurrencyScreen : public Scene {
public:
    // over `results`, the coins from the score and for the stars (0x2291a8)
    static SurgeryCurrencyScreen* create(SurgeryEndScreen* results, int coinsFromScore, int coinsBonus);
    void load() override;  // vf87, 0x2293e8

private:
    SurgeryCurrencyScreen() = default;
    void onButton(cocos2d::CCObject* sender);  // 0x229ff8
    void closed(cocos2d::CCNode* node);        // 0x22a250

    cocos2d::CCNode* m_root = nullptr;     // +0x420: the cheque, moved
    UIDefinition m_def;                    // +0x424
    SurgeryEndScreen* m_results = nullptr; // +0x2130
    int m_coinsFromScore = 0;              // +0x2134
    int m_coinsBonus = 0;                  // +0x2138
    bool m_closing = false;  // not in the original: a second OK while it went down popped the results too
};

}  // namespace mt
