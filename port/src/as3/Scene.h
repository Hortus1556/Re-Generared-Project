// mt::Scene: the base of every screen of the game (vtable 0x41b4d0).
//
// A cocos scene that is also a touch, accelerometer and keypad delegate.
// SceneMgr stacks screens: it loads one when it comes in (load, vf87) and
// unloads it when it goes or is covered for good (unload, vf88), tells it
// when it is covered (vf90), shown (vf91) and about to go (vf92), and asks
// whether it wants the loading screen first (vf89). Its layout, built, is
// its `ui` (+0xd4); while covered its touches and its layout's two menus are
// turned off.
#pragma once

#include <string>

#include "as3/UI.h"
#include "engine/cocos/CCLayer.h"

namespace mt {

class Scene : public cocos2d::CCScene,
              public cocos2d::CCTouchDelegate,
              public cocos2d::CCAccelerometerDelegate,
              public cocos2d::CCKeypadDelegate {
public:
    Scene();  // 0x2561cc
    ~Scene() override;

    virtual void load() {}               // vf87
    virtual void unload() {}             // vf88
    // vf89 (0x256354): every screen of the game says yes (none overrides it;
    // SurgeryScreen's 0x4d234 says yes too), so every replaceScene went
    // through the loading screen
    virtual bool needsLoadingScreen() { return true; }
    virtual void onCovered() {}          // vf90
    virtual void onShown();              // vf91: the time it was shown
    virtual void onRemoved() {}          // vf92
    virtual void vf93() {}
    virtual void vf94() {}

    // its own touches: a targeted delegate of priority 0 that swallows them
    bool isTouchRegistered();              // 0x256358
    void setTouchRegistered(bool on);      // 0x2563ec
    // touches right after it shows are not taken (0x256398)
    bool shownForLessThanMinimum();
    void setName(const std::string& n) { m_name = n; }  // 0x2563d4
    const std::string& getName() const { return m_name; }  // 0x2563dc

    UIInstance ui;  // +0xd4: its menus +0x408, +0x40c

protected:
    double m_shownAt = 0;       // +0x410
    std::string m_name;         // +0x418
    float m_minimumShown = 0.2f;  // +0x41c
};

}  // namespace mt
