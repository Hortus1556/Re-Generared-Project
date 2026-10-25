// cocos2d-x 2.1.1's CCDirector, ported to Bleed Engine, with what it owns:
// the scheduler, the action manager, the touch dispatcher, the accelerometer
// and the keypad dispatcher. MIT: LICENSE-cocos2d-x.txt.
//
// The engine runs the loop: a game calls mainLoop() once a pass (the
// display link's frame in 2.1.1). Its delta time is the pass's length, the
// animation interval (platform::setPassSeconds), not the wall clock, so play
// is the same however fast the machine is and the engine smooths what moves
// between passes. The screen is the game's in points (GameInfo's size, y up
// for the nodes), drawn with cocos's 2D projection; art is made for
// contentScaleFactor pixels a point. The platform's pointer is one touch
// (handleTouches*, as CCEGLViewProtocol's), the Escape key the keypad's back.
#pragma once

#include <functional>
#include <vector>

#include "engine/cocos/CCArray.h"
#include "engine/cocos/CCGeometry.h"

namespace cocos2d {

class CCScene;
class CCNode;
class CCScheduler;
class CCActionManager;
class CCTouchDispatcher;
class CCTouch;
class CCAccelerometerDelegate;
class CCKeypadDelegate;

class CCAccelerometer {
public:
    void setDelegate(CCAccelerometerDelegate* d) { m_pDelegate = d; }
    CCAccelerometerDelegate* getDelegate() { return m_pDelegate; }
    void setAccelerometerInterval(float interval) { m_fInterval = interval; }
    float getAccelerometerInterval() { return m_fInterval; }
    // the device's reading, if it has an accelerometer (platform::deviceAcceleration)
    void update(float dt);

private:
    CCAccelerometerDelegate* m_pDelegate = nullptr;
    float m_fInterval = 1 / 60.0f;
    float m_fElapsed = 0;
};

enum ccKeypadMSGType { kTypeBackClicked = 1, kTypeMenuClicked };

class CCKeypadDispatcher {
public:
    void addDelegate(CCKeypadDelegate* d);
    void removeDelegate(CCKeypadDelegate* d);
    bool dispatchKeypadMSG(ccKeypadMSGType msg);

private:
    std::vector<CCKeypadDelegate*> m_delegates;
    bool m_bLocked = false;
    std::vector<CCKeypadDelegate*> m_toAdd, m_toRemove;
};

class CCDirector : public CCObject {
public:
    static CCDirector* sharedDirector();
    ~CCDirector() override;

    CCScene* getRunningScene() { return m_pRunningScene; }
    double getAnimationInterval() { return m_dAnimationInterval; }
    void setAnimationInterval(double interval);
    bool isDisplayStats() { return false; }
    void setDisplayStats(bool) {}
    bool isPaused() { return m_bPaused; }
    unsigned int getTotalFrames() { return m_uTotalFrames; }
    float getDeltaTime() { return m_fDeltaTime; }
    bool isSendCleanupToScene() { return m_bSendCleanupToScene; }
    CCNode* getNotificationNode() { return m_pNotificationNode; }
    void setNotificationNode(CCNode* node);
    // Not 2.1.1's: drawn over everything each frame (after the notification
    // node), just before the frame is presented, with the projection and
    // matrices it sets itself: a game's debug overlay (AS3's F1 menu).
    void setOverlay(std::function<void()> draw) { m_overlay = std::move(draw); }
    // Not 2.1.1's: drawn last of all each frame, after the overlay: the
    // game's pointer art (AS3's, on a PC).
    void setPointerOverlay(std::function<void()> draw) { m_pointerOverlay = std::move(draw); }

