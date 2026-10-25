// SurgeryScreen's HUD on a big screen and from the keyboard (not the
// original's; docs/as3.md, "The surgery's HUD on a big screen"):
//
// - The layout by the screen's size (platform::largeScreen): on a PC, a
//   tablet or a foldable unfolded the eleven tool buttons are one bar across
//   the top, right of the heart meter and the pause button, in the order of
//   their keys (kBar: Cutter, Tongs, Lighter, Cream, Stapler, Vacuum,
//   Chainsaw, Injection, Tazer, Clamp, Scanner: the user's), scaled to
//   fit; a tool the surgery does not offer shows its greyed card (as the
//   Flash AS2's dimmed cards), so each keeps its place. What hangs off a
//   button goes with it: its "tooleffect" spot (the frozen and poisoned
//   tools, the sheen, the Reverend's blessing), the injection's out button
//   (under it) with its graph and cooldown bars, the clamps' count. The tag
//   partner's button moves under the bar's right end, the tutorial's PAUSED
//   box under the bar. On a phone the original's two columns stay. The
//   choice is made as the HUD is built and followed while the surgery runs
//   (a foldable folded or unfolded, a window moved to another display), once
//   the buttons are at rest (not slid away by Brutality, the injection's
//   menu shut). Debug: AS_HUD_LAYOUT=top|sides, or the F1 menu.
// - The play area (ResolutionMgr's rect: canvas 149,45 342x255, the screen's
//   y 52..307 from the top) lies wholly under the bar, which ends about 31
//   points down: creatures and new injuries never go under it.
// - The number keys (1..9, 0, and the key after 0) press the buttons in the
//   bar's order, as a click would (a refused tool refused the same way); the
//   injection's key picks it to inject, I to draw out (each its mode's
//   button: the one beside pressed as a player would, the menu opened first
//   if shut). Each button shows its key (small, at the card's top right
//   corner) where there is a keyboard (a PC). Escape (a phone's back key)
//   presses the pause button, and on the pause screen its back button.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "as3/HudEffects.h"
#include "as3/Injection.h"
#include "as3/Log.h"
#include "as3/PartnerEffects.h"
#include "as3/PauseScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SurgeryScreen.h"
#include "as3/ToolArt.h"
#include "engine/cocos/CCClippingNode.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCDrawNode.h"
#include "engine/cocos/CCSpriteFrameCache.h"
#include "engine/cocos/CCLabelBMFont.h"
#include "engine/cocos/CCMenu.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

// the tools in the bar's order, the order of their number keys
const int kBar[kToolCount] = {kCutter,   kTongs,     kLighter, kCream, kStapler, kVacuum,
                              kChainsaw, kInjection, kTazer,   kClamp, kScanner};
// the keys' labels, in the bar's order (number keys 1..9, 0, the next)
const char* const kKeyNames[kToolCount] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-"};
// the injection's out button's (to draw out): a letter key
const char kOutKey = 'i';
const char* const kOutKeyName = "I";

// a tool's place in the bar
int slotOf(int tool) { return static_cast<int>(std::find(kBar, kBar + kToolCount, tool) - kBar); }
const char* keyName(int tool) { return kKeyNames[slotOf(tool)]; }

// the cards' faces in canvas points (the buttons' art is a little larger:
// 60x51, the cutter's and tongs' more, their tools sticking out)
constexpr float kCardW = 55, kCardH = 46;
// The bar's grey cards made longer downward (the bar has room down to the
// play area, 52 points from the top) as the port's tool art's long card is
// (ToolArt.h: the clean card, its stripes going on above its bottom edge),
// the tool over it where the button draws it (cardLonger: by how much, in
// the card's points; none without the tool art).
float cardLonger() {
    float longer = 0;
    CCRect inside;
    CCTexture2D* card = toolart::card(0);
    return card && toolart::cardLayout(longer, inside) ? longer / card->getContentScale() : 0;
}
// the keys' labels' margin inside the card (points)
constexpr float kKeyMargin = 1.5f;
// The meter (ingame_combometer: the heart and clock's circle and the points
// meter's housing in one picture, the housing's left end hidden behind the
// circle) is split for the bar: the circle stays top left with the pause
// button beside it, and the points meter (its housing, bars, multiplier
// box, score and hearts) goes to the bottom right, so the bar has the top
// for the tools. In the picture's pixels (y down): the circle, its top tab
// and its right knob (kept: a clipped copy of the picture); the housing's
// top band from kHousingFrom, its lower box from kBoxFrom, its rounded end
// from kCapFrom, that end mirrored as the left end the circle hid.
constexpr float kCircleX = 140, kCircleY = 150, kCircleR = 138;
constexpr float kTabX0 = 146, kTabY0 = 0, kTabX1 = 194, kTabY1 = 32;
constexpr float kKnobX0 = 260, kKnobY0 = 122, kKnobX1 = 296, kKnobY1 = 160;
constexpr float kHousingFrom = 298, kBoxFrom = 283, kBoxTop = 192, kCapFrom = 650;
// (the port's, play-tested) the score's box on top of the housing, upside
// down, its open edge under the housing's top rim (the picture's row)
constexpr float kBoxTuck = 74;
// the points meter's gap to the screen's right and bottom edges, the pause
// button's to the circle (canvas points)
constexpr float kMeterMargin = 2, kPauseBeside = 1;
// the points meter's parts besides its housing (they move with it)
const char* const kPointsParts[] = {"ingame_combometer_bar01", "ingame_combometer_bar02", "ingame_combometer_bar03",
                                    "ingame_combometer_bar04", "ingame_combometer_bar05", "ingame_combometer_bar06",
                                    "ingame_combometer_bar07", "ingame_combometer_bar08", "ingame_combometer_bar09",
                                    "ingame_combometer_rainbowbar", "ingame_combometer_scorebox",
                                    "ingame_combometer_nuclearGlow", "ingame_comboheart", "comboheart", "combo_score",
                                    "combometer"};

