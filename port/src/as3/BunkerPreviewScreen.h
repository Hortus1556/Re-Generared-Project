// mt::BunkerPreviewScreen (made by 0x25763c; 0x594 bytes; vtable 0x41b6d8):
// after the last surgery (the Exile Clinic's alanprobe) is won the first
// time, its outro story (DialogueScreen's end, 0xcb7b4) shows the Bunker's
// map rising, shaking, to its upper part (where the bonus surgery hubris2
// now is) over 5.2 seconds to the sound of a crumbling wall, then after 1.3
// seconds the surgery's results (SurgeryEndScreen). The outro shows it
// when the result's firstTime (+0x1d) is set.
#pragma once

#include "as3/Scene.h"
#include "as3/SurgeryInfo.h"

namespace cocos2d {
class CCSprite;
}

namespace mt {

class BunkerPreviewScreen : public Scene {
public:
    // the won surgery, passed on to its results screen (0x25763c)
    static BunkerPreviewScreen* create(const SurgeryInfo& surgery, const SurgeryParams& params,
                                       const SurgeryResult& result);
    void load() override;    // vf87, 0x257874
    void unload() override;  // vf88, 0x257d58

private:
    BunkerPreviewScreen() = default;  // 0x25d8e8
    void shake(float dt);              // 0x257cb8
    void done(cocos2d::CCNode* node);  // 0x257d1c

    cocos2d::CCSprite* m_map = nullptr;  // +0x420
    SurgeryInfo m_surgery;               // +0x424
    SurgeryParams m_params;              // +0x548
    SurgeryResult m_result;              // +0x554
    float m_lastY = 0;                   // +0x58c
    float m_shake = 0;                   // +0x590: points to the side, its sign turned each frame
};

}  // namespace mt
