#include "as2/GameController.h"

#include <algorithm>
#include <cmath>

#include "as2/Connector.h"
#include "as2/Game.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/Loader.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/SceneManager.h"
#include "as2/Texture2D.h"
#include "as2/tools/Stitch.h"
#include "as2/menus/Bandages.h"
#include "as2/menus/Hud.h"
#include "platform/Platform.h"

namespace as2 {

GameController* gameController = nullptr;
Game* game = nullptr;
GameState* gameState = nullptr;

GameController::GameController() {
    gameController = this;
    SaveDetails::CreateNewSaveDataFile();
    returnedFromCredits = false;
    // userInterfaceIdiom: the iPad.
    isIPad = true;
    bPostTotalScore = false;
}

void GameController::OneTimeInit() {
    // UIScreen bounds: the iPad's portrait 768 x 1024 points.
    screenBounds = {{0, 0}, {768, 1024}};
    Connector::sharedConnector();
    mCurrentSplashLevel = 0;
    mSplashStartTimer = 0;
}

void GameController::initOpenGL() {
    glInitialised = true;
    // isIphone4 (a 640-wide screen) is false on the iPad: all factors 1,
    // then the iPad doubles the art's scale.
    ScreenScaleFactor = InverseScaleFactor = GfxScaleFactor = 1;
    if (isIPad) GfxScaleFactor += GfxScaleFactor;
    ActualScreenSize = {screenBounds.size.height * ScreenScaleFactor, screenBounds.size.width * ScreenScaleFactor};
    // The original turned its portrait framebuffer to landscape here
    // (glOrthof on the portrait bounds, translate, rotate 270); the port's
    // screen is landscape already, set up by the window's drawing.
    SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("cutter", true);
    SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("Default2");
}

void GameController::touchesBegan() {
    if (mCurrentSplashLevel < 2 && mSplashStartTimer < 130) mSplashStartTimer = 130;
}

bool GameController::readTilt() {
    float x, y;
    if (!platform::deviceTilt(x, y)) return false;
    // The light's offset is the tilt times the screen (_accelerometer[1]
    // across, [0] down), toward the lower edge.
    double keep = std::pow(0.95, platform::passSeconds() / 0.01);
    _accelerometer[1] = _accelerometer[1] * keep + x * (1 - keep);
    _accelerometer[0] = _accelerometer[0] * keep + y * (1 - keep);
    return true;
}

namespace {

// L0AccelerationIsShaking: two of the axes changed by more than the
// threshold.
bool accelerationIsShaking(const double last[3], const double now[3], double threshold) {
    bool dx = std::fabs(last[0] - now[0]) > threshold, dy = std::fabs(last[1] - now[1]) > threshold,
         dz = std::fabs(last[2] - now[2]) > threshold;
    return (dx && dy) || (dx && dz) || (dy && dz);
}

}  // namespace

void GameController::updateShake() {
    bool was = mShakeDetected;
    readShake();
    if (mShakeDetected != was) platform::debugTrace("shake %s", mShakeDetected ? "on" : "off");
}

void GameController::readShake() {
    float ax, ay, az;
    if (platform::deviceAcceleration(ax, ay, az)) {
        double now[3] = {ax, ay, az};
        if (!firstAcceleration) {
            if (!histeresisExcited && accelerationIsShaking(lastAcceleration, now, 0.5399)) {
                histeresisExcited = mShakeDetected = true;
            } else if (histeresisExcited && !accelerationIsShaking(lastAcceleration, now, 0.4887)) {
                histeresisExcited = mShakeDetected = false;
            }
        }
        for (int k = 0; k < 3; ++k) lastAcceleration[k] = now[k];
        firstAcceleration = false;
        return;
    }
    // The pointer: a hard stroke turned back on the last one is a reversal;
    // kShakeQuiet passes without one, it starts over.
    if (firstAcceleration) {
        mLastPointer = mPointer;
        mLastStroke = {0, 0};
        mStrokeReversals = 0;
        firstAcceleration = false;
    }
    Point v = {mPointer.x - mLastPointer.x, mPointer.y - mLastPointer.y};
    mLastPointer = mPointer;
    mStrokeReversed = false;
    if (v.x * v.x + v.y * v.y >= kShakeStroke * kShakeStroke) {
        if (v.x * mLastStroke.x + v.y * mLastStroke.y < 0) {
            mStrokeReversed = true;
            ++mStrokeReversals;
        }
        mLastStroke = v;
        mStrokeQuiet = 0;
    } else if (++mStrokeQuiet > kShakeQuiet) {
        mStrokeReversals = 0;
        mLastStroke = {0, 0};
    }
    // (Pressed, it works a tool: no shake, and none on letting go.)
    if (pointerPressed) mStrokeReversals = 0;
    if (mStrokeReversals >= kShakeReversals) histeresisExcited = mShakeDetected = true;
    else if (mStrokeReversals == 0) histeresisExcited = mShakeDetected = false;
}

double GameController::balance(int axis) const {
    float x, y;
    return platform::deviceTilt(x, y) ? _accelerometer[axis] : mKeyTilt[axis];
}

void GameController::arrowKey(int dx, int dy, bool down) {
    // (0 down, 1 across, as the accelerometer.)
    int axis = dx ? 1 : 0, way = dx ? dx : dy;
    if (down) mArrowsHeld[axis] = way;
    else if (mArrowsHeld[axis] == way) mArrowsHeld[axis] = 0;
}

void GameController::pointerAt(float x, float y) {
    mPointer = {x, y};
    float tx, ty;
    if (platform::deviceTilt(tx, ty)) return;  // the device's tilt steers it
    float w = ActualScreenSize.width * InverseScaleFactor, h = ActualScreenSize.height * InverseScaleFactor;
    if (w <= 0 || h <= 0) return;
    _accelerometer[1] = (x - w * 0.5f) / w;
    _accelerometer[0] = (y - h * 0.5f) / h;
}

std::string GameController::getPatientName(int level) const {
    return level >= 0 && level < 30 ? GameData::patientName[level] : "";
}

int GameController::getRequiredScoreForGrade(int grade, int level) const {
    if (grade < 0 || grade > 3 || level < 0 || level >= 30) return 0;
    // Miss Distress's A asked for the very score of a frame-perfect run
    // (2262: every point and every frame of the clock), out of a person's
    // reach: 3% under it, as the other missions' A leave.
    if (grade == 0 && level == 4) return 2195;
    return GameData::requiredScore[grade][level];
}

void GameController::StartGameOnLevel() {
    if (game) releaseRef(game);
    my_game = Game::initWithMissionData(Connector::sharedConnector().get_current_mission());
}

void GameController::showPauseMenu() {
    bool cfo = Connector::sharedConnector().current_mission_id >= 20;
    // (The CFO pack's own pause, the first pack's the Flash AS2's.)
    MenuManager::sharedMenuManager().setupNewViewController(cfo ? "CFOPauseMenu" : "PauseMenu",
                                                            cfo ? "CFOPauseMenu" : "PauseMenu");
}

void GameController::updateScene() {
    if (!glInitialised) initOpenGL();
    // postTotalHighScore went to Game Center: removed.
    bPostTotalScore = false;
    if (game && game->mKillGame) {
        if (gameState) releaseRef(gameState);
        releaseRef(game);
    }
    if (!game) return;
    updateShake();
    for (int axis = 0; axis < 2; ++axis)
        mKeyTilt[axis] = std::clamp(mKeyTilt[axis] + mArrowsHeld[axis] / 128.0, -0.125, 0.125);
    SceneManager& sm = SceneManager::sharedSceneManagerInstance();
    if (sm.mOldLayerFade > 0 && sm.mOldLayerFadeSpeed > 0) {
        sm.mOldLayerFade = sm.mOldLayerFade - sm.mOldLayerFadeSpeed;
        if (sm.mOldLayerFade <= 0) {
            sm.mOldLayerFadeSpeed = 0;
            sm.mOldLayerFade = 0;
            if (gameState) gameState->fadeComplete();
        }
    } else if (game->torch) {
        readTilt();
        game->drawTorchWithAccelerometer(_accelerometer);
    }
    if (game) game->checkRender();
}

void GameController::renderScene() {
    if (!glInitialised) initOpenGL();
    SingletonTextureManager& tm = SingletonTextureManager::sharedTextureManagerInstance();
    if (mCurrentSplashLevel < 2) {
        // The two splash screens: 130 frames fading in over 10, then 10 out.
        ++mSplashStartTimer;
        if (mSplashStartTimer > 140) {
            mSplashStartTimer = 0;
            if (mCurrentSplashLevel == 0) {
                mCurrentSplashLevel = 1;
                tm.getTexture2D("surgeonsplash");
                MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
                al.loadSoundWithKey("SELECT", "button-select", "caf", false);
                al.loadSoundWithKey("SELECT2", "CFOclick", "mp3", false);
                al.loadSoundWithKey("FLATLINE", "flatline", "mp3", false);
            } else {
                mCurrentSplashLevel = 2;
                Loader::sharedLoader().startMenuLoad();
                // Game Center's sign-in (processGameCenterAuthDone) is gone:
                // straight to the menu.
                MenuManager::sharedMenuManager().ReplaceViewController("MainMenu", "MainMenu", "fade", "fade");
                mCurrentSplashLevel = 3;
            }
            return;
        }
        float a;
        if (mSplashStartTimer < 130) {
            a = std::min(mSplashStartTimer / 10.0f, 1.0f);
        } else {
            a = 1 - (mSplashStartTimer - 130) / 10.0f;
            if (a < 0) a = 0;
        }
        glColor(a, a, a, a);
        glPushMatrix();
        glScalef(InverseScaleFactor, InverseScaleFactor, 1);
        Texture2D* t = tm.getTexture2D(mCurrentSplashLevel == 0 ? "Default2" : "surgeonsplash");
        if (t) t->drawAtPoint(ActualScreenSize.width * 0.5f, ActualScreenSize.height * 0.5f, 1);
        glPopMatrix();
        return;
    }
    Loader& loader = Loader::sharedLoader();
    if (loader.mLoadingState != 0) {
        // (Not while the bandages play: they cover the change, and the
        // work would slow them down.)
        if (!bandages::playing()) loader.Load();
        if (mCurrentSplashLevel < 4) return;
        loader.render();
        return;
    }
    // (A failed mission stays, stopped, under its obituary; the CFO pack's
    // own summary, the iPad's, over black as there.)
    if (game && game->mFailed && mLevelPack == 1) return;
    SceneManager::sharedSceneManagerInstance().renderScene();
    drawStitches();
    // Over it all, the HUD and the pointer (the Flash AS2's).
    Hud::shared().draw();
}

void GameController::drawStitches() {
    // The stitches being drawn: point sprites of "stitch" along the lines.
    if (!gameState || !gameState->currentTool) return;
    Stitch* st = dynamic_cast<Stitch*>(gameState->currentTool);
    if (!st || st->mStitchCount < 1) return;
    glColor(0.5f, 0, 1, 1);
    Texture2D* tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("stitch");
    if (!tex) return;
    const float size = 10;  // glPointSize on the iPad
    glPushMatrix();
    for (int i = 0; i < st->mStitchCount; ++i) {
        Point a = st->mStitches[i], b = st->mStitches[i + 1];
        // renderLineFromPoint:toPoint: a point every 3 units.
        float dx = b.x - a.x, dy = b.y - a.y;
        int n = std::max(1, static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy) / 3.0f)));
        for (int k = 0; k < n; ++k) {
            float t = static_cast<float>(k) / n;
            float st4[4] = {0, 0, tex->maxS(), tex->maxT()};
            // (Drawn at the finger: a new piece of thread shows at once.)
            platform::appearAtOnce();
            tex->drawInRect(a.x + dx * t - size / 2, a.y + dy * t - size / 2, size, size, st4,
                            platform::DrawId(0x5354000000000000ull + i * 4096 + k));
        }
    }
    glPopMatrix();
}

}  // namespace as2