// The compact bar's height (canvas points): the bar's before the meter was
// split, about 0.58 of a long card; and how near the big bar's place a point
// still to treat makes the bar compact (hudFrame).
constexpr float kCompactBarHeight = 34, kCrowdMargin = 6;
// the big bar's height at most (canvas points; play-tested: the cards
// filling the whole width were too big)
constexpr float kBarHeight = 40;
// the bar's gaps: from the screen's top, after the pause button, at the right edge
constexpr float kTopGap = 3, kPauseGap = 3, kEdgeGap = 2;
// between the injection's card and the out button under it (its graph, the
// white line joining them, there)
constexpr float kMenuGap = 8;

// A button pressed as a click on it would: only one shown (with all its
// parents) and on, in a menu that is on; false if refused.
bool press(CCNode* n) {
    auto* item = dynamic_cast<CCMenuItem*>(n);
    if (!item) return false;
    for (CCNode* p = item; p; p = p->getParent())
        if (!p->isVisible()) return false;
    auto* menu = dynamic_cast<CCMenu*>(item->getParent());
    if (!item->isEnabled() || (menu && !menu->isEnabled())) return false;
    item->selected();
    item->unselected();
    item->activate();
    return true;
}

// Where a button takes a touch, in its parent's coordinates: its touch rect
// (its card) if it has one, else its whole picture.
CCRect touchBounds(CCNode* n) {
    auto* item = dynamic_cast<CCMenuItem*>(n);
    if (!item || item->getTouchRect().size.width <= 0 || !n->getParent()) return n->boundingBox();
    const CCRect& r = item->getTouchRect();
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const CCPoint& p : {r.origin, ccp(r.getMaxX(), r.origin.y), ccp(r.origin.x, r.getMaxY()), ccp(r.getMaxX(), r.getMaxY())}) {
        const CCPoint q = n->getParent()->convertToNodeSpace(n->convertToWorldSpace(p));
        x0 = std::min(x0, q.x), y0 = std::min(y0, q.y), x1 = std::max(x1, q.x), y1 = std::max(y1, q.y);
    }
    return CCRectMake(x0, y0, x1 - x0, y1 - y0);
}

}  // namespace

int SurgeryScreen::s_hudLayoutOverride = -1;  // (-1: AS_HUD_LAYOUT not read yet)

bool SurgeryScreen::wantTopBar() const {
    if (s_hudLayoutOverride < 0) {
        s_hudLayoutOverride = 0;
        if (const char* v = platform::debugMode() ? std::getenv("AS_HUD_LAYOUT") : nullptr)
            s_hudLayoutOverride = std::strcmp(v, "top") == 0 ? 1 : std::strcmp(v, "sides") == 0 ? 2 : 0;
    }
    if (s_hudLayoutOverride == 1) return true;
    if (s_hudLayoutOverride == 2) return false;
    return platform::largeScreen();
}

// Each tool's card's middle within its art, from the original's
// "tooleffect" spots (the frost, the sheen... drawn over the card): the one
// beside each button (their names do not always match their buttons).
CCPoint SurgeryScreen::hudCardMiddle(const std::string& button) const {
    CCPoint best = ccp(29, 25);
    const UIItemDef* b = nullptr;
    for (const UIItemDef& d : m_hudDef.items)
        if (d.name == button) b = &d;
    if (!b) return best;
    float bestD = 1e9f;
    for (const UIItemDef& d : m_hudDef.items) {
        if (d.name.compare(0, 11, "tooleffect_") != 0 || d.mode != b->mode) continue;
        const float dx = static_cast<float>(d.x - b->x), dy = static_cast<float>(d.y - b->y);
        const float dist = std::fabs(dx - 29) + std::fabs(dy - 25);
        if (dist < bestD) bestD = dist, best = ccp(dx, dy);
    }
    return best;
}

