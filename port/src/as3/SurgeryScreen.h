// mt::SurgeryScreen (made by 0x2fb14; 0x3024 bytes; re/as3/notes/surgery.md):
// the surgery itself. Its stages each show the patient's body (a
// background) with injuries (InjuryState) to treat with the tools along the
// sides (ToolState); the heart (health) falls with the injuries' damage and
// the tools' misuse, the clock runs down. When a stage's injuries are all
// done the surgery goes on to its next stage; after the last it is won. The
// patient dying or the time running out loses it.
//
// (SurgeryTools.cpp: the tools' parts; SurgeryPartners.cpp: the effects and
// the tag partner; SurgeryTutorial.cpp: the tutorial.)
#pragma once

#include <map>
#include <string>
#include <vector>

#include "as3/ComboMeter.h"
#include "as3/EffectList.h"
#include "as3/InjuryState.h"
#include "as3/Scene.h"
#include "as3/SurgeryData.h"
#include "as3/SurgeryHooks.h"
#include "as3/SurgeryInfo.h"
#include "as3/ToolState.h"
#include "as3/TutorialMgr.h"

namespace mt {

class ClampsController;

class SurgeryScreen : public Scene, public EffectList::Listener {
public:
    static SurgeryScreen* create(const SurgeryInfo& surgery, const SurgeryParams& params);  // 0x2fb14
    ~SurgeryScreen() override;

    void load() override;  // vf87
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // vf96
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // vf97
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // vf98

    // ---- the F1 debug menu's (debug mode only; not the original's: SurgeryDebug.cpp, DebugMenu.h) ----
    static SurgeryScreen* running();  // the surgery among the screens (under a pause too), or null
    bool debugPlayable();             // shown on top, not over
    void debugWin();                  // won now (every stage counted done)
    void debugLose();                 // lost now (the heart stopped)
    void debugNextStage();            // the stage's injuries taken away: the game's own stage logic goes on
    void debugHeal();                 // the heart full
    void debugAddTime(float seconds);
    bool debugHasPartner();           // a tag partner's button shown
    void debugFillTag();              // the tag partner's button charged
    void debugLeave();                // left for another screen: no more updates, touches or sounds
    void debugFreeTools();            // every tool's button shown and on
    static bool s_debugGodMode;       // the heart and clock kept up, no death
    void debugGodMode();              // (checkEnd, each frame) its part
    // the current stage's injuries still to treat (the hit areas' overlay)
    const std::vector<InjuryState*>& debugInjuries() { return run().injuries; }

    // ---- the HUD on a big screen and the keyboard (SurgeryHud.cpp; not the original's) ----
    // A number key (1..9, 10 for 0, 11 the key after it): the tool in that
    // place of the bar's order, as a click on its button.
    void toolKey(int n);
    void letterKey(char c);
    // Escape (a phone's back key): the pause button, as a click; on the
    // pause screen over it, its back button. False if neither took it.
    bool escapeKey();
    // a key's button pressed (toolKey, letterKey: SurgeryHud.cpp)
    void keyPress(const std::string& button, const char* key);
    // What the PC's pointer shows (PcPointer.h): false while the surgery is
    // not the screen on top or is over; `tool` the selected one (-1 none).
    bool pointerShown(int& tool, bool& drawOut);  // (PcPointer: the tool held, the injection drawing out)
    // The layout asked for (debug: AS_HUD_LAYOUT, the F1 menu): 0 by the
    // screen's size, 1 the tool bar across the top, 2 the original's sides.
    static int s_hudLayoutOverride;

private:
    // a stage's injuries as it runs (0x9c bytes at +0x8dc)
    struct StageRun {
        bool done = false;   // +0x84
        bool begun = false;  // +0x85
        std::vector<InjuryState*> injuries;  // +0x88: still to treat
        std::vector<InjuryState*> treated;   // +0x94
    };
    struct ToolData {  // tools.json (0x31b64)
        float cooldown = 0;
        bool hasCooldown = false;
        int healthToGive = 0, wrongUsageDamage = 0;
        float timeBetweenWrongUsages = 0, turnOnGrace = 0, afterHealGrace = 0;
        bool multipleInjuries = false;
    };
    enum TouchKind { kBegan = 0, kEnded = 1, kMoved = 2 };
    struct QueuedTouch {  // +0x2f80: 10 a frame
        int kind;
        cocos2d::CCPoint point;
    };

