// Not in the original: a debug screen that plays one of the game's
// animations (mt::Animation), to see and test them outside the surgery.
//
// With -d, AS_ANIM=<file>,<sheet>[+<sheet>...],<name>[~][*<loops>],... shows it
// instead of the title screen: the animation file (common/anim_*_anim.json)
// with its sheets' frames loaded, placed with its registration point in the
// middle of the screen, playing the first name (`loops` times, for ever if
// not given; backwards with ~) then the others queued after it; an entry @<point> attaches a
// yellow dot to that control point (Animation::attach). A label says what plays and
// its frame. A tap shows the viewer again with the names turned round by
// one (the second first); the viewer asks for the loading screen, so each
// tap also shows LoadingScreen for its frame.
//   AS_ANIM=common/anim_creature_bat_anim.json,anim_bat_sheet,reaction*2,hanging
#pragma once

#include <string>
#include <vector>

#include "as3/Scene.h"

namespace mt {

class Animation;

class AnimationViewer : public Scene {
public:
    // from AS_ANIM (debug mode only); nullptr if not set
    static AnimationViewer* createFromEnv();
    static AnimationViewer* create(const std::string& file, const std::vector<std::string>& sheets,
                                   const std::vector<std::string>& names);

    void load() override;
    void unload() override;
    void onShown() override;
    bool needsLoadingScreen() override { return true; }
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

private:
    void tick(float dt);

    std::string m_file;
    std::vector<std::string> m_sheets;
    std::vector<std::string> m_names;  // "name" or "name*loops"
    Animation* m_anim = nullptr;
    std::vector<cocos2d::CCNode*> m_markers;  // retained
    cocos2d::CCLabelBMFont* m_label = nullptr;
};

}  // namespace mt
