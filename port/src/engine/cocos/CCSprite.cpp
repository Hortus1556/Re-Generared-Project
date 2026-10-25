// cocos2d-x 2.1.1's CCSprite.cpp and CCSpriteBatchNode.cpp, ported.
// MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCSprite.h"

#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSpriteFrameCache.h"
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

CCSprite* CCSprite::create() {
    CCSprite* s = new CCSprite;
    return created(s, s->init());
}
CCSprite* CCSprite::create(const char* fileName) {
    CCSprite* s = new CCSprite;
    return created(s, s->initWithFile(fileName));
}
CCSprite* CCSprite::create(const char* fileName, const CCRect& rect) {
    CCSprite* s = new CCSprite;
    return created(s, s->initWithFile(fileName, rect));
}
CCSprite* CCSprite::createWithTexture(CCTexture2D* texture) {
    CCSprite* s = new CCSprite;
    return created(s, s->initWithTexture(texture));
}
CCSprite* CCSprite::createWithTexture(CCTexture2D* texture, const CCRect& rect) {
    CCSprite* s = new CCSprite;
    return created(s, s->initWithTexture(texture, rect));
}
CCSprite* CCSprite::createWithSpriteFrame(CCSpriteFrame* frame) {
    CCSprite* s = new CCSprite;
    return created(s, frame && s->initWithSpriteFrame(frame));
}
CCSprite* CCSprite::createWithSpriteFrameName(const char* frameName) {
    return createWithSpriteFrame(CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frameName));
}

CCSprite::~CCSprite() { CC_SAFE_RELEASE(m_pobTexture); }

bool CCSprite::init() { return initWithTexture(nullptr, CCRectZero); }

bool CCSprite::initWithTexture(CCTexture2D* texture) {
    if (!texture) return false;
    CCRect rect = CCRectZero;
    rect.size = texture->getContentSize();
    return initWithTexture(texture, rect);
}

bool CCSprite::initWithTexture(CCTexture2D* texture, const CCRect& rect) { return initWithTexture(texture, rect, false); }

bool CCSprite::initWithTexture(CCTexture2D* texture, const CCRect& rect, bool rotated) {
    m_pobBatchNode = nullptr;
    m_bDirty = false;
    m_bOpacityModifyRGB = true;
    m_nOpacity = 255;
    m_sColor = m_sColorUnmodified = ccWHITE;
    m_sBlendFunc = {CC_BLEND_SRC, CC_BLEND_DST};
    m_bFlipX = m_bFlipY = false;
    setAnchorPoint(ccp(0.5f, 0.5f));  // the default: its centre
    m_obOffsetPosition = CCPointZero;
    m_sQuad = {};
    ccColor4B white = {255, 255, 255, 255};
    m_sQuad.bl.colors = m_sQuad.br.colors = m_sQuad.tl.colors = m_sQuad.tr.colors = white;
    setTexture(texture);  // updates the blend function
    setTextureRect(rect, rotated, rect.size);
    setBatchNode(nullptr);
    return true;
}

bool CCSprite::initWithSpriteFrame(CCSpriteFrame* frame) {
    bool ok = initWithTexture(frame->getTexture(), frame->getRect());
    setDisplayFrame(frame);
    return ok;
}

bool CCSprite::initWithSpriteFrameName(const char* frameName) {
    CCSpriteFrame* frame = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frameName);
    return frame && initWithSpriteFrame(frame);
}

bool CCSprite::initWithFile(const char* fileName) {
    CCTexture2D* t = CCTextureCache::sharedTextureCache()->addImage(fileName);
    if (!t) return false;
    CCRect rect = CCRectZero;
    rect.size = t->getContentSize();
    return initWithTexture(t, rect);
}

bool CCSprite::initWithFile(const char* fileName, const CCRect& rect) {
    CCTexture2D* t = CCTextureCache::sharedTextureCache()->addImage(fileName);
    return t && initWithTexture(t, rect);
}

