// cocos2d-x 2.1.1's CCNode.cpp, ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCNode.h"

#include "engine/cocos/CCActionManager.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCGrid.h"
#include "engine/cocos/CCScheduler.h"
#include "engine/cocos/CCTouch.h"

namespace cocos2d {

static unsigned int s_globalOrderOfArrival = 1;

CCNode::CCNode() {
    // the director's scheduler and action manager
    CCDirector* director = CCDirector::sharedDirector();
    m_pActionManager = director->getActionManager();
    m_pActionManager->retain();
    m_pScheduler = director->getScheduler();
    m_pScheduler->retain();
}

CCNode::~CCNode() {
    // The scheduler does not retain its update targets (2.1.1 did): a node
    // freed with its per-frame update still scheduled (taken off its parent
    // without a cleanup) takes itself off, so no stale entry is left for a
    // later object at the same address to inherit
    if (m_pScheduler) m_pScheduler->unscheduleUpdateForTarget(this);
    CC_SAFE_RELEASE(m_pActionManager);
    CC_SAFE_RELEASE(m_pScheduler);
    CC_SAFE_RELEASE(m_pGrid);
    CC_SAFE_RELEASE(m_pUserObject);
    if (m_pChildren) {
        for (CCObject* o : m_pChildren->data) static_cast<CCNode*>(o)->m_pParent = nullptr;
    }
    CC_SAFE_RELEASE(m_pChildren);
}

CCNode* CCNode::create() {
    CCNode* n = new CCNode;
    if (n->init()) {
        n->autorelease();
        return n;
    }
    delete n;
    return nullptr;
}

void CCNode::setZOrder(int z) {
    _setZOrder(z);
    if (m_pParent) m_pParent->reorderChild(this, z);
}

void CCNode::setAnchorPoint(const CCPoint& point) {
    if (!point.equals(m_obAnchorPoint)) {
        m_obAnchorPoint = point;
        m_obAnchorPointInPoints = ccp(m_obContentSize.width * m_obAnchorPoint.x, m_obContentSize.height * m_obAnchorPoint.y);
        m_bTransformDirty = m_bInverseDirty = true;
    }
}

void CCNode::setContentSize(const CCSize& size) {
    if (!size.equals(m_obContentSize)) {
        m_obContentSize = size;
        m_obAnchorPointInPoints = ccp(m_obContentSize.width * m_obAnchorPoint.x, m_obContentSize.height * m_obAnchorPoint.y);
        m_bTransformDirty = m_bInverseDirty = true;
    }
}

void CCNode::ignoreAnchorPointForPosition(bool ignore) {
    if (ignore != m_bIgnoreAnchorPointForPosition) {
        m_bIgnoreAnchorPointForPosition = ignore;
        m_bTransformDirty = m_bInverseDirty = true;
    }
}

void CCNode::setGrid(CCGridBase* grid) {
    CC_SAFE_RETAIN(grid);
    CC_SAFE_RELEASE(m_pGrid);
    m_pGrid = grid;
}

void CCNode::setUserObject(CCObject* o) {
    CC_SAFE_RETAIN(o);
    CC_SAFE_RELEASE(m_pUserObject);
    m_pUserObject = o;
}

// ---- children

void CCNode::childrenAlloc() {
    m_pChildren = new CCArray;
    m_pChildren->initWithCapacity(4);
}

CCNode* CCNode::getChildByTag(int tag) {
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data) {
            CCNode* n = static_cast<CCNode*>(o);
            if (n && n->m_nTag == tag) return n;
        }
    return nullptr;
}

void CCNode::addChild(CCNode* child, int zOrder, int tag) {
    if (!child || child->m_pParent) return;  // 2.1.1 asserts
    if (!m_pChildren) childrenAlloc();
    insertChild(child, zOrder);
    child->m_nTag = tag;
    child->setParent(this);
    child->setOrderOfArrival(s_globalOrderOfArrival++);
    if (m_bRunning) {
        child->onEnter();
        child->onEnterTransitionDidFinish();
    }
}

void CCNode::removeFromParentAndCleanup(bool cleanup) {
    if (m_pParent) m_pParent->removeChild(this, cleanup);
}

