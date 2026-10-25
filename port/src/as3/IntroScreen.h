// mt::IntroScreen (made by 0x2a32d4; 0x428 bytes; vtable 0x41c510): the
// intro movie (Videos/intro.m4v), shown before a new player's first story.
// It ends when the movie does, or on a second tap within a second of the
// first (or Escape, on the PC: VideoPlayer); then the first area's story
// begins (DialogueScreen).
#pragma once

#include "as3/Scene.h"

namespace mt {

class IntroScreen : public Scene {
public:
    static IntroScreen* create();  // 0x2a32d4
    void onShown() override;       // vf91, 0x2a36c0
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // vf95, 0x2a3488

private:
    void tick(float dt);  // 0x2a38c0
    void finish();        // 0x2a34bc
    float m_skipWindow = 1.0f;  // +0x420: a second tap within this skips
    float m_sinceTap = 0;       // +0x424
};

}  // namespace mt
