// cocos2d-x 2.1.1's CCTouch.cpp, CCTouchHandler.cpp and CCTouchDispatcher.cpp,
// ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCTouch.h"

#include "engine/cocos/CCDirector.h"

namespace cocos2d {

CCPoint CCTouch::getLocation() const { return CCDirector::sharedDirector()->convertToGL(m_point); }
CCPoint CCTouch::getPreviousLocation() const { return CCDirector::sharedDirector()->convertToGL(m_prevPoint); }
CCPoint CCTouch::getStartLocation() const { return CCDirector::sharedDirector()->convertToGL(m_startPoint); }

// A delegate that is an object (a layer) is retained while it has a handler.
static void retainDelegate(CCTouchDelegate* d) {
    if (CCObject* o = dynamic_cast<CCObject*>(d)) o->retain();
}
static void releaseDelegate(CCTouchDelegate* d) {
    if (CCObject* o = dynamic_cast<CCObject*>(d)) o->release();
}

CCTouchDispatcher::~CCTouchDispatcher() { forceRemoveAllDelegates(); }

void CCTouchDispatcher::forceAddHandler(CCTouchHandler* h, CCArray* array) {
    unsigned int u = 0;
    for (CCObject* o : array->data) {
        CCTouchHandler* other = static_cast<CCTouchHandler*>(o);
        if (other->getPriority() < h->getPriority()) ++u;  // after the lower priorities, before equal ones (as 2.1.1)
        if (other->getDelegate() == h->getDelegate()) {  // 2.1.1 asserts
            releaseDelegate(h->getDelegate());
            return;
        }
    }
    array->insertObject(h, u);
}

void CCTouchDispatcher::addStandardDelegate(CCTouchDelegate* d, int priority) {
    CCStandardTouchHandler* h = new CCStandardTouchHandler;
    h->m_pDelegate = d;
    retainDelegate(d);
    h->m_nPriority = priority;
    if (!m_bLocked) {
        forceAddHandler(h, &m_standard);
    } else {
        for (size_t i = 0; i < m_toRemove.size(); ++i)
            if (m_toRemove[i] == d) {  // issue #752
                m_toRemove.erase(m_toRemove.begin() + i);
                releaseDelegate(d);
                h->release();
                return;
            }
        m_toAdd.addObject(h);
        m_bToAdd = true;
    }
    h->release();
}

void CCTouchDispatcher::addTargetedDelegate(CCTouchDelegate* d, int priority, bool swallowsTouches) {
    CCTargetedTouchHandler* h = new CCTargetedTouchHandler;
    h->m_pDelegate = d;
    retainDelegate(d);
    h->m_nPriority = priority;
    h->m_bSwallowsTouches = swallowsTouches;
    if (!m_bLocked) {
        forceAddHandler(h, &m_targeted);
    } else {
        for (size_t i = 0; i < m_toRemove.size(); ++i)
            if (m_toRemove[i] == d) {  // issue #752
                m_toRemove.erase(m_toRemove.begin() + i);
                releaseDelegate(d);
                h->release();
                return;
            }
        m_toAdd.addObject(h);
        m_bToAdd = true;
    }
    h->release();
}

void CCTouchDispatcher::forceRemoveDelegate(CCTouchDelegate* d) {
    for (CCArray* a : {&m_standard, &m_targeted})
        for (size_t i = 0; i < a->data.size(); ++i) {
            CCTouchHandler* h = static_cast<CCTouchHandler*>(a->data[i]);
            if (h->getDelegate() == d) {
                releaseDelegate(d);
                a->removeObjectAtIndex(static_cast<unsigned int>(i));
                break;
            }
        }
}

void CCTouchDispatcher::removeDelegate(CCTouchDelegate* d) {
    if (!d) return;
    if (!m_bLocked) {
        forceRemoveDelegate(d);
        return;
    }
    if (CCTouchHandler* h = findHandler(&m_toAdd, d)) {  // issue #752
        releaseDelegate(d);
        m_toAdd.removeObject(h);
        return;
    }
    m_toRemove.push_back(d);
    m_bToRemove = true;
}

void CCTouchDispatcher::forceRemoveAllDelegates() {
    for (CCArray* a : {&m_standard, &m_targeted}) {
        for (CCObject* o : a->data) releaseDelegate(static_cast<CCTouchHandler*>(o)->getDelegate());
        a->removeAllObjects();
    }
}

void CCTouchDispatcher::removeAllDelegates() {
    if (!m_bLocked) forceRemoveAllDelegates();
    else m_bToQuit = true;
}

CCTouchHandler* CCTouchDispatcher::findHandler(CCArray* array, CCTouchDelegate* d) {
    for (CCObject* o : array->data)
        if (static_cast<CCTouchHandler*>(o)->getDelegate() == d) return static_cast<CCTouchHandler*>(o);
    return nullptr;
}

CCTouchHandler* CCTouchDispatcher::findHandler(CCTouchDelegate* d) {
    if (CCTouchHandler* h = findHandler(&m_targeted, d)) return h;
    return findHandler(&m_standard, d);
}

void CCTouchDispatcher::setPriority(int priority, CCTouchDelegate* d) {
    CCTouchHandler* h = findHandler(d);
    if (!h || h->getPriority() == priority) return;
    h->setPriority(priority);
    // rearranged (2.1.1 sorts with a stable comparison)
    for (CCArray* a : {&m_targeted, &m_standard})
        std::stable_sort(a->data.begin(), a->data.end(), [](CCObject* x, CCObject* y) {
            return static_cast<CCTouchHandler*>(x)->getPriority() < static_cast<CCTouchHandler*>(y)->getPriority();
        });
}

void CCTouchDispatcher::touches(CCSet* pTouches, CCEvent* event, unsigned int index) {
    m_bLocked = true;
    unsigned int targetedCount = m_targeted.count(), standardCount = m_standard.count();
    bool needsMutableSet = targetedCount && standardCount;
    CCSet* mutableTouches = needsMutableSet ? pTouches->mutableCopy() : pTouches;

    // the targeted handlers first
    if (targetedCount > 0) {
        std::vector<CCObject*> touchList(pTouches->begin(), pTouches->end());
        for (CCObject* to : touchList) {
            CCTouch* touch = static_cast<CCTouch*>(to);
            std::vector<CCObject*> handlers = m_targeted.data;
            for (CCObject* ho : handlers) {
                CCTargetedTouchHandler* h = static_cast<CCTargetedTouchHandler*>(ho);
                bool claimed = false;
                if (index == CCTOUCHBEGAN) {
                    claimed = h->getDelegate()->ccTouchBegan(touch, event);
                    if (claimed) h->getClaimedTouches()->addObject(touch);
                } else if (h->getClaimedTouches()->containsObject(touch)) {
                    claimed = true;
                    switch (index) {
                        case CCTOUCHMOVED: h->getDelegate()->ccTouchMoved(touch, event); break;
                        case CCTOUCHENDED:
                            h->getDelegate()->ccTouchEnded(touch, event);
                            h->getClaimedTouches()->removeObject(touch);
                            break;
                        case CCTOUCHCANCELLED:
                            h->getDelegate()->ccTouchCancelled(touch, event);
                            h->getClaimedTouches()->removeObject(touch);
                            break;
                    }
                }
                if (claimed && h->isSwallowsTouches()) {
                    if (needsMutableSet) mutableTouches->removeObject(touch);
                    break;
                }
            }
        }
    }

    // the standard handlers second
    if (standardCount > 0 && mutableTouches->count() > 0) {
        std::vector<CCObject*> handlers = m_standard.data;
        for (CCObject* ho : handlers) {
            CCStandardTouchHandler* h = static_cast<CCStandardTouchHandler*>(ho);
            switch (index) {
                case CCTOUCHBEGAN: h->getDelegate()->ccTouchesBegan(mutableTouches, event); break;
                case CCTOUCHMOVED: h->getDelegate()->ccTouchesMoved(mutableTouches, event); break;
                case CCTOUCHENDED: h->getDelegate()->ccTouchesEnded(mutableTouches, event); break;
                case CCTOUCHCANCELLED: h->getDelegate()->ccTouchesCancelled(mutableTouches, event); break;
            }
        }
    }
    if (needsMutableSet) mutableTouches->release();

    // the adds and removes made meanwhile
    m_bLocked = false;
    if (m_bToRemove) {
        m_bToRemove = false;
        std::vector<CCTouchDelegate*> removes;
        removes.swap(m_toRemove);
        for (CCTouchDelegate* d : removes) forceRemoveDelegate(d);
    }
    if (m_bToAdd) {
        m_bToAdd = false;
        std::vector<CCObject*> adds = m_toAdd.data;
        for (CCObject* o : adds) {
            CCTouchHandler* h = static_cast<CCTouchHandler*>(o);
            forceAddHandler(h, dynamic_cast<CCTargetedTouchHandler*>(h) ? &m_targeted : &m_standard);
        }
        m_toAdd.removeAllObjects();
    }
    if (m_bToQuit) {
        m_bToQuit = false;
        forceRemoveAllDelegates();
    }
}

}  // namespace cocos2d
