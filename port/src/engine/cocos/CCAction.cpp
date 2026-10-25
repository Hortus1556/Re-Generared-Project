// cocos2d-x 2.1.1's CCActionInterval.cpp, CCActionInstant.cpp and
// CCActionEase.cpp (the actions in use), ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCAction.h"

#include <algorithm>
#include <cfloat>

namespace cocos2d {

template <class T>
static T* autoreleased(T* a) {
    a->autorelease();
    return a;
}

// ---------------------------------------------------------------- interval

bool CCActionInterval::initWithDuration(float d) {
    m_fDuration = d;
    if (m_fDuration == 0) m_fDuration = FLT_EPSILON;  // no division by 0
    m_elapsed = 0;
    m_bFirstTick = true;
    return true;
}

void CCActionInterval::step(float dt) {
    if (m_bFirstTick) {
        m_bFirstTick = false;
        m_elapsed = 0;
    } else {
        m_elapsed += dt;
    }
    update(std::max(0.0f, std::min(1.0f, m_elapsed / std::max(m_fDuration, FLT_EPSILON))));
}

void CCActionInterval::startWithTarget(CCNode* target) {
    CCFiniteTimeAction::startWithTarget(target);
    m_elapsed = 0.0f;
    m_bFirstTick = true;
}

// ---- CCSequence

CCSequence::~CCSequence() {
    CC_SAFE_RELEASE(m_pActions[0]);
    CC_SAFE_RELEASE(m_pActions[1]);
}

CCSequence* CCSequence::createWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two) {
    CCSequence* s = new CCSequence;
    s->initWithTwoActions(one, two);
    return autoreleased(s);
}

CCSequence* CCSequence::create(CCFiniteTimeAction* action1, ...) {
    va_list params;
    va_start(params, action1);
    CCSequence* s = createWithVariableList(action1, params);
    va_end(params);
    return s;
}

CCSequence* CCSequence::createWithVariableList(CCFiniteTimeAction* action1, va_list args) {
    CCFiniteTimeAction* now = nullptr;
    CCFiniteTimeAction* prev = action1;
    bool oneAction = true;
    while (action1) {
        now = va_arg(args, CCFiniteTimeAction*);
        if (now) {
            prev = createWithTwoActions(prev, now);
            oneAction = false;
        } else {
            // one action alone: made a sequence with the simplest finite action
            if (oneAction) prev = createWithTwoActions(prev, ExtraAction::create());
            break;
        }
    }
    return static_cast<CCSequence*>(prev);
}

CCSequence* CCSequence::create(CCArray* actions) {
    unsigned int count = actions->count();
    if (count == 0) return nullptr;
    CCFiniteTimeAction* prev = static_cast<CCFiniteTimeAction*>(actions->objectAtIndex(0));
    if (count > 1) {
        for (unsigned int i = 1; i < count; ++i)
            prev = createWithTwoActions(prev, static_cast<CCFiniteTimeAction*>(actions->objectAtIndex(i)));
    } else {
        prev = createWithTwoActions(prev, ExtraAction::create());
    }
    return static_cast<CCSequence*>(prev);
}

bool CCSequence::initWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two) {
    CCActionInterval::initWithDuration(one->getDuration() + two->getDuration());
    m_pActions[0] = one;
    one->retain();
    m_pActions[1] = two;
    two->retain();
    return true;
}

CCObject* CCSequence::copyWithZone(void*) {
    CCSequence* c = new CCSequence;
    c->initWithTwoActions(static_cast<CCFiniteTimeAction*>(m_pActions[0]->copy()->autorelease()),
                          static_cast<CCFiniteTimeAction*>(m_pActions[1]->copy()->autorelease()));
    return c;
}

void CCSequence::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_split = m_pActions[0]->getDuration() / m_fDuration;
    m_last = -1;
}

void CCSequence::stop() {
    if (m_last != -1) m_pActions[m_last]->stop();  // issue #1305
    CCActionInterval::stop();
}