void CCSprite::setTexture(CCTexture2D* texture) {
    if (m_pobTexture != texture) {
        CC_SAFE_RETAIN(texture);
        CC_SAFE_RELEASE(m_pobTexture);
        m_pobTexture = texture;
        updateBlendFunc();
    }
}

void CCSprite::updateBlendFunc() {
    // straight alpha (every picture here): SRC_ALPHA, and the colour not premultiplied by the opacity
    if (!m_pobTexture || !m_pobTexture->hasPremultipliedAlpha()) {
        m_sBlendFunc = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};
        setOpacityModifyRGB(false);
    } else {
        m_sBlendFunc = {CC_BLEND_SRC, CC_BLEND_DST};
        setOpacityModifyRGB(true);
    }
}

void CCSprite::setTextureRect(const CCRect& rect) { setTextureRect(rect, false, rect.size); }

void CCSprite::setTextureRect(const CCRect& rect, bool rotated, const CCSize& untrimmedSize) {
    m_bRectRotated = rotated;
    setContentSize(untrimmedSize);
    setVertexRect(rect);
    setTextureCoords(rect);
    CCPoint relativeOffset = m_obUnflippedOffsetPositionFromCenter;
    if (m_bFlipX) relativeOffset.x = -relativeOffset.x;  // issue #732
    if (m_bFlipY) relativeOffset.y = -relativeOffset.y;
    m_obOffsetPosition.x = relativeOffset.x + (m_obContentSize.width - m_obRect.size.width) / 2;
    m_obOffsetPosition.y = relativeOffset.y + (m_obContentSize.height - m_obRect.size.height) / 2;
    // the quad's corners, in the sprite's own space
    float x1 = m_obOffsetPosition.x, y1 = m_obOffsetPosition.y;
    float x2 = x1 + m_obRect.size.width, y2 = y1 + m_obRect.size.height;
    m_sQuad.bl.vertices = vertex3(x1, y1, 0);
    m_sQuad.br.vertices = vertex3(x2, y1, 0);
    m_sQuad.tl.vertices = vertex3(x1, y2, 0);
    m_sQuad.tr.vertices = vertex3(x2, y2, 0);
}

int CCSprite::opaqueAt(const CCPoint& local) const {
    // where on its quad (0..1 from the bottom left), then in the sheet, as
    // setTextureCoords maps the quad's corners
    if (!m_pobTexture || m_obRect.size.width <= 0 || m_obRect.size.height <= 0) return 0;
    float u = (local.x - m_obOffsetPosition.x) / m_obRect.size.width;
    float v = (local.y - m_obOffsetPosition.y) / m_obRect.size.height;
    if (u < 0 || u > 1 || v < 0 || v > 1) return 0;
    if (m_bFlipX) u = 1 - u;
    if (m_bFlipY) v = 1 - v;
    const float s = m_pobTexture->getContentScale();
    float x, y;  // pixels, from the sheet's top left
    if (m_bRectRotated) {
        x = (m_obRect.origin.x + v * m_obRect.size.height) * s;
        y = (m_obRect.origin.y + u * m_obRect.size.width) * s;
    } else {
        x = (m_obRect.origin.x + u * m_obRect.size.width) * s;
        y = (m_obRect.origin.y + (1 - v) * m_obRect.size.height) * s;
    }
    const int a = m_pobTexture->alphaAt(x, y);
    return a < 0 ? 255 : a;
}

