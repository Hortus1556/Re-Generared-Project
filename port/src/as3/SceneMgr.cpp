#include "as3/SceneMgr.h"

#include "as3/LoadingScreen.h"
#include "as3/MediatonicSplashScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/Scene.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSpriteFrameCache.h"
#include "platform/Platform.h"

#include <cstdlib>
#include <typeinfo>

namespace mt {

namespace {

// (Debug, AS_PROFILE: each screen change is traced with its time.)
bool profiling() {
    static const bool on = platform::debugMode() && std::getenv("AS_PROFILE");
    return on;
}

// Whether the passes go on while a screen's sheets are decoded: not under
// automation (unless AS_ASYNC_LOAD), where the screen's first pass must not
// depend on how long the decoding took, so scripts and seeded runs repeat.
bool loadsWhilePassing() {
    static const bool on = !platform::automationActive() || (platform::debugMode() && std::getenv("AS_ASYNC_LOAD"));
    return on;
}

std::string nameOf(Scene* s) {
    if (!s) return "-";
    if (!s->getName().empty()) return s->getName();
    return typeid(*s).name();
}

}  // namespace

SceneMgr* SceneMgr::sharedSceneMgr() {
    static SceneMgr* s = new SceneMgr;
    return s;
}

SceneMgr::SceneMgr() {
    init();
    schedule(schedule_selector(SceneMgr::update));
}

SceneMgr::~SceneMgr() {
    unschedule(schedule_selector(SceneMgr::update));
    for (Entry& e : m_stack) e.scene->release();
    CC_SAFE_RELEASE(m_pending);
    CC_SAFE_RELEASE(m_waiting);
}

Scene* SceneMgr::topScene() { return m_stack.empty() ? nullptr : m_stack.back().scene; }

// On: its touches and layers as they were; off: they are saved and turned off.
void SceneMgr::setSceneInput(Entry& e, bool on) {
    if (on) {
        if (e.touchWasOn) e.scene->setTouchRegistered(true);
    } else {
        e.touchWasOn = e.scene->isTouchRegistered();
        e.scene->setTouchRegistered(false);
    }
    if (!e.loaded) return;
    if (cocos2d::CCLayer* a = e.scene->ui.menuA) {
        if (on) {
            if (e.layerAWasOn) a->setTouchEnabled(true);
        } else {
            e.layerAWasOn = a->isTouchEnabled();
            a->setTouchEnabled(false);
        }
    }
    if (cocos2d::CCLayer* b = e.scene->ui.menuB) {
        if (on) {
            if (e.layerBWasOn) b->setTouchEnabled(true);
        } else {
            e.layerBWasOn = b->isTouchEnabled();
            b->setTouchEnabled(false);
        }
    }
}

void SceneMgr::replaceScene(Scene* scene) {
    if (m_pending || m_state != kIdle) return;
    m_startedAt = platform::passTime();
    if (!m_stack.empty()) setSceneInput(m_stack.back(), false);
    m_pending = scene;
    scene->retain();
    if (profiling()) profileStart(scene, "replace");
    for (Entry& e : m_stack) e.scene->onRemoved();
    m_state = kReplacing;
}

void SceneMgr::pushScene(Scene* scene, bool unloadBelow) {
    if (m_state != kIdle || m_stack.empty()) return;
    m_state = kCovering;  // the one below loses its input next frame
    if (unloadBelow) {
        m_stack.back().scene->unload();
        m_stack.back().loaded = false;
    }
    scene->retain();
    if (profiling()) profileStart(scene, "push");
    const double t0 = platform::now();
    scene->load();
    if (profiling()) platform::debugTrace("  %s loaded in %.0f ms", nameOf(scene).c_str(), (platform::now() - t0) * 1000);
    addChild(scene, m_baseZ + static_cast<int>(m_stack.size()));
    m_stack.back().scene->onCovered();
    m_stack.push_back(Entry{scene});
    m_stack.back().scene->onShown();
}

void SceneMgr::popScene() {
    if (m_stack.size() < 2) return;
    Entry top = m_stack.back();
    top.scene->onRemoved();
    removeChild(top.scene, true);
    top.scene->unload();
    top.scene->release();
    m_stack.pop_back();
    Entry& below = m_stack.back();
    if (profiling()) profileStart(below.scene, "pop to");
    if (!below.loaded) {
        const double t0 = platform::now();
        below.scene->load();
        if (profiling())
            platform::debugTrace("  %s loaded in %.0f ms", nameOf(below.scene).c_str(), (platform::now() - t0) * 1000);
        below.loaded = true;
    }
    setSceneInput(below, true);
    below.scene->onShown();
}

// The screen coming in: the caches purged of what the last ones used, then
// loaded and shown on top.
void SceneMgr::showPending() {
    m_state = kIdle;
    cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->removeUnusedSpriteFrames();
    cocos2d::CCDirector::sharedDirector()->purgeCachedData();
    cocos2d::CCTextureCache::sharedTextureCache()->removeUnusedTextures();
    Scene* s = m_pending;
    const double t0 = platform::now();
    s->load();
    if (profiling()) platform::debugTrace("  %s loaded in %.0f ms", nameOf(s).c_str(), (platform::now() - t0) * 1000);
    // (the port's) Its sheets are decoded on worker threads (CCTextureCache):
    // the loading screen stays until they are done, the passes going on,
    // where the original's load held the loading screen's frame.
    if (m_waiting && loadsWhilePassing() && !cocos2d::CCTextureCache::sharedTextureCache()->uploadDecoded(0.004)) {
        m_state = kLoading;
        return;
    }
    showLoaded();
}

void SceneMgr::showLoaded() {
    m_state = kIdle;
    if (m_waiting) {
        removeChild(m_waiting, true);
        m_waiting->release();
        m_waiting = nullptr;
    }
    cocos2d::CCTextureCache::sharedTextureCache()->finishDecodings();  // (uploaded)
    Scene* s = m_pending;
    addChild(s, m_baseZ + static_cast<int>(m_stack.size()));
    m_stack.push_back(Entry{s});  // the stack holds the pending one's reference
    m_pending = nullptr;
    m_stack.back().scene->onShown();
}

// Every screen taken down; then the new one, through the splash or the
// loading screen when there is one.
void SceneMgr::startWaiting() {
    while (!m_stack.empty()) {
        Scene* s = m_stack.back().scene;
        removeChild(s, true);
        s->unload();
        s->release();
        m_stack.pop_back();
    }
    // (the port's) With no screen up, the screen takes the window's shape
    // if it changed (a window resized, a foldable folded or unfolded):
    // the waiting screen and the new one are laid out for it. Screens
    // pushed later take their parent's size.
    if (ResolutionMgr::sharedResolutionMgr()->fitWindow()) setContentSize(cocos2d::CCDirector::sharedDirector()->getWinSize());
    if (!m_firstLaunch) {
        if (!m_pending->needsLoadingScreen()) {
            showPending();
            return;
        }
        m_waiting = LoadingScreen::create();
    } else {
        m_waiting = MediatonicSplashScreen::create();
    }
    m_waiting->retain();
    m_waiting->load();
    addChild(m_waiting);
    m_state = kWaiting;
}

// (debug, AS_PROFILE)
void SceneMgr::profileStart(Scene* to, const char* how) {
    m_profileFrom = platform::now();
    m_profilePass = cocos2d::CCDirector::sharedDirector()->getTotalFrames();
    m_profileTo = nameOf(to);
    platform::debugTrace("scene %s %s (pass %u)", how, m_profileTo.c_str(), m_profilePass);
}

void SceneMgr::update(float dt) {
    // (debug, AS_PROFILE: the change's first frame has been drawn)
    if (!m_profileTo.empty() && m_state == kIdle && !m_pending) {
        const unsigned pass = cocos2d::CCDirector::sharedDirector()->getTotalFrames();
        platform::debugTrace("scene %s shown: %u passes, %.0f ms", m_profileTo.c_str(), pass - m_profilePass,
                             (platform::now() - m_profileFrom) * 1000);
        m_profileTo.clear();
    }
    SoundMgr::sharedSoundMgr()->update(dt);
    switch (m_state) {
        case kReplacing:
            startWaiting();
            return;
        case kWaiting:
            if (m_firstLaunch) {
                // (the original reported the load time to its analytics here)
                m_firstLaunch = false;
                m_state = kSplash;
                return;
            }
            // the loading screen has been drawn: now the new screen loads (at once, as the original)
            break;
        case kSplash:
            // the splash stays at least its seconds from when the change began
            if (platform::passTime() - m_startedAt < m_splashSeconds) return;
            break;
        case kCovering: {
            if (m_stack.size() >= 2) setSceneInput(m_stack[m_stack.size() - 2], false);
            m_state = kIdle;
            return;
        }
        case kLoading:
            // (uploaded as they are done, a few milliseconds a pass)
            if (cocos2d::CCTextureCache::sharedTextureCache()->uploadDecoded(0.004)) showLoaded();
            return;
        default:
            return;
    }
    // The waiting screen unloaded (the original also took it off here; the
    // port keeps it shown until the new screen's sheets are decoded:
    // showLoaded), then the new screen comes in.
    m_waiting->unload();
    showPending();
}

}  // namespace mt
