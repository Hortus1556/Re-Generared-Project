// cocos2d-x 2.1.1's CCObject and autorelease pool (cocoa/CCObject,
// cocoa/CCAutoreleasePool), ported to Bleed Engine: reference counted by the
// engine's own Object. MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include <vector>

#include "core/Object.h"

namespace cocos2d {

class CCNode;
class CCEvent;

class CCObject : public Object {
public:
    CCObject() = default;
    ~CCObject() override = default;
    // Released when the director's frame ends (the pool is drained once a pass).
    CCObject* autorelease();
    bool isSingleReference() const { return retainCount() == 1; }
    virtual bool isEqual(const CCObject* o) { return this == o; }
    virtual void update(float dt) {}
    // CCCopying::copyWithZone: what an action's copy() makes.
    virtual CCObject* copyWithZone(void*) { return nullptr; }
    CCObject* copy() { return copyWithZone(nullptr); }
};

typedef void (CCObject::*SEL_SCHEDULE)(float);
typedef void (CCObject::*SEL_CallFunc)();
typedef void (CCObject::*SEL_CallFuncN)(CCNode*);
typedef void (CCObject::*SEL_CallFuncND)(CCNode*, void*);
typedef void (CCObject::*SEL_CallFuncO)(CCObject*);
typedef void (CCObject::*SEL_MenuHandler)(CCObject*);
typedef void (CCObject::*SEL_EventHandler)(CCEvent*);
typedef int (CCObject::*SEL_Compare)(CCObject*);
#define schedule_selector(_SELECTOR) (cocos2d::SEL_SCHEDULE)(&_SELECTOR)
#define callfunc_selector(_SELECTOR) (cocos2d::SEL_CallFunc)(&_SELECTOR)
#define callfuncN_selector(_SELECTOR) (cocos2d::SEL_CallFuncN)(&_SELECTOR)
#define callfuncND_selector(_SELECTOR) (cocos2d::SEL_CallFuncND)(&_SELECTOR)
#define callfuncO_selector(_SELECTOR) (cocos2d::SEL_CallFuncO)(&_SELECTOR)
#define menu_selector(_SELECTOR) (cocos2d::SEL_MenuHandler)(&_SELECTOR)

// CCPoolManager: the objects autoreleased since the pool was last drained.
class CCPoolManager {
public:
    static CCPoolManager* sharedPoolManager();
    void addObject(CCObject* o) { objects_.push_back(o); }
    // CCPoolManager::pop as the director's mainLoop calls it: releases them all.
    void pop();

private:
    std::vector<CCObject*> objects_;
};

#define CC_SAFE_RETAIN(p) do { if (p) (p)->retain(); } while (0)
#define CC_SAFE_RELEASE(p) do { if (p) (p)->release(); } while (0)
#define CC_SAFE_RELEASE_NULL(p) do { if (p) { (p)->release(); (p) = nullptr; } } while (0)
#define CC_SAFE_DELETE(p) do { delete (p); (p) = nullptr; } while (0)

}  // namespace cocos2d