void CCSprite::setTextureCoords(CCRect rect) {
    // (the texture's own scale: the port's, CCTexture2D::getContentScale)
    float s = m_pobTexture ? m_pobTexture->getContentScale() : CCDirector::sharedDirector()->getContentScaleFactor();
    rect = CCRectMake(rect.origin.x * s, rect.origin.y * s, rect.size.width * s, rect.size.height * s);  // in pixels
    if (!m_pobTexture) return;
    float atlasWidth = static_cast<float>(m_pobTexture->getPixelsWide());
    float atlasHeight = static_cast<float>(m_pobTexture->getPixelsHigh());
    float left, right, top, bottom;
    if (m_bRectRotated) {
        // stored turned: its width runs down the sheet
        left = rect.origin.x / atlasWidth;
        right = (rect.origin.x + rect.size.height) / atlasWidth;
        top = rect.origin.y / atlasHeight;
        bottom = (rect.origin.y + rect.size.width) / atlasHeight;
        if (m_bFlipX) std::swap(top, bottom);
        if (m_bFlipY) std::swap(left, right);
        m_sQuad.bl.texCoords = {left, top};
        m_sQuad.br.texCoords = {left, bottom};
        m_sQuad.tl.texCoords = {right, top};
        m_sQuad.tr.texCoords = {right, bottom};
    } else {
        left = rect.origin.x / atlasWidth;
        right = (rect.origin.x + rect.size.width) / atlasWidth;
        top = rect.origin.y / atlasHeight;
        bottom = (rect.origin.y + rect.size.height) / atlasHeight;
        if (m_bFlipX) std::swap(left, right);
        if (m_bFlipY) std::swap(top, bottom);
        m_sQuad.bl.texCoords = {left, bottom};
        m_sQuad.br.texCoords = {right, bottom};
        m_sQuad.tl.texCoords = {left, top};
        m_sQuad.tr.texCoords = {right, top};
    }
}

void CCSprite::addChild(CCNode* child, int zOrder, int tag) {
    if (m_pobBatchNode) {
        // in a batch, a sprite's children are its sprites too (2.1.1 asserts so)
        if (CCSprite* s = dynamic_cast<CCSprite*>(child)) s->setBatchNode(m_pobBatchNode);
    }
    CCNode::addChild(child, zOrder, tag);
}

void CCSprite::draw() {
    if (m_obRect.size.width <= 0 && m_obRect.size.height <= 0) return;
    // the quad: bl, br, tl, tr
    float xy[8] = {m_sQuad.bl.vertices.x, m_sQuad.bl.vertices.y, m_sQuad.br.vertices.x, m_sQuad.br.vertices.y,
                   m_sQuad.tl.vertices.x, m_sQuad.tl.vertices.y, m_sQuad.tr.vertices.x, m_sQuad.tr.vertices.y};
    float uv[8] = {m_sQuad.bl.texCoords.u, m_sQuad.bl.texCoords.v, m_sQuad.br.texCoords.u, m_sQuad.br.texCoords.v,
                   m_sQuad.tl.texCoords.u, m_sQuad.tl.texCoords.v, m_sQuad.tr.texCoords.u, m_sQuad.tr.texCoords.v};
    const ccColor4B& c = m_sQuad.bl.colors;
    float rgba[4] = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
    platform::BlendDst dst(m_sBlendFunc.dst);
    platform::drawQuad(serial(), m_pobTexture ? m_pobTexture->getName() : 0, xy, m_pobTexture ? uv : nullptr, rgba,
                       m_sBlendFunc.src);
}

void CCSprite::setFlipX(bool flip) {
    if (m_bFlipX != flip) {
        m_bFlipX = flip;
        setTextureRect(m_obRect, m_bRectRotated, m_obContentSize);
    }
}

void CCSprite::setFlipY(bool flip) {
    if (m_bFlipY != flip) {
        m_bFlipY = flip;
        setTextureRect(m_obRect, m_bRectRotated, m_obContentSize);
    }
}

void CCSprite::updateColor() {
    ccColor4B c = {m_sColor.r, m_sColor.g, m_sColor.b, m_nOpacity};
    m_sQuad.bl.colors = m_sQuad.br.colors = m_sQuad.tl.colors = m_sQuad.tr.colors = c;
}

void CCSprite::setOpacity(GLubyte opacity) {
    m_nOpacity = opacity;
    if (m_bOpacityModifyRGB) setColor(m_sColorUnmodified);  // the colour premultiplied again
    updateColor();
}

