// The F1 debug menu of the iOS-based games (debug mode only; AS2 on the
// UIKit toolkit, AS3 drawing the window over its cocos scene): a
// panel over the whole window with a button for each item the game gives,
// asked for afresh each time it opens (so they can depend on the screen).
// A click runs the item and closes the panel; a switch stays open, its
// button lit while on. Clicks on the panel go nowhere else.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "engine/ui/UIKit.h"

namespace ui {

struct DebugItem {
    std::string label;
    std::function<void()> action;
    bool enabled = true;
    bool isSwitch = false;  // stays open; `on` lights it
    bool on = false;
    bool newRow = false;    // starts a new row
    bool caption = false;   // a caption (text on the panel), not a button
    float width = 150;      // points
};

// The panel's measures, in window points: the defaults suit a window of
// about 1024x768 (AS2's); a small one (AS3's 568x320) gives smaller ones.
struct DebugPanelStyle {
    float margin = 24, gap = 8, rowHeight = 40;
    float fontSize = 15, titleSize = 20;
};

class DebugPanel : public View, public ActionTarget {
public:
    explicit DebugPanel(std::function<std::vector<DebugItem>()> items, DebugPanelStyle style = {});
    ~DebugPanel() override;

    // Opened on top of the window, or closed.
    void toggle();
    bool isOpen() const { return superview() != nullptr; }

    void performAction(const std::string& selector, View* sender) override;

private:
    void build();
    std::function<std::vector<DebugItem>()> items_;
    DebugPanelStyle style_;
    std::vector<DebugItem> shown_;
};

}  // namespace ui