    using Scene::init;
    bool init(const SurgeryInfo& surgery, const SurgeryParams& params);  // vf95 0x41bc4
    void loadTools();                     // 0x31b64
    void loadInjuryDefs(const json::Value& root);  // 0x32060
    void buildLayers();                   // 0x32cec
    void buildStages();                   // 0x75f58's injuries
    void buildTools();                    // 0x4182c
    void buildHud();                      // 0x3f298
    void tick(float dt);                  // 0x3b2ec
    void processTouches();                // 0x30cdc
    void touchBegan();                    // 0x2fc38
    void touchMoved();                    // 0x30c58
    void touchEnded();                    // 0x2ff18
    void useTool(float dt);               // 0x39624
    void misuse(int kind);                // 0x34d44
    void scream();                        // 0x34e88
    HitResult hitInjuries(const cocos2d::CCPoint& touch);  // 0x398e4
    void runEvents(std::vector<InjuryEvent>& events, bool fromTool);  // 0x4c0c8 / 0x4b294
    InjuryState* createInjury(const InjuryEvent& e, bool fromTool);  // event 2 (0x4c0c8, 0x4b294)
    void damage(float amount);            // 0x32ef0
    void heal(float amount);              // 0x3a188
    void updateInjuries(float dt);        // 0x32e38
    void sumInjuryDamage();               // 0x32ea4
    void refreshInjuries();               // 0x333b4
    void drainInjuryEvents();             // 0x33464
    void removeTreated();                 // 0x330fc
    void checkStageDone();                // 0x3c990
    void checkDependencies();             // 0x33244
    void checkStageJump();                // 0x3a244
    void checkEnd();                      // 0x35138
    void beginStage(int stage);           // 0x32b00
    void endStage(bool keepTool);         // 0x33d18
    void showStage();                     // 0x3ca50
    void showBackground();                // 0x778fc
    void buildInjuryGraphics(InjuryState* injury);  // 0x77c4c
    void pickUp(const HitResult& r);
    void hold(cocos2d::CCSprite* sprite);  // 0x39d08
    void dropHeld();                       // 0x3dc94
    void placeHeld();                      // 0x34c40
    void showTray();                       // 0x39d3c
    void hideTray();                       // 0x33f24
    void releasePiece(InjuryState* injury);
    // a transplant's old organ all on the tray: its new one comes up on it
    // (0x2ff18's tray branch)
    void showTrayOrgan(InjuryState* transplant);
    // the tongs pressed on the new organ on the tray: taken (0x2fc38)
    bool takeTrayOrgan();
    void refreshHud();                    // 0x37850
    void selectTool(int tool);            // 0x3900c
    void onButton(cocos2d::CCObject* sender);  // 0x30d9c
    void win();                           // 0x36ec4
    void lose();                          // 0x35aa8
    void finishWin();                     // 0x3cc64
    void finishLose();                    // 0x3d6f0
    void cleanUp();                       // 0x3d574
    // the pause button: the surgery frozen, the pause screen over it (0x3eb7c)
    void pause();
    // The background, its extras and the injuries wobble as a liquid (each
    // layer's CCLiquid tagged 0, 1, 2), unless they already do (0x42a60: a
    // short one; 0x4c0c8 case 15: `extend` lengthens the running ones and
    // renews the blue screen)
    void wobble(float duration, float grid, unsigned int waves, float amplitude, bool extend);
    // a layer's wobble over: its grid off (the original left it on, as last
    // moved, so the picture stayed a little warped)
    void endWobbles();
    // the stage's injuries' animations held (0x433bc) or going on (0x43438)
    void pauseInjuryAnimations(bool pause);
    // the heart meter: its circle under the heart and its highlight (in
    // 0x3f298), coloured by the heart (0x35034)
    void buildHeartMeter();
    void refreshHeartColour();
    // the clock's warnings: "60/30/10 Seconds Left" in the out of time box,
    // risen where the tutorial's box shows for 3 s (0x3a65c), and down again
    // (0x3ab9c; at once when built); the tutorial's text hidden meanwhile
    // (0x3b06c, 0x3b1ac)
    void showClockWarning(const std::string& text);
    void hideClockWarning();
    void hideClockWarningAtOnce();
    void hideTutorialText();
    void showTutorialText();
    cocos2d::CCPoint m_clockBoxAt, m_clockItemAt, m_clockTextAt;  // +0x2ea8, +0x2eb0, +0x2eb8
    bool m_clockWarningShown = false;  // +0x1009
    // the heart's warning popup (LifeWarningScreen) once a heart fallen under
    // 10 has been back to 20 (+0x8f9; 0x33ce0), and its end (0x33dec)
    bool m_lifeWarningArmed = true;
    void showLifeWarning();
    void lifeWarningClosed();
    // back from a screen pushed over it: unfrozen unless an effect holds it
    // (the high five's slot: the partner, the serum) (vf91 0x434c8)
    void onShown() override;
    // what the screens pushed over the surgery call back (SurgeryHooks.h)
    SurgeryHooks hooks();
    SurgeryResult result(bool won) const; // 0x4f384
    StageRun& run() { return m_runs[m_stage - 1]; }
    const SurgeryStageDef& stageDef() const { return m_def.stages[m_stage - 1]; }