void CCSequence::update(float t) {
    int found = 0;
    float new_t = 0.0f;
    if (t < m_split) {
        found = 0;
        new_t = m_split != 0 ? t / m_split : 1;
    } else {
        found = 1;
        new_t = m_split == 1 ? 1 : (t - m_split) / (1 - m_split);
    }
    if (found == 1) {
        if (m_last == -1) {  // action 0 was skipped: run it whole
            m_pActions[0]->startWithTarget(m_pTarget);
            m_pActions[0]->update(1.0f);
            m_pActions[0]->stop();
        } else if (m_last == 0) {  // switching to action 1: action 0 ends
            m_pActions[0]->update(1.0f);
            m_pActions[0]->stop();
        }
    }
    if (found == m_last && m_pActions[found]->isDone()) return;  // the last one found, done
    if (found != m_last) m_pActions[found]->startWithTarget(m_pTarget);
    m_pActions[found]->update(new_t);
    m_last = found;
}

CCActionInterval* CCSequence::reverse() {
    return createWithTwoActions(m_pActions[1]->reverse(), m_pActions[0]->reverse());
}

// ---- CCRepeat

CCRepeat::~CCRepeat() { CC_SAFE_RELEASE(m_pInnerAction); }

CCRepeat* CCRepeat::create(CCFiniteTimeAction* action, unsigned int times) {
    CCRepeat* r = new CCRepeat;
    r->initWithAction(action, times);
    return autoreleased(r);
}

bool CCRepeat::initWithAction(CCFiniteTimeAction* action, unsigned int times) {
    CCActionInterval::initWithDuration(action->getDuration() * times);
    m_uTimes = times;
    m_pInnerAction = action;
    action->retain();
    // an instant action runs one time less in update: startWithTarget runs it
    m_bActionInstant = dynamic_cast<CCActionInstant*>(action) != nullptr;
    if (m_bActionInstant) m_uTimes -= 1;
    m_uTotal = 0;
    return true;
}

CCObject* CCRepeat::copyWithZone(void*) {
    CCRepeat* c = new CCRepeat;
    c->initWithAction(static_cast<CCFiniteTimeAction*>(m_pInnerAction->copy()->autorelease()), m_uTimes);
    return c;
}

void CCRepeat::startWithTarget(CCNode* target) {
    m_uTotal = 0;
    m_fNextDt = m_pInnerAction->getDuration() / m_fDuration;
    CCActionInterval::startWithTarget(target);
    m_pInnerAction->startWithTarget(target);
}

void CCRepeat::stop() {
    m_pInnerAction->stop();
    CCActionInterval::stop();
}

void CCRepeat::update(float dt) {
    if (dt >= m_fNextDt) {
        while (dt > m_fNextDt && m_uTotal < m_uTimes) {
            m_pInnerAction->update(1.0f);
            m_uTotal++;
            m_pInnerAction->stop();
            m_pInnerAction->startWithTarget(m_pTarget);
            m_fNextDt += m_pInnerAction->getDuration() / m_fDuration;
        }
        if (dt >= 1.0f && m_uTotal < m_uTimes) m_uTotal++;  // issue #1288: the right end value
        // an instant action is not set back or updated: it has no duration
        if (!m_bActionInstant) {
            if (m_uTotal == m_uTimes) {
                m_pInnerAction->update(1);
                m_pInnerAction->stop();
            } else {
                // issue #390: no jerk
                m_pInnerAction->update(dt - (m_fNextDt - m_pInnerAction->getDuration() / m_fDuration));
            }
        }
    } else {
        m_pInnerAction->update(fmodf(dt * m_uTimes, 1.0f));
    }
}

CCActionInterval* CCRepeat::reverse() { return create(m_pInnerAction->reverse(), m_uTimes); }

// ---- CCRepeatForever

CCRepeatForever::~CCRepeatForever() { CC_SAFE_RELEASE(m_pInnerAction); }

CCRepeatForever* CCRepeatForever::create(CCActionInterval* action) {
    CCRepeatForever* r = new CCRepeatForever;
    r->initWithAction(action);
    return autoreleased(r);
}

bool CCRepeatForever::initWithAction(CCActionInterval* action) {
    action->retain();
    m_pInnerAction = action;
    return true;
}

CCObject* CCRepeatForever::copyWithZone(void*) {
    CCRepeatForever* c = new CCRepeatForever;
    c->initWithAction(static_cast<CCActionInterval*>(m_pInnerAction->copy()->autorelease()));
    return c;
}

void CCRepeatForever::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_pInnerAction->startWithTarget(target);
}

void CCRepeatForever::step(float dt) {
    m_pInnerAction->step(dt);
    if (m_pInnerAction->isDone()) {
        float diff = m_pInnerAction->getElapsed() - m_pInnerAction->getDuration();
        m_pInnerAction->startWithTarget(m_pTarget);
        m_pInnerAction->step(0.0f);  // issues #390, 1247: no jerk
        m_pInnerAction->step(diff);
    }
}

