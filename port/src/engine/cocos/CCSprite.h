// cocos2d-x 2.1.1's CCSprite and CCSpriteBatchNode, ported to Bleed Engine.
// MIT: LICENSE-cocos2d-x.txt.
//
// A sprite is a textured quad: a rectangle of its texture (a sprite frame:
// trimmed, maybe stored rotated), its content size the frame's original
// size, flipped, tinted and faded. It draws itself through the engine's
// recorded drawing (platform::drawQuad, its serial its owner: what moves
// between passes is smoothed), with its blend function.
// CCSpriteBatchNode: in 2.1.1 its sprites' quads go into one texture atlas
// drawn at once; here each sprite draws itself, in the order the atlas held
// them (the batch's children in z order, depth first), which looks the same
// (the engine batches its draws by itself).
#pragma once

#include <string>

#include "engine/cocos/CCNode.h"
#include "engine/cocos/CCTexture2D.h"

namespace cocos2d {

class CCSpriteFrame;
class CCSpriteBatchNode;

class CCSprite : public CCNode, public CCRGBAProtocol, public CCBlendProtocol {
public:
    static CCSprite* create();
    static CCSprite* create(const char* fileName);
    static CCSprite* create(const char* fileName, const CCRect& rect);
    static CCSprite* createWithTexture(CCTexture2D* texture);
    static CCSprite* createWithTexture(CCTexture2D* texture, const CCRect& rect);
    static CCSprite* createWithSpriteFrame(CCSpriteFrame* frame);
    static CCSprite* createWithSpriteFrameName(const char* frameName);
    ~CCSprite() override;

    bool init() override;
    virtual bool initWithTexture(CCTexture2D* texture);
    virtual bool initWithTexture(CCTexture2D* texture, const CCRect& rect);
    virtual bool initWithTexture(CCTexture2D* texture, const CCRect& rect, bool rotated);
    virtual bool initWithSpriteFrame(CCSpriteFrame* frame);
    virtual bool initWithSpriteFrameName(const char* frameName);
    virtual bool initWithFile(const char* fileName);
    virtual bool initWithFile(const char* fileName, const CCRect& rect);

    void draw() override;
    void addChild(CCNode* child) override { CCNode::addChild(child); }
    void addChild(CCNode* child, int zOrder) override { CCNode::addChild(child, zOrder); }
    void addChild(CCNode* child, int zOrder, int tag) override;
    void setDirty(bool d) { m_bDirty = d; }
    bool isDirty() { return m_bDirty; }

    virtual void setTexture(CCTexture2D* texture);
    virtual CCTexture2D* getTexture() { return m_pobTexture; }
    virtual void setTextureRect(const CCRect& rect);
    virtual void setTextureRect(const CCRect& rect, bool rotated, const CCSize& untrimmedSize);
    virtual void setVertexRect(const CCRect& rect) { m_obRect = rect; }
    virtual void setDisplayFrame(CCSpriteFrame* frame);
    virtual bool isFrameDisplayed(CCSpriteFrame* frame);
    virtual CCSpriteFrame* displayFrame();
    const CCRect& getTextureRect() { return m_obRect; }
    bool isTextureRectRotated() { return m_bRectRotated; }
    const CCPoint& getOffsetPosition() { return m_obOffsetPosition; }
    // (the port's) Its texture's alpha (CCTexture2D::alphaAt) where it is
    // drawn at `local`, a point of its own space (points, from its bottom
    // left): 0 off its picture; 255 on it when the texture keeps no alpha.
    int opaqueAt(const CCPoint& local) const;

    void setFlipX(bool flip);
    bool isFlipX() { return m_bFlipX; }
    void setFlipY(bool flip);
    bool isFlipY() { return m_bFlipY; }

    // CCRGBAProtocol
    void setColor(const ccColor3B& color) override;
    const ccColor3B& getColor() override { return m_sColorUnmodified; }
    GLubyte getOpacity() override { return m_nOpacity; }
    void setOpacity(GLubyte opacity) override;
    void setOpacityModifyRGB(bool modify) override;
    bool isOpacityModifyRGB() override { return m_bOpacityModifyRGB; }
    // CCBlendProtocol
    void setBlendFunc(ccBlendFunc f) override { m_sBlendFunc = f; }
    ccBlendFunc getBlendFunc() override { return m_sBlendFunc; }

    // batch node bookkeeping (no atlas here: kept for the API)
    virtual void setBatchNode(CCSpriteBatchNode* batch) { m_pobBatchNode = batch; }
    virtual CCSpriteBatchNode* getBatchNode() { return m_pobBatchNode; }
    const ccV3F_C4B_T2F_Quad& getQuad() { return m_sQuad; }

protected:
    void setTextureCoords(CCRect rect);
    virtual void updateBlendFunc();
    virtual void updateColor();

    CCTexture2D* m_pobTexture = nullptr;
    CCSpriteBatchNode* m_pobBatchNode = nullptr;
    bool m_bDirty = false;
    ccBlendFunc m_sBlendFunc = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};
    CCRect m_obRect;
    bool m_bRectRotated = false;
    CCPoint m_obOffsetPosition;
    CCPoint m_obUnflippedOffsetPositionFromCenter;
    ccV3F_C4B_T2F_Quad m_sQuad = {};
    bool m_bOpacityModifyRGB = false;
    bool m_bFlipX = false, m_bFlipY = false;
    GLubyte m_nOpacity = 255;
    ccColor3B m_sColor = ccWHITE, m_sColorUnmodified = ccWHITE;
};

class CCSpriteBatchNode : public CCNode, public CCBlendProtocol {
public:
    static CCSpriteBatchNode* createWithTexture(CCTexture2D* texture, unsigned int capacity = 29);
    static CCSpriteBatchNode* create(const char* fileImage, unsigned int capacity = 29);
    ~CCSpriteBatchNode() override { CC_SAFE_RELEASE(m_pobTexture); }
    bool initWithTexture(CCTexture2D* texture, unsigned int capacity);
    bool initWithFile(const char* fileImage, unsigned int capacity);
    CCTexture2D* getTexture() { return m_pobTexture; }
    void setTexture(CCTexture2D* texture);
    void addChild(CCNode* child) override { CCNode::addChild(child); }
    void addChild(CCNode* child, int zOrder) override { CCNode::addChild(child, zOrder); }
    void addChild(CCNode* child, int zOrder, int tag) override;
    void removeChild(CCNode* child, bool cleanup) override;
    void removeAllChildrenWithCleanup(bool cleanup) override;
    void setBlendFunc(ccBlendFunc f) override { m_blendFunc = f; }
    ccBlendFunc getBlendFunc() override { return m_blendFunc; }

protected:
    CCTexture2D* m_pobTexture = nullptr;
    ccBlendFunc m_blendFunc = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};
};

}  // namespace cocos2d
