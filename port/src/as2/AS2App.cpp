#include "as2/AS2App.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include "as2/Connector.h"
#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/Loader.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/TypeAndTell.h"
#include "as2/menus/Bandages.h"
#include "as2/menus/FlashStage.h"
#include "as2/menus/Hud.h"
#include "as2/menus/MainMenu.h"
#include "as2/menus/Panel.h"
#include "as2/Registry.h"
#include "engine/MoviePlayer.h"
#include "engine/ui/DebugPanel.h"
#include "engine/ui/UIKit.h"
#include "platform/DrawList.h"

namespace as2 {

namespace {

struct Movie {
    std::unique_ptr<MoviePlayer> player;
    std::function<void()> finished;
    double lastTap = -10;
};
Movie gMovie;

void finishMovie() {
    std::function<void()> done = std::move(gMovie.finished);
    gMovie.finished = nullptr;
    gMovie.player.reset();
    if (done) done();
}

}  // namespace

void playMovie(const std::string& file, std::function<void()> finished) {
    gMovie.player = MoviePlayer::open(file);
    gMovie.finished = std::move(finished);
    if (!gMovie.player) {
        platform::debugTrace("movie: cannot play %s", file.c_str());
        finishMovie();
        return;
    }
    gMovie.player->aspectFill = false;  // MPMovieScalingModeAspectFit
    gMovie.player->start();
    platform::debugTrace("movie %s", file.c_str());
}

AS2App::AS2App() {
    registerClasses();
    flashstage::preload();
    new GameController;
    ui::Window& w = ui::Window::shared();
    // MainWindow.nib: the window and its EAGLView, filling the screen.
    EAGLView* gl = new EAGLView;
    gl->className = "EAGLView";
    gl->setFrame({{0, 0}, {static_cast<float>(platform::screenWidth()), static_cast<float>(platform::screenHeight())}});
    gl->drawHook = [] { gameController->renderScene(); };
    w.addSubview(gl);
    gameController->mEAGLView = gl;
    releaseRef(gl);
    gameController->OneTimeInit();
}

AS2App::~AS2App() { releaseRef(debugPanel); }

namespace {

// Every injury of the stage taken away: the game moves on as when they are
// treated. (Only these: the last one gone, the next stage's come at once.)
void takeStageAway() {
    std::vector<InjuryGraphic*> now(gameState->_injuries.begin(), gameState->_injuries.end());
    for (InjuryGraphic* i : now) retainRef(i);
    for (InjuryGraphic* i : now) {
        if (gameState->_injuries.indexOfObject(i) >= 0) gameState->removeInjury(i);
        releaseRef(i);
    }
}

}  // namespace

void AS2App::charKey(char c) {
    // PC only: a type-n-tell's keys typed on the keyboard.
    if (!gameState) return;
    for (InjuryGraphic* i : gameState->_injuries)
        if (auto* t = dynamic_cast<TypeAndTell*>(i)) t->typeKey(c);
}

void AS2App::debugMenuKey() {
    if (!debugPanel) debugPanel = new ui::DebugPanel([this] { return debugItems(); });
    debugPanel->toggle();
}

std::vector<ui::DebugItem> AS2App::debugItems() {
    std::vector<ui::DebugItem> items;
    // The missions: from the menus (no mission running, no movie).
    bool menus = !game && !gMovie.player;
    for (int n = 1; n <= 30; ++n) {
        ui::DebugItem it;
        it.label = std::to_string(n);
        it.width = 56;
        it.enabled = menus;
        it.newRow = n == 1 || n == 11 || n == 21;
        it.action = [n] {
            Connector& c = Connector::sharedConnector();
            c.set_current_mission(n - 1);
            c.set_current_mission_id(n - 1);
            gameController->mLevelPack = n > 20 ? 1 : 0;
            gameController->isEndScene = false;
            MyOpenAL::sharedMyOpenAL().stopSoundWithKey("background");
            Loader::sharedLoader().startMission();
        };
        items.push_back(it);
    }
    // In a mission.
    bool playing = game && gameState && !game->mWon && !game->mFailed;
    auto inMission = [&](const char* label, std::function<void()> action) {
        ui::DebugItem it;
        it.label = label;
        it.enabled = playing;
        it.action = std::move(action);
        items.push_back(it);
    };
    inMission("Win now", [] { game->missionCompleted(); });
    items.back().newRow = true;
    inMission("Next stage", [] { takeStageAway(); });
    inMission("Heal", [] { gameState->health = 100; });
    inMission("+60 s", [] { gameState->time += 60 * 30; });
    inMission("Lose", [] { game->missionFailed(false); });
    ui::DebugItem god;
    god.label = "God mode";
    god.isSwitch = true;
    god.on = gameController->godMode;
    god.action = [] { gameController->godMode = !gameController->godMode; };
    items.push_back(god);
    ui::DebugItem smooth;
    smooth.label = "Smooth motion";
    smooth.isSwitch = true;
    smooth.on = platform::smoothMotion();
    smooth.newRow = true;
    smooth.action = [] { platform::setSmoothMotion(!platform::smoothMotion()); };
    items.push_back(smooth);
    ui::DebugItem unlock;
    unlock.label = "Unlock all";
    unlock.action = [] {
        // A mission opens once the one before has a score: each unplayed one
        // gets 1 (in the save in use).
        for (int level = 1; level <= 30; ++level)
            if (SaveDetails::GetScoreForLevel(level) == 0) SaveDetails::SaveScoreForLevel(1, level);
    };
    items.push_back(unlock);
    return items;
}

void AS2App::gotoStage() {
    // AS_GOTO=<mission>:<stage> (debug mode): from the main menu straight
    // into the mission (as the debug menu's buttons), then each stage before
    // the one wanted taken away (as "Next stage") a second after it begins.
    static int mission = -1, stage = 0, settle = 0;
    static bool started = false;
    if (mission < 0) {
        mission = 0;
        const char* v = platform::debugMode() ? std::getenv("AS_GOTO") : nullptr;
        if (v) std::sscanf(v, "%d:%d", &mission, &stage);
        // AS_GOD (debug mode): god mode from the start, as its menu item.
        if (platform::debugMode() && std::getenv("AS_GOD")) gameController->godMode = true;
    }
    if (mission <= 0) return;
    if (!game) {
        if (started || gMovie.player || !dynamic_cast<MainMenu*>(MenuManager::sharedMenuManager().mCurrentViewController)) return;
        if (++settle < 30) return;
        Connector& c = Connector::sharedConnector();
        c.set_current_mission(mission - 1);
        c.set_current_mission_id(mission - 1);
        gameController->mLevelPack = mission > 20 ? 1 : 0;
        gameController->isEndScene = false;
        MyOpenAL::sharedMyOpenAL().stopSoundWithKey("background");
        Loader::sharedLoader().startMission();
        started = true;
        settle = 0;
        return;
    }
    if (!gameState || game->mWon || game->mFailed || gameState->currentStage >= stage) {
        if (gameState && gameState->currentStage >= stage) mission = 0;  // there
        return;
    }
    if (gameState->_injuries.count() == 0 || ++settle < 30) return;
    settle = 0;
    takeStageAway();
}

void AS2App::cycle() {
    gotoStage();
    if (gMovie.player) {
        if (gMovie.player->update()) {
            gMovie.player->draw();
            platform::endFrame();
            return;
        }
        finishMovie();
    }
    // God mode (the debug menu): the heart and the clock kept up.
    if (gameController->godMode && game && gameState && !game->mWon && !game->mFailed) {
        // (A brain out keeps its countdown: its new brain's battery
        // restarts it.)
        if (gameState->health > 0 && gameState->health < 100 && !gameState->brainOut()) gameState->health = 100;
        if (gameState->time < 60 * 30) gameState->time = 60 * 30;
        gameState->inShock = false;
    }
    // (Debug, AS_PROFILE: a pass over 30 ms is traced with where it went.)
    static const bool profiling = platform::debugMode() && std::getenv("AS_PROFILE");
    const double t0 = platform::now();
    ui::Window& w = ui::Window::shared();
    w.update();
    const double t1 = platform::now();
    // The display link: every frame (60 a second) at interval 1, every
    // other at 2 (a mission: the port's passes are then 30 a second, so
    // what moves is smoothed between them), else one frame in `interval`.
    int interval = gameController->mEAGLView->animationFrameInterval;
    platform::setPassSeconds(interval == 2 ? 1.0 / 30 : 1.0 / 60);
    bool update = interval <= 2 || ++framesSinceUpdate >= interval;
    if (update) {
        framesSinceUpdate = 0;
        gameController->updateScene();
    }
    const double t2 = platform::now();
    platform::clearScreen(0, 0, 0, 1);
    // Drawing the window draws the EAGLView's content: renderScene. It runs
    // every pass, so the GL picture is there under the menus at every
    // display frame (at interval 30 it only draws, with no loading to do).
    w.drawAll();
    // The bandages over everything, while they play.
    bandages::draw();
    // In an operation the pointer is the HUD's cursor (the Flash AS2's).
    platform::setSystemCursorVisible(!Hud::shared().drawsPointer());
    const double t3 = platform::now();
    flashstage::endFrame();
    platform::endFrame();
    if (profiling && platform::now() - t0 > 0.03)
        platform::debugTrace("SLOW pass %.0f ms: ui %.0f, update %.0f, draw %.0f, end %.0f", (platform::now() - t0) * 1000,
                             (t1 - t0) * 1000, (t2 - t1) * 1000, (t3 - t2) * 1000, (platform::now() - t3) * 1000);
}

void AS2App::touchBegan(float x, float y) {
    if (gMovie.player) {
        // PCGMovieOverlay: a double tap ends the movie.
        double now = platform::now();
        if (now - gMovie.lastTap < 0.4) finishMovie();
        gMovie.lastTap = now;
        return;
    }
    // (Touches wait while the bandages play.)
    if (bandages::playing()) return;
    gameController->pointerAt(x, y);
    gameController->pointerPressed = true;
    ui::Window::shared().touchBegan({x, y});
}

void AS2App::touchMoved(float x, float y, float px, float py) {
    if (gMovie.player) return;
    gameController->pointerAt(x, y);
    ui::Window::shared().touchMoved({x, y}, {px, py});
}

void AS2App::touchEnded(float x, float y, float px, float py) {
    if (gMovie.player) return;
    gameController->pointerPressed = false;
    ui::Window::shared().touchEnded({x, y}, {px, py});
}

void AS2App::escapeKey() {
    // Not in the iPad game (which had no keys): Escape skips a movie, and in
    // a mission presses the pause button.
    if (gMovie.player) {
        finishMovie();
        return;
    }
    if (Panel* p = dynamic_cast<Panel*>(MenuManager::sharedMenuManager().mCurrentViewController)) p->pauseKey();
}

void AS2App::numberKey(int n) {
    // PC only: the number keys pick the tools (1..9, 0 for the tenth).
    if (gMovie.player) return;
    if (Panel* p = dynamic_cast<Panel*>(MenuManager::sharedMenuManager().mCurrentViewController)) p->toolKey(n);
}

void AS2App::arrowKey(int dx, int dy, bool down) {
    // PC only: the arrows tilt the "device" for what is balanced by hand.
    gameController->arrowKey(dx, dy, down);
}

void AS2App::mouseMoved(float x, float y) {
    // The torch's light follows the pointer (the iPad's tilt).
    gameController->pointerAt(x, y);
}

}  // namespace as2