CCActionInterval* CCRepeatForever::reverse() { return create(m_pInnerAction->reverse()); }

// ---- CCSpawn

CCSpawn::~CCSpawn() {
    CC_SAFE_RELEASE(m_pOne);
    CC_SAFE_RELEASE(m_pTwo);
}

CCSpawn* CCSpawn::createWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two) {
    CCSpawn* s = new CCSpawn;
    s->initWithTwoActions(one, two);
    return autoreleased(s);
}

CCSpawn* CCSpawn::create(CCFiniteTimeAction* action1, ...) {
    va_list params;
    va_start(params, action1);
    CCSpawn* s = createWithVariableList(action1, params);
    va_end(params);
    return s;
}

CCSpawn* CCSpawn::createWithVariableList(CCFiniteTimeAction* action1, va_list args) {
    CCFiniteTimeAction* now = nullptr;
    CCFiniteTimeAction* prev = action1;
    bool oneAction = true;
    while (action1) {
        now = va_arg(args, CCFiniteTimeAction*);
        if (now) {
            prev = createWithTwoActions(prev, now);
            oneAction = false;
        } else {
            if (oneAction) prev = createWithTwoActions(prev, ExtraAction::create());
            break;
        }
    }
    return static_cast<CCSpawn*>(prev);
}

CCSpawn* CCSpawn::create(CCArray* actions) {
    unsigned int count = actions->count();
    if (count == 0) return nullptr;
    CCFiniteTimeAction* prev = static_cast<CCFiniteTimeAction*>(actions->objectAtIndex(0));
    if (count > 1) {
        for (unsigned int i = 1; i < count; ++i)
            prev = createWithTwoActions(prev, static_cast<CCFiniteTimeAction*>(actions->objectAtIndex(i)));
    } else {
        prev = createWithTwoActions(prev, ExtraAction::create());
    }
    return static_cast<CCSpawn*>(prev);
}

bool CCSpawn::initWithTwoActions(CCFiniteTimeAction* one, CCFiniteTimeAction* two) {
    float d1 = one->getDuration(), d2 = two->getDuration();
    CCActionInterval::initWithDuration(std::max(d1, d2));
    m_pOne = one;
    m_pTwo = two;
    // the shorter is padded with a delay
    if (d1 > d2) m_pTwo = CCSequence::createWithTwoActions(two, CCDelayTime::create(d1 - d2));
    else if (d1 < d2) m_pOne = CCSequence::createWithTwoActions(one, CCDelayTime::create(d2 - d1));
    m_pOne->retain();
    m_pTwo->retain();
    return true;
}

CCObject* CCSpawn::copyWithZone(void*) {
    CCSpawn* c = new CCSpawn;
    c->initWithTwoActions(static_cast<CCFiniteTimeAction*>(m_pOne->copy()->autorelease()),
                          static_cast<CCFiniteTimeAction*>(m_pTwo->copy()->autorelease()));
    return c;
}

void CCSpawn::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_pOne->startWithTarget(target);
    m_pTwo->startWithTarget(target);
}

void CCSpawn::stop() {
    m_pOne->stop();
    m_pTwo->stop();
    CCActionInterval::stop();
}

void CCSpawn::update(float time) {
    if (m_pOne) m_pOne->update(time);
    if (m_pTwo) m_pTwo->update(time);
}

CCActionInterval* CCSpawn::reverse() { return createWithTwoActions(m_pOne->reverse(), m_pTwo->reverse()); }

// ---- rotate

CCRotateTo* CCRotateTo::create(float duration, float deltaAngle) {
    CCRotateTo* r = new CCRotateTo;
    r->initWithDuration(duration, deltaAngle);
    return autoreleased(r);
}

bool CCRotateTo::initWithDuration(float duration, float deltaAngle) {
    CCActionInterval::initWithDuration(duration);
    m_fDstAngleX = m_fDstAngleY = deltaAngle;
    return true;
}

CCObject* CCRotateTo::copyWithZone(void*) {
    CCRotateTo* c = new CCRotateTo;
    c->CCActionInterval::initWithDuration(m_fDuration);
    c->m_fDstAngleX = m_fDstAngleX;
    c->m_fDstAngleY = m_fDstAngleY;
    return c;
}

