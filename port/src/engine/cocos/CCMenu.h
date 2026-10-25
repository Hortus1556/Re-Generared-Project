// cocos2d-x 2.1.1's menus (menu_nodes/CCMenu, CCMenuItem: CCMenuItem,
// CCMenuItemSprite, CCMenuItemImage, CCMenuItemToggle), ported to Bleed
// Engine. MIT: LICENSE-cocos2d-x.txt.
//
// A menu is a layer of items that takes touches (targeted, priority -128,
// swallowing): the item under a press is selected while the touch stays on
// it, and activated (its target's selector called) when it is let go there.
// A menu, or any of its parents, hidden takes none.
#pragma once

#include <cstdarg>

#include "engine/cocos/CCLayer.h"

namespace cocos2d {

class CCSpriteFrame;

enum { kCCMenuHandlerPriority = -128 };
enum tCCMenuState { kCCMenuStateWaiting, kCCMenuStateTrackingTouch };

class CCMenuItem : public CCNode {
public:
    static CCMenuItem* create();
    static CCMenuItem* create(CCObject* rec, SEL_MenuHandler selector);
    bool initWithTarget(CCObject* rec, SEL_MenuHandler selector);
    CCRect rect();
    // (the port's) where a touch takes the item, in its own coordinates: a
    // part of its picture (a card without the art sticking out of it); a
    // zero size (the default) is the whole item, as in 2.1.1
    void setTouchRect(const CCRect& r) { m_touchRect = r; }
    const CCRect& getTouchRect() const { return m_touchRect; }
    virtual void activate();
    virtual void selected() { m_bIsSelected = true; }
    virtual void unselected() { m_bIsSelected = false; }
    virtual bool isEnabled() { return m_bIsEnabled; }
    virtual void setEnabled(bool value) { m_bIsEnabled = value; }
    virtual bool isSelected() { return m_bIsSelected; }
    void setTarget(CCObject* rec, SEL_MenuHandler selector) {
        m_pListener = rec;
        m_pfnSelector = selector;
    }

protected:
    bool m_bIsSelected = false;
    bool m_bIsEnabled = false;
    CCObject* m_pListener = nullptr;
    SEL_MenuHandler m_pfnSelector = nullptr;
    CCRect m_touchRect;
};

class CCMenuItemSprite : public CCMenuItem, public CCRGBAProtocol {
public:
    static CCMenuItemSprite* create(CCNode* normal, CCNode* selected, CCNode* disabled = nullptr);
    static CCMenuItemSprite* create(CCNode* normal, CCNode* selected, CCObject* target, SEL_MenuHandler selector);
    static CCMenuItemSprite* create(CCNode* normal, CCNode* selected, CCNode* disabled, CCObject* target,
                                    SEL_MenuHandler selector);
    bool initWithNormalSprite(CCNode* normal, CCNode* selected, CCNode* disabled, CCObject* target,
                              SEL_MenuHandler selector);
    virtual CCNode* getNormalImage() { return m_pNormalImage; }
    virtual void setNormalImage(CCNode* image);
    virtual CCNode* getSelectedImage() { return m_pSelectedImage; }
    virtual void setSelectedImage(CCNode* image);
    virtual CCNode* getDisabledImage() { return m_pDisabledImage; }
    virtual void setDisabledImage(CCNode* image);
    void selected() override;
    void unselected() override;
    void setEnabled(bool enabled) override;

