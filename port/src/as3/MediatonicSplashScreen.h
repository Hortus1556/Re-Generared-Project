// mt::MediatonicSplashScreen (vtable 0x41c8f8): the developer's logo, which
// SceneMgr shows while the first screen comes in at launch.
#pragma once

#include "as3/Scene.h"

namespace mt {

class MediatonicSplashScreen : public Scene {
public:
    static MediatonicSplashScreen* create();  // 0x2b0f74
    void load() override;                     // vf87, 0x2b107c
};

}  // namespace mt