static void shortestTurn(float start, float dst, float& outStart, float& outDiff) {
    outStart = start > 0 ? fmodf(start, 360.0f) : fmodf(start, -360.0f);
    outDiff = dst - outStart;
    if (outDiff > 180) outDiff -= 360;
    if (outDiff < -180) outDiff += 360;
}

void CCRotateTo::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    shortestTurn(target->getRotationX(), m_fDstAngleX, m_fStartAngleX, m_fDiffAngleX);
    shortestTurn(target->getRotationY(), m_fDstAngleY, m_fStartAngleY, m_fDiffAngleY);
}

void CCRotateTo::update(float time) {
    if (m_pTarget) {
        m_pTarget->setRotationX(m_fStartAngleX + m_fDiffAngleX * time);
        m_pTarget->setRotationY(m_fStartAngleY + m_fDiffAngleY * time);
    }
}

CCRotateBy* CCRotateBy::create(float duration, float deltaAngle) {
    CCRotateBy* r = new CCRotateBy;
    r->initWithDuration(duration, deltaAngle);
    return autoreleased(r);
}

bool CCRotateBy::initWithDuration(float duration, float deltaAngle) {
    CCActionInterval::initWithDuration(duration);
    m_fAngleX = m_fAngleY = deltaAngle;
    return true;
}

CCObject* CCRotateBy::copyWithZone(void*) {
    CCRotateBy* c = new CCRotateBy;
    c->CCActionInterval::initWithDuration(m_fDuration);
    c->m_fAngleX = m_fAngleX;
    c->m_fAngleY = m_fAngleY;
    return c;
}

void CCRotateBy::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_fStartAngleX = target->getRotationX();
    m_fStartAngleY = target->getRotationY();
}

void CCRotateBy::update(float time) {
    if (m_pTarget) {
        m_pTarget->setRotationX(m_fStartAngleX + m_fAngleX * time);
        m_pTarget->setRotationY(m_fStartAngleY + m_fAngleY * time);
    }
}

CCActionInterval* CCRotateBy::reverse() {
    CCRotateBy* r = create(m_fDuration, -m_fAngleX);
    r->m_fAngleY = -m_fAngleY;
    return r;
}

// ---- move

CCMoveBy* CCMoveBy::create(float duration, const CCPoint& deltaPosition) {
    CCMoveBy* m = new CCMoveBy;
    m->initWithDuration(duration, deltaPosition);
    return autoreleased(m);
}

bool CCMoveBy::initWithDuration(float duration, const CCPoint& deltaPosition) {
    CCActionInterval::initWithDuration(duration);
    m_positionDelta = deltaPosition;
    return true;
}

CCObject* CCMoveBy::copyWithZone(void*) {
    CCMoveBy* c = new CCMoveBy;
    c->initWithDuration(m_fDuration, m_positionDelta);
    return c;
}

void CCMoveBy::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_previousPosition = m_startPosition = target->getPosition();
}

void CCMoveBy::update(float t) {
    if (m_pTarget) {
        // moves the node by others moved it meanwhile too (2.1.1's stacking)
        CCPoint currentPos = m_pTarget->getPosition();
        CCPoint diff = ccpSub(currentPos, m_previousPosition);
        m_startPosition = ccpAdd(m_startPosition, diff);
        CCPoint newPos = ccpAdd(m_startPosition, ccpMult(m_positionDelta, t));
        m_pTarget->setPosition(newPos);
        m_previousPosition = newPos;
    }
}

CCActionInterval* CCMoveBy::reverse() { return create(m_fDuration, ccp(-m_positionDelta.x, -m_positionDelta.y)); }

CCMoveTo* CCMoveTo::create(float duration, const CCPoint& position) {
    CCMoveTo* m = new CCMoveTo;
    m->initWithDuration(duration, position);
    return autoreleased(m);
}

bool CCMoveTo::initWithDuration(float duration, const CCPoint& position) {
    CCActionInterval::initWithDuration(duration);
    m_endPosition = position;
    return true;
}

CCObject* CCMoveTo::copyWithZone(void*) {
    CCMoveTo* c = new CCMoveTo;
    c->initWithDuration(m_fDuration, m_endPosition);
    return c;
}

void CCMoveTo::startWithTarget(CCNode* target) {
    CCMoveBy::startWithTarget(target);
    m_positionDelta = ccpSub(m_endPosition, target->getPosition());
}

// ---- scale

