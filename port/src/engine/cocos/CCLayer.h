// cocos2d-x 2.1.1's scenes and layers (layers_scenes_transitions_nodes/CCScene,
// CCLayer: CCLayer, CCLayerColor, CCLayerGradient), and the delegates a node
// can register with (accelerometer, keypad), ported to Bleed Engine.
// MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include "engine/cocos/CCNode.h"
#include "engine/cocos/CCTouch.h"

namespace cocos2d {

class CCScene : public CCNode {
public:
    CCScene();
    static CCScene* create();
    bool init() override;
};

// CCAcceleration: the device's acceleration in gravities, in the screen's
// axes as the game's orientation sees them (platform::deviceAcceleration).
struct CCAcceleration {
    double x = 0, y = 0, z = 0;
    double timestamp = 0;
};
class CCAccelerometerDelegate {
public:
    virtual ~CCAccelerometerDelegate() = default;
    virtual void didAccelerate(CCAcceleration* acceleration) {}
};

// The keypad (Android's back and menu keys; on a PC, Escape is back).
class CCKeypadDelegate {
public:
    virtual ~CCKeypadDelegate() = default;
    virtual void keyBackClicked() {}
    virtual void keyMenuClicked() {}
};

enum ccTouchesMode { kCCTouchesAllAtOnce, kCCTouchesOneByOne };

class CCLayer : public CCNode, public CCTouchDelegate, public CCAccelerometerDelegate, public CCKeypadDelegate {
public:
    CCLayer();
    static CCLayer* create();
    bool init() override;
    void onEnter() override;
    void onExit() override;
    void onEnterTransitionDidFinish() override;
    virtual void registerWithTouchDispatcher();
    virtual bool isTouchEnabled() { return m_bTouchEnabled; }
    virtual void setTouchEnabled(bool enabled);
    virtual void setTouchMode(ccTouchesMode mode);
    virtual int getTouchMode() { return m_eTouchMode; }
    virtual void setTouchPriority(int priority);
    virtual int getTouchPriority() { return m_nTouchPriority; }
    virtual bool isAccelerometerEnabled() { return m_bAccelerometerEnabled; }
    virtual void setAccelerometerEnabled(bool enabled);
    virtual bool isKeypadEnabled() { return m_bKeypadEnabled; }
    virtual void setKeypadEnabled(bool enabled);
    bool ccTouchBegan(CCTouch*, CCEvent*) override { return true; }  // 2.1.1 asserts "override me"

protected:
    bool m_bTouchEnabled = false;
    bool m_bAccelerometerEnabled = false;
    bool m_bKeypadEnabled = false;
    ccTouchesMode m_eTouchMode = kCCTouchesAllAtOnce;
    int m_nTouchPriority = 0;
};

class CCLayerColor : public CCLayer, public CCRGBAProtocol, public CCBlendProtocol {
public:
    static CCLayerColor* create();
    static CCLayerColor* create(const ccColor4B& color);
    static CCLayerColor* create(const ccColor4B& color, GLfloat width, GLfloat height);
    bool init() override;
    virtual bool initWithColor(const ccColor4B& color);
    virtual bool initWithColor(const ccColor4B& color, GLfloat width, GLfloat height);
    void draw() override;
    void setContentSize(const CCSize& size) override;
    void changeWidthAndHeight(GLfloat w, GLfloat h) { setContentSize(CCSizeMake(w, h)); }
    void changeWidth(GLfloat w) { setContentSize(CCSizeMake(w, m_obContentSize.height)); }
    void changeHeight(GLfloat h) { setContentSize(CCSizeMake(m_obContentSize.width, h)); }

    void setColor(const ccColor3B& color) override { m_tColor = color; updateColor(); }
    const ccColor3B& getColor() override { return m_tColor; }
    GLubyte getOpacity() override { return m_cOpacity; }
    void setOpacity(GLubyte o) override { m_cOpacity = o; updateColor(); }
    void setOpacityModifyRGB(bool) override {}
    bool isOpacityModifyRGB() override { return false; }
    void setBlendFunc(ccBlendFunc f) override { m_tBlendFunc = f; }
    ccBlendFunc getBlendFunc() override { return m_tBlendFunc; }

protected:
    virtual void updateColor();
    ccColor3B m_tColor = ccWHITE;
    GLubyte m_cOpacity = 255;
    ccBlendFunc m_tBlendFunc = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};
    ccVertex2F m_pSquareVertices[4] = {};
    ccColor4F m_pSquareColors[4] = {};
};

class CCLayerGradient : public CCLayerColor {
public:
    static CCLayerGradient* create(const ccColor4B& start, const ccColor4B& end);
    static CCLayerGradient* create(const ccColor4B& start, const ccColor4B& end, const CCPoint& v);
    using CCLayerColor::initWithColor;
    bool initWithColor(const ccColor4B& start, const ccColor4B& end);
    bool initWithColor(const ccColor4B& start, const ccColor4B& end, const CCPoint& v);
    void setStartColor(const ccColor3B& c) { setColor(c); }
    const ccColor3B& getStartColor() { return m_tColor; }
    void setEndColor(const ccColor3B& c) { m_endColor = c; updateColor(); }
    const ccColor3B& getEndColor() { return m_endColor; }
    void setStartOpacity(GLubyte o) { m_cStartOpacity = o; updateColor(); }
    GLubyte getStartOpacity() { return m_cStartOpacity; }
    void setEndOpacity(GLubyte o) { m_cEndOpacity = o; updateColor(); }
    GLubyte getEndOpacity() { return m_cEndOpacity; }
    void setVector(const CCPoint& v) { m_AlongVector = v; updateColor(); }
    const CCPoint& getVector() { return m_AlongVector; }
    void setCompressedInterpolation(bool c) { m_bCompressedInterpolation = c; updateColor(); }
    bool isCompressedInterpolation() { return m_bCompressedInterpolation; }

protected:
    void updateColor() override;
    ccColor3B m_endColor = ccBLACK;
    GLubyte m_cStartOpacity = 255, m_cEndOpacity = 255;
    CCPoint m_AlongVector = ccp(0, -1);
    bool m_bCompressedInterpolation = true;
};

}  // namespace cocos2d