void SurgeryScreen::splitMeter() {
    // (once) the meter's two pieces, children of the meter's item (so they
    // follow it and draw where its picture does, under the bars): the circle, a
    // copy of the picture clipped to the circle, its tab and its knob; the
    // housing, pieces of the picture with its hidden end mirrored
    // (the layout's item: a menu item whose picture is at its origin)
    CCNode* meter = ui.node("ingame_combometer");
    auto* item = dynamic_cast<CCMenuItemSprite*>(meter);
    auto* image = item ? dynamic_cast<CCSprite*>(item->getNormalImage()) : nullptr;
    CCSpriteFrame* f = image ? image->displayFrame() : nullptr;
    if (!f || f->isRotated()) return;
    CCTexture2D* tex = f->getTexture();
    const float cs = tex->getContentScale();
    const CCRect& px = f->getRectInPixels();
    const CCSize& full = f->getOriginalSize();
    const CCSize trim = f->getRect().size;
    // the picture's top left in the meter's own points (y up), and a pixel
    // (x, y down) of the picture there
    const CCPoint topLeft = ccp((full.width - trim.width) * 0.5f + f->getOffset().x,
                                (full.height - trim.height) * 0.5f + f->getOffset().y + trim.height);
    auto at = [&](float x, float y) { return ccp(topLeft.x + x / cs, topLeft.y - y / cs); };
    auto piece = [&](float x0, float y0, float x1, float y1) {
        CCSprite* s = CCSprite::createWithTexture(tex, CCRectMake((px.origin.x + x0) / cs, (px.origin.y + y0) / cs,
                                                                  (x1 - x0) / cs, (y1 - y0) / cs));
        s->setAnchorPoint(ccp(0, 1));
        s->setPosition(at(x0, y0));
        return s;
    };
    // the circle
    CCDrawNode* shape = CCDrawNode::create();
    const ccColor4F white = {1, 1, 1, 1};
    auto ellipse = [&](float cx, float cy, float rx, float ry) {
        CCPoint v[40];
        for (int i = 0; i < 40; ++i) {
            const float a = 6.2831853f * i / 40;
            v[i] = at(cx + rx * std::cos(a), cy + ry * std::sin(a));
        }
        shape->drawPolygon(v, 40, white, 0, white);
    };
    ellipse(kCircleX, kCircleY, kCircleR, kCircleR);
    ellipse((kKnobX0 + kKnobX1) / 2, (kKnobY0 + kKnobY1) / 2, (kKnobX1 - kKnobX0) / 2, (kKnobY1 - kKnobY0) / 2);
    CCPoint tab[4] = {at(kTabX0, kTabY0), at(kTabX1, kTabY0), at(kTabX1, kTabY1), at(kTabX0, kTabY1)};
    shape->drawPolygon(tab, 4, white, 0, white);
    CCClippingNode* circle = CCClippingNode::create();
    circle->setStencil(shape);
    circle->addChild(piece(0, 0, trim.width * cs, trim.height * cs));
    meter->addChild(circle);
    m_meterCircle = circle;
    // the points meter's housing
    CCNode* housing = CCNode::create();
    const float w = trim.width * cs, h = trim.height * cs;
    housing->addChild(piece(kHousingFrom, 0, kCapFrom, kBoxTop));
    CCSprite* box = piece(kBoxFrom, kBoxTop, kCapFrom, h);
    box->setFlipY(true);
    box->setPosition(at(kBoxFrom, kBoxTuck - (h - kBoxTop)));
    housing->addChild(box, -1);
    housing->addChild(piece(kCapFrom, 0, w, kBoxTop));
    CCSprite* end = piece(kCapFrom, 0, w, kBoxTop);
    end->setFlipX(true);
    end->setPosition(at(kHousingFrom - (w - kCapFrom), 0));
    housing->addChild(end);
    meter->addChild(housing);
    m_meterHousing = housing;
    m_meterCircleRight = at(kKnobX1, 0).x;  // (the meter's points)
    const float top = std::min(0.0f, kBoxTuck - (h - kBoxTop));
    m_meterHousingSpan = CCRectMake(at(kHousingFrom - (w - kCapFrom), 0).x, at(0, kBoxTop).y,
                                    (w - kHousingFrom + (w - kCapFrom)) / cs, (kBoxTop - top) / cs);
    m_meterRow0 = topLeft.y;
    m_meterPixels = cs;
}

