// mt::WelcomeScreen (made by 0x23aed4; 0x2134 bytes; vtable 0x41af68): the
// title screen, the first the game shows: play, and the credits.
//
// Removed (online, docs/rules.md §2): Adult Swim's news feed and "more
// games"; their buttons are hidden. The Game Center button opens the port's
// local achievements (AchievementsScreen.h). The current slot's blood packs
// are recharged here (BloodPacks.h).
#pragma once

#include "as3/Scene.h"

namespace mt {

class WelcomeScreen : public Scene {
public:
    static WelcomeScreen* create();  // 0x23aed4
    void load() override;            // vf87, 0x23b0ec
    void onButton(cocos2d::CCObject* sender);  // 0x23bc00

private:
    UIDefinition m_def;  // +0x420
};

}  // namespace mt