CCScaleTo* CCScaleTo::create(float duration, float s) { return create(duration, s, s); }

CCScaleTo* CCScaleTo::create(float duration, float sx, float sy) {
    CCScaleTo* a = new CCScaleTo;
    a->initWithDuration(duration, sx, sy);
    return autoreleased(a);
}

bool CCScaleTo::initWithDuration(float duration, float sx, float sy) {
    CCActionInterval::initWithDuration(duration);
    m_fEndScaleX = sx;
    m_fEndScaleY = sy;
    return true;
}

CCObject* CCScaleTo::copyWithZone(void*) {
    CCScaleTo* c = new CCScaleTo;
    c->initWithDuration(m_fDuration, m_fEndScaleX, m_fEndScaleY);
    return c;
}

void CCScaleTo::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_fStartScaleX = target->getScaleX();
    m_fStartScaleY = target->getScaleY();
    m_fDeltaX = m_fEndScaleX - m_fStartScaleX;
    m_fDeltaY = m_fEndScaleY - m_fStartScaleY;
}

void CCScaleTo::update(float time) {
    if (m_pTarget) {
        m_pTarget->setScaleX(m_fStartScaleX + m_fDeltaX * time);
        m_pTarget->setScaleY(m_fStartScaleY + m_fDeltaY * time);
    }
}

// ---- fades: on a node with a colour and an opacity (CCRGBAProtocol)

CCFadeIn* CCFadeIn::create(float d) {
    CCFadeIn* a = new CCFadeIn;
    a->initWithDuration(d);
    return autoreleased(a);
}
CCObject* CCFadeIn::copyWithZone(void*) {
    CCFadeIn* c = new CCFadeIn;
    c->initWithDuration(m_fDuration);
    return c;
}
void CCFadeIn::update(float time) {
    if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(m_pTarget)) p->setOpacity((GLubyte)(255 * time));
}
CCActionInterval* CCFadeIn::reverse() { return CCFadeOut::create(m_fDuration); }

CCFadeOut* CCFadeOut::create(float d) {
    CCFadeOut* a = new CCFadeOut;
    a->initWithDuration(d);
    return autoreleased(a);
}
CCObject* CCFadeOut::copyWithZone(void*) {
    CCFadeOut* c = new CCFadeOut;
    c->initWithDuration(m_fDuration);
    return c;
}
void CCFadeOut::update(float time) {
    if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(m_pTarget)) p->setOpacity(GLubyte(255 * (1 - time)));
}
CCActionInterval* CCFadeOut::reverse() { return CCFadeIn::create(m_fDuration); }

CCFadeTo* CCFadeTo::create(float duration, GLubyte opacity) {
    CCFadeTo* a = new CCFadeTo;
    a->initWithDuration(duration, opacity);
    return autoreleased(a);
}
bool CCFadeTo::initWithDuration(float duration, GLubyte opacity) {
    CCActionInterval::initWithDuration(duration);
    m_toOpacity = opacity;
    return true;
}
CCObject* CCFadeTo::copyWithZone(void*) {
    CCFadeTo* c = new CCFadeTo;
    c->initWithDuration(m_fDuration, m_toOpacity);
    return c;
}
void CCFadeTo::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(target)) m_fromOpacity = p->getOpacity();
}
void CCFadeTo::update(float time) {
    if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(m_pTarget))
        p->setOpacity((GLubyte)(m_fromOpacity + (m_toOpacity - m_fromOpacity) * time));
}

// ---- delay, reverse time, the padding

CCDelayTime* CCDelayTime::create(float d) {
    CCDelayTime* a = new CCDelayTime;
    a->initWithDuration(d);
    return autoreleased(a);
}
CCObject* CCDelayTime::copyWithZone(void*) {
    CCDelayTime* c = new CCDelayTime;
    c->initWithDuration(m_fDuration);
    return c;
}

