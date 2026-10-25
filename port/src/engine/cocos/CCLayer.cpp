// cocos2d-x 2.1.1's CCScene.cpp and CCLayer.cpp (CCLayer, CCLayerColor,
// CCLayerGradient), ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCLayer.h"

#include "engine/cocos/CCDirector.h"
#include "platform/DrawList.h"

namespace cocos2d {

template <class T>
static T* created(T* n, bool ok) {
    if (ok) {
        n->autorelease();
        return n;
    }
    delete n;
    return nullptr;
}

// ------------------------------------------------------------------ CCScene

CCScene::CCScene() {
    m_bIgnoreAnchorPointForPosition = true;
    setAnchorPoint(ccp(0.5f, 0.5f));
}

CCScene* CCScene::create() {
    CCScene* s = new CCScene;
    return created(s, s->init());
}

bool CCScene::init() {
    setContentSize(CCDirector::sharedDirector()->getWinSize());
    return true;
}

// ------------------------------------------------------------------ CCLayer

CCLayer::CCLayer() {
    m_bIgnoreAnchorPointForPosition = true;
    setAnchorPoint(ccp(0.5f, 0.5f));
}

CCLayer* CCLayer::create() {
    CCLayer* l = new CCLayer;
    return created(l, l->init());
}

bool CCLayer::init() {
    setContentSize(CCDirector::sharedDirector()->getWinSize());
    m_bTouchEnabled = false;
    m_bAccelerometerEnabled = false;
    return true;
}

void CCLayer::registerWithTouchDispatcher() {
    CCTouchDispatcher* d = CCDirector::sharedDirector()->getTouchDispatcher();
    if (m_eTouchMode == kCCTouchesAllAtOnce) d->addStandardDelegate(this, 0);
    else d->addTargetedDelegate(this, m_nTouchPriority, true);
}

void CCLayer::setTouchEnabled(bool enabled) {
    if (m_bTouchEnabled == enabled) return;
    m_bTouchEnabled = enabled;
    if (m_bRunning) {
        if (enabled) registerWithTouchDispatcher();
        else CCDirector::sharedDirector()->getTouchDispatcher()->removeDelegate(this);
    }
}

void CCLayer::setTouchMode(ccTouchesMode mode) {
    if (m_eTouchMode == mode) return;
    m_eTouchMode = mode;
    if (m_bTouchEnabled) {
        setTouchEnabled(false);
        setTouchEnabled(true);
    }
}

void CCLayer::setTouchPriority(int priority) {
    if (m_nTouchPriority == priority) return;
    m_nTouchPriority = priority;
    if (m_bTouchEnabled) {
        setTouchEnabled(false);
        setTouchEnabled(true);
    }
}

void CCLayer::setAccelerometerEnabled(bool enabled) {
    if (enabled == m_bAccelerometerEnabled) return;
    m_bAccelerometerEnabled = enabled;
    if (m_bRunning) CCDirector::sharedDirector()->getAccelerometer()->setDelegate(enabled ? this : nullptr);
}

void CCLayer::setKeypadEnabled(bool enabled) {
    if (enabled == m_bKeypadEnabled) return;
    m_bKeypadEnabled = enabled;
    if (m_bRunning) {
        CCKeypadDispatcher* k = CCDirector::sharedDirector()->getKeypadDispatcher();
        if (enabled) k->addDelegate(this);
        else k->removeDelegate(this);
    }
}

void CCLayer::onEnter() {
    CCDirector* d = CCDirector::sharedDirector();
    // the parent nodes register first: the events go to them last
    if (m_bTouchEnabled) registerWithTouchDispatcher();
    CCNode::onEnter();
    if (m_bAccelerometerEnabled) d->getAccelerometer()->setDelegate(this);
    if (m_bKeypadEnabled) d->getKeypadDispatcher()->addDelegate(this);
}

void CCLayer::onExit() {
    CCDirector* d = CCDirector::sharedDirector();
    if (m_bTouchEnabled) d->getTouchDispatcher()->removeDelegate(this);
    if (m_bAccelerometerEnabled) d->getAccelerometer()->setDelegate(nullptr);
    if (m_bKeypadEnabled) d->getKeypadDispatcher()->removeDelegate(this);
    CCNode::onExit();
}

void CCLayer::onEnterTransitionDidFinish() {
    if (m_bAccelerometerEnabled) CCDirector::sharedDirector()->getAccelerometer()->setDelegate(this);
    CCNode::onEnterTransitionDidFinish();
}

// ------------------------------------------------------------- CCLayerColor

CCLayerColor* CCLayerColor::create() {
    CCLayerColor* l = new CCLayerColor;
    return created(l, l->init());
}
CCLayerColor* CCLayerColor::create(const ccColor4B& color) {
    CCLayerColor* l = new CCLayerColor;
    return created(l, l->initWithColor(color));
}
CCLayerColor* CCLayerColor::create(const ccColor4B& color, GLfloat width, GLfloat height) {
    CCLayerColor* l = new CCLayerColor;
    return created(l, l->initWithColor(color, width, height));
}

bool CCLayerColor::init() {
    CCSize s = CCDirector::sharedDirector()->getWinSize();
    return initWithColor(ccc4(0, 0, 0, 0), s.width, s.height);
}

bool CCLayerColor::initWithColor(const ccColor4B& color) {
    CCSize s = CCDirector::sharedDirector()->getWinSize();
    return initWithColor(color, s.width, s.height);
}

bool CCLayerColor::initWithColor(const ccColor4B& color, GLfloat w, GLfloat h) {
    if (!CCLayer::init()) return false;
    m_tBlendFunc = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};
    m_tColor = ccc3(color.r, color.g, color.b);
    m_cOpacity = color.a;
    for (ccVertex2F& v : m_pSquareVertices) v = {0, 0};
    updateColor();
    setContentSize(CCSizeMake(w, h));
    return true;
}

