// GameController: the game's loop (the EAGLView's display link calls
// updateScene: and renderScene once a frame), the splash screens, the
// screen's scale factors, and the run's shared state (the mission won, the
// level pack, the player's progress for the menus).
#pragma once

#include <string>

#include "engine/ui/UIKit.h"

namespace as2 {

class Game;
class GameState;
class GameController;

// The original's globals.
extern GameController* gameController;
extern Game* game;
extern GameState* gameState;

// EAGLView: the GL view under the menus, and its display link's frame
// interval (1: every display frame, 60 a second; 2 in a mission; 30 in
// the menus, where only UIKit moves).
class EAGLView : public ui::GLView {
public:
    int animationFrameInterval = 1;
    void setAnimationFrameInterval(int n) {
        if (n > 0) animationFrameInterval = n;
    }
};

class GameController {
public:
    GameController();

    // AS2AppDelegate application:didFinishLaunchingWithOptions: then
    // OneTimeInit; initOpenGL is done on the first frame.
    void OneTimeInit();
    void initOpenGL();
    // One display-link frame: update, then draw (recorded).
    void updateScene();
    void renderScene();
    // The needle's thread being drawn (renderScene).
    void drawStitches();

    // Splash: a touch skips to the fade out.
    void touchesBegan();

    std::string getPatientName(int level) const;
    int getRequiredScoreForGrade(int grade, int level) const;
    void StartGameOnLevel();
    void showPauseMenu();
    // GameCenter and OpenFeint are removed: scores and achievements go
    // nowhere.
    void SubmitAchievement(const std::string&, double) {}
    void submitHighscore(long long, int) {}
    bool isOpenFeintReady() const { return true; }

    // The accelerometer (the torch's tilt): smoothed x, y, z.
    double _accelerometer[3] = {0, 0, 0};
    float getXAccelerometer() const { return static_cast<float>(_accelerometer[0]); }
    float getYAccelerometer() const { return static_cast<float>(_accelerometer[1]); }
    void resetShake() { firstAcceleration = true; }
    // Each pass: whether the device is being shaken (mShakeDetected), as
    // the original's accelerometer:didAccelerate: (excited once two axes
    // change by more than 0.54 g between readings, calm again once they
    // change by no more than 0.49 g). Without an accelerometer (a PC), the
    // pointer shaken hard: strokes of kShakeStroke points a pass or more
    // turned back kShakeReversals times, each within kShakeQuiet passes of
    // the last (else it starts over, and the shake calms); only while it is
    // not pressed (pressed, it works a tool: a chainsaw sawing to and fro is
    // no shake).
    void updateShake();
    void readShake();
    // This pass's pointer stroke turned back a hard one (pressed or not):
    // a PC's rub (the big centipede's).
    bool pointerReversed() const { return mStrokeReversed; }
    // (Twice the speed and the reversals of a first measure, 15 points and
    // two, which any brisk move of a mouse made.)
    static constexpr float kShakeStroke = 30;  // screen points a pass (30 a second in a mission)
    static constexpr int kShakeReversals = 4, kShakeQuiet = 10;
    // A device with an accelerometer (platform::deviceTilt): its tilt read
    // each pass, smoothed as the original's accelerometer:didAccelerate:
    // did (0.95 of the last value and 0.05 of the reading, at its 100
    // readings a second). False without one.
    bool readTilt();
    // Without one (a PC): the pointer at a screen point tilts the "device"
    // so the torch's light is there (Game drawTorchWithAccelerometer: an
    // offset of the tilt times the screen from the middle).
    void pointerAt(float x, float y);
    // Where the pointer (or finger) last was, pressed or not, in screen
    // points.
    Point pointer() const { return mPointer; }
    bool pointerPressed = false;  // a finger down (the mouse's button held)
    // A tilt held by hand while the pointer is busy (the transistor's
    // needle, kept on its gauge while the cuts are stitched): the device's
    // own (as _accelerometer: 0 down, 1 across) or, without one, the arrow
    // keys' (arrowKey: held, they tilt it 1/128 a pass, up to 1/8 either
    // way).
    double balance(int axis) const;
    void arrowKey(int dx, int dy, bool down);

    bool glInitialised = false;
    Rect screenBounds;  // the portrait screen's bounds in points (768 x 1024)
    int state = 0;
    bool isEndScene = false;
    int winScore = 0, winTime = 0, winLevel = 0;
    bool winUpgradeTool = false;
    int mSplashStartTimer = 0;
    int mCurrentSplashLevel = 0;
    bool returnedFromCredits = false;
    EAGLView* mEAGLView = nullptr;
    std::string OFUserIDToUse = "Default";
    float GfxScaleFactor = 1, ScreenScaleFactor = 1, InverseScaleFactor = 1;
    int mLevelPack = 0;
    Size ActualScreenSize;  // landscape, in pixels (1024 x 768)
    bool isIPad = true;
    bool firstAcceleration = true, histeresisExcited = false, mShakeDetected = false;
    double lastAcceleration[3] = {0, 0, 0};
    double mKeyTilt[2] = {0, 0};
    int mArrowsHeld[2] = {0, 0};  // the way each axis's arrow is held (0 none)
    Point mPointer, mLastPointer, mLastStroke;
    int mStrokeReversals = 0, mStrokeQuiet = 0;
    bool mStrokeReversed = false;
    bool debugCFODifficultyTweak = false;
    // God mode (the debug menu): the heart and the clock kept up, no shock,
    // and a stopped heart no death.
    bool godMode = false;
    bool bPostTotalScore = false;
    Game* my_game = nullptr;
};

}  // namespace as2
