#include "as3/HorizontalScrollLayer.h"

#include <cmath>
#include <cstdlib>

#include "as3/ResolutionMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCDirector.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr float kCanvasWidth = 640, kCanvasHeight = 384;  // 0x11d490, 0x11d48c
constexpr float kFastSwipe = 200;   // canvas points between two moves (0x11d610)
constexpr int kTapSlop = 5;         // points the touch may move and still select
constexpr float kMoveSeconds = 0.3f;

// a point of the screen (cocos points) on the canvas
CCPoint onCanvas(const CCPoint& p) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float s = r->canvasScale;
    return ccp(p.x / s + (kCanvasWidth - r->width / s) * 0.5f, p.y / s + (kCanvasHeight - r->height / s) * 0.5f);
}

}  // namespace

HorizontalScrollLayer* HorizontalScrollLayer::create(const std::vector<CCNode*>& pages, int width, int gap,
                                                     bool scaled) {
    HorizontalScrollLayer* l = new HorizontalScrollLayer;
    if (l->init(pages, width, gap, scaled)) {
        l->autorelease();
        return l;
    }
    delete l;
    return nullptr;
}

HorizontalScrollLayer::~HorizontalScrollLayer() { unschedule(schedule_selector(HorizontalScrollLayer::update)); }

bool HorizontalScrollLayer::init(const std::vector<CCNode*>& pages, int width, int gap, bool scaled) {
    if (!CCLayer::init()) return false;
    m_mode = 1;
    m_pressing = false;
    m_container = CCNode::create();
    m_container->setAnchorPoint(CCPointZero);
    m_container->setPosition(CCPointZero);
    m_page = 1;
    m_pages = pages;
    m_count = static_cast<int>(m_pages.size());
    m_spacing = width + gap;
    m_scaled = scaled;
    for (size_t i = 0; i < m_pages.size(); ++i) {
        CCNode* p = m_pages[i];
        p->setAnchorPoint(ccp(0.5f, 0.5f));
        p->setPosition(ccp(static_cast<float>(static_cast<int>(i) * m_spacing), 0));
        m_container->addChild(p);
    }
    addChild(m_container);
    schedule(schedule_selector(HorizontalScrollLayer::update));
    return true;
}

void HorizontalScrollLayer::onEnter() {
    m_touchOn = true;
    CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, 0, false);
    CCLayer::onEnter();
}

void HorizontalScrollLayer::onExit() {
    m_touchOn = false;
    CCDirector::sharedDirector()->getTouchDispatcher()->removeDelegate(this);
    CCLayer::onExit();
}

void HorizontalScrollLayer::setTouchOn(bool on) {
    if (on != m_touchOn) {
        CCTouchDispatcher* d = CCDirector::sharedDirector()->getTouchDispatcher();
        if (on) d->addTargetedDelegate(this, 0, false);
        else d->removeDelegate(this);
    }
    m_touchOn = on;
}

void HorizontalScrollLayer::update(float) {
    if (!m_scaled) return;
    // smaller the further from the middle: 0.75 at 100 canvas points
    float perPoint = -0.25f / (ResolutionMgr::sharedResolutionMgr()->canvasScale * 100.0f);
    for (size_t i = 0; i < m_pages.size(); ++i) {
        int d = std::abs(static_cast<int>(m_container->getPosition().x + static_cast<float>(m_spacing * static_cast<int>(i))));
        float s = perPoint * static_cast<float>(d) + 1.0f;
        m_pages[i]->setScale(s < 0.75f ? 0.75f : s > 1.0f ? 1.0f : s);
    }
}

void HorizontalScrollLayer::moveToPage(int page) {
    m_container->runAction(CCMoveTo::create(kMoveSeconds, ccp(static_cast<float>((1 - page) * m_spacing), 0)));
    // (the original wrapped the move in a CCEaseBounce, the base class, whose
    // update is CCActionEase's: the move as it is, not eased)
    m_page = page;
    // (the original passed pages[page - 1] even for a page out of the row;
    // the page's own node is told, the nearest one then)
    int i = page < 1 ? 0 : page > m_count ? m_count - 1 : page - 1;
    if (m_pageChanged.target && m_count > 0) m_pageChanged(m_pages[i]);
}