    // CCRGBAProtocol: given to the images
    void setColor(const ccColor3B& color) override;
    const ccColor3B& getColor() override;
    void setOpacity(GLubyte opacity) override;
    GLubyte getOpacity() override;
    void setOpacityModifyRGB(bool) override {}
    bool isOpacityModifyRGB() override { return false; }

protected:
    virtual void updateImagesVisibility();
    CCNode* m_pNormalImage = nullptr;
    CCNode* m_pSelectedImage = nullptr;
    CCNode* m_pDisabledImage = nullptr;
};

class CCMenuItemImage : public CCMenuItemSprite {
public:
    static CCMenuItemImage* create();
    static CCMenuItemImage* create(const char* normal, const char* selected, const char* disabled = nullptr);
    static CCMenuItemImage* create(const char* normal, const char* selected, const char* disabled, CCObject* target,
                                   SEL_MenuHandler selector);
    bool init() override;
    bool initWithNormalImage(const char* normal, const char* selected, const char* disabled, CCObject* target,
                             SEL_MenuHandler selector);
    void setNormalSpriteFrame(CCSpriteFrame* frame);
    void setSelectedSpriteFrame(CCSpriteFrame* frame);
    void setDisabledSpriteFrame(CCSpriteFrame* frame);
};

class CCMenuItemToggle : public CCMenuItem, public CCRGBAProtocol {
public:
    ~CCMenuItemToggle() override;
    // the items end with NULL
    static CCMenuItemToggle* createWithTarget(CCObject* target, SEL_MenuHandler selector, CCMenuItem* item, ...);
    static CCMenuItemToggle* create(CCMenuItem* item);
    bool initWithTarget(CCObject* target, SEL_MenuHandler selector, CCMenuItem* item, va_list args);
    bool initWithItem(CCMenuItem* item);
    void addSubItem(CCMenuItem* item) { m_pSubItems->addObject(item); }
    CCMenuItem* selectedItem() { return static_cast<CCMenuItem*>(m_pSubItems->objectAtIndex(m_uSelectedIndex)); }
    unsigned int getSelectedIndex() { return m_uSelectedIndex; }
    void setSelectedIndex(unsigned int index);
    CCArray* getSubItems() { return m_pSubItems; }
    void activate() override;
    void selected() override;
    void unselected() override;
    void setEnabled(bool enabled) override;

    void setColor(const ccColor3B& color) override;
    const ccColor3B& getColor() override { return m_tColor; }
    void setOpacity(GLubyte opacity) override;
    GLubyte getOpacity() override { return m_cOpacity; }
    void setOpacityModifyRGB(bool) override {}
    bool isOpacityModifyRGB() override { return false; }

private:
    unsigned int m_uSelectedIndex = 0;
    CCArray* m_pSubItems = nullptr;
    GLubyte m_cOpacity = 255;
    ccColor3B m_tColor = ccWHITE;
};

class CCMenu : public CCLayer, public CCRGBAProtocol {
public:
    static CCMenu* create();
    static CCMenu* create(CCMenuItem* item, ...);  // ends with NULL
    static CCMenu* createWithArray(CCArray* items);
    static CCMenu* createWithItem(CCMenuItem* item);
    bool init() override;
    bool initWithArray(CCArray* items);
    void addChild(CCNode* child) override { CCLayer::addChild(child); }
    void addChild(CCNode* child, int zOrder) override { CCLayer::addChild(child, zOrder); }
    void addChild(CCNode* child, int zOrder, int tag) override;
    // the pressed item forgotten when it is taken off (2.1.1's override)
    void removeChild(CCNode* child) override { removeChild(child, true); }
    void removeChild(CCNode* child, bool cleanup) override;
    // (the same for all of them: not in 2.1.1, which kept the freed item)
    void removeAllChildrenWithCleanup(bool cleanup) override {
        m_pSelectedItem = nullptr;
        CCLayer::removeAllChildrenWithCleanup(cleanup);
    }
    void registerWithTouchDispatcher() override;
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override;
    void ccTouchEnded(CCTouch* touch, CCEvent* event) override;
    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override;
    void ccTouchMoved(CCTouch* touch, CCEvent* event) override;
    void onExit() override;
    void setHandlerPriority(int newPriority);
    virtual bool isEnabled() { return m_bEnabled; }
    virtual void setEnabled(bool value) { m_bEnabled = value; }

    void setColor(const ccColor3B& color) override;
    const ccColor3B& getColor() override { return m_tColor; }
    void setOpacity(GLubyte opacity) override;
    GLubyte getOpacity() override { return m_cOpacity; }
    void setOpacityModifyRGB(bool) override {}
    bool isOpacityModifyRGB() override { return false; }

protected:
    CCMenuItem* itemForTouch(CCTouch* touch);
    bool m_bEnabled = false;
    tCCMenuState m_eState = kCCMenuStateWaiting;
    CCMenuItem* m_pSelectedItem = nullptr;
    GLubyte m_cOpacity = 255;
    ccColor3B m_tColor = ccWHITE;
};

}  // namespace cocos2d
