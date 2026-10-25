// mt::HorizontalScrollLayer (made by 0x11cd28; 0x154 bytes; vtable 0x417018):
// a row of pages swiped across, one in the middle, used by the save slots,
// the credits, the world map and the partner select screen.
//
// The pages are put `spacing` apart in a container that moves with the
// finger; a swipe of more than a third of the spacing (or a fast one, over
// 200 canvas points between two moves) goes on by the pages it covered, at
// least one, else the page stays; the container then moves to it in 0.3 s.
// It tells its target of four things, each with the page: pressed (a touch
// began on a page), released (the touch moved more than 5 points from where
// it began, or ended), selected (a touch ended without moving off it) and
// page changed. In mode 1 only the page in the middle is pressed and
// selected; in any other mode the page under the touch is. With `scaled`
// the pages away from the middle shrink, to 0.75 at 100 canvas points.
#pragma once

#include <vector>

#include "engine/cocos/CCLayer.h"

namespace mt {

class HorizontalScrollLayer : public cocos2d::CCLayer {
public:
    // 0x11cd28: `pages` placed `width + gap` apart
    static HorizontalScrollLayer* create(const std::vector<cocos2d::CCNode*>& pages, int width, int gap,
                                         bool scaled);
    ~HorizontalScrollLayer() override;

    void onEnter() override;   // 0x11cf80
    void onExit() override;    // 0x11cfe4
    void update(float dt) override;  // 0x11d0dc: the pages' scale
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // 0x11d1d0
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // 0x11d4a0
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;  // 0x11d61c

    void setTouchOn(bool on);       // 0x11cfac
    void moveToPage(int page);      // 0x11d00c: moved there in 0.3 s, then "page changed"
    void setPage(int page);         // 0x11d080: put there at once
    void setMode(int mode) { m_mode = mode; }  // 0x11d0d4
    int page() const { return m_page; }        // 1 for the first
    int pageCount() const { return m_count; }
    const std::vector<cocos2d::CCNode*>& pages() const { return m_pages; }
    cocos2d::CCNode* container() const { return m_container; }
    // the target and its function told of each (0x11d8bc, 0x11d8c8, 0x11d8d4, 0x11d8e0)
    void setPressed(cocos2d::CCObject* t, cocos2d::SEL_CallFuncN s) { m_pressed = {t, s}; }
    void setReleased(cocos2d::CCObject* t, cocos2d::SEL_CallFuncN s) { m_released = {t, s}; }
    void setSelected(cocos2d::CCObject* t, cocos2d::SEL_CallFuncN s) { m_selected = {t, s}; }
    void setPageChanged(cocos2d::CCObject* t, cocos2d::SEL_CallFuncN s) { m_pageChanged = {t, s}; }

private:
    HorizontalScrollLayer() = default;
    using cocos2d::CCLayer::init;
    bool init(const std::vector<cocos2d::CCNode*>& pages, int width, int gap, bool scaled);  // 0x11ce60
    struct Callback {
        cocos2d::CCObject* target = nullptr;
        cocos2d::SEL_CallFuncN selector = nullptr;
        void operator()(cocos2d::CCNode* page) const {
            if (target) (target->*selector)(page);
        }
    };
    bool hits(cocos2d::CCNode* page, float x);
    cocos2d::CCPoint touchPoint(cocos2d::CCTouch* touch);  // in the parent's space

    std::vector<cocos2d::CCNode*> m_pages;      // +0xec
    cocos2d::CCNode* m_container = nullptr;     // +0xf8
    int m_spacing = 0;                          // +0xfc
    int m_page = 1;                             // +0x100
    int m_count = 0;                            // +0x104
    int m_touchStartX = 0;                      // +0x108
    bool m_pressing = false;                    // +0x10c: a touch on a page, not moved off yet
    cocos2d::CCPoint m_lastCanvas;              // +0x110: the touch, on the canvas
    bool m_fast = false;                        // +0x118
    bool m_touchOn = false;                     // +0x119
    int m_mode = 1;                             // +0x11c
    bool m_scaled = false;                      // +0x120
    Callback m_pressed, m_released, m_selected, m_pageChanged;  // +0x124.. (+0x134..)
};

}  // namespace mt
