// cocos2d-x 2.1.1's CCDirector.cpp (and the accelerometer, the keypad
// dispatcher, CCEGLViewProtocol's touch handling), ported.
// MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCDirector.h"

#include "engine/cocos/CCFileUtils.h"

#include <algorithm>
#include <cstdlib>

#include "engine/cocos/CCActionManager.h"
#include "engine/cocos/CCLabelBMFont.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCScheduler.h"
#include "engine/cocos/CCSpriteFrameCache.h"
#include "engine/cocos/CCTexture2D.h"
#include "engine/cocos/CCTouch.h"
#include "engine/cocos/CCTransition.h"
#include "engine/cocos/SimpleAudioEngine.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"

namespace cocos2d {

// ------------------------------------------------------------- the pool

CCObject* CCObject::autorelease() {
    CCPoolManager::sharedPoolManager()->addObject(this);
    return this;
}

CCPoolManager* CCPoolManager::sharedPoolManager() {
    static CCPoolManager pm;
    return &pm;
}

void CCPoolManager::pop() {
    // what a release lets go of can autorelease more: drained until empty
    while (!objects_.empty()) {
        std::vector<CCObject*> batch;
        batch.swap(objects_);
        for (CCObject* o : batch) o->release();
    }
}

// ------------------------------------------------------- accelerometer

void CCAccelerometer::update(float dt) {
    if (!m_pDelegate) return;
    m_fElapsed += dt;
    if (m_fElapsed < m_fInterval) return;
    m_fElapsed = 0;
    float x, y, z;
    if (!platform::deviceAcceleration(x, y, z)) return;  // no accelerometer (a PC)
    CCAcceleration a;
    a.x = x;
    a.y = y;
    a.z = z;
    a.timestamp = platform::passTime();
    m_pDelegate->didAccelerate(&a);
}

// ------------------------------------------------------------- keypad

void CCKeypadDispatcher::addDelegate(CCKeypadDelegate* d) {
    if (!d) return;
    if (m_bLocked) {
        m_toAdd.push_back(d);
        return;
    }
    if (std::find(m_delegates.begin(), m_delegates.end(), d) == m_delegates.end()) m_delegates.push_back(d);
}

void CCKeypadDispatcher::removeDelegate(CCKeypadDelegate* d) {
    if (!d) return;
    if (m_bLocked) {
        m_toRemove.push_back(d);
        return;
    }
    m_delegates.erase(std::remove(m_delegates.begin(), m_delegates.end(), d), m_delegates.end());
}

bool CCKeypadDispatcher::dispatchKeypadMSG(ccKeypadMSGType msg) {
    m_bLocked = true;
    std::vector<CCKeypadDelegate*> all = m_delegates;
    for (CCKeypadDelegate* d : all) {
        if (msg == kTypeBackClicked) d->keyBackClicked();
        else if (msg == kTypeMenuClicked) d->keyMenuClicked();
    }
    m_bLocked = false;
    for (CCKeypadDelegate* d : m_toRemove) removeDelegate(d);
    for (CCKeypadDelegate* d : m_toAdd) addDelegate(d);
    m_toRemove.clear();
    m_toAdd.clear();
    return true;
}

// ------------------------------------------------------------ director

CCDirector* CCDirector::sharedDirector() {
    static CCDirector* d = new CCDirector;
    return d;
}

CCScheduler* ccDirectorScheduler() { return CCDirector::sharedDirector()->getScheduler(); }

CCDirector::CCDirector() {
    m_pScheduler = new CCScheduler;
    m_pActionManager = new CCActionManager;
    // the actions step before everything else
    m_pScheduler->scheduleUpdateForTarget(m_pActionManager, kCCPrioritySystem, false);
    m_pTouchDispatcher = new CCTouchDispatcher;
    m_obWinSizeInPoints = CCSizeMake(platform::screenWidth(), platform::screenHeight());
    m_dAnimationInterval = platform::passSeconds();
}

CCDirector::~CCDirector() {
    CC_SAFE_RELEASE(m_pRunningScene);
    CC_SAFE_RELEASE(m_pNotificationNode);
    CC_SAFE_RELEASE(m_pTouch);
    CC_SAFE_RELEASE(m_pScheduler);
    CC_SAFE_RELEASE(m_pActionManager);
    CC_SAFE_RELEASE(m_pTouchDispatcher);
}

void CCDirector::setDesignResolutionSize(float width, float height) {
    if (width <= 0 || height <= 0) return;  // (as 2.1.1)
    m_obWinSizeInPoints = CCSizeMake(width, height);
    platform::setScreenSize(static_cast<int>(width + 0.5f), static_cast<int>(height + 0.5f));
}

void CCDirector::setAnimationInterval(double interval) {
    m_dAnimationInterval = interval;
    platform::setPassSeconds(interval);  // the engine's pass is the display link's frame
}

void CCDirector::setNotificationNode(CCNode* node) {
    CC_SAFE_RETAIN(node);
    CC_SAFE_RELEASE(m_pNotificationNode);
    m_pNotificationNode = node;
}

void CCDirector::setScheduler(CCScheduler* s) {
    if (s == m_pScheduler) return;
    CC_SAFE_RETAIN(s);
    CC_SAFE_RELEASE(m_pScheduler);
    m_pScheduler = s;
}

void CCDirector::setActionManager(CCActionManager* am) {
    if (am == m_pActionManager) return;
    CC_SAFE_RETAIN(am);
    CC_SAFE_RELEASE(m_pActionManager);
    m_pActionManager = am;
}

void CCDirector::runWithScene(CCScene* scene) {
    pushScene(scene);
    startAnimation();
}

void CCDirector::replaceScene(CCScene* scene) {
    if (!m_pRunningScene && m_obScenesStack.count() == 0) {  // 2.1.1 asserts runWithScene came first
        runWithScene(scene);
        return;
    }
    m_bSendCleanupToScene = true;
    m_obScenesStack.replaceObjectAtIndex(m_obScenesStack.count() - 1, scene);
    m_pNextScene = scene;
}

void CCDirector::pushScene(CCScene* scene) {
    m_bSendCleanupToScene = false;
    m_obScenesStack.addObject(scene);
    m_pNextScene = scene;
}

void CCDirector::popScene() {
    m_obScenesStack.removeLastObject();
    unsigned int c = m_obScenesStack.count();
    if (c == 0) {
        end();
    } else {
        m_bSendCleanupToScene = true;
        m_pNextScene = static_cast<CCScene*>(m_obScenesStack.objectAtIndex(c - 1));
    }
}

void CCDirector::popToRootScene() {
    unsigned int c = m_obScenesStack.count();
    if (c == 1) {
        m_obScenesStack.removeLastObject();
        end();
        return;
    }
    while (c > 1) {
        CCScene* current = static_cast<CCScene*>(m_obScenesStack.lastObject());
        if (current->isRunning()) {
            current->onExitTransitionDidStart();
            current->onExit();
        }
        current->cleanup();
        m_obScenesStack.removeLastObject();
        c--;
    }
    m_pNextScene = static_cast<CCScene*>(m_obScenesStack.lastObject());
    m_bSendCleanupToScene = false;
}

void CCDirector::setNextScene() {
    bool runningIsTransition = dynamic_cast<CCTransitionScene*>(m_pRunningScene) != nullptr;
    bool newIsTransition = dynamic_cast<CCTransitionScene*>(m_pNextScene) != nullptr;
    // not a transition: the running scene exits (and is cleaned up, issue #709)
    if (!newIsTransition) {
        if (m_pRunningScene) {
            m_pRunningScene->onExitTransitionDidStart();
            m_pRunningScene->onExit();
        }
        if (m_bSendCleanupToScene && m_pRunningScene) m_pRunningScene->cleanup();
    }
    if (m_pRunningScene) m_pRunningScene->release();
    m_pRunningScene = m_pNextScene;
    m_pNextScene->retain();
    m_pNextScene = nullptr;
    if (!runningIsTransition && m_pRunningScene) {
        m_pRunningScene->onEnter();
        m_pRunningScene->onEnterTransitionDidFinish();
    }
}

void CCDirector::pause() {
    if (m_bPaused) return;
    m_dOldAnimationInterval = m_dAnimationInterval;
    setAnimationInterval(1 / 4.0);  // paused, it does not use the CPU
    m_bPaused = true;
}

void CCDirector::resume() {
    if (!m_bPaused) return;
    setAnimationInterval(m_dOldAnimationInterval);
    m_bPaused = false;
    m_fDeltaTime = 0;
}

void CCDirector::purgeCachedData() {
    // (2.1.1's: fonts, unused textures and the file lookups; the sprite
    // frames stay, whoever needs them gone removes them)
    CCLabelBMFont::purgeCachedData();
    CCTextureCache::sharedTextureCache()->removeUnusedTextures();
    CCFileUtils::sharedFileUtils()->purgeCachedEntries();
}

void CCDirector::drawScene() {
    // (Debug, AS_PROFILE: a pass over 20 ms is traced with where it went.)
    static const bool profiling = platform::debugMode() && std::getenv("AS_PROFILE");
    const double t0 = profiling ? platform::now() : 0;
    m_fDeltaTime = m_uTotalFrames == 0 ? 0 : static_cast<float>(m_dAnimationInterval);
    if (!m_bPaused) {
        m_pScheduler->update(m_fDeltaTime);
        m_accelerometer.update(m_fDeltaTime);
    }
    const double t1 = profiling ? platform::now() : 0;
    platform::clearScreen(0, 0, 0, 1);
    // to avoid flickr, the next scene is set here, after the update
    if (m_pNextScene) setNextScene();
    // cocos's 2D projection: points, y up, the origin at the bottom left
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, m_obWinSizeInPoints.width, 0, m_obWinSizeInPoints.height, -1024, 1024);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    if (m_pRunningScene) m_pRunningScene->visit();
    if (m_pNotificationNode) m_pNotificationNode->visit();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    if (m_overlay) m_overlay();  // (not 2.1.1's: setOverlay)
    if (m_pointerOverlay) m_pointerOverlay();  // (nor this: setPointerOverlay)
    m_uTotalFrames++;
    const double t2 = profiling ? platform::now() : 0;
    platform::endFrame();  // the frame presented (swapBuffers in 2.1.1)
    if (profiling && platform::now() - t0 > 0.02)
        platform::debugTrace("SLOW pass %u: %.0f ms (update %.0f, draw %.0f, end %.0f)", m_uTotalFrames,
                             (platform::now() - t0) * 1000, (t1 - t0) * 1000, (t2 - t1) * 1000,
                             (platform::now() - t2) * 1000);
}