void HorizontalScrollLayer::setPage(int page) {
    m_container->setPosition(ccp(static_cast<float>(m_spacing * (1 - page)), 0));
    m_page = page;
    update(0);
}

CCPoint HorizontalScrollLayer::touchPoint(CCTouch* touch) {
    return getParent()->convertToNodeSpace(touch->getLocation());
}

bool HorizontalScrollLayer::hits(CCNode* page, float x) {
    // A page is as wide as the spacing (by its scale). Fixed: the original
    // took the page's middle as half the layer's width from its parent's
    // edge, rather than the layer's own place, and its hit span from a
    // quarter of the width left of it to three quarters right: on the PC it
    // missed a fifth of a slot's right side and took the gap left of it.
    float w = page->getScaleX() * static_cast<float>(m_spacing);
    float middle = getPosition().x + m_container->getPosition().x + page->getPosition().x;
    return middle - w * 0.5f < x && x <= middle + w * 0.5f;
}

bool HorizontalScrollLayer::ccTouchBegan(CCTouch* touch, CCEvent*) {
    CCPoint p = touchPoint(touch);
    // the layer's rectangle, about its place
    const CCPoint& at = getPosition();
    const CCSize& size = getContentSize();
    if (p.x > at.x + size.width * 0.5f || p.x < at.x - size.width * 0.5f) return false;
    if (p.y > at.y + size.height * 0.5f || p.y < at.y - size.height * 0.5f) return false;
    m_touchStartX = static_cast<int>(p.x);
    if (m_mode == 1) {
        if (m_pressed.target) {
            m_pressing = true;
            m_pressed(m_pages[m_page - 1]);
        }
    } else {
        for (CCNode* page : m_pages)
            if (hits(page, p.x) && m_pressed.target) {
                m_pressing = true;
                m_pressed(page);
            }
    }
    m_lastCanvas = onCanvas(p);
    m_fast = false;
    return true;
}

void HorizontalScrollLayer::ccTouchMoved(CCTouch* touch, CCEvent*) {
    CCPoint p = touchPoint(touch);
    CCPoint c = onCanvas(p);
    m_fast = ccpDistance(m_lastCanvas, c) > kFastSwipe;
    m_lastCanvas = c;
    float dx = p.x - static_cast<float>(m_touchStartX);
    m_container->setPosition(ccp(static_cast<float>(m_spacing * (1 - m_page)) + dx, 0));
    if (m_pressing && std::abs(static_cast<int>(dx)) > kTapSlop) {
        m_pressing = false;
        if (m_released.target) m_released(m_pages[m_page - 1]);
    }
}

void HorizontalScrollLayer::ccTouchEnded(CCTouch* touch, CCEvent*) {
    CCPoint p = touchPoint(touch);
    int x = static_cast<int>(p.x);
    int s = m_spacing, start = m_touchStartX;
    // the pages the swipe covered, at least one
    auto covered = [&] {
        int n = std::abs(static_cast<int>(std::round(static_cast<float>(x - start) / static_cast<float>(s))));
        return n == 0 ? 1 : n;
    };
    int page;
    if (x - start < -(s / 3) || (x < start && m_fast)) {
        page = m_page + covered();
        if (page > m_count) page = m_count;
    } else if (s / 3 < x - start || (start < x && m_fast)) {
        page = m_page - covered();
        if (page < 1) page = 1;
    } else {
        page = m_page;
    }
    moveToPage(page);
    if (!m_pressing) return;
    m_pressing = false;
    for (CCNode* p2 : m_pages)
        if (m_released.target) m_released(p2);
    if (m_mode == 1) {
        if (m_selected.target) m_selected(m_pages[m_page - 1]);
    } else {
        for (CCNode* p2 : m_pages)
            if (hits(p2, p.x) && m_selected.target) m_selected(p2);
    }
}

}  // namespace mt
