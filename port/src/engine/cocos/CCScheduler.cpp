// cocos2d-x 2.1.1's CCScheduler.cpp, ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCScheduler.h"

#include <algorithm>

namespace cocos2d {

// ---------------------------------------------------------------- CCTimer

CCTimer* CCTimer::timerWithTarget(CCObject* target, SEL_SCHEDULE selector, float seconds) {
    CCTimer* t = new CCTimer;
    t->initWithTarget(target, selector, seconds, kCCRepeatForever, 0.0f);
    t->autorelease();
    return t;
}

bool CCTimer::initWithTarget(CCObject* target, SEL_SCHEDULE selector, float seconds, unsigned int repeat,
                             float delay) {
    m_pTarget = target;
    m_pfnSelector = selector;
    m_fElapsed = -1;
    m_fInterval = seconds;
    m_fDelay = delay;
    m_bUseDelay = delay > 0.0f;
    m_uRepeat = repeat;
    m_bRunForever = repeat == kCCRepeatForever;
    return true;
}

void CCTimer::update(float dt) {
    if (m_fElapsed == -1) {  // the first frame only starts the clock
        m_fElapsed = 0;
        m_uTimesExecuted = 0;
        return;
    }
    m_fElapsed += dt;
    if (m_bRunForever && !m_bUseDelay) {  // standard timer usage
        if (m_fElapsed >= m_fInterval) {
            if (m_pTarget && m_pfnSelector) (m_pTarget->*m_pfnSelector)(m_fElapsed);
            m_fElapsed = 0;
        }
        return;
    }
    if (m_bUseDelay) {
        if (m_fElapsed >= m_fDelay) {
            if (m_pTarget && m_pfnSelector) (m_pTarget->*m_pfnSelector)(m_fElapsed);
            m_fElapsed = m_fElapsed - m_fDelay;
            m_uTimesExecuted += 1;
            m_bUseDelay = false;
        }
    } else if (m_fElapsed >= m_fInterval) {
        if (m_pTarget && m_pfnSelector) (m_pTarget->*m_pfnSelector)(m_fElapsed);
        m_fElapsed = 0;
        m_uTimesExecuted += 1;
    }
    if (!m_bRunForever && m_uTimesExecuted > m_uRepeat) {
        extern CCScheduler* ccDirectorScheduler();
        ccDirectorScheduler()->unscheduleSelector(m_pfnSelector, m_pTarget);
    }
}

// ------------------------------------------------------------ CCScheduler

CCScheduler::~CCScheduler() { unscheduleAll(); }

CCScheduler::TimerEntry* CCScheduler::timersOf(const CCObject* target) {
    for (TimerEntry* e : m_timers)
        if (e->target == target) return e;
    return nullptr;
}

void CCScheduler::removeTimerEntry(TimerEntry* e) {
    auto it = std::find(m_timers.begin(), m_timers.end(), e);
    if (it == m_timers.end()) return;
    // Fixed: an entry at or before the one stepping in update moves the
    // rest down; without this the entry after it was skipped for the frame
    // (2.1.1's linked hash skipped none)
    if (static_cast<int>(it - m_timers.begin()) <= m_nCurrentTargetIndex) --m_nCurrentTargetIndex;
    m_timers.erase(it);
    for (CCTimer* t : e->timers) t->release();
    e->target->release();
    delete e;
}

void CCScheduler::scheduleSelector(SEL_SCHEDULE selector, CCObject* target, float interval, unsigned int repeat,
                                   float delay, bool paused) {
    if (!selector || !target) return;
    TimerEntry* e = timersOf(target);
    if (!e) {
        e = new TimerEntry{target, {}, paused};
        target->retain();
        m_timers.push_back(e);
    }
    for (CCTimer* t : e->timers)
        if (t->getSelector() == selector) {  // already scheduled: its interval updated
            t->setInterval(interval);
            return;
        }
    CCTimer* t = new CCTimer;
    t->initWithTarget(target, selector, interval, repeat, delay);
    e->timers.push_back(t);
}

void CCScheduler::unscheduleSelector(SEL_SCHEDULE selector, CCObject* target) {
    if (!target || !selector) return;
    TimerEntry* e = timersOf(target);
    if (!e) return;
    for (size_t i = 0; i < e->timers.size(); ++i) {
        CCTimer* t = e->timers[i];
        if (t->getSelector() != selector) continue;
        if (t == m_pCurrentTimer && !m_bCurrentTimerSalvaged) {
            t->retain();  // it is running: released when its step is done
            m_bCurrentTimerSalvaged = true;
        }
        e->timers.erase(e->timers.begin() + i);
        t->release();
        // the index of what is stepping moves down with it (2.1.1's
        // timerIndex): a timer unscheduling an earlier one of its target no
        // longer skips the one after it
        if (e->timerIndex >= static_cast<int>(i)) e->timerIndex--;
        if (e->timers.empty()) {
            if (m_pCurrentTarget == e) m_bCurrentTargetSalvaged = true;
            else removeTimerEntry(e);
        }
        return;
    }
}

void CCScheduler::scheduleUpdateForTarget(CCObject* target, int priority, bool paused) {
    for (UpdateEntry& u : m_updates)
        if (u.target == target) {
            u.markedForDeletion = false;  // TODO in 2.1.1 too: a changed priority is not applied
            return;
        }
    // after every entry of the same priority (priorityIn / appendIn)
    auto at = std::upper_bound(m_updates.begin(), m_updates.end(), priority,
                               [](int p, const UpdateEntry& u) { return p < u.priority; });
    m_updates.insert(at, UpdateEntry{target, priority, paused, false});
}

void CCScheduler::unscheduleUpdateForTarget(const CCObject* target) {
    for (size_t i = 0; i < m_updates.size(); ++i)
        if (m_updates[i].target == target) {
            if (m_bUpdateHashLocked) m_updates[i].markedForDeletion = true;
            else m_updates.erase(m_updates.begin() + i);
            return;
        }
}

void CCScheduler::unscheduleAllForTarget(CCObject* target) {
    if (!target) return;
    if (TimerEntry* e = timersOf(target)) {
        if (e == m_pCurrentTarget) {
            for (CCTimer* t : e->timers)
                if (t == m_pCurrentTimer && !m_bCurrentTimerSalvaged) {
                    t->retain();
                    m_bCurrentTimerSalvaged = true;
                }
            for (CCTimer* t : e->timers) t->release();
            e->timers.clear();
            m_bCurrentTargetSalvaged = true;
        } else {
            removeTimerEntry(e);
        }
    }
    unscheduleUpdateForTarget(target);
}

void CCScheduler::unscheduleAll() {
    std::vector<TimerEntry*> timers = m_timers;
    for (TimerEntry* e : timers) unscheduleAllForTarget(e->target);
    std::vector<UpdateEntry> updates = m_updates;
    for (UpdateEntry& u : updates)
        if (u.priority > kCCPrioritySystem) unscheduleUpdateForTarget(u.target);  // the system's stay
}

void CCScheduler::pauseTarget(CCObject* target) {
    if (TimerEntry* e = timersOf(target)) e->paused = true;
    for (UpdateEntry& u : m_updates)
        if (u.target == target) u.paused = true;
}

void CCScheduler::resumeTarget(CCObject* target) {
    if (TimerEntry* e = timersOf(target)) e->paused = false;
    for (UpdateEntry& u : m_updates)
        if (u.target == target) u.paused = false;
}

bool CCScheduler::isTargetPaused(CCObject* target) {
    if (TimerEntry* e = timersOf(target)) return e->paused;
    for (UpdateEntry& u : m_updates)
        if (u.target == target) return u.paused;
    return false;
}

void CCScheduler::update(float dt) {
    m_bUpdateHashLocked = true;
    if (m_fTimeScale != 1.0f) dt *= m_fTimeScale;

    // the per-frame updates, by priority; one added during this pass starts on the next
    std::vector<CCObject*> targets;
    for (UpdateEntry& u : m_updates) targets.push_back(u.target);
    for (CCObject* t : targets)
        for (UpdateEntry& u : m_updates)
            if (u.target == t) {
                if (!u.paused && !u.markedForDeletion) t->update(dt);
                break;
            }

    // the timers, target by target; the lists may change while they run
    // (the indexes are moved back by removals: unscheduleSelector,
    // removeTimerEntry)
    for (m_nCurrentTargetIndex = 0; m_nCurrentTargetIndex < static_cast<int>(m_timers.size());
         ++m_nCurrentTargetIndex) {
        TimerEntry* e = m_timers[m_nCurrentTargetIndex];
        m_pCurrentTarget = e;
        m_bCurrentTargetSalvaged = false;
        if (!e->paused) {
            for (e->timerIndex = 0; e->timerIndex < static_cast<int>(e->timers.size()); ++e->timerIndex) {
                m_pCurrentTimer = e->timers[e->timerIndex];
                m_bCurrentTimerSalvaged = false;
                CCTimer* running = m_pCurrentTimer;
                running->update(dt);
                // it removed itself: retained for its step, let go now
                if (m_bCurrentTimerSalvaged) running->release();
                m_pCurrentTimer = nullptr;
            }
        }
        m_pCurrentTarget = nullptr;
        // only once no timer was scheduled for it meanwhile (issue #481)
        if (m_bCurrentTargetSalvaged && e->timers.empty()) removeTimerEntry(e);
    }
    m_nCurrentTargetIndex = -1;

    // the updates marked for deletion during the pass
    m_updates.erase(std::remove_if(m_updates.begin(), m_updates.end(),
                                   [](const UpdateEntry& u) { return u.markedForDeletion; }),
                    m_updates.end());
    m_bUpdateHashLocked = false;
}

}  // namespace cocos2d