void CCNode::removeChild(CCNode* child, bool cleanup) {
    if (!m_pChildren) return;
    if (m_pChildren->containsObject(child)) detachChild(child, cleanup);
}

void CCNode::removeChildByTag(int tag, bool cleanup) {
    if (CCNode* child = getChildByTag(tag)) removeChild(child, cleanup);
}

void CCNode::removeAllChildrenWithCleanup(bool cleanup) {
    if (!m_pChildren || m_pChildren->count() == 0) return;
    for (CCObject* o : m_pChildren->data) {
        CCNode* n = static_cast<CCNode*>(o);
        if (!n) continue;
        // first onExit, then cleanup
        if (m_bRunning) {
            n->onExitTransitionDidStart();
            n->onExit();
        }
        if (cleanup) n->cleanup();
        n->setParent(nullptr);  // at the end
    }
    m_pChildren->removeAllObjects();
}

void CCNode::detachChild(CCNode* child, bool doCleanup) {
    if (m_bRunning) {
        child->onExitTransitionDidStart();
        child->onExit();
    }
    // without cleanup the child's actions and selectors stay
    if (doCleanup) child->cleanup();
    child->setParent(nullptr);
    m_pChildren->removeObject(child);
}

void CCNode::insertChild(CCNode* child, int z) {
    m_bReorderChildDirty = true;
    m_pChildren->addObject(child);
    child->_setZOrder(z);
}

void CCNode::reorderChild(CCNode* child, int zOrder) {
    m_bReorderChildDirty = true;
    child->setOrderOfArrival(s_globalOrderOfArrival++);
    child->_setZOrder(zOrder);
}

void CCNode::sortAllChildren() {
    if (!m_bReorderChildDirty) return;
    // insertion sort: by z order, then order of arrival
    std::vector<CCObject*>& x = m_pChildren->data;
    for (int i = 1; i < static_cast<int>(x.size()); i++) {
        CCNode* item = static_cast<CCNode*>(x[i]);
        int j = i - 1;
        while (j >= 0 && (item->m_nZOrder < static_cast<CCNode*>(x[j])->m_nZOrder ||
                          (item->m_nZOrder == static_cast<CCNode*>(x[j])->m_nZOrder &&
                           item->m_uOrderOfArrival < static_cast<CCNode*>(x[j])->m_uOrderOfArrival))) {
            x[j + 1] = x[j];
            j = j - 1;
        }
        x[j + 1] = item;
    }
    m_bReorderChildDirty = false;
}

// ---- running

namespace {

// Fixed: the children as they are now, each retained while the snapshot
// lives, so what one of them does in its onEnter/onExit/cleanup (removing a
// sibling) cannot free one still to be called; one taken off meanwhile is
// left out (`mine`), as 2.1.1's walk of the live array left it. (2.1.1's
// walk then skipped the next child and read past the array's end; the
// port's copy of bare pointers called the freed sibling.)
struct ChildSnapshot {
    std::vector<CCObject*> kids;
    explicit ChildSnapshot(CCArray* children) {
        if (children) kids = children->data;
        for (CCObject* o : kids) o->retain();
    }
    static bool mine(CCObject* o, CCNode* parent) { return static_cast<CCNode*>(o)->getParent() == parent; }
    ~ChildSnapshot() {
        for (CCObject* o : kids) o->release();
    }
    ChildSnapshot(const ChildSnapshot&) = delete;
    ChildSnapshot& operator=(const ChildSnapshot&) = delete;
};

}  // namespace

void CCNode::onEnter() {
    ChildSnapshot kids(m_pChildren);
    for (CCObject* o : kids.kids)
        if (ChildSnapshot::mine(o, this)) static_cast<CCNode*>(o)->onEnter();
    resumeSchedulerAndActions();
    m_bRunning = true;
}

void CCNode::onEnterTransitionDidFinish() {
    ChildSnapshot kids(m_pChildren);
    for (CCObject* o : kids.kids)
        if (ChildSnapshot::mine(o, this)) static_cast<CCNode*>(o)->onEnterTransitionDidFinish();
}

void CCNode::onExitTransitionDidStart() {
    ChildSnapshot kids(m_pChildren);
    for (CCObject* o : kids.kids)
        if (ChildSnapshot::mine(o, this)) static_cast<CCNode*>(o)->onExitTransitionDidStart();
}