CCReverseTime::~CCReverseTime() { CC_SAFE_RELEASE(m_pOther); }
CCReverseTime* CCReverseTime::create(CCFiniteTimeAction* action) {
    CCReverseTime* a = new CCReverseTime;
    a->initWithAction(action);
    return autoreleased(a);
}
bool CCReverseTime::initWithAction(CCFiniteTimeAction* action) {
    CCActionInterval::initWithDuration(action->getDuration());
    CC_SAFE_RELEASE(m_pOther);
    m_pOther = action;
    action->retain();
    return true;
}
CCObject* CCReverseTime::copyWithZone(void*) {
    CCReverseTime* c = new CCReverseTime;
    c->initWithAction(static_cast<CCFiniteTimeAction*>(m_pOther->copy()->autorelease()));
    return c;
}
void CCReverseTime::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_pOther->startWithTarget(target);
}
void CCReverseTime::stop() {
    m_pOther->stop();
    CCActionInterval::stop();
}
void CCReverseTime::update(float time) {
    if (m_pOther) m_pOther->update(1 - time);
}
CCActionInterval* CCReverseTime::reverse() {
    return static_cast<CCActionInterval*>(m_pOther->copy()->autorelease());
}

ExtraAction* ExtraAction::create() {
    ExtraAction* a = new ExtraAction;
    return autoreleased(a);
}

// ----------------------------------------------------------------- instant

CCCallFunc::~CCCallFunc() { CC_SAFE_RELEASE(m_pSelectorTarget); }

CCCallFunc* CCCallFunc::create(CCObject* selectorTarget, SEL_CallFunc selector) {
    CCCallFunc* a = new CCCallFunc;
    a->initWithTarget(selectorTarget);
    a->m_pCallFunc = selector;
    return autoreleased(a);
}

bool CCCallFunc::initWithTarget(CCObject* selectorTarget) {
    CC_SAFE_RETAIN(selectorTarget);
    CC_SAFE_RELEASE(m_pSelectorTarget);
    m_pSelectorTarget = selectorTarget;
    return true;
}

CCObject* CCCallFunc::copyWithZone(void*) {
    CCCallFunc* c = new CCCallFunc;
    c->initWithTarget(m_pSelectorTarget);
    c->m_pCallFunc = m_pCallFunc;
    return c;
}

CCCallFuncN* CCCallFuncN::create(CCObject* selectorTarget, SEL_CallFuncN selector) {
    CCCallFuncN* a = new CCCallFuncN;
    a->initWithTarget(selectorTarget, selector);
    return autoreleased(a);
}

bool CCCallFuncN::initWithTarget(CCObject* selectorTarget, SEL_CallFuncN selector) {
    CCCallFunc::initWithTarget(selectorTarget);
    m_pCallFuncN = selector;
    return true;
}

CCObject* CCCallFuncN::copyWithZone(void*) {
    CCCallFuncN* c = new CCCallFuncN;
    c->initWithTarget(m_pSelectorTarget, m_pCallFuncN);
    return c;
}

// -------------------------------------------------------------------- ease

CCActionEase::~CCActionEase() { CC_SAFE_RELEASE(m_pInner); }

CCActionEase* CCActionEase::create(CCActionInterval* action) {
    CCActionEase* a = new CCActionEase;
    a->initWithAction(action);
    return autoreleased(a);
}

bool CCActionEase::initWithAction(CCActionInterval* action) {
    CCActionInterval::initWithDuration(action->getDuration());
    m_pInner = action;
    action->retain();
    return true;
}

CCObject* CCActionEase::copyWithZone(void*) {
    CCActionEase* c = new CCActionEase;
    c->initWithAction(static_cast<CCActionInterval*>(m_pInner->copy()->autorelease()));
    return c;
}

void CCActionEase::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    m_pInner->startWithTarget(m_pTarget);
}

void CCActionEase::stop() {
    m_pInner->stop();
    CCActionInterval::stop();
}

CCActionInterval* CCActionEase::reverse() { return create(m_pInner->reverse()); }

bool CCEaseRateAction::initWithAction(CCActionInterval* action, float rate) {
    CCActionEase::initWithAction(action);
    m_fRate = rate;
    return true;
}

#define EASE_RATE(Class)                                                                            \
    Class* Class::create(CCActionInterval* action, float rate) {                                   \
        Class* a = new Class;                                                                       \
        a->initWithAction(action, rate);                                                            \
        return autoreleased(a);                                                                     \
    }                                                                                               \
    CCObject* Class::copyWithZone(void*) {                                                          \
        Class* c = new Class;                                                                       \
        c->initWithAction(static_cast<CCActionInterval*>(m_pInner->copy()->autorelease()), m_fRate); \
        return c;                                                                                   \
    }
EASE_RATE(CCEaseIn)
EASE_RATE(CCEaseOut)
EASE_RATE(CCEaseInOut)
#undef EASE_RATE

