#include "engine/ui/DebugPanel.h"

#include <string>

#include "platform/Platform.h"

namespace ui {

namespace {

TextView* label(View* parent, const std::string& text, float size, Rect r) {
    TextView* t = new TextView;
    t->text = text;
    t->fontName = "Helvetica-Bold";
    t->fontSize = size;
    t->textColor = {1, 1, 1, 1};
    t->textAlignment = TextView::kCenterAlign;
    t->inset = 0;
    t->userInteractionEnabled = false;
    t->setFrame(r);
    parent->addSubview(t);
    t->release();
    return t;
}

}  // namespace

DebugPanel::DebugPanel(std::function<std::vector<DebugItem>()> items, DebugPanelStyle style)
    : items_(std::move(items)), style_(style) {
    className = "DebugPanel";
    setBackgroundColor({0, 0, 0, 0.8f});
}

DebugPanel::~DebugPanel() = default;

void DebugPanel::toggle() {
    if (isOpen()) {
        removeFromSuperview();
        return;
    }
    Window& w = Window::shared();
    // (the game's screen as it is now: a game may change its size, AS3)
    w.bounds = {static_cast<float>(platform::screenWidth()), static_cast<float>(platform::screenHeight())};
    w.center = {w.bounds.width / 2, w.bounds.height / 2};
    setFrame({{0, 0}, w.bounds});
    build();
    w.addSubview(this);
}

void DebugPanel::build() {
    while (subviews().count() > 0) subviews()[subviews().count() - 1]->removeFromSuperview();
    shown_ = items_();
    const float kMargin = style_.margin, kGap = style_.gap, kHeight = style_.rowHeight;
    const float titleH = style_.titleSize * 1.3f;
    label(this, "Debug menu (F1)", style_.titleSize, {{kMargin, kMargin}, {bounds.width - 2 * kMargin, titleH}});
    float x = kMargin, y = kMargin + 2 * style_.titleSize;
    // a line of text and where it sits in a row (15-point text: 22 high, 10.5 down a 40-point row)
    const float textH = style_.fontSize * 22 / 15, textY = (kHeight - style_.fontSize * 19 / 15) / 2;
    for (size_t i = 0; i < shown_.size(); ++i) {
        const DebugItem& it = shown_[i];
        if (i > 0 && (it.newRow || x + it.width > bounds.width - kMargin)) {
            x = kMargin;
            y += kHeight + kGap;
        }
        if (it.caption) {
            // text on the panel, left-aligned in its place
            TextView* t = label(this, it.label, style_.fontSize, {{x, y + textY}, {it.width, textH}});
            t->textAlignment = TextView::kLeft;
            t->textColor = {1, 0.85f, 0.4f, 1};
            x += it.width + kGap;
            continue;
        }
        Button* b = new Button;
        b->tag = static_cast<int>(i);
        b->setFrame({{x, y}, {it.width, kHeight}});
        b->enabled = it.enabled;
        // Grey; lit green when a switch is on; faded when it cannot be used.
        Color c = it.isSwitch && it.on ? Color{0.15f, 0.55f, 0.2f, 1} : Color{0.25f, 0.25f, 0.28f, 1};
        if (!it.enabled) c.a = 0.4f;
        b->setBackgroundColor(c);
        b->addTarget(this, "item", Button::kTouchUpInside);
        addSubview(b);
        b->release();
        TextView* t = label(b, it.label, style_.fontSize, {{0, textY}, {it.width, textH}});
        if (!it.enabled) t->textColor.a = 0.5f;
        x += it.width + kGap;
    }
}

void DebugPanel::performAction(const std::string&, View* sender) {
    size_t i = static_cast<size_t>(sender->tag);
    if (i >= shown_.size() || !shown_[i].enabled) return;
    std::function<void()> run = shown_[i].action;
    bool stay = shown_[i].isSwitch;
    retainRef(this);
    if (stay) {
        if (run) run();
        build();  // the switch shown as it is now
    } else {
        removeFromSuperview();
        if (run) run();
    }
    releaseRef(this);
}

}  // namespace ui