void CCNode::onExit() {
    pauseSchedulerAndActions();
    m_bRunning = false;
    ChildSnapshot kids(m_pChildren);
    for (CCObject* o : kids.kids)
        if (ChildSnapshot::mine(o, this)) static_cast<CCNode*>(o)->onExit();
}

void CCNode::cleanup() {
    stopAllActions();
    unscheduleAllSelectors();
    ChildSnapshot kids(m_pChildren);
    for (CCObject* o : kids.kids)
        if (ChildSnapshot::mine(o, this)) static_cast<CCNode*>(o)->cleanup();
}

void CCNode::visit() {
    if (!m_bVisible) return;
    glPushMatrix();
    if (m_pGrid && m_pGrid->isActive()) m_pGrid->beforeDraw();
    transform();
    size_t i = 0;
    if (m_pChildren && m_pChildren->count() > 0) {
        sortAllChildren();
        std::vector<CCObject*>& kids = m_pChildren->data;
        // the children below z 0
        for (; i < kids.size(); i++) {
            CCNode* n = static_cast<CCNode*>(kids[i]);
            if (n && n->m_nZOrder < 0) n->visit();
            else break;
        }
        draw();
        for (; i < kids.size(); i++)
            if (CCNode* n = static_cast<CCNode*>(kids[i])) n->visit();
    } else {
        draw();
    }
    m_uOrderOfArrival = 0;  // reset for the next frame
    if (m_pGrid && m_pGrid->isActive()) m_pGrid->afterDraw(this);
    glPopMatrix();
}

CCRect CCNode::boundingBox() {
    CCRect rect = CCRectMake(0, 0, m_obContentSize.width, m_obContentSize.height);
    return CCRectApplyAffineTransform(rect, nodeToParentTransform());
}

// ---- actions

void CCNode::setActionManager(CCActionManager* am) {
    if (am != m_pActionManager) {
        stopAllActions();
        CC_SAFE_RETAIN(am);
        CC_SAFE_RELEASE(m_pActionManager);
        m_pActionManager = am;
    }
}

CCAction* CCNode::runAction(CCAction* action) {
    m_pActionManager->addAction(action, this, !m_bRunning);
    return action;
}
void CCNode::stopAllActions() { m_pActionManager->removeAllActionsFromTarget(this); }
void CCNode::stopAction(CCAction* action) { m_pActionManager->removeAction(action); }
void CCNode::stopActionByTag(int tag) { m_pActionManager->removeActionByTag(tag, this); }
CCAction* CCNode::getActionByTag(int tag) { return m_pActionManager->getActionByTag(tag, this); }
unsigned int CCNode::numberOfRunningActions() { return m_pActionManager->numberOfRunningActionsInTarget(this); }

// ---- scheduling

void CCNode::setScheduler(CCScheduler* s) {
    if (s != m_pScheduler) {
        unscheduleAllSelectors();
        CC_SAFE_RETAIN(s);
        CC_SAFE_RELEASE(m_pScheduler);
        m_pScheduler = s;
    }
}

bool CCNode::isScheduled(SEL_SCHEDULE) { return false; }  // 2.1.1: "TODO"

void CCNode::scheduleUpdateWithPriority(int priority) {
    m_pScheduler->scheduleUpdateForTarget(this, priority, !m_bRunning);
}
void CCNode::unscheduleUpdate() { m_pScheduler->unscheduleUpdateForTarget(this); }
void CCNode::schedule(SEL_SCHEDULE selector, float interval) { schedule(selector, interval, kCCRepeatForever, 0.0f); }
void CCNode::schedule(SEL_SCHEDULE selector, float interval, unsigned int repeat, float delay) {
    if (!selector || interval < 0) return;
    m_pScheduler->scheduleSelector(selector, this, interval, repeat, delay, !m_bRunning);
}
void CCNode::unschedule(SEL_SCHEDULE selector) {
    if (selector) m_pScheduler->unscheduleSelector(selector, this);
}
void CCNode::unscheduleAllSelectors() { m_pScheduler->unscheduleAllForTarget(this); }
void CCNode::resumeSchedulerAndActions() {
    m_pScheduler->resumeTarget(this);
    m_pActionManager->resumeTarget(this);
}
void CCNode::pauseSchedulerAndActions() {
    m_pScheduler->pauseTarget(this);
    m_pActionManager->pauseTarget(this);
}

