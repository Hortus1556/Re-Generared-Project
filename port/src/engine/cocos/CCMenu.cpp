// cocos2d-x 2.1.1's CCMenu.cpp and CCMenuItem.cpp, ported.
// MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCMenu.h"

#include <climits>

#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace cocos2d {

enum { kNormalTag = 0x1, kSelectedTag = 0x2, kDisableTag = 0x3 };
static const unsigned int kCurrentItem = 0xc0c05001;

template <class T>
static T* created(T* n, bool ok) {
    if (ok) {
        n->autorelease();
        return n;
    }
    delete n;
    return nullptr;
}

// ------------------------------------------------------------ CCMenuItem

CCMenuItem* CCMenuItem::create() { return create(nullptr, nullptr); }
CCMenuItem* CCMenuItem::create(CCObject* rec, SEL_MenuHandler selector) {
    CCMenuItem* i = new CCMenuItem;
    return created(i, i->initWithTarget(rec, selector));
}

bool CCMenuItem::initWithTarget(CCObject* rec, SEL_MenuHandler selector) {
    setAnchorPoint(ccp(0.5f, 0.5f));
    m_pListener = rec;
    m_pfnSelector = selector;
    m_bIsEnabled = true;
    m_bIsSelected = false;
    return true;
}

void CCMenuItem::activate() {
    if (m_bIsEnabled && m_pListener && m_pfnSelector) (m_pListener->*m_pfnSelector)(this);
}

CCRect CCMenuItem::rect() {
    return CCRectMake(m_obPosition.x - m_obContentSize.width * m_obAnchorPoint.x,
                      m_obPosition.y - m_obContentSize.height * m_obAnchorPoint.y, m_obContentSize.width,
                      m_obContentSize.height);
}

// ------------------------------------------------------ CCMenuItemSprite

CCMenuItemSprite* CCMenuItemSprite::create(CCNode* normal, CCNode* selected, CCNode* disabled) {
    return create(normal, selected, disabled, nullptr, nullptr);
}
CCMenuItemSprite* CCMenuItemSprite::create(CCNode* normal, CCNode* selected, CCObject* target, SEL_MenuHandler selector) {
    return create(normal, selected, nullptr, target, selector);
}
CCMenuItemSprite* CCMenuItemSprite::create(CCNode* normal, CCNode* selected, CCNode* disabled, CCObject* target,
                                           SEL_MenuHandler selector) {
    CCMenuItemSprite* i = new CCMenuItemSprite;
    return created(i, i->initWithNormalSprite(normal, selected, disabled, target, selector));
}

bool CCMenuItemSprite::initWithNormalSprite(CCNode* normal, CCNode* selected, CCNode* disabled, CCObject* target,
                                            SEL_MenuHandler selector) {
    CCMenuItem::initWithTarget(target, selector);
    setNormalImage(normal);
    setSelectedImage(selected);
    setDisabledImage(disabled);
    if (m_pNormalImage) setContentSize(m_pNormalImage->getContentSize());
    return true;
}

void CCMenuItemSprite::setNormalImage(CCNode* image) {
    if (image == m_pNormalImage) return;
    if (image) {
        addChild(image, 0, kNormalTag);
        image->setAnchorPoint(ccp(0, 0));
    }
    if (m_pNormalImage) removeChild(m_pNormalImage, true);
    m_pNormalImage = image;
    if (m_pNormalImage) setContentSize(m_pNormalImage->getContentSize());  // 2.1.1 assumes one
    updateImagesVisibility();
}

void CCMenuItemSprite::setSelectedImage(CCNode* image) {
    if (image == m_pNormalImage) return;  // (2.1.1 compares with the normal image)
    if (image) {
        addChild(image, 0, kSelectedTag);
        image->setAnchorPoint(ccp(0, 0));
    }
    if (m_pSelectedImage) removeChild(m_pSelectedImage, true);
    m_pSelectedImage = image;
    updateImagesVisibility();
}

void CCMenuItemSprite::setDisabledImage(CCNode* image) {
    if (image == m_pNormalImage) return;
    if (image) {
        addChild(image, 0, kDisableTag);
        image->setAnchorPoint(ccp(0, 0));
    }
    if (m_pDisabledImage) removeChild(m_pDisabledImage, true);
    m_pDisabledImage = image;
    updateImagesVisibility();
}

