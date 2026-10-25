// cocos2d-x 2.1.1's CCScheduler and CCTimer, ported to Bleed Engine.
// MIT: LICENSE-cocos2d-x.txt.
//
// Each frame (CCDirector::drawScene): the per-frame updates, by priority
// (negative, zero, positive; the action manager runs at the system's,
// first), then every timer, target by target in the order they were first
// scheduled. A timer fires at most once a frame, given the time elapsed
// since it last fired. Unscheduling during the update is safe.
#pragma once

#include <climits>
#include <vector>

#include "engine/cocos/CCObject.h"

namespace cocos2d {

enum { kCCPrioritySystem = INT_MIN, kCCPriorityNonSystemMin = kCCPrioritySystem + 1 };
#define kCCRepeatForever (UINT_MAX - 1)

class CCTimer : public CCObject {
public:
    static CCTimer* timerWithTarget(CCObject* target, SEL_SCHEDULE selector, float seconds = 0);
    bool initWithTarget(CCObject* target, SEL_SCHEDULE selector, float seconds, unsigned int repeat, float delay);
    float getInterval() const { return m_fInterval; }
    void setInterval(float interval) { m_fInterval = interval; }
    SEL_SCHEDULE getSelector() const { return m_pfnSelector; }
    void update(float dt) override;

protected:
    CCObject* m_pTarget = nullptr;
    float m_fElapsed = -1;
    bool m_bRunForever = false;
    bool m_bUseDelay = false;
    unsigned int m_uTimesExecuted = 0;
    unsigned int m_uRepeat = 0;  // 0 = once, 1 is 2 x executed
    float m_fDelay = 0;
    float m_fInterval = 0;
    SEL_SCHEDULE m_pfnSelector = nullptr;
};

class CCScheduler : public CCObject {
public:
    CCScheduler() = default;
    ~CCScheduler() override;
    float getTimeScale() const { return m_fTimeScale; }
    void setTimeScale(float s) { m_fTimeScale = s; }
    void update(float dt) override;

    void scheduleSelector(SEL_SCHEDULE selector, CCObject* target, float interval, unsigned int repeat, float delay,
                          bool paused);
    void scheduleSelector(SEL_SCHEDULE selector, CCObject* target, float interval, bool paused) {
        scheduleSelector(selector, target, interval, kCCRepeatForever, 0.0f, paused);
    }
    // update(dt) of the target each frame; lower priorities first
    void scheduleUpdateForTarget(CCObject* target, int priority, bool paused);
    void unscheduleSelector(SEL_SCHEDULE selector, CCObject* target);
    void unscheduleUpdateForTarget(const CCObject* target);
    void unscheduleAllForTarget(CCObject* target);
    void unscheduleAll();
    void pauseTarget(CCObject* target);
    void resumeTarget(CCObject* target);
    bool isTargetPaused(CCObject* target);

private:
    struct UpdateEntry {
        // not retained (2.1.1 retained it; a node takes itself off when it
        // is freed instead, ~CCNode)
        CCObject* target;
        int priority;
        bool paused;
        bool markedForDeletion;
    };
    struct TimerEntry {
        CCObject* target;  // retained while it has timers
        std::vector<CCTimer*> timers;  // retained
        bool paused;
        // the timer stepping in update (2.1.1's timerIndex): moved back when
        // one at or before it is unscheduled, so none after it is skipped
        int timerIndex = 0;
    };
    TimerEntry* timersOf(const CCObject* target);
    void removeTimerEntry(TimerEntry* e);
    float m_fTimeScale = 1.0f;
    std::vector<UpdateEntry> m_updates;  // sorted by priority, stable
    std::vector<TimerEntry*> m_timers;   // in the order targets were first scheduled
    bool m_bUpdateHashLocked = false;
    TimerEntry* m_pCurrentTarget = nullptr;
    // the entry stepping in update: moved back when an entry before it is
    // removed (2.1.1 walked a linked hash, where that skipped none); -1 outside
    int m_nCurrentTargetIndex = -1;
    bool m_bCurrentTargetSalvaged = false;
    CCTimer* m_pCurrentTimer = nullptr;
    bool m_bCurrentTimerSalvaged = false;
};

}  // namespace cocos2d