void SurgeryScreen::showMeterSplit(bool on) {
    // the meter whole (the phone's layout, the original's), or split: the
    // circle where it is, the points meter's housing and parts moved by the
    // same offset to the bottom right, the pause button beside the circle
    CCNode* meter = ui.node("ingame_combometer");
    auto* item = dynamic_cast<CCMenuItemSprite*>(meter);
    auto* image = item ? dynamic_cast<CCSprite*>(item->getNormalImage()) : nullptr;
    if (!image || !m_meterCircle) return;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float c = r->canvasScale, W = static_cast<float>(r->width);
    image->setOpacity(on ? 0 : 255);
    m_meterCircle->setVisible(on);
    m_meterHousing->setVisible(on);
    auto sideSpot = [&](CCNode* n) {
        for (const HudSpot& s : m_sideSpots)
            if (s.node == n) return s.at;
        return n->getPosition();
    };
    // the offset: the housing's and its parts' bounds (the glow aside, a
    // light around the box) at the screen's bottom right
    CCPoint offset = CCPointZero;
    // each part's place before the offset: its own, the score's number
    // mirrored as its box is, from under the housing to on top of it
    auto base = [&](CCNode* x) {
        CCPoint at = sideSpot(x);
        if (!on || x != ui.node("combo_score") || !x->getParent()) return at;
        x->setPosition(at);
        const CCRect b = x->boundingBox();
        const CCPoint mid = meter->convertToNodeSpace(x->getParent()->convertToWorldSpace(ccp(b.getMidX(), b.getMidY())));
        const float row = (m_meterRow0 - mid.y) * m_meterPixels;
        const CCPoint up = meter->convertToWorldSpace(ccp(mid.x, m_meterRow0 - (kBoxTuck + kBoxTop - row) / m_meterPixels));
        return ccpAdd(at, ccpSub(x->getParent()->convertToNodeSpace(up), x->getParent()->convertToNodeSpace(
                                                                             meter->convertToWorldSpace(mid))));
    };
    if (on) {
        const CCPoint origin = ccpSub(meter->getPosition(), meter->getAnchorPointInPoints());
        CCRect span = m_meterHousingSpan;
        span.origin = ccpAdd(span.origin, origin);
        for (const char* n : kPointsParts) {
            CCNode* x = ui.node(n);
            if (!x || std::string(n) == "ingame_combometer_nuclearGlow" || !x->getParent()) continue;
            x->setPosition(base(x));
            CCRect b = x->boundingBox();
            if (b.size.width <= 0) continue;
            b.origin = meter->getParent()->convertToNodeSpace(x->getParent()->convertToWorldSpace(b.origin));
            const float x0 = std::min(span.getMinX(), b.getMinX()), y0 = std::min(span.getMinY(), b.getMinY());
            const float x1 = std::max(span.getMaxX(), b.getMaxX()), y1 = std::max(span.getMaxY(), b.getMaxY());
            span = CCRectMake(x0, y0, x1 - x0, y1 - y0);
        }
        offset = ccp(W - kMeterMargin * c - span.getMaxX(), kMeterMargin * c - span.getMinY());
    }
    m_meterHousing->setPosition(offset);
    for (const char* n : kPointsParts)
        if (CCNode* x = ui.node(n)) x->setPosition(ccpAdd(base(x), offset));
    // the pause button beside the circle (its left edge)
    if (CCNode* p = ui.node("ingame_pause")) {
        p->setPosition(sideSpot(p));
        if (on) {
            const float right = meter->getPosition().x - meter->getAnchorPointInPoints().x + m_meterCircleRight;
            p->setPositionX(p->getPositionX() + (right + kPauseBeside * c - p->boundingBox().getMinX()));
        }
    }
}

void SurgeryScreen::lengthenCards(bool on) {
    // (the bar's) built once, on each card (and the injection's out card):
    // the long card and the tool over the button's own pictures, which the
    // bar hides
    if (on && m_longCards.empty() && cardLonger() > 0) {
        for (int t = 0; t <= kToolCount; ++t) {
            const std::string n = t < kToolCount ? std::string("ingame_tool_") + kToolNames[t] : "ingame_tool_Injection_out";
            auto* b = dynamic_cast<CCMenuItemToggle*>(ui.node(n));
            std::string prefix;
            for (const UIItemDef& d : m_hudDef.items)
                if (d.name == n) prefix = d.spritePrefix;
            LongCard lc;
            lc.tool = toolart::fromSpritePrefix(prefix);
            toolart::ToolLayout place;
            CCTexture2D* art = toolart::tool(lc.tool, 0);
            if (!b || !art || !toolart::layout(lc.tool, place)) continue;
            lc.button = b;
            lc.holder = CCNode::create();
            b->addChild(lc.holder, 5);  // (under its key)
            // the card's top left in the button's box (its pictures'
            // untrimmed size, points)
            const float cs = art->getContentScale();
            lc.cardTopLeft = ccp(place.cardTopLeft.x / cs, b->getContentSize().height - place.cardTopLeft.y / cs);
            lc.card = CCSprite::createWithTexture(toolart::card(0));
            lc.card->setAnchorPoint(ccp(0, 1));
            lc.card->setPosition(lc.cardTopLeft);
            lc.holder->addChild(lc.card);
            lc.toolSprite = CCSprite::createWithTexture(art);
            lc.toolSprite->setAnchorPoint(CCPointZero);
            lc.holder->addChild(lc.toolSprite);
            for (unsigned i = 0; i < b->getSubItems()->count(); ++i)
                lc.images.push_back(static_cast<CCNode*>(b->getSubItems()->objectAtIndex(i)));
            m_longCards.push_back(lc);
        }
    }
    for (LongCard& lc : m_longCards) {
        lc.holder->setVisible(on);
        for (CCNode* image : lc.images) image->setVisible(!on);
    }
    syncLongCards();
}

void SurgeryScreen::syncLongCards() {
    // each card in its button's state (its toggle's: normal, selected,
    // turned off)
    for (LongCard& lc : m_longCards) {
        const int state = std::min<int>(static_cast<int>(lc.button->getSelectedIndex()), 2);
        if (state == lc.state) continue;
        lc.state = state;
        for (auto [sprite, tex] : {std::make_pair(lc.card, toolart::card(state)), std::make_pair(lc.toolSprite, toolart::tool(lc.tool, state))})
            if (tex) {
                sprite->setTexture(tex);
                sprite->setTextureRect(CCRectMake(0, 0, tex->getContentSize().width, tex->getContentSize().height));
            }
    }
}

