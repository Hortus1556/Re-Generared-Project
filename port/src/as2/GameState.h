// GameState: the state of a mission being played: score, combo, the HUD
// (heart rate, timer, score, the jogger dial), the stage's background,
// health and time, the injuries and blood, the touch in world points, the
// world's scroll, zoom and shake, the red flash, the tutorials.
#pragma once

#include <string>
#include <vector>

#include "core/Json.h"
#include "core/Object.h"

namespace as2 {

class ITool;
class InjuryGraphic;
class Injury;
class Popup;
class Sprite;

class GameState : public Object {
public:
    GameState();  // -init: sets the global gameState
    ~GameState() override;

    void setMouseXY(float x, float y);  // setMouseX:MouseY: (panel points / 2)
    // Where a world point is drawn on the game screen (the scene's
    // transform: positions count twice, zoom about (240, 160)).
    Point worldToScreen(Point p) const;
    void setShakeAsHigh(int shake);
    void updatePupils();
    void ChangeBG(const std::string& bg);
    // A tutorial's arrow under tool `tool` (1-10): the HUD's.
    void displayArrowOnTool(int tool);
    void showBatteryAnimAtXY(int x, int y);
    void checkBleedFingerFix();
    void flashRed();
    void createPopup(const std::string& image, int x, int y, InjuryGraphic* injury);
    void removeInjury(InjuryGraphic* injury);
    std::string getTutorialText() const;
    void clickedTutorial();
    void setupTutorial();
    void checkTutorial(int event, int stage);
    // A tutorial popup of the port's own (what the original left unsaid),
    // shown once like the game's, when tutorials are on and none is open.
    void showHint(const std::string& text);
    void fadeComplete();
    void setupStage(int stage);
    void addBlood(Injury* blood, int velocity);
    InjuryGraphic* addInjury(Injury* injury, bool playerCreated, bool force);
    void render();
    void eyeMovementForSprite(Sprite* eye, float xOrigin, float yOrigin, float xTrg, float yTrg);
    Rect getBoundingBoxForInjury(Injury* injury);
    // Empty in the original.
    void addOrgan(Object*) {}
    void removeBlood() {}
    void removeOrgans() {}

    // The brain out (a brain transplant's old one lifted out): the heart
    // runs down from where it is to 0 over kBrainOutTicks (18 seconds,
    // nothing else moving it meanwhile), shown on the clock and as a
    // crossed-out brain on the dial; at 0 it dies as any heart. The new brain's battery restarts it in time.
    // (The original stopped the heart at once and failed the mission 825
    // frames later, 27.5 seconds that showed nowhere.)
    static constexpr int kBrainOutTicks = 18 * 30;
    void loseBrain();
    void restoreBrain(float restartAmount);
    bool brainOut() const { return brainOutLeft >= 0; }

    // The heart stopped for good: once at 0 (run down, or a heart attack:
    // driven up to kHeartAttack it drops to 0 at once) nothing restarts it,
    // neither a shot nor the battery; the death runs out and the mission
    // fails. (Not in the graveyard, whose risen patient starts at 0, nor
    // for the vampire, who has no beat; not in god mode.)
    static constexpr float kHeartAttack = 200;
    bool heartStopped = false;
    // Whether the heart counts here (see heartStopped).
    bool heartCounts() const;

    bool inShock = false;
    int inShockTimer = 300;
    int brainOutLeft = -1;  // ticks left of the brain out (-1 none)
    float brainOutFrom = 0;  // the heart as the brain went out
    bool tutUp = false;
    const json::Value* current_tutorial = nullptr;  // into level_tutorial (or mHint)
    json::Value mHint;
    json::Value level_tutorial;                     // this mission's tutorials
    int injuryTimer = 0;
    long long score = 0;
    int combo = 0;
    int currentStage = 1;
    ITool* currentTool = nullptr;  // not retained (one of _tools)
    Array<ITool> possible_tools;
    Array<ITool> _tools;           // those the HUD offers
    // The dial's runner (the HUD draws it): its state (1-4 by the heart,
    // 5 dead, 6 shocked, 7 the brain out), its frame within it, and the
    // port's own picture for 6 and 7 with its scale.
    int joggerFrame = 1, joggerFrameType = 1;
    std::string joggerPicture;
    float joggerScale = 1;
    Sprite* spriteBg = nullptr;
    Sprite* spriteBgOld = nullptr;
    Sprite* spriteFg = nullptr;
    Sprite* spriteRedFlash = nullptr;
    Sprite* spriteBattery = nullptr;
    std::string _bg, _bgOld;
    int time = 0;
    float health = 0;
    int tracedHeart = -1;  // the heart rate last traced
    bool tracedShock = false;
    Array<InjuryGraphic> _injuries;
    Array<InjuryGraphic> _blood;
    // The injuries the tool is on: the original set this to the game's
    // findCorrectInjury: list itself each tool frame (nil before, as empty).
    Array<InjuryGraphic>& currentInjuries();
    Array<InjuryGraphic> _display_list;
    Array<InjuryGraphic> _fade_list;
    Array<InjuryGraphic> mTransplants;
    std::vector<std::string> mTexturesToClear, mTexturesToKeep, toRemoveFromClearList;
    Array<Popup> mPopups;
    std::vector<const json::Value*> mUsedTutorials;
    float mouseX = 0, mouseY = 0;
    float unscaledMouseX = 0, unscaledMouseY = 0;
    int soundTimer = 0;
    bool paused = true;
    float scale = 1;
    float _xScroll = 0, _yScroll = 0;
    float _xWorld = 0, _yWorld = 0;
    int currentShake = 0;
    int shakeTimer = 0;
    int flashRedTimer = 0;
    float flashRedDirection = 0;
    bool newTouch = false;
    bool updateToolPanel = false;
    int batteryAnim = -1;
    bool showTutorial = false;
    int bubbleTimer = 0;
    Sprite* sprLeftEye = nullptr;
    Sprite* sprRightEye = nullptr;
    Sprite* sprLeftEyeMask = nullptr;
    Sprite* sprRightEyeMask = nullptr;
};

}  // namespace as2