    // the screen, in points and in pixels
    CCSize getWinSize() { return m_obWinSizeInPoints; }
    CCSize getWinSizeInPixels() {
        return CCSizeMake(m_obWinSizeInPoints.width * m_fContentScaleFactor, m_obWinSizeInPoints.height * m_fContentScaleFactor);
    }
    CCSize getVisibleSize() { return m_obWinSizeInPoints; }
    CCPoint getVisibleOrigin() { return CCPointZero; }
    void setWinSize(const CCSize& points) { m_obWinSizeInPoints = points; }
    // 2.1.1's CCEGLView::setDesignResolutionSize with kResolutionShowAll (the
    // view is the engine's: the platform letterboxes it in the window): the
    // screen becomes `points` from the next frame drawn, the director's win
    // size and the platform's game screen (platform::setScreenSize) both.
    // Nodes already laid out keep their places; a game lays its screens out
    // again (AS3: at its next screen change).
    void setDesignResolutionSize(float width, float height);
    float getContentScaleFactor() { return m_fContentScaleFactor; }
    void setContentScaleFactor(float s) { m_fContentScaleFactor = s; }
    // window coordinates (points, y down) and GL ones (y up)
    CCPoint convertToGL(const CCPoint& uiPoint) { return ccp(uiPoint.x, m_obWinSizeInPoints.height - uiPoint.y); }
    CCPoint convertToUI(const CCPoint& glPoint) { return ccp(glPoint.x, m_obWinSizeInPoints.height - glPoint.y); }

    void runWithScene(CCScene* scene);
    void pushScene(CCScene* scene);
    void popScene();
    void popToRootScene();
    void replaceScene(CCScene* scene);
    void end() { m_bPurgeDirecotorInNextLoop = true; }
    void pause();
    void resume();
    void stopAnimation() { m_bInvalid = true; }
    void startAnimation() { m_bInvalid = false; }
    void purgeCachedData();
    void setDepthTest(bool) {}
    void setAlphaBlending(bool) {}
    void setProjection(int) {}

    // one frame: called by the game once a pass
    void mainLoop();
    void drawScene();
    bool hasEnded() { return m_bEnded; }

    CCScheduler* getScheduler() { return m_pScheduler; }
    void setScheduler(CCScheduler* s);
    CCActionManager* getActionManager() { return m_pActionManager; }
    void setActionManager(CCActionManager* am);
    CCTouchDispatcher* getTouchDispatcher() { return m_pTouchDispatcher; }
    CCAccelerometer* getAccelerometer() { return &m_accelerometer; }
    CCKeypadDispatcher* getKeypadDispatcher() { return &m_keypad; }

    // the platform's pointer as touch 0, in window points (y down); CCEGLViewProtocol::handleTouches*
    void handleTouchBegin(float x, float y);
    void handleTouchMove(float x, float y);
    void handleTouchEnd(float x, float y);
    void handleTouchCancel(float x, float y);

private:
    CCDirector();
    void setNextScene();
    CCScheduler* m_pScheduler;
    CCActionManager* m_pActionManager;
    CCTouchDispatcher* m_pTouchDispatcher;
    CCAccelerometer m_accelerometer;
    CCKeypadDispatcher m_keypad;
    CCScene* m_pRunningScene = nullptr;
    CCScene* m_pNextScene = nullptr;
    CCNode* m_pNotificationNode = nullptr;
    std::function<void()> m_overlay;
    std::function<void()> m_pointerOverlay;
    CCArray m_obScenesStack;
    bool m_bSendCleanupToScene = false;
    bool m_bPaused = false;
    bool m_bInvalid = false;
    bool m_bPurgeDirecotorInNextLoop = false;
    bool m_bEnded = false;
    double m_dAnimationInterval = 1.0 / 60;
    double m_dOldAnimationInterval = 1.0 / 60;
    float m_fDeltaTime = 0;
    unsigned int m_uTotalFrames = 0;
    CCSize m_obWinSizeInPoints;
    float m_fContentScaleFactor = 1;
    CCTouch* m_pTouch = nullptr;  // the pointer's, while it is down
};

// The director's scheduler (what CCTimer unschedules itself from).
CCScheduler* ccDirectorScheduler();

}  // namespace cocos2d
