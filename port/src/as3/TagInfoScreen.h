// mt::TagInfoScreen (made by 0x29bc48; 0x213c bytes; vtable 0x41c328): the
// popup over the surgery the first time a tag partner's power is used: the
// partner's picture, the power's title and what it does (partners.json's
// tutorial_title and tutorial_desc), and ok. The surgery's sounds pause
// while it shows; it drops in from below, and goes back down when ok is
// pressed, the surgery then going on.
#pragma once

#include <string>

#include "as3/Scene.h"

namespace mt {

class TagInfoScreen : public Scene {
public:
    // Over `surgery` (the surgery screen), whose `resume` is called as the
    // popup goes (the original called SurgeryScreen 0x33740 on it) (0x29bc48)
    static TagInfoScreen* create(cocos2d::CCObject* surgery, cocos2d::SEL_CallFunc resume);
    void load() override;    // vf87, 0x29be9c
    void unload() override;  // vf88, 0x29cbd8

private:
    void onButton(cocos2d::CCObject* sender);  // 0x29c978
    void closed(cocos2d::CCNode* node);        // 0x29cbf4

    cocos2d::CCNode* m_root = nullptr;      // +0x420: the popup, moved
    cocos2d::CCNode* m_dim = nullptr;       // +0x424
    cocos2d::CCObject* m_surgery = nullptr; // +0x428
    cocos2d::SEL_CallFunc m_resume = nullptr;
    UIDefinition m_def;                     // +0x42c
    bool m_closing = false;  // not in the original: a second ok while it went down popped the surgery too
};

}  // namespace mt