// ---- transforms

void CCNode::transform() {
    CCAffineTransform t = nodeToParentTransform();
    // the affine transform as a 4x4 matrix (CGAffineToGL), z from vertexZ
    float m[16] = {t.a, t.b, 0, 0, t.c, t.d, 0, 0, 0, 0, 1, 0, t.tx, t.ty, m_fVertexZ, 1};
    glMultMatrixf(m);
}

void CCNode::transformAncestors() {
    if (m_pParent) {
        m_pParent->transformAncestors();
        m_pParent->transform();
    }
}

void CCNode::updateTransform() {
    ChildSnapshot kids(m_pChildren);
    for (CCObject* o : kids.kids)
        if (ChildSnapshot::mine(o, this)) static_cast<CCNode*>(o)->updateTransform();
}

CCAffineTransform CCNode::nodeToParentTransform() {
    if (m_bTransformDirty) {
        float x = m_obPosition.x, y = m_obPosition.y;
        if (m_bIgnoreAnchorPointForPosition) {
            x += m_obAnchorPointInPoints.x;
            y += m_obAnchorPointInPoints.y;
        }
        // rotation, X and Y apart (the same on both: a plain rotation)
        float cx = 1, sx = 0, cy = 1, sy = 0;
        if (m_fRotationX || m_fRotationY) {
            float radiansX = -CC_DEGREES_TO_RADIANS(m_fRotationX);
            float radiansY = -CC_DEGREES_TO_RADIANS(m_fRotationY);
            cx = cosf(radiansX);
            sx = sinf(radiansX);
            cy = cosf(radiansY);
            sy = sinf(radiansY);
        }
        bool needsSkewMatrix = m_fSkewX || m_fSkewY;
        // the anchor point inline when there is no skew
        if (!needsSkewMatrix && !m_obAnchorPointInPoints.equals(CCPointZero)) {
            x += cy * -m_obAnchorPointInPoints.x * m_fScaleX + -sx * -m_obAnchorPointInPoints.y * m_fScaleY;
            y += sy * -m_obAnchorPointInPoints.x * m_fScaleX + cx * -m_obAnchorPointInPoints.y * m_fScaleY;
        }
        m_sTransform = CCAffineTransformMake(cy * m_fScaleX, sy * m_fScaleX, -sx * m_fScaleY, cx * m_fScaleY, x, y);
        if (needsSkewMatrix) {
            CCAffineTransform skewMatrix = CCAffineTransformMake(1.0f, tanf(CC_DEGREES_TO_RADIANS(m_fSkewY)),
                                                                 tanf(CC_DEGREES_TO_RADIANS(m_fSkewX)), 1.0f, 0.0f, 0.0f);
            m_sTransform = CCAffineTransformConcat(skewMatrix, m_sTransform);
            if (!m_obAnchorPointInPoints.equals(CCPointZero))
                m_sTransform = CCAffineTransformTranslate(m_sTransform, -m_obAnchorPointInPoints.x, -m_obAnchorPointInPoints.y);
        }
        m_bTransformDirty = false;
    }
    return m_sTransform;
}

CCAffineTransform CCNode::parentToNodeTransform() {
    if (m_bInverseDirty) {
        m_sInverse = CCAffineTransformInvert(nodeToParentTransform());
        m_bInverseDirty = false;
    }
    return m_sInverse;
}

CCAffineTransform CCNode::nodeToWorldTransform() {
    CCAffineTransform t = nodeToParentTransform();
    for (CCNode* p = m_pParent; p; p = p->getParent()) t = CCAffineTransformConcat(t, p->nodeToParentTransform());
    return t;
}

CCPoint CCNode::convertTouchToNodeSpace(CCTouch* touch) { return convertToNodeSpace(touch->getLocation()); }
CCPoint CCNode::convertTouchToNodeSpaceAR(CCTouch* touch) { return convertToNodeSpaceAR(touch->getLocation()); }

}  // namespace cocos2d