void SurgeryScreen::placeKeyLabels() {
    // The keys' labels: small, in the card's top right corner, off its
    // picture, whatever the card's scale; the clamps' card's corner holds
    // its count: there just left of the count's text (again as it changes:
    // refreshHud).
    const float c = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCNode* count = ui.node("clampnumber");
    for (CCNode* l : m_keyLabels) {
        CCNode* b = l->getParent();
        const std::string n = ui.nameOf(m_hudDef, b);
        const CCPoint mid = hudCardMiddle(n == "ingame_tool_Injection_out" ? "ingame_tool_Injection" : n);
        // (the font's digits are a little taller than the clock's: as small
        // as those at 0.6)
        l->setScaleX(0.35f / (b && b->getScaleX() > 0 ? b->getScaleX() : 1));
        l->setScaleY(0.35f / (b && b->getScaleY() > 0 ? b->getScaleY() : 1));
        CCPoint at = ccp((mid.x + kCardW * 0.5f - 5) * c, (mid.y + kCardH * 0.5f - 5) * c);
        // in the bar, the long card's bottom right corner, within its edges
        const LongCard* lc = nullptr;
        for (const LongCard& x : m_longCards)
            if (x.button == b && x.holder->isVisible()) lc = &x;
        float longer = 0;
        CCRect inside;
        if (lc && toolart::cardLayout(longer, inside)) {
            const float cs = toolart::card(0)->getContentScale();
            const float w = l->getContentSize().width * l->getScaleX(), h = l->getContentSize().height * l->getScaleY();
            at = ccp(lc->cardTopLeft.x + inside.getMaxX() / cs - kKeyMargin - w * 0.5f,
                     lc->cardTopLeft.y - inside.getMaxY() / cs + kKeyMargin + h * 0.5f);
        } else if (n == "ingame_tool_Clamp" && count && count->isVisible()) {
            // (the count's first letter's left: the letters are a label's
            // sprites under it; its box is wider than its text)
            float left = 1e9f;
            std::function<void(CCNode*)> letters = [&](CCNode* x) {
                if (!x->isVisible()) return;
                if (x->getChildrenCount() == 0) {
                    if (dynamic_cast<CCLabelBMFont*>(x->getParent()))
                        left = std::min(left, b->convertToNodeSpace(x->getParent()->convertToWorldSpace(
                                                  x->boundingBox().origin)).x);
                    return;
                }
                CCArray* kids = x->getChildren();
                for (unsigned i = 0; i < kids->count(); ++i) letters(static_cast<CCNode*>(kids->objectAtIndex(i)));
            };
            letters(count);
            if (left < 1e9f) at.x = std::min(at.x, left - (l->getContentSize().width * 0.5f + 1 * c) * l->getScaleX());
        }
        l->setPosition(at);
    }
}