    SurgeryInfo m_surgery;            // +0x430
    SurgeryParams m_params;           // +0x554
    std::map<int, ToolData> m_toolData;  // +0x578
    std::map<std::string, InjuryDef> m_injuryDefs;  // +0x560
    SurgeryDef m_def;                 // +0x5b8
    std::vector<StageRun> m_runs;     // +0x8dc + 0x9c a stage
    std::string m_music;              // +0x8dc
    float m_health = 100;             // +0x8e0: the heart
    float m_healthShown = 100;        // +0x8e4
    int m_hearts = 0;                 // +0x8e8: Lady Saturday's revivals
    float m_time = 0;                 // +0x8ec: seconds left
    int m_stage = 1;                  // +0x8f4 (1-based)
    float m_stageTime = 0;            // 1.45 +0x1040: seconds on the stage (paused time not counted)
    bool m_ending = false;            // +0x8f8
    ToolState* m_tool = nullptr;      // +0x8fc: the selected tool
    std::map<int, ToolState*> m_tools;  // +0x590
    std::vector<ToolState*> m_toolList;  // +0x930
    int m_completedStages = 0;        // +0xf7c
    int m_jumpToStage = 0;            // +0xf80
    bool m_noNewInjuries = false;     // +0xf90: a transplant being done, no injury made meanwhile
    float m_weightDone = 0;           // +0xfd0
    float m_injuryDamageMultiplier = -1;  // +0xfdc
    float m_toolGrace = 0;            // +0x5b0
    float m_timer5b4 = 0;             // +0x5b4
    float m_deathGrace = 0;           // +0x2d64 (the death shield upgrade)
    bool m_deathShieldUsed = false;   // +0x2d60
    float m_warningTimer = 0;         // +0x3010: the heart's blip
    float m_warningEvery = 3;         // +0x3014: its time at 15 health, shorter below
    float m_tickTock = 1;             // +0x301c
    cocos2d::CCPoint m_touch;         // +0x5a8
    // (the port's) the tongs' reach on a touch device (SurgeryTools.cpp):
    // the touch going on moved onto what they take
    cocos2d::CCPoint m_touchShift;
    bool m_reachPending = false, m_probing = false;
    bool tongsReachWanted() const;
    HitResult tongsReach();
    std::vector<QueuedTouch> m_touches;  // +0x2f80
    bool m_touchMoved = false;        // +0x3000
    cocos2d::CCSprite* m_bg = nullptr;     // +0x100c
    cocos2d::CCNode* m_bgExtras = nullptr; // +0x1010
    cocos2d::CCNode* m_injuryLayer = nullptr;  // +0x1014
    cocos2d::CCNode* m_toolLayer = nullptr;    // +0x1018
    cocos2d::CCSprite* m_topLayer = nullptr;   // +0x101c
    cocos2d::CCNode* m_hud = nullptr;          // +0x1020
    UIDefinition m_hudDef;                     // +0x1028
    bool m_over = false;
    ComboMeter m_combo;                    // +0x2e60
    cocos2d::CCSprite* m_held = nullptr;   // +0x2d34: what the tongs hold
    cocos2d::CCPoint m_heldOffset;         // +0x1000
    cocos2d::CCSprite* m_tray = nullptr;   // +0x2d38
    cocos2d::CCSprite* m_heartFill = nullptr;  // +0x2d40: the heart meter's circle, coloured by the heart
    cocos2d::CCSprite* m_trayHand = nullptr;  // +0x2d3c
    bool m_trayShown = false;              // +0xf84
    // a transplant's new organ on the tray, to be taken with the tongs
    // (+0xf88: its piece, +0xf8c: the transplant; its sprite on the screen)
    PieceState* m_trayPiece = nullptr;
    InjuryState* m_trayInjury = nullptr;

