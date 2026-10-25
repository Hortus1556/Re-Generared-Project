// The surgery editor's buttons and panels (docs/as3.md, "Surgery editor").
// The original's editor was a developer's tool drawn with plain cocos menu
// labels in Arial (EditorScreen::initUI 0x447400, EditorMenu::init
// 0x43aa2c, the popups' init); the game has no art for it. The port draws
// it in the game's own manner instead: plates with the game's thick dark
// outline and bright fills, lettered in its font (Londrina Solid, the
// .fnt files of 4.0x/), so it reads as part of the game. They work with
// the mouse and by touch: a press shows, a tap acts when let go on the
// same button, a drag off cancels.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "engine/cocos/CCDrawNode.h"
#include "engine/cocos/CCLabelBMFont.h"
#include "as3/Scene.h"
#include "engine/cocos/CCNode.h"

namespace mt {
namespace editor {

// The plates' colours (fill), from the game's buttons.
enum class Tone { Orange, Green, Blue, Red, Purple, Grey, Dark, Yellow };

// A rounded rectangle (w x h, origin at its bottom left) with a dark
// outline, its fill lighter at the top: the game's plates.
cocos2d::CCDrawNode* plate(float w, float h, Tone tone, float radius = 6);
// A label in a game font, its height `size` points, shrunk to `maxWidth`.
cocos2d::CCLabelBMFont* text(const std::string& s, float size, float maxWidth = 0,
                             const char* font = "font-UIGlobalsSmall");

class Button : public cocos2d::CCNode {
public:
    static Button* create(const std::string& label, float w, float h, Tone tone, std::function<void()> tap,
                          float textSize = 12);
    void setLabel(const std::string& label);
    const std::string& label() const { return m_label; }
    void setTone(Tone tone);
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    void setPressed(bool on);
    bool hit(const cocos2d::CCPoint& world);  // the point (cocos, world) on it, it and its parents shown
    std::function<void()> onTap;
    // optional: a press that begins a drag (the palette's cells); called with
    // the point as the finger moves off it, once
    std::function<void(const cocos2d::CCPoint&)> onDragOut;
    void addIcon(cocos2d::CCNode* icon);  // a picture inside, centred

private:
    void rebuild();
    std::string m_label;
    float m_w = 0, m_h = 0, m_textSize = 12;
    Tone m_tone = Tone::Orange;
    bool m_enabled = true, m_pressed = false;
    cocos2d::CCDrawNode* m_plate = nullptr;
    cocos2d::CCLabelBMFont* m_text = nullptr;
};

// A set of buttons that take touches: the editor's bars and each popup.
// Modal: while shown it takes every touch (outside its buttons, nothing).
class Panel : public cocos2d::CCNode {
public:
    static Panel* create(bool modal);
    Button* add(Button* b, float x, float y);  // placed by its bottom left, in the panel's space
    void clearButtons();
    // a touch (cocos points): true if this panel took it
    bool touchBegan(const cocos2d::CCPoint& p);
    void touchMoved(const cocos2d::CCPoint& p);
    void touchEnded(const cocos2d::CCPoint& p);
    bool isModal() const { return m_modal; }
    bool tracking() const { return m_down != nullptr; }
    std::vector<Button*> buttons;

protected:
    bool m_modal = false;

private:
    Button* m_down = nullptr;
    bool m_draggedOut = false;
};

// A popup: the screen shaded, a plate in the middle with a title, and a
// panel of buttons over it.
class Popup : public Panel {
public:
    static Popup* create(const std::string& title, float w, float h, Tone tone = Tone::Blue);
    float width() const { return m_w; }
    float height() const { return m_h; }
    // places by the plate's bottom left
    Button* put(Button* b, float x, float y) { return add(b, m_x + x, m_y + y); }
    cocos2d::CCNode* putNode(cocos2d::CCNode* n, float x, float y);
    cocos2d::CCLabelBMFont* putText(const std::string& s, float x, float y, float size, float maxWidth,
                                    bool centred = false);
    void setTitle(const std::string& title);

private:
    float m_x = 0, m_y = 0, m_w = 0, m_h = 0;
    cocos2d::CCLabelBMFont* m_title = nullptr;
};

// A screen of the editor's: its bars (panels) and a stack of popups over
// them, the touches given to the top popup, else to the bars, else to the
// screen itself (its canvas). Escape closes the top popup, else goes back.
class PanelScene : public Scene {
public:
    void load() override;
    void onRemoved() override;
    ~PanelScene() override;
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void keyBackClicked() override;

    void addPanel(Panel* panel, int z);
    void pushPopup(Popup* popup);
    void popPopup();
    void closePopups();
    bool popupOpen() const { return !m_popups.empty(); }
    Popup* topPopup() { return m_popups.empty() ? nullptr : m_popups.back(); }
    // a message with OK; a question with yes/no (`onYes` on yes)
    void message(const std::string& title, const std::string& body, std::function<void()> onOk = nullptr);
    void confirm(const std::string& title, const std::string& body, const std::string& yes,
                 std::function<void()> onYes);
    // a line of text typed (the keyboard; Paste puts the clipboard's in)
    void askText(const std::string& title, const std::string& initial, size_t maxLen,
                 std::function<void(const std::string&)> done);

protected:
    virtual bool canvasBegan(const cocos2d::CCPoint&) { return false; }
    virtual void canvasMoved(const cocos2d::CCPoint&) {}
    virtual void canvasEnded(const cocos2d::CCPoint&) {}
    virtual void back() {}
    // the touch going on is the canvas's from now (a palette's cell dragged out)
    void touchToCanvas() {
        m_touched = nullptr;
        m_canvasTouch = true;
    }
    cocos2d::CCNode* m_popupLayer = nullptr;  // over everything

private:
    std::vector<Panel*> m_panels;
    std::vector<Popup*> m_popups;
    std::vector<Popup*> m_closed;  // taken off, freed at the next touch (a button may be running)
    Panel* m_touched = nullptr;
    bool m_canvasTouch = false;
};

// The keyboard's typing while a text is asked for (AppDelegate forwards it).
void keyboardText(const char* utf8);
void keyboardKey(int key);  // 0 backspace, 1 enter, 2 escape

}  // namespace editor
}  // namespace mt
