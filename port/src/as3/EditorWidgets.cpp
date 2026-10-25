#include "as3/EditorWidgets.h"

#include <algorithm>
#include <cmath>

#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/UI.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCTouch.h"
#include "platform/Platform.h"

namespace mt {
namespace editor {

using namespace cocos2d;

namespace {

struct Rgb {
    float r, g, b;
};

Rgb toneColour(Tone t) {
    switch (t) {
    case Tone::Orange: return {0.96f, 0.45f, 0.12f};
    case Tone::Green: return {0.42f, 0.74f, 0.16f};
    case Tone::Blue: return {0.16f, 0.55f, 0.84f};
    case Tone::Red: return {0.84f, 0.16f, 0.14f};
    case Tone::Purple: return {0.52f, 0.27f, 0.69f};
    case Tone::Grey: return {0.55f, 0.55f, 0.55f};
    case Tone::Dark: return {0.20f, 0.22f, 0.25f};
    case Tone::Yellow: return {0.98f, 0.78f, 0.14f};
    }
    return {0.5f, 0.5f, 0.5f};
}

// a rounded rectangle's outline as a convex polygon
std::vector<CCPoint> rounded(float x, float y, float w, float h, float r) {
    r = std::max(0.0f, std::min(r, std::min(w, h) * 0.5f));
    std::vector<CCPoint> p;
    const float cx[4] = {x + w - r, x + r, x + r, x + w - r}, cy[4] = {y + h - r, y + h - r, y + r, y + r};
    for (int c = 0; c < 4; ++c)
        for (int k = 0; k <= 4; ++k) {
            float a = static_cast<float>(M_PI) * 0.5f * (static_cast<float>(c) + static_cast<float>(k) / 4.0f);
            p.push_back(ccp(cx[c] + std::cos(a) * r, cy[c] + std::sin(a) * r));
        }
    return p;
}

void fill(CCDrawNode* d, std::vector<CCPoint> p, ccColor4F c) {
    // (premultiplied: the node blends ONE, ONE_MINUS_SRC_ALPHA)
    c.r *= c.a;
    c.g *= c.a;
    c.b *= c.a;
    d->drawPolygon(p.data(), static_cast<unsigned int>(p.size()), c, 0, c);
}

}  // namespace

CCDrawNode* plate(float w, float h, Tone tone, float radius) {
    CCDrawNode* d = CCDrawNode::create();
    Rgb c = toneColour(tone);
    // the game's outline, a darker band at the bottom, the fill, a shine
    fill(d, rounded(0, 0, w, h, radius), ccc4f(0.11f, 0.09f, 0.08f, 1));
    fill(d, rounded(2, 2, w - 4, h - 4, radius - 2), ccc4f(c.r * 0.62f, c.g * 0.62f, c.b * 0.62f, 1));
    fill(d, rounded(2, 4.5f, w - 4, h - 6.5f, radius - 2), ccc4f(c.r, c.g, c.b, 1));
    if (h > 14 && h < 50 && tone != Tone::Dark)
        fill(d, rounded(5, h * 0.55f, w - 10, h * 0.45f - 5, radius - 3), ccc4f(1, 1, 1, 0.22f));
    d->setContentSize(CCSizeMake(w, h));
    return d;
}

CCLabelBMFont* text(const std::string& s, float size, float maxWidth, const char* font) {
    CCLabelBMFont* l = createLabel(s.empty() ? " " : s.c_str(), font, 0, kCCTextAlignmentCenter);
    if (!l) return nullptr;
    // the font's line, from a line of capitals
    static float line = 0;
    if (line <= 0) {
        CCLabelBMFont* m = createLabel("Ag", "font-UIGlobalsSmall", 0, kCCTextAlignmentCenter);
        line = m && m->getContentSize().height > 0 ? m->getContentSize().height : 16;
    }
    float scale = size * 1.4f / line;  // (the size of its capitals: the line is taller)
    float w = l->getContentSize().width * scale;
    if (maxWidth > 0 && w > maxWidth) scale *= maxWidth / w;
    l->setScale(scale);
    return l;
}

// ---- Button ---------------------------------------------------------------

Button* Button::create(const std::string& label, float w, float h, Tone tone, std::function<void()> tap,
                       float textSize) {
    Button* b = new Button;
    b->init();
    b->autorelease();
    b->m_label = label;
    b->m_w = w;
    b->m_h = h;
    b->m_tone = tone;
    b->m_textSize = textSize;
    b->onTap = std::move(tap);
    b->setContentSize(CCSizeMake(w, h));
    b->rebuild();
    return b;
}

void Button::rebuild() {
    if (m_plate) m_plate->removeFromParentAndCleanup(true);
    if (m_text) m_text->removeFromParentAndCleanup(true);
    m_plate = plate(m_w, m_h, m_enabled ? m_tone : Tone::Grey, std::min(8.0f, m_h * 0.3f));
    addChild(m_plate, 0);
    m_text = nullptr;
    if (!m_label.empty()) {
        m_text = text(m_label, m_textSize, m_w - 8);
        if (m_text) {
            m_text->setPosition(ccp(m_w * 0.5f, m_h * 0.5f + 1));
            if (!m_enabled) m_text->setOpacity(150);
            addChild(m_text, 2);
        }
    }
}

void Button::setLabel(const std::string& label) {
    if (label == m_label) return;
    m_label = label;
    rebuild();
}

void Button::setTone(Tone tone) {
    if (tone == m_tone) return;
    m_tone = tone;
    rebuild();
}

void Button::setEnabled(bool on) {
    if (on == m_enabled) return;
    m_enabled = on;
    rebuild();
}

void Button::setPressed(bool on) {
    m_pressed = on;
    // pressed: pushed in, as the game's "-selected" frames
    setScale(on ? 0.94f : 1.0f);
    setPosition(getPosition());
}

void Button::addIcon(CCNode* icon) {
    if (!icon) return;
    icon->setPosition(ccp(m_w * 0.5f, m_h * 0.5f));
    addChild(icon, 1);
}

bool Button::hit(const CCPoint& world) {
    for (CCNode* n = this; n; n = n->getParent())
        if (!n->isVisible()) return false;
    // the plate's rect, in its own space (scaled about its middle when pressed)
    CCPoint p = convertToNodeSpace(world);
    return p.x >= 0 && p.y >= 0 && p.x <= m_w && p.y <= m_h;
}

// ---- Panel ----------------------------------------------------------------

Panel* Panel::create(bool modal) {
    Panel* p = new Panel;
    p->init();
    p->autorelease();
    p->m_modal = modal;
    return p;
}

Button* Panel::add(Button* b, float x, float y) {
    // anchored in its middle so a press shrinks it about there
    b->setAnchorPoint(ccp(0.5f, 0.5f));
    b->setPosition(ccp(x + b->getContentSize().width * 0.5f, y + b->getContentSize().height * 0.5f));
    addChild(b, 10);
    buttons.push_back(b);
    return b;
}

void Panel::clearButtons() {
    for (Button* b : buttons) b->removeFromParentAndCleanup(true);
    buttons.clear();
    m_down = nullptr;
}

bool Panel::touchBegan(const CCPoint& p) {
    if (!isVisible()) return false;
    for (CCNode* n = getParent(); n; n = n->getParent())
        if (!n->isVisible()) return false;
    m_down = nullptr;
    m_draggedOut = false;
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it) {
        if ((*it)->hit(p)) {
            if (!(*it)->isEnabled()) {
                SoundMgr::sharedSoundMgr()->playSound("button_click_denied", false);
                return true;
            }
            m_down = *it;
            m_down->setPressed(true);
            return true;
        }
    }
    return m_modal;
}

void Panel::touchMoved(const CCPoint& p) {
    if (!m_down) return;
    bool on = m_down->hit(p);
    if (!on && m_down->onDragOut && !m_draggedOut) {
        m_draggedOut = true;
        m_down->setPressed(false);
        Button* b = m_down;
        m_down = nullptr;
        b->onDragOut(p);
        return;
    }
    m_down->setPressed(on);
}

void Panel::touchEnded(const CCPoint& p) {
    Button* b = m_down;
    m_down = nullptr;
    if (!b) return;
    b->setPressed(false);
    if (b->hit(p) && b->onTap) {
        SoundMgr::sharedSoundMgr()->playSound("button_click", false);
        // (a copy: the tap may remove the button)
        std::function<void()> f = b->onTap;
        f();
    }
}

// ---- Popup ----------------------------------------------------------------

Popup* Popup::create(const std::string& title, float w, float h, Tone tone) {
    Popup* p = new Popup;
    p->init();
    p->autorelease();
    const CCSize win = CCDirector::sharedDirector()->getWinSize();
    // (on a screen too small for it, 512 points wide at 4:3, the popup is
    // shrunk about the screen's middle to fit, its touches with it)
    const float k = std::min(1.0f, std::min((win.width - 8) / w, (win.height - 8) / h));
    if (k < 1) {
        p->setScale(k);
        p->setPosition(ccp(win.width * 0.5f * (1 - k), win.height * 0.5f * (1 - k)));
    }
    // the game's popups shade what is under them (GeneralTipPopupScreen: black at half)
    CCLayerColor* shade = CCLayerColor::create(ccc4(0, 0, 0, 140), win.width / k, win.height / k);
    shade->setPosition(ccp(win.width * 0.5f * (1 - 1 / k), win.height * 0.5f * (1 - 1 / k)));
    p->addChild(shade, -2);
    p->m_w = w;
    p->m_h = h;
    p->m_x = std::floor((win.width - w) * 0.5f);
    p->m_y = std::floor((win.height - h) * 0.5f);
    CCDrawNode* back = plate(w, h, Tone::Dark, 10);
    back->setPosition(ccp(p->m_x, p->m_y));
    p->addChild(back, -1);
    CCDrawNode* band = plate(w - 16, 26, tone, 8);
    band->setPosition(ccp(p->m_x + 8, p->m_y + h - 32));
    p->addChild(band, -1);
    p->setTitle(title);
    p->m_modal = true;
    return p;
}

CCNode* Popup::putNode(CCNode* n, float x, float y) {
    n->setPosition(ccp(m_x + x, m_y + y));
    addChild(n, 5);
    return n;
}

CCLabelBMFont* Popup::putText(const std::string& s, float x, float y, float size, float maxWidth, bool centred) {
    CCLabelBMFont* l = text(s, size, maxWidth);
    if (!l) return nullptr;
    l->setAnchorPoint(ccp(centred ? 0.5f : 0.0f, 0.5f));
    putNode(l, x, y);
    return l;
}

void Popup::setTitle(const std::string& title) {
    if (m_title) m_title->removeFromParentAndCleanup(true);
    m_title = text(title, 15, m_w - 30, "font-UIGlobalsSmall");
    if (m_title) {
        m_title->setPosition(ccp(m_x + m_w * 0.5f, m_y + m_h - 19));
        addChild(m_title, 5);
    }
}

// ---- PanelScene -----------------------------------------------------------

namespace {

// the text being typed (askText's), while its popup is up
struct TextField {
    std::string text;
    size_t maxLen = 32;
    CCLabelBMFont* label = nullptr;
    Popup* popup = nullptr;
    std::function<void()> enter, escape;
    void show() {
        if (label) label->setString((text + "_").c_str());
    }
};
TextField* g_field = nullptr;

}  // namespace

void keyboardText(const char* utf8) {
    if (!g_field) return;
    for (const char* c = utf8; *c; ++c) {
        unsigned char ch = static_cast<unsigned char>(*c);
        // (the game's fonts have printable ASCII)
        if (ch >= 32 && ch <= 126 && g_field->text.size() < g_field->maxLen) g_field->text += static_cast<char>(ch);
    }
    g_field->show();
}

void keyboardKey(int key) {
    if (!g_field) return;
    if (key == 0) {
        if (!g_field->text.empty()) g_field->text.pop_back();
        g_field->show();
    } else if (key == 1) {
        if (g_field->enter) g_field->enter();
    } else if (g_field->escape) {
        g_field->escape();
    }
}

void PanelScene::load() {
    m_popupLayer = CCNode::create();
    addChild(m_popupLayer, 1000);
    setTouchRegistered(true);
    CCDirector::sharedDirector()->getKeypadDispatcher()->addDelegate(this);
}

void PanelScene::onRemoved() {
    CCDirector::sharedDirector()->getKeypadDispatcher()->removeDelegate(this);
    if (g_field) {
        platform::stopTextInput();
        delete g_field;
        g_field = nullptr;
    }
}

void PanelScene::addPanel(Panel* panel, int z) {
    addChild(panel, z);
    m_panels.push_back(panel);
}

void PanelScene::pushPopup(Popup* popup) {
    // a touch held on what is under it is let go
    if (m_touched) m_touched->touchEnded(ccp(-1000, -1000));
    m_touched = nullptr;
    m_canvasTouch = false;
    m_popupLayer->addChild(popup, static_cast<int>(m_popups.size()));
    m_popups.push_back(popup);
}

void PanelScene::popPopup() {
    if (m_popups.empty()) return;
    Popup* p = m_popups.back();
    m_popups.pop_back();
    if (m_touched == p) m_touched = nullptr;
    if (g_field && g_field->popup == p) {
        platform::stopTextInput();
        delete g_field;
        g_field = nullptr;
    }
    p->retain();
    p->removeFromParentAndCleanup(true);
    m_closed.push_back(p);
}

PanelScene::~PanelScene() {
    for (Popup* p : m_closed) p->release();
}

void PanelScene::closePopups() {
    while (!m_popups.empty()) popPopup();
}

bool PanelScene::ccTouchBegan(CCTouch* touch, CCEvent*) {
    for (Popup* c : m_closed) c->release();
    m_closed.clear();
    CCPoint p = touch->getLocation();
    m_touched = nullptr;
    m_canvasTouch = false;
    if (!m_popups.empty()) {
        m_touched = m_popups.back();
        m_touched->touchBegan(p);
        return true;
    }
    for (auto it = m_panels.rbegin(); it != m_panels.rend(); ++it)
        if ((*it)->touchBegan(p)) {
            m_touched = *it;
            return true;
        }
    m_canvasTouch = canvasBegan(p);
    return true;
}

void PanelScene::ccTouchMoved(CCTouch* touch, CCEvent*) {
    CCPoint p = touch->getLocation();
    if (m_touched) m_touched->touchMoved(p);
    else if (m_canvasTouch) canvasMoved(p);
}

void PanelScene::ccTouchEnded(CCTouch* touch, CCEvent*) {
    CCPoint p = touch->getLocation();
    Panel* t = m_touched;
    m_touched = nullptr;
    if (t) t->touchEnded(p);
    else if (m_canvasTouch) canvasEnded(p);
    m_canvasTouch = false;
}

void PanelScene::keyBackClicked() {
    if (!m_popups.empty()) {
        SoundMgr::sharedSoundMgr()->playSound("button_click", false);
        popPopup();
    } else {
        back();
    }
}

namespace {

// a popup's text, wrapped to `w` and as large as fits in `h` (the game's tutorial boxes' font)
CCLabelBMFont* bodyText(const std::string& body, float w, float h) {
    const float big = 1.2f;
    CCLabelBMFont* l = createLabel(body.c_str(), "font-TutorialBody01", w / big, kCCTextAlignmentCenter);
    if (!l) return nullptr;
    float lw = std::max(1.0f, l->getContentSize().width), lh = std::max(1.0f, l->getContentSize().height);
    l->setScale(std::min(big, std::min(w / lw, h / lh)));
    return l;
}

}  // namespace

void PanelScene::message(const std::string& title, const std::string& body, std::function<void()> onOk) {
    Popup* p = Popup::create(title, 360, 170, Tone::Blue);
    if (CCLabelBMFont* l = bodyText(body, 330, 92)) p->putNode(l, 180, 90);
    p->put(Button::create("OK", 100, 30, Tone::Green, [this, onOk] {
               popPopup();
               if (onOk) onOk();
           }), 130, 10);
    pushPopup(p);
}

void PanelScene::confirm(const std::string& title, const std::string& body, const std::string& yes,
                         std::function<void()> onYes) {
    Popup* p = Popup::create(title, 360, 160, Tone::Orange);
    if (CCLabelBMFont* l = bodyText(body, 330, 80)) p->putNode(l, 180, 87);
    p->put(Button::create("NO", 100, 30, Tone::Red, [this] { popPopup(); }), 60, 10);
    p->put(Button::create(yes, 120, 30, Tone::Green, [this, onYes] {
               popPopup();
               if (onYes) onYes();
           }), 180, 10);
    pushPopup(p);
}

void PanelScene::askText(const std::string& title, const std::string& initial, size_t maxLen,
                         std::function<void(const std::string&)> done) {
    Popup* p = Popup::create(title, 380, 150, Tone::Purple);
    CCDrawNode* field = plate(340, 34, Tone::Grey, 6);
    p->putNode(field, 20, 62);
    if (g_field) delete g_field;
    g_field = new TextField;
    g_field->text = initial.substr(0, maxLen);
    g_field->maxLen = maxLen;
    g_field->popup = p;
    g_field->label = text(" ", 15, 0, "font-UIGlobalsSmall");
    if (g_field->label) {
        g_field->label->setAnchorPoint(ccp(0, 0.5f));
        p->putNode(g_field->label, 30, 79);
    }
    g_field->show();
    auto finish = [this, done] {
        std::string t = g_field ? g_field->text : "";
        popPopup();
        if (done) done(t);
    };
    g_field->enter = finish;
    g_field->escape = [this] { popPopup(); };
    p->put(Button::create("PASTE", 80, 28, Tone::Blue, [] {
               if (!g_field) return;
               std::string clip = platform::clipboardText();
               std::string clean;
               for (char ch : clip)
                   if (ch >= 32 && ch <= 126) clean += ch;
               g_field->text = clean.substr(0, g_field->maxLen);
               g_field->show();
           }), 20, 12);
    p->put(Button::create("CLEAR", 80, 28, Tone::Grey, [] {
               if (!g_field) return;
               g_field->text.clear();
               g_field->show();
           }), 105, 12);
    p->put(Button::create("CANCEL", 80, 28, Tone::Red, [this] { popPopup(); }), 190, 12);
    p->put(Button::create("OK", 90, 28, Tone::Green, finish), 275, 12);
    pushPopup(p);
    platform::startTextInput();
}

}  // namespace editor
}  // namespace mt
