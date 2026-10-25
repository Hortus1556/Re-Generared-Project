#include "as3/AppDelegate.h"

#include <cstdlib>
#include <ctime>

#include "as3/AnimationViewer.h"
#include "as3/DebugMenu.h"
#include "as3/EditorWidgets.h"
#include "as3/EffectViewer.h"
#include "as3/ParticleViewer.h"
#include "as3/PcPointer.h"
#include "as3/PauseScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SurgeryDebugStart.h"
#include "as3/SurgeryEndScreen.h"
#include "as3/SurgeryScreen.h"
#include "as3/VideoPlayer.h"
#include "as3/WelcomeScreen.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCTexture2D.h"

namespace as3 {

using namespace cocos2d;

AppDelegate::AppDelegate() {
    // (the original first set up CocosDenshion, then the payments, Game
    // Center, stats and ads singletons, the reminders it scheduled as local
    // notifications, and the iCloud save: online or device-only, removed)
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    // (debug: AS_SEED=<n> makes a scripted run repeat exactly)
    if (const char* seed = platform::debugMode() ? std::getenv("AS_SEED") : nullptr)
        std::srand(static_cast<unsigned>(std::atol(seed)));
    CCTexture2D::PVRImagesHavePremultipliedAlpha(true);

    CCDirector* director = CCDirector::sharedDirector();
    director->setAnimationInterval(1.0 / 60);
    director->setProjection(0);  // 2D
    // (the port's) The screen: as much of the canvas as the window's shape
    // shows, in canvas points (ResolutionMgr::fitWindow: 512x384 at 4:3,
    // 568x320 at 16:9, up to 626x320), its art the iPad retina's 4.0x (4
    // pixels a point); followed again at each screen change (SceneMgr).
    director->setContentScaleFactor(4);
    mt::ResolutionMgr* screen = mt::ResolutionMgr::sharedResolutionMgr();
    if (!screen->fitWindow()) {
        CCSize win = director->getWinSize();
        screen->init(static_cast<int>(win.width), static_cast<int>(win.height),
                     static_cast<int>(director->getContentScaleFactor()));
    }
    // (the play area, on the canvas: the same on every screen)
    screen->setRect(149, 45, 342, 255);

    director->runWithScene(mt::SceneMgr::sharedSceneMgr());
    mt::debugMenuInstall();  // (debug mode: the F1 menu's overlay, DebugMenu.h)
    mt::pointerInstall();    // (a PC: the yellow pointer in a surgery, PcPointer.h)
    // (the original asked the content server for newer data here)
    // (debug mode: AS_ANIM's animation viewer instead, AnimationViewer.h)
    // (debug mode: AS_PARTICLES's particle viewer instead, ParticleViewer.h)
    if (mt::AnimationViewer* viewer = mt::AnimationViewer::createFromEnv())
        mt::SceneMgr::sharedSceneMgr()->replaceScene(viewer);
    else if (mt::Scene* particles = mt::particleViewer())
        mt::SceneMgr::sharedSceneMgr()->replaceScene(particles);
    else if (mt::Scene* effects = mt::effectViewer())  // (AS_EFFECTS: EffectViewer.h)
        mt::SceneMgr::sharedSceneMgr()->replaceScene(effects);
    else if (mt::Scene* results = mt::endScreenViewer())  // (AS_ENDSCREEN: SurgeryEndScreen.h)
        mt::SceneMgr::sharedSceneMgr()->replaceScene(results);
    else if (mt::Scene* paused = mt::pauseViewer())  // (AS_PAUSE: PauseScreen.h)
        mt::SceneMgr::sharedSceneMgr()->replaceScene(paused);
    else if (mt::Scene* surgery = mt::surgeryFromEnv())  // (AS_SURGERY: SurgeryDebugStart.h)
        mt::SceneMgr::sharedSceneMgr()->replaceScene(surgery);
    else if (mt::Scene* screen = mt::screenFromEnv())  // (AS_SCREEN: SurgeryDebugStart.h)
        mt::SceneMgr::sharedSceneMgr()->replaceScene(screen);
    else
        mt::SceneMgr::sharedSceneMgr()->replaceScene(mt::WelcomeScreen::create());
}

AppDelegate::~AppDelegate() {}

void AppDelegate::cycle() { CCDirector::sharedDirector()->mainLoop(); }

// (a touch begun on the open debug menu is the menu's: DebugMenu.h)
void AppDelegate::touchBegan(float x, float y) {
    if (!mt::debugMenuTouch(0, x, y)) CCDirector::sharedDirector()->handleTouchBegin(x, y);
}
void AppDelegate::touchMoved(float x, float y, float, float) {
    if (!mt::debugMenuTouch(1, x, y)) CCDirector::sharedDirector()->handleTouchMove(x, y);
}
void AppDelegate::touchEnded(float x, float y, float, float) {
    if (!mt::debugMenuTouch(2, x, y)) CCDirector::sharedDirector()->handleTouchEnd(x, y);
}
void AppDelegate::debugMenuKey() { mt::debugMenuKey(); }
// (a PC's keyboard: the number keys pick the surgery's tools, SurgeryHud.cpp)
void AppDelegate::numberKey(int n) {
    if (mt::SurgeryScreen* s = mt::SurgeryScreen::running()) s->toolKey(n);
}
void AppDelegate::charKey(char c) {
    if (mt::SurgeryScreen* s = mt::SurgeryScreen::running()) s->letterKey(c);
}
void AppDelegate::textInput(const char* utf8) { mt::editor::keyboardText(utf8); }
void AppDelegate::textEditKey(platform::EditKey key) {
    mt::editor::keyboardKey(key == platform::EditKey::Backspace ? 0 : key == platform::EditKey::Enter ? 1 : 2);
}
void AppDelegate::escapeKey() {
    if (mt::debugMenuEscape()) return;  // (the debug menu open: Escape closes it)
    // a movie playing: Escape stops it (not in the original: a PC addition)
    if (mt::VideoPlayer::sharedVideoPlayer()->isPlaying()) {
        mt::VideoPlayer::sharedVideoPlayer()->stop();
        return;
    }
    // a surgery: its pause button, or its pause screen's back (SurgeryHud.cpp)
    if (mt::SurgeryScreen* s = mt::SurgeryScreen::running())
        if (s->escapeKey()) return;
    CCDirector::sharedDirector()->getKeypadDispatcher()->dispatchKeypadMSG(kTypeBackClicked);
}

}  // namespace as3