CCActionInterval* CCEaseIn::reverse() { return CCEaseIn::create(m_pInner->reverse(), 1 / m_fRate); }
CCActionInterval* CCEaseOut::reverse() { return CCEaseOut::create(m_pInner->reverse(), 1 / m_fRate); }

void CCEaseInOut::update(float time) {
    time *= 2;
    if (time < 1) m_pInner->update(0.5f * powf(time, m_fRate));
    else m_pInner->update(1.0f - 0.5f * powf(2 - time, m_fRate));
}
CCActionInterval* CCEaseInOut::reverse() { return CCEaseInOut::create(m_pInner->reverse(), m_fRate); }

bool CCEaseElastic::initWithAction(CCActionInterval* action, float period) {
    CCActionEase::initWithAction(action);
    m_fPeriod = period;
    return true;
}

static const float M_PI_X_2 = (float)M_PI * 2.0f;

CCEaseElasticIn* CCEaseElasticIn::create(CCActionInterval* action, float period) {
    CCEaseElasticIn* a = new CCEaseElasticIn;
    a->initWithAction(action, period);
    return autoreleased(a);
}
CCObject* CCEaseElasticIn::copyWithZone(void*) {
    CCEaseElasticIn* c = new CCEaseElasticIn;
    c->initWithAction(static_cast<CCActionInterval*>(m_pInner->copy()->autorelease()), m_fPeriod);
    return c;
}
void CCEaseElasticIn::update(float time) {
    float newT = 0;
    if (time == 0 || time == 1) {
        newT = time;
    } else {
        float s = m_fPeriod / 4;
        time = time - 1;
        newT = -powf(2, 10 * time) * sinf((time - s) * M_PI_X_2 / m_fPeriod);
    }
    m_pInner->update(newT);
}

CCEaseElasticOut* CCEaseElasticOut::create(CCActionInterval* action, float period) {
    CCEaseElasticOut* a = new CCEaseElasticOut;
    a->initWithAction(action, period);
    return autoreleased(a);
}
CCObject* CCEaseElasticOut::copyWithZone(void*) {
    CCEaseElasticOut* c = new CCEaseElasticOut;
    c->initWithAction(static_cast<CCActionInterval*>(m_pInner->copy()->autorelease()), m_fPeriod);
    return c;
}
void CCEaseElasticOut::update(float time) {
    float newT = 0;
    if (time == 0 || time == 1) {
        newT = time;
    } else {
        float s = m_fPeriod / 4;
        newT = powf(2, -10 * time) * sinf((time - s) * M_PI_X_2 / m_fPeriod) + 1;
    }
    m_pInner->update(newT);
}
CCActionInterval* CCEaseElasticIn::reverse() { return CCEaseElasticOut::create(m_pInner->reverse(), m_fPeriod); }
CCActionInterval* CCEaseElasticOut::reverse() { return CCEaseElasticIn::create(m_pInner->reverse(), m_fPeriod); }

float CCEaseBounce::bounceTime(float time) {
    if (time < 1 / 2.75) return 7.5625f * time * time;
    if (time < 2 / 2.75) {
        time -= 1.5f / 2.75f;
        return 7.5625f * time * time + 0.75f;
    }
    if (time < 2.5 / 2.75) {
        time -= 2.25f / 2.75f;
        return 7.5625f * time * time + 0.9375f;
    }
    time -= 2.625f / 2.75f;
    return 7.5625f * time * time + 0.984375f;
}

CCEaseBounceInOut* CCEaseBounceInOut::create(CCActionInterval* action) {
    CCEaseBounceInOut* a = new CCEaseBounceInOut;
    a->initWithAction(action);
    return autoreleased(a);
}
CCObject* CCEaseBounceInOut::copyWithZone(void*) {
    CCEaseBounceInOut* c = new CCEaseBounceInOut;
    c->initWithAction(static_cast<CCActionInterval*>(m_pInner->copy()->autorelease()));
    return c;
}
void CCEaseBounceInOut::update(float time) {
    float newT = 0;
    if (time < 0.5f) {
        time = time * 2;
        newT = (1 - bounceTime(1 - time)) * 0.5f;
    } else {
        newT = bounceTime(time * 2 - 1) * 0.5f + 0.5f;
    }
    m_pInner->update(newT);
}
CCActionInterval* CCEaseBounceInOut::reverse() { return CCEaseBounceInOut::create(m_pInner->reverse()); }

}  // namespace cocos2d
