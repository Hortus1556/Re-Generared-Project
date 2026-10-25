// mt::SceneMgr: the root scene (a singleton, 0x24fa9c; vtable 0x41b350),
// which holds the game's screens (mt::Scene) as a stack of children.
//
// replaceScene (0x24fbc8) takes every screen down and brings the new one in
// through a waiting screen: the Mediatonic splash at the first launch (held
// at least `m_splashSeconds`), else the loading screen if the new one asks
// for it (Scene::needsLoadingScreen), else none. pushScene (0x2500ec) puts
// one over the top, turning off the input of the one below (and unloading
// it if asked); popScene (0x2503cc) takes the top one off and gives the one
// below its input back (loading it again if it was unloaded). update() runs
// that, once a frame, and the sound manager's update.
#pragma once

#include <string>
#include <vector>

#include "engine/cocos/CCLayer.h"

namespace mt {

class Scene;

class SceneMgr : public cocos2d::CCScene {
public:
    static SceneMgr* sharedSceneMgr();  // 0x24fa9c
    ~SceneMgr() override;
    void update(float dt) override;  // 0x24fcdc

    void replaceScene(Scene* scene);                // 0x24fbc8
    void pushScene(Scene* scene, bool unloadBelow);  // 0x2500ec
    void popScene();                                // 0x2503cc
    Scene* topScene();
    int sceneCount() { return static_cast<int>(m_stack.size()); }

private:
    SceneMgr();
    struct Entry {
        Scene* scene;            // retained
        bool touchWasOn = false;   // +4: its touches, as it was covered
        bool layerAWasOn = false;  // +5
        bool layerBWasOn = false;  // +6
        bool loaded = true;        // +7
    };
    void setSceneInput(Entry& e, bool on);  // 0x24fc3c
    void showPending();
    void showLoaded();
    void startWaiting();

    // (kLoading, the port's: the new screen loaded, its sheets still being
    // decoded behind the waiting screen)
    enum State { kIdle = 0, kReplacing = 1, kWaiting = 2, kSplash = 3, kCovering = 4, kLoading = 5 };
    int m_baseZ = 0;            // +0xc8: the screens' z orders start here
    int m_splashSeconds = 2;    // +0xd0
    int m_state = kIdle;        // +0xd4
    Scene* m_pending = nullptr; // +0xd8: the screen coming in (retained)
    std::vector<Entry> m_stack; // +0xdc
    Scene* m_waiting = nullptr; // +0xf0: the splash or loading screen (retained)
    bool m_firstLaunch = true;  // +0xf4
    double m_startedAt = 0;     // +0xf8: when the change began
    // (debug, AS_PROFILE: the change being timed)
    void profileStart(Scene* to, const char* how);
    std::string m_profileTo;
    double m_profileFrom = 0;
    unsigned m_profilePass = 0;
};

}  // namespace mt