    // the tutorial (SurgeryTutorial.cpp, TutorialMgr.h)
    void startTutorial();                  // load's part (0x41d38): made, read unless done
    void runTutorial(float dt);            // 0x3c238: a frame, its actions' events
    void showTutorialPopup(const std::string& text, const std::string& character, bool closeButton);  // 0x42b34
    void closeTutorialPopup(bool instant);  // 0x3ebb8
    void showTutorialPause();              // 0x33524
    void hideTutorialPause(bool instant);  // 0x33a88
    void pauseHudForTutorial();            // 0x33d18's HUD part
    void tutorialOk();                     // 0x30d9c: "ingame_tutorial_ok"
    void clampForTutorial();               // 0x35138's start
    void completeTutorial();               // 0x3cc64 → 0xe7f4c
    std::unique_ptr<TutorialMgr> m_tutorial;     // +0x2e64
    std::map<std::string, bool> m_enabledItems;  // +0x2e68: the HUD's buttons a player may use
    bool m_popupShown = false;             // +0x1008: the tutorial box is up
    bool m_tutorialPaused = false;         // a popup paused the game (the port's)
    cocos2d::CCPoint m_tutorialBoxAt, m_tutorialPartnerAt, m_tutorialPauseAt, m_tutorialOkAt,
        m_tutorialTextAt;                  // +0x2e80, +0x2e88, +0x2e90, +0x2e98, +0x2ea0
    std::vector<std::pair<cocos2d::CCMenuItem*, bool>> m_pausedItems;  // +0x2ec0: enabled before the pause

    // ---- the effects (EffectList.h) and the tag partner ----
public:
    void effectFinished(Effect* effect) override;  // 0x35cf0 (EffectList::Listener)
    void tagInfoClosed();                          // 0x33740: TagInfoScreen's ok
private:
    EffectList::ToolContext toolContext();          // what 0x3df88 reads
    // a feedback popup at a point (+ half its height), with Nowe's coins
    // when her power runs (0x398e4, 0x3dbbc, 0x34d44, 0x35cf0)
    void showTag(int kind, int points, const cocos2d::CCPoint& at, bool coins);
    void comboPopup();                    // 0x3dbbc: a combo action at the touch
    void resumeGame();                    // 0x33e04
    void setupTagPartner();               // 0x41d38's part
    void buildTagButton();                // 0x3f298's part
    void refreshTagButton();              // 0x37850's part
    void chargeTag();
    void placeTagSparkle();  // (the port's) the ready sparkle on the tag button, wherever it is                     // 0x37054
    void tagPressed();                    // 0x3eb38
    void startHighFive();                 // 0x33758
    void startPartnerEffects();           // 0x36204
    void startPartnerPower();             // 0x36778
    void updatePartnerPowers(float dt);   // 0x3b2ec's timers
    void healthCaughtUp();                // 0x3b2ec: the shown heart reached the real one
    bool reviveWithSaturday();            // 0x35138's revive
    void slideToolButtons(bool out);      // 0x37318 / 0x3bf58
    void endBrutality();                  // 0x3bf58
    InjuryState* batonTarget();           // 0x3c198
    bool reverendPicksTool();             // 0x38d8c (with 0x38bf0)