void SurgeryScreen::layoutHud(bool top) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float c = r->canvasScale;
    const float W = static_cast<float>(r->width), H = static_cast<float>(r->height);
    auto node = [&](const std::string& name) { return ui.node(name); };
    // what hangs off the buttons, kept at its original place to go back to
    // (the tag partner's picture, greyed charge and sparkle move by as much
    // as its button)
    const char* const kHung[] = {"ingame_tool_Injection_out", "ingame_injection_graph", "clampnumber",
                                 "ingame_injection_bar_grey", "ingame_injection_bar_green", "ingame_tagpartner"};

    if (!m_hudLaidOut) {
        // The original's places, to go back to.
        m_hudLaidOut = true;
        for (int t = 0; t < kToolCount; ++t)
            for (const std::string& n : {std::string("ingame_tool_") + kToolNames[t], std::string("tooleffect_") + kToolNames[t]})
                if (CCNode* x = node(n)) m_sideSpots.push_back({x, x->getPosition()});
        for (const char* n : kHung)
            if (CCNode* x = node(n)) m_sideSpots.push_back({x, x->getPosition()});
        // (and the tutorial's PAUSED box's shown place: moved by a shift)
        if (CCNode* x = node("ingame_tutorial_pause_box")) m_sideSpots.push_back({x, x->getPosition()});
        // (the pause button and the points meter's parts: moved for the bar)
        for (const char* n : kPointsParts)
            if (CCNode* x = node(n)) m_sideSpots.push_back({x, x->getPosition()});
        if (CCNode* x = node("ingame_pause")) m_sideSpots.push_back({x, x->getPosition()});
        splitMeter();
        // The keys' labels, on each tool's button (and the out button's I),
        // at the card's top right corner: where there is a keyboard to press
        // them.
        if (!platform::touchDevice())
            for (int t = 0; t <= kToolCount; ++t) {
                const std::string n = t < kToolCount ? std::string("ingame_tool_") + kToolNames[t] : "ingame_tool_Injection_out";
                CCNode* b = node(n);
                CCLabelBMFont* l = b ? createLabel(t < kToolCount ? keyName(t) : kOutKeyName, "font-ScoreUI04", 20,
                                                   kCCTextAlignmentCenter)
                                     : nullptr;
                if (!l) continue;
                // (the score popups' font, the clock's family: the clock's
                // own has digits only, no I)
                l->setColor(ccc3(51, 51, 51));  // (the clock's grey)
                l->setAnchorPoint(ccp(0.5f, 0.5f));
                b->addChild(l, 10);
                m_keyLabels.push_back(l);
            }
    }

    auto sideSpot = [&](CCNode* n) {
        for (const HudSpot& s : m_sideSpots)
            if (s.node == n) return s.at;
        return n ? n->getPosition() : CCPointZero;
    };
    // (the injection's two buttons: the one in its place and the one beside)
    CCNode* in = node("ingame_tool_Injection");
    CCNode* out = node("ingame_tool_Injection_out");
    if (!m_injectionFirst) {
        m_injectionFirst = in;
        m_injectionSecond = out;
    }
    CCNode* tag = node("ingame_tagpartner");
    const CCPoint tagWas = tag ? tag->getPosition() : CCPointZero;
    float k = 1;

    if (!top) {
        for (const HudSpot& s : m_sideSpots) {
            if (s.node == node("ingame_tutorial_pause_box")) continue;
            s.node->setPosition(s.at);
            s.node->setScale(1);  // (both ways)
        }
        // (the injection's place and the one beside, whichever is where)
        if (m_injectionFirst && m_injectionSecond) {
            m_injectionFirst->setPosition(sideSpot(in));
            m_injectionSecond->setPosition(sideSpot(out));
        }
        if (CCNode* l = node("clampnumber")) l->setScale(1);
        showMeterSplit(false);
    } else {
        // The meter split: the points meter to the bottom right, the pause
        // button beside the circle (the bar then starts there).
        showMeterSplit(true);
        // The bar: from the pause button's right to the screen's edge, a
        // place for each tool, the cards scaled to fill it.
        float x0 = 0;
        if (CCNode* p = node("ingame_pause")) x0 = p->boundingBox().getMaxX();
        x0 += kPauseGap * c;
        const float slot = (W - kEdgeGap * c - x0) / kToolCount;
        k = std::min(slot / (kCardW * c), 1.0f);
        // (and no lower than the play area's top: ResolutionMgr's rect, on
        // the canvas, its top 300 of 384)
        const float playTop = H * 0.5f + (r->rect.getMaxY() - 192) * c;
        const float room = H - playTop - kTopGap * c;
        k = std::min(k, room / (kCardH * c + cardLonger()));
        k = std::min(k, kBarHeight * c / (kCardH * c + cardLonger()));
        // (where the big bar ends, for hudFrame; compact: no taller than the
        // bar was before)
        m_barBigBottom = H - kTopGap * c - (kCardH * c + cardLonger()) * k;
        m_barLeft = x0;
        if (m_barCompact) k = std::min(k, kCompactBarHeight * c / (kCardH * c + cardLonger()));
        const float cy = H - kTopGap * c - kCardH * c * k * 0.5f;
        CCPoint middle[kToolCount];  // (by tool)
        for (int t = 0; t < kToolCount; ++t) {
            const std::string n = std::string("ingame_tool_") + kToolNames[t];
            middle[t] = ccp(x0 + (slotOf(t) + 0.5f) * slot, cy);
            const CCPoint off = hudCardMiddle(n);
            if (CCNode* b = node(n)) {
                b->setScale(k);
                b->setPosition(ccp(middle[t].x - off.x * c * k, middle[t].y - off.y * c * k));
            }
            if (CCNode* e = node(std::string("tooleffect_") + kToolNames[t])) {
                e->setScale(k);
                e->setPosition(middle[t]);
            }
        }
        // the injection's out button (and whichever is not in its place)
        // under it, the graph between them
        if (in && out && m_injectionFirst && m_injectionSecond) {
            const CCPoint off = hudCardMiddle("ingame_tool_Injection");
            const CCPoint first = ccp(middle[kInjection].x - off.x * c * k, middle[kInjection].y - off.y * c * k);
            out->setScale(k);
            m_injectionFirst->setPosition(first);
            // (under the injection's card and its longer part)
            const float drop = (kCardH + kMenuGap) * c * k + cardLonger() * k;
            m_injectionSecond->setPosition(ccp(first.x, first.y - drop));
            if (CCNode* g = node("ingame_injection_graph")) {
                g->setScale(k);
                g->setRotation(90);  // (its arrow from one to the other, now downward)
                const CCSize gs = g->getContentSize();
                // (turned about its corner: its middle between the two cards)
                g->setPosition(ccp(middle[kInjection].x - gs.height * k * 0.5f,
                                   middle[kInjection].y - drop * 0.5f + gs.width * k * 0.5f));
            }
        }
        // the clamps' count on the clamps' card's top right corner, as at
        // the side (its place from the original's layout)
        if (CCNode* l = node("clampnumber"))
            if (CCNode* b = node("ingame_tool_Clamp")) {
                const CCPoint d = ccpSub(sideSpot(l), sideSpot(b));
                l->setScale(std::max(k, 0.75f));
                l->setPosition(ccp(b->getPositionX() + d.x * k, b->getPositionY() + d.y * k));
            }
    }
    // (the graph back as it was at the side)
    if (!top)
        if (CCNode* g = node("ingame_injection_graph")) g->setRotation(0);
    // the bars on the injection's in button, wherever it is (at the side
    // the original's own places, unless the buttons were swapped)
    if (top || m_injectionFirst != in) placeInjectionBars();
    else if (CCNode* g = node("ingame_injection_bar_green")) g->setScaleY(1);

    // The tag partner's button: at the side its own place; with the bar,
    // under the bar's right end. Its picture, greyed charge and sparkle
    // with it.
    if (tag) {
        CCPoint to = sideSpot(tag);
        if (top) {
            const CCSize ts = tag->getContentSize();
            const float barBottom = H - kTopGap * c - kCardH * c * k - cardLonger() * k - 2 * c;
            to = ccp(W - kEdgeGap * c - ts.width, barBottom - ts.height);
        }
        const CCPoint d = ccpSub(to, tagWas);
        tag->setPosition(to);
        if (CCNode* image = node("ingame_tag_image")) image->setPosition(ccpAdd(image->getPosition(), d));
        if (m_tagGrey) m_tagGrey->setPosition(ccpAdd(m_tagGrey->getPosition(), d));
        placeTagSparkle();
    }
    // The tutorial's PAUSED box: its top just under the bar.
    float shift = 0;
    if (top)
        if (CCNode* box = node("ingame_tutorial_pause_box")) {
            const float barBottom = H - kTopGap * c - kCardH * c * k - cardLonger() * k;
            const float boxTop = sideSpot(box).y + box->getContentSize().height;
            shift = std::min(0.0f, barBottom - 2 * c - boxTop);
        }
    if (shift != m_pauseBoxShift) {
        if (CCNode* box = node("ingame_tutorial_pause_box"))
            box->setPositionY(box->getPositionY() + shift - m_pauseBoxShift);
        m_tutorialPauseAt.y += shift - m_pauseBoxShift;
        m_pauseBoxShift = shift;
    }
    // The effects made over the buttons follow them.
    for (Effect* e : m_effects.activeOfType(kEffectToolsSheen)) static_cast<ToolsSheenEffect*>(e)->followTools(ui);
    for (Effect* e : m_effects.activeOfType(kEffectReverandTools)) static_cast<ReverandToolsEffect*>(e)->followTools(ui);

    lengthenCards(top);
    placeKeyLabels();
    // Each tool's button takes a touch on its card only (the art sticking
    // out of it, the cutter's and the tongs' more than the others', took
    // touches meant for the next card or the patient): both layouts.
    for (int t = 0; t <= kToolCount; ++t) {
        const std::string n = t < kToolCount ? std::string("ingame_tool_") + kToolNames[t] : "ingame_tool_Injection_out";
        if (auto* item = dynamic_cast<CCMenuItem*>(node(n))) {
            const CCPoint mid = hudCardMiddle(t < kToolCount ? n : std::string("ingame_tool_Injection"));
            const float longer = top ? cardLonger() : 0;  // (the card's longer part in the bar)
            item->setTouchRect(CCRectMake((mid.x - kCardW * 0.5f) * c, (mid.y - kCardH * 0.5f) * c - longer, kCardW * c,
                                          kCardH * c + longer));
        }
    }

    // Every tool shown in the bar, the ones not offered greyed (their
    // toggles are off: 2), so each keeps its place; at the side the
    // original leaves them out.
    for (int t = 0; t < kToolCount; ++t) {
        bool offered = std::find(m_def.tools.begin(), m_def.tools.end(), t) != m_def.tools.end();
        if (!offered) ui.setVisible(std::string("ingame_tool_") + kToolNames[t], top);
    }
    m_topBar = top;
    log("hud layout %s scale %.2f (screen %.0f dp)", top ? "top" : "sides", k, platform::screenShortSideDp());
    // (traced for test scripts: the buttons' middles and sizes, cocos points)
    for (int t = 0; t <= kToolCount; ++t) {
        const std::string n = t < kToolCount ? std::string("ingame_tool_") + kToolNames[t] : "ingame_tool_Injection_out";
        CCNode* b = node(n);
        // (only the tools the surgery offers, as buildHud's trace: a greyed
        // card in the bar is not one to pick)
        if (!b || (t < kToolCount && std::find(m_def.tools.begin(), m_def.tools.end(), t) == m_def.tools.end()))
            continue;
        const CCRect bb = touchBounds(b);
        if (t < kToolCount)
            log("tool %s button %.0f,%.0f size %.0f,%.0f key %s", kToolNames[t], bb.getMidX(), bb.getMidY(),
                bb.size.width, bb.size.height, keyName(t));
        else
            log("injection out button %.0f,%.0f size %.0f,%.0f key %s", bb.getMidX(), bb.getMidY(), bb.size.width,
                bb.size.height, kOutKeyName);
    }
    // (and the pause button, which the bar's layout moves beside the circle)
    if (CCNode* p = node("ingame_pause")) {
        const CCRect bb = p->boundingBox();
        log("pause button %.0f,%.0f", bb.getMidX(), bb.getMidY());
    }
}

