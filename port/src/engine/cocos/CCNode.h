// cocos2d-x 2.1.1's CCNode (base_nodes/CCNode), ported to Bleed Engine.
// MIT: LICENSE-cocos2d-x.txt.
//
// The scene graph's node: position (points, y up, origin at the screen's
// bottom left), anchor point, content size, scale, rotation (degrees,
// clockwise; X and Y apart for a rotational skew), skew, z order, visibility,
// tag, user data; children sorted by z order and order of arrival; its
// actions (CCActionManager) and selectors (CCScheduler); onEnter/onExit as the
// running scene takes it in or lets it go.
// Drawing: visit() multiplies the node's transform onto GL's modelview (the
// director sets cocos's 2D projection), draws the children below z 0, the
// node itself (draw(), a subclass's: sprites, labels... through the engine's
// recorded drawing, platform/DrawList), then the rest. Left out of 2.1.1:
// scripting, the camera, GL server state and shader programs (the engine
// draws with its own pipeline).
#pragma once

#include "engine/cocos/CCArray.h"
#include "engine/cocos/ccTypes.h"

namespace cocos2d {

class CCAction;
class CCActionManager;
class CCScheduler;
class CCTouch;
class CCGridBase;

enum { kCCNodeTagInvalid = -1 };

class CCNode : public CCObject {
public:
    CCNode();
    ~CCNode() override;
    static CCNode* create();
    virtual bool init() { return true; }