    EffectList m_effects{this};           // +0x2d84..+0x2e58
    Effect* m_trail = nullptr;            // +0x2d34 for the chainsaw: its trail cursor (retained)
    bool m_showTrail = false;             // +0x3008: the cursor shown at the next frame
    Effect* m_stageEffect = nullptr;      // +0x2da8 (1.45): the stage's own effect (retained)
    void showStageEffect();               // (1.45: the end of runSurgeryStageSwitch 0x4aae8)
    void effectsFrame(float dt);          // the effects' part of 0x3b2ec
    std::string m_partnerId;              // +0xf9c ("" none)
    int m_partner = -1;                   // +0xf98: its index (0x429758)
    float m_tagCharge = 0;                // +0xf94: the actions done
    float m_tagNeeded = 0;                // +4000: actions_to_unlock (x0.8 with the upgrade)
    bool m_tagCharging = true;            // +0xf91
    bool m_tagReady = false;              // +0xfa4
    bool m_tagUsed = false;               // +0xfd8: the power used this surgery
    bool m_powerRunning = false;          // +0x2d6c: a timed power (no charging meanwhile)
    float m_flowTimer = 0, m_flowTimer2 = 0;  // +0xfb4, +0xfb8
    float m_botTimer = 0;                 // +0xfbc: the clock stands still
    float m_reverendTimer = 0;            // +0xfc0: the tools pick themselves, no harm done
    float m_brutalityTimer = 0;           // +0xfc4
    float m_batonTimer = 0;               // +0xfc8
    bool m_touchDown = false;             // +0xfcc
    std::vector<InjuryState*> m_batoned;  // the injuries a baton is on its way to (their +4)
    cocos2d::CCMenuItemToggle* m_tagButton = nullptr;  // +0x3004
    cocos2d::CCNode* m_tagGrey = nullptr;   // +0x2d44: the greyed partner over the charge
    cocos2d::CCNode* m_tagGreyMask = nullptr;  // its clipping node's stencil

    // ---- the tools' own parts of the screen (SurgeryTools.cpp) ----
    // An injury's area on the body (event 12 "graphics": 0x1c bytes at
    // +0xfe8): no injury may be made over one (unless allowed), and the
    // injection heals only off them
    struct GraphicsRect {
        cocos2d::CCRect rect;           // +0
        InjuryState* injury = nullptr;  // +0x10
        float rotation = 0;             // +0x14
        bool flag18 = true;             // +0x18
        bool injectable = true;         // +0x19: not the injury's m_fd94
    };
    void addGraphicsRect(const InjuryEvent& e, InjuryState* injury);  // 0x3435c
    // a point in an area (turned with its injury), of an unfinished injury;
    // only the injectable ones if asked (0x343a0)
    bool inGraphicsRect(const cocos2d::CCPoint& p, bool injectableOnly);
    // a finished injury leaving: its area and its clamps go (0x33188's part)
    void injuryRemoved(InjuryState* injury);
    void placeTool();                     // 0x34434: the tool at the touch, its node with it
    void toolsFrame(float dt);            // 0x34470: the cooldowns, the selected tool's frame
    // 0x39624's start by tool: the scanner's scan, the clamps' own touch, the
    // injection's in mode off the injuries' areas; false: no hit test
    bool toolBeforeHitTest();
    void toolHit(const HitResult& r);     // 0x39624: the clamps' and injection's part of a hit
    void scan();                          // 0x39104: the scanner's blips and arrow
    void refreshInjectionBars();          // 0x34580: the injection's cooldown bars
    void showInjectionMenu();             // 0x3e1b4: its two buttons and graph
    void hideInjectionMenu();             // 0x33fa4
    void swapInjectionButtons();          // 0x3e4e4: the mode chosen comes first
    void placeInjectionBars();            // 0x3e768: the bars on the first button
    // 0x30d9c's injection buttons: true if `name` was one
    bool injectionButton(const std::string& name, cocos2d::CCObject* sender);
    void freezeTools(float seconds);      // event 17 (0x33f68): the tools frozen a while
    void unfreezeTools();                 // 0x3a2b0
    void deselectTool();                  // 0x33ea4: the tool let go of and put down
    ClampsController* clamps();           // the clamps tool, if the surgery has it
    std::vector<GraphicsRect> m_graphics;  // +0xfe8
    bool m_injectionMenu = false;          // +0x3001: its two buttons shown
    cocos2d::CCNode* m_injectionFirst = nullptr;   // +0x2d50: the button in the injection's place
    cocos2d::CCNode* m_injectionSecond = nullptr;  // +0x2d54: the other, beside it
    float m_frozenTimer = 0;               // +0xfac
    bool m_toolsFrozen = false;            // +0xfb0