void SurgeryScreen::hudFrame() {
    // (the port's) The bar compact while an injury still to treat has a
    // point where the big bar would be (a few surgeries place some at the
    // very top: the leg veins' far clamp spots, Emoshun's black heart ring;
    // a point done may still be worked on, a clamp's wheel turned), big again
    // once none has; changed between touches only.
    if (m_topBar && m_hud && !m_runs.empty() && m_hudCheck % 10 == 0 && !m_touchDown && m_brutalityTimer <= 0 &&
        !m_injectionMenu) {
        const float c = ResolutionMgr::sharedResolutionMgr()->canvasScale;
        bool crowded = false;
        for (const InjuryState* inj : run().injuries) {
            if (crowded || inj->finished || !inj->root || !inj->root->isVisible()) continue;
            for (const HitAreaState& a : inj->areas)
                for (const HitPointState& p : a.points)
                    if (p.position.y >= m_barBigBottom - kCrowdMargin * c &&
                        p.position.x >= m_barLeft - kCrowdMargin * c)
                        crowded = true;
        }
        if (crowded != m_barCompact) {
            m_barCompact = crowded;
            log("hud bar %s", crowded ? "compact" : "big");
            layoutHud(true);
        }
    }
    // (debug: every point still to treat that lies under a HUD button is
    // traced once, "hud covers ...": the play-tests prove there is none)
    if (platform::debugMode() && m_hud && !m_runs.empty() && m_hudCheck % 10 == 0) {
        static const SurgeryScreen* seenIn = nullptr;
        static std::vector<std::pair<const InjuryState*, int>> seen;
        if (seenIn != this) seenIn = this, seen.clear();
        std::vector<std::pair<std::string, CCRect>> covers;
        for (int t = 0; t <= kToolCount + 2; ++t) {
            const std::string n = t < kToolCount        ? std::string("ingame_tool_") + kToolNames[t]
                                  : t == kToolCount     ? std::string("ingame_tool_Injection_out")
                                  : t == kToolCount + 1 ? std::string("ingame_pause")
                                                        : std::string("ingame_tagpartner");
            CCNode* b = ui.node(n);
            bool shown = b != nullptr;
            for (CCNode* p = b; p && shown; p = p->getParent()) shown = p->isVisible();
            if (!shown) continue;
            // (where it takes a touch: its card, else the whole item)
            covers.emplace_back(n, touchBounds(b));
        }
        auto covered = [&](const CCPoint& q) {
            for (const auto& c : covers)
                if (c.second.containsPoint(q)) return true;
            return false;
        };
        const CCSize win = CCDirector::sharedDirector()->getWinSize();
        for (const InjuryState* inj : run().injuries) {
            if (inj->finished || !inj->root || !inj->root->isVisible()) continue;
            int k = 0;
            for (const HitAreaState& a : inj->areas)
                for (const HitPointState& p : a.points) {
                    ++k;
                    if (p.done) continue;
                    for (const auto& c : covers) {
                        if (!c.second.containsPoint(p.position)) continue;
                        if (std::find(seen.begin(), seen.end(), std::make_pair(inj, k)) != seen.end()) break;
                        seen.emplace_back(inj, k);
                        // reachable: a touch can begin within its reach (its
                        // rect) off every button, on the screen; else only a
                        // stroke begun elsewhere passes over it
                        bool reach = false;
                        for (int i = 0; i <= 8 && !reach; ++i)
                            for (int j = 0; j <= 8 && !reach; ++j) {
                                const CCPoint q = ccp(p.rect.origin.x + p.rect.size.width * i / 8.0f,
                                                      p.rect.origin.y + p.rect.size.height * j / 8.0f);
                                reach = q.x >= 0 && q.y >= 0 && q.x < win.width && q.y < win.height && !covered(q);
                            }
                        log("hud covers #%d %s point %.0f,%.0f under %s, %s", inj->soundChannel, inj->def->name.c_str(),
                            p.position.x, p.position.y, c.first.c_str(), reach ? "reachable beside it" : "UNREACHABLE");
                        break;
                    }
                }
        }
    }
    syncLongCards();
    // The layout followed (each half second; at rest only).
    if (++m_hudCheck % 30 != 0) return;
    if (!m_hud || wantTopBar() == m_topBar) return;
    if (m_brutalityTimer > 0 || m_injectionMenu) return;
    layoutHud(!m_topBar);
}