void CCSprite::setColor(const ccColor3B& color) {
    m_sColorUnmodified = color;
    if (m_bOpacityModifyRGB) {
        m_sColor.r = static_cast<GLubyte>(color.r * m_nOpacity / 255.0f);
        m_sColor.g = static_cast<GLubyte>(color.g * m_nOpacity / 255.0f);
        m_sColor.b = static_cast<GLubyte>(color.b * m_nOpacity / 255.0f);
    } else {
        m_sColor = color;
    }
    updateColor();
}

void CCSprite::setOpacityModifyRGB(bool modify) {
    if (m_bOpacityModifyRGB != modify) {
        m_bOpacityModifyRGB = modify;
        setColor(m_sColorUnmodified);
    }
}

void CCSprite::setDisplayFrame(CCSpriteFrame* frame) {
    m_obUnflippedOffsetPositionFromCenter = frame->getOffset();
    CCTexture2D* t = frame->getTexture();
    if (t != m_pobTexture) setTexture(t);  // the texture before the rect
    m_bRectRotated = frame->isRotated();
    setTextureRect(frame->getRect(), m_bRectRotated, frame->getOriginalSize());
}

bool CCSprite::isFrameDisplayed(CCSpriteFrame* frame) {
    CCRect r = frame->getRect();
    return r.equals(m_obRect) && frame->getTexture()->getName() == m_pobTexture->getName() &&
           frame->getOffset().equals(m_obUnflippedOffsetPositionFromCenter);
}

CCSpriteFrame* CCSprite::displayFrame() {
    float s = m_pobTexture ? m_pobTexture->getContentScale() : CCDirector::sharedDirector()->getContentScaleFactor();
    CCRect px = CCRectMake(m_obRect.origin.x * s, m_obRect.origin.y * s, m_obRect.size.width * s, m_obRect.size.height * s);
    CCSize orig = CCSizeMake(m_obContentSize.width * s, m_obContentSize.height * s);
    CCPoint off = ccp(m_obUnflippedOffsetPositionFromCenter.x * s, m_obUnflippedOffsetPositionFromCenter.y * s);
    return CCSpriteFrame::createWithTexture(m_pobTexture, px, m_bRectRotated, off, orig);
}

// ---------------------------------------------------------- batch node

CCSpriteBatchNode* CCSpriteBatchNode::createWithTexture(CCTexture2D* texture, unsigned int capacity) {
    CCSpriteBatchNode* b = new CCSpriteBatchNode;
    return created(b, b->initWithTexture(texture, capacity));
}

CCSpriteBatchNode* CCSpriteBatchNode::create(const char* fileImage, unsigned int capacity) {
    CCSpriteBatchNode* b = new CCSpriteBatchNode;
    return created(b, b->initWithFile(fileImage, capacity));
}

bool CCSpriteBatchNode::initWithTexture(CCTexture2D* texture, unsigned int) {
    setTexture(texture);
    m_pChildren = new CCArray;
    return true;
}

bool CCSpriteBatchNode::initWithFile(const char* fileImage, unsigned int capacity) {
    CCTexture2D* t = CCTextureCache::sharedTextureCache()->addImage(fileImage);
    return initWithTexture(t, capacity);
}

void CCSpriteBatchNode::setTexture(CCTexture2D* texture) {
    CC_SAFE_RETAIN(texture);
    CC_SAFE_RELEASE(m_pobTexture);
    m_pobTexture = texture;
}

void CCSpriteBatchNode::addChild(CCNode* child, int zOrder, int tag) {
    if (CCSprite* s = dynamic_cast<CCSprite*>(child)) s->setBatchNode(this);
    CCNode::addChild(child, zOrder, tag);
}

void CCSpriteBatchNode::removeChild(CCNode* child, bool cleanup) {
    if (CCSprite* s = dynamic_cast<CCSprite*>(child)) s->setBatchNode(nullptr);
    CCNode::removeChild(child, cleanup);
}

void CCSpriteBatchNode::removeAllChildrenWithCleanup(bool cleanup) {
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data)
            if (CCSprite* s = dynamic_cast<CCSprite*>(o)) s->setBatchNode(nullptr);
    CCNode::removeAllChildrenWithCleanup(cleanup);
}

}  // namespace cocos2d