void CCLayerColor::setContentSize(const CCSize& size) {
    m_pSquareVertices[1].x = size.width;
    m_pSquareVertices[2].y = size.height;
    m_pSquareVertices[3].x = size.width;
    m_pSquareVertices[3].y = size.height;
    CCLayer::setContentSize(size);
}

void CCLayerColor::updateColor() {
    for (ccColor4F& c : m_pSquareColors) c = {m_tColor.r / 255.0f, m_tColor.g / 255.0f, m_tColor.b / 255.0f, m_cOpacity / 255.0f};
}

void CCLayerColor::draw() {
    // a triangle strip of the four corners, coloured each
    float xy[8], rgba[16];
    for (int i = 0; i < 4; ++i) {
        xy[i * 2] = m_pSquareVertices[i].x;
        xy[i * 2 + 1] = m_pSquareVertices[i].y;
        rgba[i * 4] = m_pSquareColors[i].r;
        rgba[i * 4 + 1] = m_pSquareColors[i].g;
        rgba[i * 4 + 2] = m_pSquareColors[i].b;
        rgba[i * 4 + 3] = m_pSquareColors[i].a;
    }
    static const unsigned short strip[6] = {0, 1, 2, 1, 3, 2};
    // the engine's meshes blend (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA): the default here
    platform::drawMesh(serial(), xy, 2, 4, rgba, strip, 6);
}

// ---------------------------------------------------------- CCLayerGradient

CCLayerGradient* CCLayerGradient::create(const ccColor4B& start, const ccColor4B& end) {
    CCLayerGradient* l = new CCLayerGradient;
    return created(l, l->initWithColor(start, end));
}
CCLayerGradient* CCLayerGradient::create(const ccColor4B& start, const ccColor4B& end, const CCPoint& v) {
    CCLayerGradient* l = new CCLayerGradient;
    return created(l, l->initWithColor(start, end, v));
}

bool CCLayerGradient::initWithColor(const ccColor4B& start, const ccColor4B& end) {
    return initWithColor(start, end, ccp(0, -1));
}

bool CCLayerGradient::initWithColor(const ccColor4B& start, const ccColor4B& end, const CCPoint& v) {
    m_endColor = ccc3(end.r, end.g, end.b);
    m_cEndOpacity = end.a;
    m_cStartOpacity = start.a;
    m_AlongVector = v;
    m_bCompressedInterpolation = true;
    return CCLayerColor::initWithColor(ccc4(start.r, start.g, start.b, 255));
}

void CCLayerGradient::updateColor() {
    CCLayerColor::updateColor();
    float h = ccpLength(m_AlongVector);
    if (h == 0) return;
    float c = sqrtf(2.0f);
    CCPoint u = ccp(m_AlongVector.x / h, m_AlongVector.y / h);
    if (m_bCompressedInterpolation) {  // compressed interpolation mode
        float h2 = 1 / (fabsf(u.x) + fabsf(u.y));
        u = ccpMult(u, h2 * c);
    }
    float opacityf = m_cOpacity / 255.0f;
    ccColor4F S = {m_tColor.r / 255.0f, m_tColor.g / 255.0f, m_tColor.b / 255.0f, m_cStartOpacity * opacityf / 255.0f};
    ccColor4F E = {m_endColor.r / 255.0f, m_endColor.g / 255.0f, m_endColor.b / 255.0f, m_cEndOpacity * opacityf / 255.0f};
    // corners (-1,-1), (1,-1), (-1,1), (1,1)
    const float k[4] = {(c + u.x + u.y), (c - u.x + u.y), (c + u.x - u.y), (c - u.x - u.y)};
    for (int i = 0; i < 4; ++i) {
        float f = k[i] / (2.0f * c);
        m_pSquareColors[i] = {E.r + (S.r - E.r) * f, E.g + (S.g - E.g) * f, E.b + (S.b - E.b) * f, E.a + (S.a - E.a) * f};
    }
}

}  // namespace cocos2d