    // ---- the HUD's layout (SurgeryHud.cpp; not the original's) ----
    // On a big screen (platform::largeScreen: any PC, a tablet) the tools'
    // buttons are one bar across the top, in the order of their number keys;
    // on a phone the original's two columns. Chosen as the HUD is built and
    // again whenever the screen changes (hudFrame).
    bool wantTopBar() const;
    cocos2d::CCPoint hudCardMiddle(const std::string& button) const;
    void placeKeyLabels();
    // the bar's cards made longer (SurgeryHud.cpp: cardLonger), their
    // longer part kept in their state
    void lengthenCards(bool on);
    void syncLongCards();
    struct LongCard {
        cocos2d::CCMenuItemToggle* button = nullptr;
        cocos2d::CCNode* holder = nullptr;      // (a child of the button)
        std::string tool;                       // its tool art's name (ToolArt.h)
        cocos2d::CCPoint cardTopLeft;           // in the button's box (points)
        cocos2d::CCSprite* card = nullptr;      // the long card
        cocos2d::CCSprite* toolSprite = nullptr;
        std::vector<cocos2d::CCNode*> images;   // the button's own pictures (hidden in the bar)
        int state = -1;
    };
    std::vector<LongCard> m_longCards;               // layoutHud's, and as the clamps' count changes
    void layoutHud(bool top);
    // (the port's) the bar compact while a point to treat lies where the big
    // bar would be (SurgeryHud.cpp's hudFrame)
    bool m_barCompact = false;
    float m_barBigBottom = 0, m_barLeft = 0;  // the big bar's bottom (cocos y) and left
    // the meter split for the bar (SurgeryHud.cpp): built once, shown or not
    void splitMeter();
    void showMeterSplit(bool on);
    cocos2d::CCNode* m_meterCircle = nullptr;   // (children of the meter's picture)
    cocos2d::CCNode* m_meterHousing = nullptr;
    float m_meterCircleRight = 0;               // the circle's right, in the meter's points
    cocos2d::CCRect m_meterHousingSpan;         // the housing's, in the meter's points
    float m_meterRow0 = 0, m_meterPixels = 1;   // the picture's top row (the meter's y), its pixels a point
    void crossfadeStage();  // (the port's) a stage change: the old background fades out over the new
    void dropBackgroundGhost();
    cocos2d::CCNode* m_bgGhost = nullptr;        // (the port's) the last stage's background, fading out
    cocos2d::CCNode* m_bgExtrasGhost = nullptr;
    float m_bgGhostLeft = 0;            // buildHud's part, and on a change
    void hudFrame();                     // each tick: the layout followed, the keys' labels
    struct HudSpot {                     // a node's place in the original's layout
        cocos2d::CCNode* node;
        cocos2d::CCPoint at;
    };
    std::vector<HudSpot> m_sideSpots;    // (captured before the first change)
    bool m_hudLaidOut = false;
    bool m_topBar = false;
    int m_hudCheck = 0;
    float m_pauseBoxShift = 0;           // the tutorial's PAUSED box moved under the bar
    std::vector<cocos2d::CCNode*> m_keyLabels;
    std::string m_clampCount;  // (the clamps' count shown: placeKeyLabels again as it changes)
};

}  // namespace mt
