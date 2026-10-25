// mt::PauseScreen (made by 0x11ddf4; 0x227c bytes; vtable 0x417248): the
// surgery paused (its pause button, SurgeryScreen 0x3eb7c), pushed over it.
// The surgery's sounds pause; a tray drops in with a bounce over the
// darkened surgery, the tag partner's hand on it (Ophelia's when there is
// none), with four buttons, and a back arrow slides in from the left:
// - back: the tray goes up and away, the sounds go on, and the surgery
//   under it is shown again (SceneMgr::popScene; the surgery's onShown
//   carries on);
// - quit (0x1200cc) and the help button, which restarts (0x120350): the
//   surgery noted as failed in the slot (lastFailedSurgery, its mode and
//   stage); quit goes back to the area's
//   map (to the title screen before the first surgery is won), restart to
//   the surgery's pre-op screen. Once the first two surgeries are won, both
//   ask first (GeneralTipPopupScreen's "quit_warning", "retry_warning");
// - music and sound effects: each turns its switch over (the slot's musicOn,
//   sfxOn, saved; SoundMgr), its on or off mark shown.
// The four tray buttons are pictures, not menu items: the screen's own
// touches press them (down 5 points while held) and act on release.
//
// Removed (docs/as3.md): the analytics; the layout's supertools_* items (the
// consumables' panel, art no build has; the consumables are removed).
#pragma once

#include <string>

#include "as3/Scene.h"
#include "as3/SurgeryHooks.h"
#include "as3/SurgeryInfo.h"

namespace cocos2d {
class CCLabelBMFont;
class CCLayerColor;
class CCSprite;
}

namespace mt {

class PauseScreen : public Scene {
public:
    // Over the surgery screen `hooks.screen` (0x11ddf4; the original passed
    // the surgery screen and its +0x8dc, read for the tag partner, which is
    // the slot's selectedTagPartner). `locked`: its tray buttons' touches
    // are ignored (+0x2278; the surgery screen passes false).
    static PauseScreen* create(const SurgeryInfo& surgery, const SurgeryHooks& hooks, const SurgeryParams& params,
                               bool locked = false);
    void load() override;     // vf87, 0x11e144
    void unload() override;   // vf88, 0x11f93c
    void onShown() override;  // vf91, 0x120820: its touches on
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // vf95, 0x11f964
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // vf96, 0x120000
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // vf97, 0x12006c

    void quit();   // 0x1200cc (also the quit warning's)
    void retry();  // 0x120350 (also the retry warning's)

private:
    PauseScreen();  // 0x126eb4
    void onButton(cocos2d::CCObject* sender);  // 0x11ef70
    void resumed(cocos2d::CCNode* node);       // 0x1200b0
    void toggleMusic();                        // 0x1203c0
    void toggleSfx();                          // 0x1205f0
    void noteFailure();                        // (quit and retry's start)
    bool earlySurgeries() const;               // the first two not both won yet
    void closeTray();                          // back: the tray up and away, then resumed()

    cocos2d::CCNode* m_root = nullptr;     // +0x420: the tray and its buttons, moved
    cocos2d::CCSprite* m_shade = nullptr;  // +0x424
    std::string m_sheet;                   // +0x428: "surgery_pause_sheet"
    SurgeryHooks m_hooks;                  // +0x42c: the surgery screen
    float m_backX = 0;                     // +0x430: the back arrow's place
    UIDefinition m_def;                    // +0x434
    SurgeryInfo m_surgery;                 // +0x2144
    SurgeryParams m_params;                // +0x2268
    cocos2d::CCNode* m_pressed = nullptr;  // +0x2274: the tray button held
    bool m_locked = false;                 // +0x2278
};

// Debug mode only: AS_PAUSE=<surgery>[,<seconds>] opens on the surgery (its
// main mode), and pushes this screen over it after that many seconds (5 by
// default) as its pause button is to; nullptr otherwise.
Scene* pauseViewer();

}  // namespace mt
