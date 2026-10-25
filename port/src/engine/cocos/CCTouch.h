// cocos2d-x 2.1.1's touches (touch_dispatcher/, cocoa/CCSet), ported to
// Bleed Engine. MIT: LICENSE-cocos2d-x.txt.
//
// The platform's pointer is one touch (on a touch device, each finger one),
// given by the director in cocos's points: getLocation() is in GL space (y
// up, as nodes are), getLocationInView() as the view saw it (y down).
// Dispatch: the targeted handlers first, by priority (lower first; equal,
// in the order added): a handler claims a touch by returning true from
// ccTouchBegan and then gets its moves and end; a claimed touch of a
// swallowing handler goes no further. Then the standard handlers with the
// touches left. Handlers added or removed during a dispatch take effect
// after it.
#pragma once

#include <vector>

#include "engine/cocos/CCArray.h"
#include "engine/cocos/CCGeometry.h"

namespace cocos2d {

class CCTouch : public CCObject {
public:
    CCPoint getLocation() const;            // GL space
    CCPoint getPreviousLocation() const;    // GL space
    CCPoint getStartLocation() const;       // GL space
    CCPoint getDelta() const { return ccpSub(getLocation(), getPreviousLocation()); }
    CCPoint getLocationInView() const { return m_point; }
    CCPoint getPreviousLocationInView() const { return m_prevPoint; }
    CCPoint getStartLocationInView() const { return m_startPoint; }
    void setTouchInfo(int id, float x, float y) {
        m_nId = id;
        m_prevPoint = m_point;
        m_point.x = x;
        m_point.y = y;
        if (!m_startPointCaptured) {
            m_startPoint = m_point;
            m_startPointCaptured = true;
        }
    }
    int getID() const { return m_nId; }

private:
    int m_nId = 0;
    bool m_startPointCaptured = false;
    CCPoint m_startPoint, m_point, m_prevPoint;  // in view (points, y down)
};

class CCEvent : public CCObject {};

// CCSet: the touches of one event.
class CCSet : public CCObject {
public:
    ~CCSet() override { removeAllObjects(); }
    void addObject(CCObject* o) {
        if (containsObject(o)) return;
        o->retain();
        m_objects.push_back(o);
    }
    void removeObject(CCObject* o) {
        for (size_t i = 0; i < m_objects.size(); ++i)
            if (m_objects[i] == o) {
                m_objects.erase(m_objects.begin() + i);
                o->release();
                return;
            }
    }
    void removeAllObjects() {
        std::vector<CCObject*> old;
        old.swap(m_objects);
        for (CCObject* o : old) o->release();
    }
    bool containsObject(CCObject* o) const {
        for (CCObject* x : m_objects)
            if (x == o) return true;
        return false;
    }
    int count() const { return static_cast<int>(m_objects.size()); }
    CCObject* anyObject() const { return m_objects.empty() ? nullptr : m_objects.front(); }
    CCSet* mutableCopy() const {
        CCSet* s = new CCSet;
        for (CCObject* o : m_objects) s->addObject(o);
        return s;
    }
    std::vector<CCObject*>::const_iterator begin() const { return m_objects.begin(); }
    std::vector<CCObject*>::const_iterator end() const { return m_objects.end(); }

private:
    std::vector<CCObject*> m_objects;
};
typedef std::vector<CCObject*>::const_iterator CCSetIterator;

class CCTouchDelegate {
public:
    virtual ~CCTouchDelegate() = default;
    // targeted
    virtual bool ccTouchBegan(CCTouch*, CCEvent*) { return false; }
    virtual void ccTouchMoved(CCTouch*, CCEvent*) {}
    virtual void ccTouchEnded(CCTouch*, CCEvent*) {}
    virtual void ccTouchCancelled(CCTouch*, CCEvent*) {}
    // standard
    virtual void ccTouchesBegan(CCSet*, CCEvent*) {}
    virtual void ccTouchesMoved(CCSet*, CCEvent*) {}
    virtual void ccTouchesEnded(CCSet*, CCEvent*) {}
    virtual void ccTouchesCancelled(CCSet*, CCEvent*) {}
};

class CCTouchHandler : public CCObject {
public:
    CCTouchDelegate* getDelegate() { return m_pDelegate; }
    int getPriority() { return m_nPriority; }
    void setPriority(int p) { m_nPriority = p; }

protected:
    CCTouchDelegate* m_pDelegate = nullptr;  // retained, as the original, when it is a CCObject
    int m_nPriority = 0;
    friend class CCTouchDispatcher;
};

class CCStandardTouchHandler : public CCTouchHandler {};

class CCTargetedTouchHandler : public CCTouchHandler {
public:
    ~CCTargetedTouchHandler() override { CC_SAFE_RELEASE(m_pClaimedTouches); }
    bool isSwallowsTouches() { return m_bSwallowsTouches; }
    CCSet* getClaimedTouches() { return m_pClaimedTouches; }

private:
    bool m_bSwallowsTouches = false;
    CCSet* m_pClaimedTouches = new CCSet;
    friend class CCTouchDispatcher;
};

enum { CCTOUCHBEGAN, CCTOUCHMOVED, CCTOUCHENDED, CCTOUCHCANCELLED };

class CCTouchDispatcher : public CCObject {
public:
    ~CCTouchDispatcher() override;
    bool isDispatchEvents() { return m_bDispatchEvents; }
    void setDispatchEvents(bool d) { m_bDispatchEvents = d; }
    void addStandardDelegate(CCTouchDelegate* d, int priority);
    void addTargetedDelegate(CCTouchDelegate* d, int priority, bool swallowsTouches);
    void removeDelegate(CCTouchDelegate* d);
    void removeAllDelegates();
    void setPriority(int priority, CCTouchDelegate* d);
    CCTouchHandler* findHandler(CCTouchDelegate* d);

    void touches(CCSet* touches, CCEvent* event, unsigned int index);
    void touchesBegan(CCSet* t, CCEvent* e) { if (m_bDispatchEvents) touches(t, e, CCTOUCHBEGAN); }
    void touchesMoved(CCSet* t, CCEvent* e) { if (m_bDispatchEvents) touches(t, e, CCTOUCHMOVED); }
    void touchesEnded(CCSet* t, CCEvent* e) { if (m_bDispatchEvents) touches(t, e, CCTOUCHENDED); }
    void touchesCancelled(CCSet* t, CCEvent* e) { if (m_bDispatchEvents) touches(t, e, CCTOUCHCANCELLED); }

private:
    void forceAddHandler(CCTouchHandler* h, CCArray* array);
    void forceRemoveDelegate(CCTouchDelegate* d);
    void forceRemoveAllDelegates();
    CCTouchHandler* findHandler(CCArray* array, CCTouchDelegate* d);
    CCArray m_targeted, m_standard, m_toAdd;
    std::vector<CCTouchDelegate*> m_toRemove;
    bool m_bLocked = false, m_bToAdd = false, m_bToRemove = false, m_bToQuit = false;
    bool m_bDispatchEvents = true;
};

}  // namespace cocos2d