void CCDirector::mainLoop() {
    if (m_bPurgeDirecotorInNextLoop) {
        m_bPurgeDirecotorInNextLoop = false;
        if (m_pRunningScene) {
            m_pRunningScene->onExitTransitionDidStart();
            m_pRunningScene->onExit();
            m_pRunningScene->cleanup();
            CC_SAFE_RELEASE_NULL(m_pRunningScene);
        }
        m_obScenesStack.removeAllObjects();
        m_bEnded = true;
    } else if (!m_bInvalid) {
        drawScene();
    }
    CocosDenshion::SimpleAudioEngine::updateShared();
    CCPoolManager::sharedPoolManager()->pop();  // the frame's autoreleased objects go
}

// --------------------------------------------------- the pointer's touch

static void dispatch(CCTouch* t, int phase) {
    CCSet set;
    set.addObject(t);
    CCEvent event;
    CCTouchDispatcher* d = CCDirector::sharedDirector()->getTouchDispatcher();
    switch (phase) {
        case CCTOUCHBEGAN: d->touchesBegan(&set, &event); break;
        case CCTOUCHMOVED: d->touchesMoved(&set, &event); break;
        case CCTOUCHENDED: d->touchesEnded(&set, &event); break;
        default: d->touchesCancelled(&set, &event); break;
    }
}

void CCDirector::handleTouchBegin(float x, float y) {
    if (m_pTouch) handleTouchCancel(x, y);  // a new press while one was down
    m_pTouch = new CCTouch;
    m_pTouch->setTouchInfo(0, x, y);
    dispatch(m_pTouch, CCTOUCHBEGAN);
}

void CCDirector::handleTouchMove(float x, float y) {
    if (!m_pTouch) return;
    m_pTouch->setTouchInfo(0, x, y);
    dispatch(m_pTouch, CCTOUCHMOVED);
}

void CCDirector::handleTouchEnd(float x, float y) {
    if (!m_pTouch) return;
    m_pTouch->setTouchInfo(0, x, y);
    CCTouch* t = m_pTouch;
    m_pTouch = nullptr;
    dispatch(t, CCTOUCHENDED);
    t->release();
}

void CCDirector::handleTouchCancel(float x, float y) {
    if (!m_pTouch) return;
    m_pTouch->setTouchInfo(0, x, y);
    CCTouch* t = m_pTouch;
    m_pTouch = nullptr;
    dispatch(t, CCTOUCHCANCELLED);
    t->release();
}

}  // namespace cocos2d
