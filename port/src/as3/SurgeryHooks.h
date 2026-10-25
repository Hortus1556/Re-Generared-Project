// What the screens pushed over a surgery (PauseScreen, the lost
// SurgeryEndScreen) ask of the surgery screen under them. The original kept
// the SurgeryScreen itself (PauseScreen +0x42c, SurgeryEndScreen +0x458) and
// called its functions; the port passes the calls as selectors on it, and
// the numbers as they were when the screen was pushed (they cannot change
// while it is covered).
#pragma once

#include "engine/cocos/CCObject.h"

namespace mt {

struct SurgeryHooks {
    cocos2d::CCObject* screen = nullptr;        // the SurgeryScreen (retained by its scene manager)
    cocos2d::SEL_CallFunc cleanUp = nullptr;    // 0x3d574: no more updates, touches or sounds
    int stage = 1;        // 0x42b2c: the stage it is on (1-based, +0x8f4)

    void call(cocos2d::SEL_CallFunc f) const {
        if (screen && f) (screen->*f)();
    }
};

}  // namespace mt