void CCMenuItemSprite::selected() {
    CCMenuItem::selected();
    if (!m_pNormalImage) return;
    if (m_pDisabledImage) m_pDisabledImage->setVisible(false);
    if (m_pSelectedImage) {
        m_pNormalImage->setVisible(false);
        m_pSelectedImage->setVisible(true);
    } else {
        m_pNormalImage->setVisible(true);
    }
}

void CCMenuItemSprite::unselected() {
    CCMenuItem::unselected();
    if (!m_pNormalImage) return;
    m_pNormalImage->setVisible(true);
    if (m_pSelectedImage) m_pSelectedImage->setVisible(false);
    if (m_pDisabledImage) m_pDisabledImage->setVisible(false);
}

void CCMenuItemSprite::setEnabled(bool enabled) {
    if (m_bIsEnabled != enabled) {
        CCMenuItem::setEnabled(enabled);
        updateImagesVisibility();
    }
}

void CCMenuItemSprite::updateImagesVisibility() {
    bool showDisabled = !m_bIsEnabled && m_pDisabledImage;
    if (m_pNormalImage) m_pNormalImage->setVisible(!showDisabled);
    if (m_pSelectedImage) m_pSelectedImage->setVisible(false);
    if (m_pDisabledImage) m_pDisabledImage->setVisible(showDisabled);
}

static CCRGBAProtocol* rgba(CCNode* n) { return dynamic_cast<CCRGBAProtocol*>(n); }

void CCMenuItemSprite::setColor(const ccColor3B& color) {
    for (CCNode* n : {m_pNormalImage, m_pSelectedImage, m_pDisabledImage})
        if (CCRGBAProtocol* p = rgba(n)) p->setColor(color);
}
const ccColor3B& CCMenuItemSprite::getColor() {
    static ccColor3B white = ccWHITE;
    CCRGBAProtocol* p = rgba(m_pNormalImage);
    return p ? p->getColor() : white;
}
void CCMenuItemSprite::setOpacity(GLubyte opacity) {
    for (CCNode* n : {m_pNormalImage, m_pSelectedImage, m_pDisabledImage})
        if (CCRGBAProtocol* p = rgba(n)) p->setOpacity(opacity);
}
GLubyte CCMenuItemSprite::getOpacity() {
    CCRGBAProtocol* p = rgba(m_pNormalImage);
    return p ? p->getOpacity() : 255;
}

// ------------------------------------------------------- CCMenuItemImage

CCMenuItemImage* CCMenuItemImage::create() {
    CCMenuItemImage* i = new CCMenuItemImage;
    return created(i, i->init());
}
CCMenuItemImage* CCMenuItemImage::create(const char* normal, const char* selected, const char* disabled) {
    return create(normal, selected, disabled, nullptr, nullptr);
}
CCMenuItemImage* CCMenuItemImage::create(const char* normal, const char* selected, const char* disabled,
                                         CCObject* target, SEL_MenuHandler selector) {
    CCMenuItemImage* i = new CCMenuItemImage;
    return created(i, i->initWithNormalImage(normal, selected, disabled, target, selector));
}

bool CCMenuItemImage::init() { return initWithNormalImage(nullptr, nullptr, nullptr, nullptr, nullptr); }

bool CCMenuItemImage::initWithNormalImage(const char* normal, const char* selected, const char* disabled,
                                          CCObject* target, SEL_MenuHandler selector) {
    CCNode* n = normal ? CCSprite::create(normal) : nullptr;
    CCNode* s = selected ? CCSprite::create(selected) : nullptr;
    CCNode* d = disabled ? CCSprite::create(disabled) : nullptr;
    return initWithNormalSprite(n, s, d, target, selector);
}

void CCMenuItemImage::setNormalSpriteFrame(CCSpriteFrame* frame) { setNormalImage(CCSprite::createWithSpriteFrame(frame)); }
void CCMenuItemImage::setSelectedSpriteFrame(CCSpriteFrame* frame) { setSelectedImage(CCSprite::createWithSpriteFrame(frame)); }
void CCMenuItemImage::setDisabledSpriteFrame(CCSpriteFrame* frame) { setDisabledImage(CCSprite::createWithSpriteFrame(frame)); }

// ------------------------------------------------------ CCMenuItemToggle

