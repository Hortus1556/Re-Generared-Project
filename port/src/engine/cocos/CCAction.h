// cocos2d-x 2.1.1's actions (actions/CCAction, CCActionInterval,
// CCActionInstant, CCActionEase), ported to Bleed Engine: the ones the games
// use. MIT: LICENSE-cocos2d-x.txt.
//
// An action runs on a node through the action manager: startWithTarget,
// then step(dt) each frame until isDone, then stop. An interval action maps
// its elapsed time to 0..1 and calls update(t); an instant one does its work
// in update(1) on its first step. copy() makes a fresh action with the same
// parameters (a repeat's or an ease's inner action is copied too).
#pragma once

#include <cstdarg>

#include "engine/cocos/CCNode.h"

namespace cocos2d {

enum { kCCActionTagInvalid = -1 };

class CCAction : public CCObject {
public:
    ~CCAction() override = default;
    virtual bool isDone() { return true; }
    virtual void startWithTarget(CCNode* target) { m_pOriginalTarget = m_pTarget = target; }
    virtual void stop() { m_pTarget = nullptr; }
    virtual void step(float dt) {}
    virtual void update(float time) override {}
    CCNode* getTarget() { return m_pTarget; }
    void setTarget(CCNode* t) { m_pTarget = t; }
    CCNode* getOriginalTarget() { return m_pOriginalTarget; }
    int getTag() { return m_nTag; }
    void setTag(int tag) { m_nTag = tag; }

protected:
    CCNode* m_pOriginalTarget = nullptr;
    CCNode* m_pTarget = nullptr;
    int m_nTag = kCCActionTagInvalid;
};

class CCFiniteTimeAction : public CCAction {
public:
    float getDuration() { return m_fDuration; }
    void setDuration(float d) { m_fDuration = d; }
    virtual CCFiniteTimeAction* reverse() { return nullptr; }

protected:
    float m_fDuration = 0;
};

// ---------------------------------------------------------------- interval

class CCActionInterval : public CCFiniteTimeAction {
public:
    bool initWithDuration(float d);
    float getElapsed() { return m_elapsed; }
    bool isDone() override { return m_elapsed >= m_fDuration; }
    void step(float dt) override;
    void startWithTarget(CCNode* target) override;
    CCActionInterval* reverse() override { return nullptr; }

protected:
    float m_elapsed = 0;
    bool m_bFirstTick = true;
};

class CCSequence : public CCActionInterval {
public:
    ~CCSequence() override;
    static CCSequence* create(CCFiniteTimeAction* action1, ...);  // ends with NULL
    static CCSequence* createWithVariableList(CCFiniteTimeAction* action1, va_list args);
    static CCSequence* create(CCArray* actions);
    static CCSequence* createWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two);
    bool initWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void stop() override;
    void update(float t) override;
    CCActionInterval* reverse() override;

protected:
    CCFiniteTimeAction* m_pActions[2] = {nullptr, nullptr};
    float m_split = 0;
    int m_last = -1;
};

class CCRepeat : public CCActionInterval {
public:
    ~CCRepeat() override;
    static CCRepeat* create(CCFiniteTimeAction* action, unsigned int times);
    bool initWithAction(CCFiniteTimeAction* action, unsigned int times);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void stop() override;
    void update(float dt) override;
    bool isDone() override { return m_uTotal == m_uTimes; }
    CCActionInterval* reverse() override;
    CCFiniteTimeAction* getInnerAction() { return m_pInnerAction; }

protected:
    unsigned int m_uTimes = 0;
    unsigned int m_uTotal = 0;
    float m_fNextDt = 0;
    bool m_bActionInstant = false;
    CCFiniteTimeAction* m_pInnerAction = nullptr;
};

class CCRepeatForever : public CCActionInterval {
public:
    ~CCRepeatForever() override;
    static CCRepeatForever* create(CCActionInterval* action);
    bool initWithAction(CCActionInterval* action);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void step(float dt) override;
    bool isDone() override { return false; }
    CCActionInterval* reverse() override;
    CCActionInterval* getInnerAction() { return m_pInnerAction; }

protected:
    CCActionInterval* m_pInnerAction = nullptr;
};