    virtual void setZOrder(int zOrder);
    virtual void _setZOrder(int z) { m_nZOrder = z; }
    virtual int getZOrder() { return m_nZOrder; }
    virtual void setVertexZ(float z) { m_fVertexZ = z; }
    virtual float getVertexZ() { return m_fVertexZ; }
    virtual void setScaleX(float s) { m_fScaleX = s; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getScaleX() { return m_fScaleX; }
    virtual void setScaleY(float s) { m_fScaleY = s; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getScaleY() { return m_fScaleY; }
    virtual void setScale(float s) { m_fScaleX = m_fScaleY = s; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getScale() { return m_fScaleX; }  // 2.1.1 asserts scaleX == scaleY
    virtual void setPosition(const CCPoint& p) { m_obPosition = p; m_bTransformDirty = m_bInverseDirty = true; }
    virtual const CCPoint& getPosition() { return m_obPosition; }
    void setPosition(float x, float y) { setPosition(ccp(x, y)); }
    void getPosition(float* x, float* y) { *x = m_obPosition.x; *y = m_obPosition.y; }
    void setPositionX(float x) { setPosition(ccp(x, m_obPosition.y)); }
    float getPositionX() { return m_obPosition.x; }
    void setPositionY(float y) { setPosition(ccp(m_obPosition.x, y)); }
    float getPositionY() { return m_obPosition.y; }
    virtual void setSkewX(float s) { m_fSkewX = s; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getSkewX() { return m_fSkewX; }
    virtual void setSkewY(float s) { m_fSkewY = s; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getSkewY() { return m_fSkewY; }
    virtual void setAnchorPoint(const CCPoint& anchorPoint);
    virtual const CCPoint& getAnchorPoint() { return m_obAnchorPoint; }
    virtual const CCPoint& getAnchorPointInPoints() { return m_obAnchorPointInPoints; }
    virtual void setContentSize(const CCSize& size);
    virtual const CCSize& getContentSize() { return m_obContentSize; }
    virtual void setVisible(bool v) { m_bVisible = v; }
    virtual bool isVisible() { return m_bVisible; }
    virtual void setRotation(float r) { m_fRotationX = m_fRotationY = r; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getRotation() { return m_fRotationX; }  // 2.1.1 asserts rotationX == rotationY
    virtual void setRotationX(float r) { m_fRotationX = r; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getRotationX() { return m_fRotationX; }
    virtual void setRotationY(float r) { m_fRotationY = r; m_bTransformDirty = m_bInverseDirty = true; }
    virtual float getRotationY() { return m_fRotationY; }
    virtual void setOrderOfArrival(unsigned int o) { m_uOrderOfArrival = o; }
    virtual unsigned int getOrderOfArrival() { return m_uOrderOfArrival; }
    virtual void ignoreAnchorPointForPosition(bool ignore);
    virtual bool isIgnoreAnchorPointForPosition() { return m_bIgnoreAnchorPointForPosition; }

    virtual void addChild(CCNode* child) { addChild(child, child->m_nZOrder, child->m_nTag); }
    virtual void addChild(CCNode* child, int zOrder) { addChild(child, zOrder, child->m_nTag); }
    virtual void addChild(CCNode* child, int zOrder, int tag);
    CCNode* getChildByTag(int tag);
    virtual CCArray* getChildren() { return m_pChildren; }
    unsigned int getChildrenCount() { return m_pChildren ? m_pChildren->count() : 0; }
    virtual void setParent(CCNode* parent) { m_pParent = parent; }
    virtual CCNode* getParent() { return m_pParent; }
    virtual void removeFromParent() { removeFromParentAndCleanup(true); }
    virtual void removeFromParentAndCleanup(bool cleanup);
    virtual void removeChild(CCNode* child) { removeChild(child, true); }
    virtual void removeChild(CCNode* child, bool cleanup);
    virtual void removeChildByTag(int tag) { removeChildByTag(tag, true); }
    virtual void removeChildByTag(int tag, bool cleanup);
    virtual void removeAllChildren() { removeAllChildrenWithCleanup(true); }
    virtual void removeAllChildrenWithCleanup(bool cleanup);
    virtual void reorderChild(CCNode* child, int zOrder);
    virtual void sortAllChildren();

    virtual CCGridBase* getGrid() { return m_pGrid; }
    virtual void setGrid(CCGridBase* grid);
    virtual int getTag() { return m_nTag; }
    virtual void setTag(int tag) { m_nTag = tag; }
    virtual void* getUserData() { return m_pUserData; }
    virtual void setUserData(void* d) { m_pUserData = d; }
    virtual CCObject* getUserObject() { return m_pUserObject; }
    virtual void setUserObject(CCObject* o);
    virtual bool isRunning() { return m_bRunning; }

    virtual void onEnter();
    virtual void onEnterTransitionDidFinish();
    virtual void onExit();
    virtual void onExitTransitionDidStart();
    virtual void cleanup();
    virtual void draw() {}
    virtual void visit();
    CCRect boundingBox();

    virtual void setActionManager(CCActionManager* am);
    virtual CCActionManager* getActionManager() { return m_pActionManager; }
    CCAction* runAction(CCAction* action);
    void stopAllActions();
    void stopAction(CCAction* action);
    void stopActionByTag(int tag);
    CCAction* getActionByTag(int tag);
    unsigned int numberOfRunningActions();

    virtual void setScheduler(CCScheduler* s);
    virtual CCScheduler* getScheduler() { return m_pScheduler; }
    bool isScheduled(SEL_SCHEDULE selector);
    void scheduleUpdate() { scheduleUpdateWithPriority(0); }
    void scheduleUpdateWithPriority(int priority);
    void unscheduleUpdate();
    void schedule(SEL_SCHEDULE selector, float interval, unsigned int repeat, float delay);
    void schedule(SEL_SCHEDULE selector, float interval);
    void scheduleOnce(SEL_SCHEDULE selector, float delay) { schedule(selector, 0.0f, 0, delay); }
    void schedule(SEL_SCHEDULE selector) { schedule(selector, 0.0f); }
    void unschedule(SEL_SCHEDULE selector);
    void unscheduleAllSelectors();
    void resumeSchedulerAndActions();
    void pauseSchedulerAndActions();
    void update(float delta) override {}

    void transform();
    void transformAncestors();
    virtual void updateTransform();
    virtual CCAffineTransform nodeToParentTransform();
    virtual CCAffineTransform parentToNodeTransform();
    virtual CCAffineTransform nodeToWorldTransform();
    virtual CCAffineTransform worldToNodeTransform() { return CCAffineTransformInvert(nodeToWorldTransform()); }
    CCPoint convertToNodeSpace(const CCPoint& worldPoint) {
        return CCPointApplyAffineTransform(worldPoint, worldToNodeTransform());
    }
    CCPoint convertToWorldSpace(const CCPoint& nodePoint) {
        return CCPointApplyAffineTransform(nodePoint, nodeToWorldTransform());
    }
    CCPoint convertToNodeSpaceAR(const CCPoint& worldPoint) {
        return ccpSub(convertToNodeSpace(worldPoint), m_obAnchorPointInPoints);
    }
    CCPoint convertToWorldSpaceAR(const CCPoint& nodePoint) {
        return convertToWorldSpace(ccpAdd(nodePoint, m_obAnchorPointInPoints));
    }
    CCPoint convertTouchToNodeSpace(CCTouch* touch);
    CCPoint convertTouchToNodeSpaceAR(CCTouch* touch);

protected:
    void childrenAlloc();
    void insertChild(CCNode* child, int z);
    void detachChild(CCNode* child, bool doCleanup);

    float m_fRotationX = 0, m_fRotationY = 0;
    float m_fScaleX = 1, m_fScaleY = 1;
    float m_fVertexZ = 0;
    CCPoint m_obPosition;
    float m_fSkewX = 0, m_fSkewY = 0;
    CCPoint m_obAnchorPointInPoints;
    CCPoint m_obAnchorPoint;
    CCSize m_obContentSize;
    CCAffineTransform m_sTransform = CCAffineTransformIdentity;
    CCAffineTransform m_sInverse = CCAffineTransformIdentity;
    CCGridBase* m_pGrid = nullptr;
    int m_nZOrder = 0;
    CCArray* m_pChildren = nullptr;
    CCNode* m_pParent = nullptr;  // weak
    int m_nTag = kCCNodeTagInvalid;
    void* m_pUserData = nullptr;
    CCObject* m_pUserObject = nullptr;
    unsigned int m_uOrderOfArrival = 0;
    CCScheduler* m_pScheduler = nullptr;
    CCActionManager* m_pActionManager = nullptr;
    bool m_bRunning = false;
    bool m_bTransformDirty = true;
    bool m_bInverseDirty = true;
    bool m_bVisible = true;
    bool m_bIgnoreAnchorPointForPosition = false;
    bool m_bReorderChildDirty = false;
};

// CCRGBAProtocol: a node with a colour and an opacity (sprites, labels, layers).
class CCRGBAProtocol {
public:
    virtual ~CCRGBAProtocol() = default;
    virtual void setColor(const ccColor3B& color) = 0;
    virtual const ccColor3B& getColor() = 0;
    virtual GLubyte getOpacity() = 0;
    virtual void setOpacity(GLubyte opacity) = 0;
    virtual void setOpacityModifyRGB(bool v) = 0;
    virtual bool isOpacityModifyRGB() = 0;
};

// CCBlendProtocol: how a node's pixels blend onto what is under them.
class CCBlendProtocol {
public:
    virtual ~CCBlendProtocol() = default;
    virtual void setBlendFunc(ccBlendFunc blendFunc) = 0;
    virtual ccBlendFunc getBlendFunc() = 0;
};

}  // namespace cocos2d
