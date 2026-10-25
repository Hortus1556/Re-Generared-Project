// mt::LifeWarningScreen (made by 0x2f1118; 0x213c bytes; vtable 0x41dad0):
// the "Low Heart Rate Warning!" popup over the surgery
// (common/ingame_popup_lowhealth.json: the heart, the title, the advice to
// heal and ok). The surgery pushes it, frozen under its pause box, when the
// heart first falls under 10 in a main surgery before
// surgery_shanksalot_earnestpickle is won (SurgeryScreen::damage, 0x32ef0 →
// 0x33ce0). The surgery's sounds pause while it shows; it drops in from
// below and goes back down when ok is pressed, the surgery then going on.
#pragma once

#include "as3/Scene.h"

namespace mt {

class LifeWarningScreen : public Scene {
public:
    // Over `surgery`, whose `resume` is called as the popup goes (the
    // original's 0x33dec: the game on, the pause box down) (0x2f1118, 0x2f1280)
    static LifeWarningScreen* create(cocos2d::CCObject* surgery, cocos2d::SEL_CallFunc resume);
    void load() override;    // vf87 0x2f12b4
    void unload() override;  // vf88 0x2f1bd8

private:
    void onButton(cocos2d::CCObject* sender);  // 0x2f1978
    void closed(cocos2d::CCNode* node);        // 0x2f1bf4

    cocos2d::CCNode* m_root = nullptr;      // +0x420: the popup, moved
    cocos2d::CCNode* m_dim = nullptr;       // +0x424
    cocos2d::CCObject* m_surgery = nullptr; // +0x428
    cocos2d::SEL_CallFunc m_resume = nullptr;
    UIDefinition m_def;                     // +0x42c
    bool m_closing = false;  // (the port's: a second ok while it went down would pop the surgery too)
};

}  // namespace mt