CCMenuItemToggle::~CCMenuItemToggle() { CC_SAFE_RELEASE(m_pSubItems); }

CCMenuItemToggle* CCMenuItemToggle::createWithTarget(CCObject* target, SEL_MenuHandler selector, CCMenuItem* item, ...) {
    va_list args;
    va_start(args, item);
    CCMenuItemToggle* t = new CCMenuItemToggle;
    t->initWithTarget(target, selector, item, args);
    t->autorelease();
    va_end(args);
    return t;
}

CCMenuItemToggle* CCMenuItemToggle::create(CCMenuItem* item) {
    CCMenuItemToggle* t = new CCMenuItemToggle;
    return created(t, t->initWithItem(item));
}

bool CCMenuItemToggle::initWithTarget(CCObject* target, SEL_MenuHandler selector, CCMenuItem* item, va_list args) {
    CCMenuItem::initWithTarget(target, selector);
    m_pSubItems = CCArray::create();
    m_pSubItems->retain();
    for (CCMenuItem* i = item; i; i = va_arg(args, CCMenuItem*)) m_pSubItems->addObject(i);
    m_uSelectedIndex = UINT_MAX;
    setSelectedIndex(0);
    return true;
}

bool CCMenuItemToggle::initWithItem(CCMenuItem* item) {
    CCMenuItem::initWithTarget(nullptr, nullptr);
    m_pSubItems = CCArray::create();
    m_pSubItems->retain();
    if (item) m_pSubItems->addObject(item);
    m_uSelectedIndex = UINT_MAX;
    setSelectedIndex(0);
    return true;
}

void CCMenuItemToggle::setSelectedIndex(unsigned int index) {
    if (index == m_uSelectedIndex || m_pSubItems->count() == 0) return;
    m_uSelectedIndex = index;
    if (CCNode* current = getChildByTag(static_cast<int>(kCurrentItem))) current->removeFromParentAndCleanup(false);
    CCMenuItem* item = static_cast<CCMenuItem*>(m_pSubItems->objectAtIndex(m_uSelectedIndex));
    addChild(item, 0, static_cast<int>(kCurrentItem));
    CCSize s = item->getContentSize();
    setContentSize(s);
    item->setPosition(ccp(s.width / 2, s.height / 2));
}

void CCMenuItemToggle::activate() {
    if (m_bIsEnabled) setSelectedIndex((m_uSelectedIndex + 1) % m_pSubItems->count());
    CCMenuItem::activate();
}

void CCMenuItemToggle::selected() {
    CCMenuItem::selected();
    selectedItem()->selected();
}

void CCMenuItemToggle::unselected() {
    CCMenuItem::unselected();
    selectedItem()->unselected();
}

void CCMenuItemToggle::setEnabled(bool enabled) {
    if (m_bIsEnabled == enabled) return;
    CCMenuItem::setEnabled(enabled);
    if (m_pSubItems)
        for (CCObject* o : m_pSubItems->data) static_cast<CCMenuItem*>(o)->setEnabled(enabled);
}

void CCMenuItemToggle::setColor(const ccColor3B& color) {
    m_tColor = color;
    if (m_pSubItems)
        for (CCObject* o : m_pSubItems->data)
            if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(o)) p->setColor(color);
}

void CCMenuItemToggle::setOpacity(GLubyte opacity) {
    m_cOpacity = opacity;
    if (m_pSubItems)
        for (CCObject* o : m_pSubItems->data)
            if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(o)) p->setOpacity(opacity);
}

// ----------------------------------------------------------------- CCMenu

CCMenu* CCMenu::create() { return createWithArray(nullptr); }

CCMenu* CCMenu::create(CCMenuItem* item, ...) {
    va_list args;
    va_start(args, item);
    CCArray* items = CCArray::create();
    for (CCMenuItem* i = item; i; i = va_arg(args, CCMenuItem*)) items->addObject(i);
    va_end(args);
    return createWithArray(items);
}

CCMenu* CCMenu::createWithArray(CCArray* items) {
    CCMenu* m = new CCMenu;
    return created(m, m->initWithArray(items));
}

CCMenu* CCMenu::createWithItem(CCMenuItem* item) { return create(item, nullptr); }

bool CCMenu::init() { return initWithArray(nullptr); }

