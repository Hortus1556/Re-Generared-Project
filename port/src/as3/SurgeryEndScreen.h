// mt::SurgeryEndScreen (0x4364 bytes; vtable 0x4162d8; constructor
// 0xc57d0, set up by 0xb8298): a surgery's results, on a letter over its
// area's background.
//
// Won (made by 0xb8100: after a challenge, or after the main surgery's
// outro): "super-hero-loop" (a restored track, in place of the original's
// "win_loop"); the patient's name and condition, the time left and the score. Its stars drop in one after another (each with "Star_Impact"),
// the new ones sparkling: the main surgery's 1 to 3 yellow ones, or after a
// challenge the middle one in the challenge's colour (blue the first, red
// the second). 1.8 seconds on, the coins won show on a cheque
// (SurgeryCurrencyScreen, over this one) to the coin sounds and points
// particles; when it goes the buttons show: next (and retry, unless the
// main surgery was won with new stars for the first time). Then, once,
// a tip may show (GeneralTipPopupScreen): a tag partner was given
// ("partner"), or else, unless an area was just completed, a new row of
// upgrades opened ("upgrades_unlocked"), or else, four times in ten, some
// upgrades can be bought ("upgrades").
//
// Lost (made by 0xb857c, pushed over the surgery screen): "lose_loop"; the
// letter of failure with the patient's name, the time left, the score and
// how much of the surgery was done; retry (its pre-op screen) and quit (the
// area's map).
//
// Next (and quit): the area's map; or, the first time an area's boss (or
// the Bunker's prxy4) is beaten, that area's ending story
// (DialogueScreen::createAreaComplete), then the world map at the next
// area; after the last surgery (the Exile Clinic's alanprobe), the Bunker's
// map.
//
// Removed (docs/as3.md): the revive (a Super Serum owned, or bought from
// the App Store: the surgery went on; its buttons hidden), the "rate us"
// prompt after a win,
// the ads, the analytics. The coins
// themselves were added by the surgery screen (SurgeryScreen::finishWin).
#pragma once

#include <cstdint>
#include <string>

#include "as3/GlobalHud.h"
#include "as3/Scene.h"
#include "as3/SurgeryHooks.h"
#include "as3/SurgeryInfo.h"

namespace cocos2d {
class CCLayerColor;
class CCParticleSystem;
}

namespace mt {

class SurgeryEndScreen : public Scene {
public:
    static SurgeryEndScreen* create(const SurgeryInfo& surgery, const SurgeryParams& params,
                                    const SurgeryResult& result);      // 0xb8100
    // Over the surgery screen `hooks.screen` (0xb857c: the original was
    // given the surgery screen itself, +0x458, for its revive: removed).
    static SurgeryEndScreen* createLost(const SurgeryInfo& surgery, const SurgeryParams& params,
                                        const SurgeryResult& result,
                                        const SurgeryHooks& hooks = SurgeryHooks());  // 0xb857c
    void load() override;       // vf87, 0xb8670
    void onShown() override;    // vf91, 0xbe4a8
    void onRemoved() override;  // vf92, 0xbe498

    // from the coins' cheque as it goes (SurgeryCurrencyScreen 0x22a250)
    void stopCoinSound();  // 0xbe1ec
    void showButtons();    // 0xbd714
    void hideShade();      // 0xbe1f4

    const SurgeryResult& result() const { return m_result; }

private:
    SurgeryEndScreen() = default;  // 0xc57d0
    using Scene::init;
    bool init(const SurgeryInfo& surgery, const SurgeryParams& params, const SurgeryResult& result);  // 0xb8298
    void onButton(cocos2d::CCObject* sender);  // 0xbcd98
    void sparkle(int star);                    // 0xbd058: particle_starhighlightFX on a new star
    void starImpact(cocos2d::CCNode* node);    // 0xbd340
    void showCoins(cocos2d::CCNode* node);     // 0xbd46c
    void coinParticles(cocos2d::CCNode* node); // 0xbd4b0
    void showTips();                           // 0xbdde8
    void next();                               // 0xbe250
    void tick(float dt);                       // 0xbddc0: the top bar, each second
    int coinChannel() const { return static_cast<int>(reinterpret_cast<intptr_t>(this)); }

    SurgeryResult m_result;           // +0x420
    SurgeryHooks m_hooks;             // +0x458 (the lost surgery's screen)
    UIDefinition m_def;               // +0x45c
    GlobalHud m_hud;                  // +0x2168
    SurgeryInfo m_surgery;            // +0x41c8
    SurgeryParams m_params;           // +0x42ec
    // +0x42f8 (0xc5af8): the surgery's id (+0), area (+8), patient (+0x10),
    // condition (+0x14), background picture (+0x1c) and its sheet (+0x20):
    // read from m_surgery
    unsigned char m_stars[3] = {};    // +0x432c: as won (the result's)
    bool m_starShown[3] = {};         // (the port's) the stars this play shows (load)
    cocos2d::CCLayerColor* m_shade = nullptr;  // +0x4330
    float m_coinDelay = 1.8f;         // +0x4334
    cocos2d::CCParticleSystem* m_coinParticles = nullptr;  // +0x4338
    bool m_won = false;               // +0x433c
    bool m_coinsShown = false;        // +0x4358: the cheque was shown (set at once when lost)
    bool m_tipsShown = false;         // +0x4359
    bool m_areaComplete = false;      // +0x435a
    int m_nextArea = -1;              // +0x435c: the area after the completed one (0-based)
    int m_completedArea = -1;         // +0x4360
};

// Debug mode only: AS_ENDSCREEN=<surgery>[,<mode>,<won>,<stars>,<gained>,<first>,<partner>]
// opens on this screen with a made-up result (see SurgeryEndScreen.cpp);
// nullptr otherwise.
Scene* endScreenViewer();

}  // namespace mt