class CCSpawn : public CCActionInterval {
public:
    ~CCSpawn() override;
    static CCSpawn* create(CCFiniteTimeAction* action1, ...);  // ends with NULL
    static CCSpawn* createWithVariableList(CCFiniteTimeAction* action1, va_list args);
    static CCSpawn* create(CCArray* actions);
    static CCSpawn* createWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two);
    bool initWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void stop() override;
    void update(float t) override;
    CCActionInterval* reverse() override;

protected:
    CCFiniteTimeAction* m_pOne = nullptr;
    CCFiniteTimeAction* m_pTwo = nullptr;
};

class CCRotateTo : public CCActionInterval {
public:
    static CCRotateTo* create(float duration, float deltaAngle);
    bool initWithDuration(float duration, float deltaAngle);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void update(float time) override;

protected:
    float m_fDstAngleX = 0, m_fStartAngleX = 0, m_fDiffAngleX = 0;
    float m_fDstAngleY = 0, m_fStartAngleY = 0, m_fDiffAngleY = 0;
};

class CCRotateBy : public CCActionInterval {
public:
    static CCRotateBy* create(float duration, float deltaAngle);
    bool initWithDuration(float duration, float deltaAngle);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void update(float time) override;
    CCActionInterval* reverse() override;

protected:
    float m_fAngleX = 0, m_fStartAngleX = 0;
    float m_fAngleY = 0, m_fStartAngleY = 0;
};

class CCMoveBy : public CCActionInterval {
public:
    static CCMoveBy* create(float duration, const CCPoint& deltaPosition);
    bool initWithDuration(float duration, const CCPoint& deltaPosition);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void update(float time) override;
    CCActionInterval* reverse() override;

protected:
    CCPoint m_positionDelta;
    CCPoint m_startPosition;
    CCPoint m_previousPosition;
};

class CCMoveTo : public CCMoveBy {
public:
    static CCMoveTo* create(float duration, const CCPoint& position);
    bool initWithDuration(float duration, const CCPoint& position);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;

protected:
    CCPoint m_endPosition;
};

class CCScaleTo : public CCActionInterval {
public:
    static CCScaleTo* create(float duration, float s);
    static CCScaleTo* create(float duration, float sx, float sy);
    bool initWithDuration(float duration, float s) { return initWithDuration(duration, s, s); }
    bool initWithDuration(float duration, float sx, float sy);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void update(float time) override;

protected:
    float m_fScaleX = 0, m_fScaleY = 0;
    float m_fStartScaleX = 0, m_fStartScaleY = 0;
    float m_fEndScaleX = 0, m_fEndScaleY = 0;
    float m_fDeltaX = 0, m_fDeltaY = 0;
};

class CCFadeIn : public CCActionInterval {
public:
    static CCFadeIn* create(float d);
    CCObject* copyWithZone(void*) override;
    void update(float time) override;
    CCActionInterval* reverse() override;
};

class CCFadeOut : public CCActionInterval {
public:
    static CCFadeOut* create(float d);
    CCObject* copyWithZone(void*) override;
    void update(float time) override;
    CCActionInterval* reverse() override;
};

class CCFadeTo : public CCActionInterval {
public:
    static CCFadeTo* create(float duration, GLubyte opacity);
    bool initWithDuration(float duration, GLubyte opacity);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void update(float time) override;

protected:
    GLubyte m_toOpacity = 0;
    GLubyte m_fromOpacity = 0;
};

class CCDelayTime : public CCActionInterval {
public:
    static CCDelayTime* create(float d);
    CCObject* copyWithZone(void*) override;
    void update(float time) override {}
    CCActionInterval* reverse() override { return CCDelayTime::create(m_fDuration); }
};

class CCReverseTime : public CCActionInterval {
public:
    ~CCReverseTime() override;
    static CCReverseTime* create(CCFiniteTimeAction* action);
    bool initWithAction(CCFiniteTimeAction* action);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void stop() override;
    void update(float time) override;
    CCActionInterval* reverse() override;

protected:
    CCFiniteTimeAction* m_pOther = nullptr;
};

// ExtraAction (2.1.1, CCActionInterval.cpp): what pads a one-action CCSequence.
class ExtraAction : public CCFiniteTimeAction {
public:
    static ExtraAction* create();
    CCObject* copyWithZone(void*) override { return create(); }
    ExtraAction* reverse() override { return create(); }
    void update(float) override {}
    void step(float) override {}
};

// ----------------------------------------------------------------- instant