bool CCMenu::initWithArray(CCArray* items) {
    if (!CCLayer::init()) return false;
    setTouchPriority(kCCMenuHandlerPriority);
    setTouchMode(kCCTouchesOneByOne);
    setTouchEnabled(true);
    m_bEnabled = true;
    // the menu in the screen's middle
    CCSize s = CCDirector::sharedDirector()->getWinSize();
    ignoreAnchorPointForPosition(true);
    setAnchorPoint(ccp(0.5f, 0.5f));
    setContentSize(s);
    setPosition(ccp(s.width / 2, s.height / 2));
    if (items) {
        int z = 0;
        for (CCObject* o : items->data) addChild(static_cast<CCMenuItem*>(o), z++);
    }
    m_pSelectedItem = nullptr;
    m_eState = kCCMenuStateWaiting;
    return true;
}

void CCMenu::addChild(CCNode* child, int zOrder, int tag) { CCLayer::addChild(child, zOrder, tag); }

// Fixed: 2.1.1 has this override and the port had left it out, so an
// item taken off (and freed) while pressed was still m_pSelectedItem, and
// the touch's end called it.
void CCMenu::removeChild(CCNode* child, bool cleanup) {
    if (m_pSelectedItem && m_pSelectedItem == dynamic_cast<CCMenuItem*>(child)) m_pSelectedItem = nullptr;
    CCLayer::removeChild(child, cleanup);
}

void CCMenu::registerWithTouchDispatcher() {
    CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, getTouchPriority(), true);
}

void CCMenu::onExit() {
    if (m_eState == kCCMenuStateTrackingTouch) {
        if (m_pSelectedItem) {
            m_pSelectedItem->unselected();
            m_pSelectedItem = nullptr;
        }
        m_eState = kCCMenuStateWaiting;
    }
    CCLayer::onExit();
}

void CCMenu::setHandlerPriority(int newPriority) {
    CCDirector::sharedDirector()->getTouchDispatcher()->setPriority(newPriority, this);
}

bool CCMenu::ccTouchBegan(CCTouch* touch, CCEvent*) {
    if (m_eState != kCCMenuStateWaiting || !m_bVisible || !m_bEnabled) return false;
    for (CCNode* c = m_pParent; c; c = c->getParent())
        if (!c->isVisible()) return false;
    m_pSelectedItem = itemForTouch(touch);
    if (m_pSelectedItem) {
        m_eState = kCCMenuStateTrackingTouch;
        m_pSelectedItem->selected();
        return true;
    }
    return false;
}

void CCMenu::ccTouchEnded(CCTouch*, CCEvent*) {
    if (m_pSelectedItem) {
        m_pSelectedItem->unselected();
        m_pSelectedItem->activate();
    }
    m_eState = kCCMenuStateWaiting;
}

void CCMenu::ccTouchCancelled(CCTouch*, CCEvent*) {
    if (m_pSelectedItem) m_pSelectedItem->unselected();
    m_eState = kCCMenuStateWaiting;
}

void CCMenu::ccTouchMoved(CCTouch* touch, CCEvent*) {
    CCMenuItem* currentItem = itemForTouch(touch);
    if (currentItem != m_pSelectedItem) {
        if (m_pSelectedItem) m_pSelectedItem->unselected();
        m_pSelectedItem = currentItem;
        if (m_pSelectedItem) m_pSelectedItem->selected();
    }
}

CCMenuItem* CCMenu::itemForTouch(CCTouch* touch) {
    CCPoint touchLocation = touch->getLocation();
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data) {
            CCMenuItem* child = dynamic_cast<CCMenuItem*>(o);
            if (child && child->isVisible() && child->isEnabled()) {
                CCPoint local = child->convertToNodeSpace(touchLocation);
                CCRect r = child->rect();
                r.origin = CCPointZero;
                if (child->getTouchRect().size.width > 0) r = child->getTouchRect();
                if (r.containsPoint(local)) return child;
            }
        }
    return nullptr;
}

void CCMenu::setColor(const ccColor3B& color) {
    m_tColor = color;
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data)
            if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(o)) p->setColor(color);
}

void CCMenu::setOpacity(GLubyte opacity) {
    m_cOpacity = opacity;
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data)
            if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(o)) p->setOpacity(opacity);
}

}  // namespace cocos2d
