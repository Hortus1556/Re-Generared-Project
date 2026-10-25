// mt::AreaLockedScreen (made by 0x2b1688 and 0x2b1a20; 0x2140 bytes; vtable
// 0x41cad8): the popup over the world map when a shut area is picked: why it
// is shut (the stars it needs, and the area before it to finish if that is
// not done). It drops in with a bounce over a darkened screen; OK slides it
// out and takes it off (SceneMgr::popScene).
#pragma once

#include <string>

#include "as3/Scene.h"

namespace mt {

class AreaLockedScreen : public Scene {
public:
    // the area before it not finished yet (0x2b1688, "AreaLockedPopup1")
    static AreaLockedScreen* create(const std::string& area, const std::string& previousArea, int stars);
    // only the stars missing (0x2b1a20, "AreaLockedPopup2")
    static AreaLockedScreen* create(const std::string& area, int stars);
    void load() override;    // vf87, 0x2b1d2c
    void unload() override;  // vf88, 0x2b2838

private:
    AreaLockedScreen() = default;
    void onButton(cocos2d::CCObject* sender);  // 0x2b25e0
    void close(cocos2d::CCNode* node);         // 0x2b2854

    cocos2d::CCNode* m_popup = nullptr;   // +0x420
    cocos2d::CCNode* m_shade = nullptr;   // +0x424
    std::string m_area;                   // +0x428: its title
    std::string m_previousArea;           // +0x42c: "" when only stars are missing
    int m_stars = 0;                      // +0x430
    UIDefinition m_def;                   // +0x434
};

}  // namespace mt