void SurgeryScreen::toolKey(int n) {
    // (PC: the number keys) the tool in that place of the bar (the
    // injection: to inject)
    if (n < 1 || n > kToolCount) return;
    keyPress(std::string("ingame_tool_") + kToolNames[kBar[n - 1]], kKeyNames[n - 1]);
}

void SurgeryScreen::letterKey(char c) {
    // (PC: a letter) I: the injection, to draw out
    if (c == kOutKey || c == kOutKey - 'a' + 'A') keyPress("ingame_tool_Injection_out", kOutKeyName);
}

void SurgeryScreen::keyPress(const std::string& button, const char* key) {
    // not under another screen, nor once it is over
    if (m_over || !m_hud || SceneMgr::sharedSceneMgr()->topScene() != this) return;
    CCNode* target = ui.node(button);
    if (!target) return;
    // an injection's mode beside its place, the menu shut: the menu opened
    // first (the button in its place pressed), as a player would
    if (target == m_injectionSecond && !m_injectionMenu && !press(m_injectionFirst)) return;
    // as the menu takes a click: only on a button shown and on, in a menu on
    if (press(target)) log("tool key %s %s", key, button.c_str());  // (for test scripts)
}

bool SurgeryScreen::escapeKey() {
    // (PC: Escape, a phone's back key) on the surgery, its pause button; on
    // its pause screen, that screen's back button (to the surgery)
    if (m_over || !m_hud) return false;
    Scene* top = SceneMgr::sharedSceneMgr()->topScene();
    if (top == this) {
        const bool pressed = press(ui.node("ingame_pause"));
        if (pressed) log("escape: pause");  // (for test scripts)
        return pressed;
    }
    if (dynamic_cast<PauseScreen*>(top)) {
        const bool pressed = press(top->ui.node("ui_button_back"));
        if (pressed) log("escape: back to the surgery");
        return pressed;
    }
    return false;
}

bool SurgeryScreen::pointerShown(int& tool, bool& drawOut) {
    tool = m_tool ? m_tool->type : -1;
    drawOut = tool == kInjection && static_cast<const Injection*>(m_tool)->mode == 0;
    return !m_over && m_hud && SceneMgr::sharedSceneMgr()->topScene() == this;
}

}  // namespace mt
