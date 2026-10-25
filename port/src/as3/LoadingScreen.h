// mt::LoadingScreen (made by 0x233dc8; 0x212c bytes; vtable 0x41ad70): what
// SceneMgr shows while a screen that asks for it (the surgery) loads.
//
// A dark backdrop, the loading picture (anim_loading_1.png of
// loading_screen_sheet, still: the original never animated it, and
// common/anim_loading_screen_anim.json, its one frame, is not used) in the
// middle, and a tip from Data/loading_tips.json under it: the next one
// each time (the slot's loadingTip), a random one first after the game
// starts. It is up for the frame it is drawn in, then the screen loads.
#pragma once

#include "as3/Scene.h"

namespace mt {

class LoadingScreen : public Scene {
public:
    static LoadingScreen* create();  // 0x233dc8
    void load() override;            // vf87, 0x233ea0
    void unload() override;          // vf88, 0x23444c
};

}  // namespace mt