class CCActionInstant : public CCFiniteTimeAction {
public:
    bool isDone() override { return true; }
    void step(float dt) override { update(1); }
    void update(float time) override {}
    CCFiniteTimeAction* reverse() override { return static_cast<CCFiniteTimeAction*>(copy()->autorelease()); }
};

class CCCallFunc : public CCActionInstant {
public:
    CCCallFunc() : m_pCallFunc(nullptr) {}
    ~CCCallFunc() override;
    static CCCallFunc* create(CCObject* selectorTarget, SEL_CallFunc selector);
    virtual bool initWithTarget(CCObject* selectorTarget);
    CCObject* copyWithZone(void*) override;
    virtual void execute() {
        if (m_pCallFunc) (m_pSelectorTarget->*m_pCallFunc)();
    }
    void update(float time) override { execute(); }
    CCObject* getTargetCallback() { return m_pSelectorTarget; }

protected:
    CCObject* m_pSelectorTarget = nullptr;  // retained
    union {
        SEL_CallFunc m_pCallFunc;
        SEL_CallFuncN m_pCallFuncN;
    };
};

class CCCallFuncN : public CCCallFunc {
public:
    static CCCallFuncN* create(CCObject* selectorTarget, SEL_CallFuncN selector);
    using CCCallFunc::initWithTarget;
    bool initWithTarget(CCObject* selectorTarget, SEL_CallFuncN selector);
    CCObject* copyWithZone(void*) override;
    void execute() override {
        if (m_pCallFuncN) (m_pSelectorTarget->*m_pCallFuncN)(m_pTarget);
    }
};

// -------------------------------------------------------------------- ease

class CCActionEase : public CCActionInterval {
public:
    ~CCActionEase() override;
    static CCActionEase* create(CCActionInterval* action);
    bool initWithAction(CCActionInterval* action);
    CCObject* copyWithZone(void*) override;
    void startWithTarget(CCNode* target) override;
    void stop() override;
    void update(float time) override { m_pInner->update(time); }
    CCActionInterval* reverse() override;
    CCActionInterval* getInnerAction() { return m_pInner; }

protected:
    CCActionInterval* m_pInner = nullptr;
};

class CCEaseRateAction : public CCActionEase {
public:
    bool initWithAction(CCActionInterval* action, float rate);
    float getRate() { return m_fRate; }
    void setRate(float r) { m_fRate = r; }

protected:
    float m_fRate = 0;
};

class CCEaseIn : public CCEaseRateAction {
public:
    static CCEaseIn* create(CCActionInterval* action, float rate);
    CCObject* copyWithZone(void*) override;
    void update(float time) override { m_pInner->update(powf(time, m_fRate)); }
    CCActionInterval* reverse() override;
};

class CCEaseOut : public CCEaseRateAction {
public:
    static CCEaseOut* create(CCActionInterval* action, float rate);
    CCObject* copyWithZone(void*) override;
    void update(float time) override { m_pInner->update(powf(time, 1 / m_fRate)); }
    CCActionInterval* reverse() override;
};

class CCEaseInOut : public CCEaseRateAction {
public:
    static CCEaseInOut* create(CCActionInterval* action, float rate);
    CCObject* copyWithZone(void*) override;
    void update(float time) override;
    CCActionInterval* reverse() override;
};

class CCEaseElastic : public CCActionEase {
public:
    bool initWithAction(CCActionInterval* action, float period = 0.3f);
    float getPeriod() { return m_fPeriod; }
    void setPeriod(float p) { m_fPeriod = p; }

protected:
    float m_fPeriod = 0.3f;
};

class CCEaseElasticIn : public CCEaseElastic {
public:
    static CCEaseElasticIn* create(CCActionInterval* action, float period = 0.3f);
    CCObject* copyWithZone(void*) override;
    void update(float time) override;
    CCActionInterval* reverse() override;
};

class CCEaseElasticOut : public CCEaseElastic {
public:
    static CCEaseElasticOut* create(CCActionInterval* action, float period = 0.3f);
    CCObject* copyWithZone(void*) override;
    void update(float time) override;
    CCActionInterval* reverse() override;
};

class CCEaseBounce : public CCActionEase {
public:
    float bounceTime(float time);
};

class CCEaseBounceInOut : public CCEaseBounce {
public:
    static CCEaseBounceInOut* create(CCActionInterval* action);
    CCObject* copyWithZone(void*) override;
    void update(float time) override;
    CCActionInterval* reverse() override;
};

}  // namespace cocos2d
