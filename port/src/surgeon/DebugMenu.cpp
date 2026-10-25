#include "surgeon/DebugMenu.h"

#include "engine/AdvancedImage.h"
#include "platform/Platform.h"
#include "surgeon/SurgeonGame.h"

namespace {
constexpr float kMargin = 12, kButtonW = 112, kButtonH = 26, kGap = 6;
}  // namespace

std::vector<DebugMenu::Placed> DebugMenu::layout() {
    std::vector<Placed> out;
    float x = kMargin, y = 40;
    float width = static_cast<float>(platform::game().screenWidth);
    for (const Item& item : game.debugItems()) {
        if (item.newRow || x + kButtonW > width - kMargin) {
            x = kMargin;
            y += kButtonH + kGap;
        }
        out.push_back({item, x, y, kButtonW, kButtonH});
        x += kButtonW + kGap;
    }
    return out;
}

void DebugMenu::draw() {
    if (!open) return;
    float w = static_cast<float>(platform::game().screenWidth), h = static_cast<float>(platform::game().screenHeight);
    AdvancedImage::drawRectangleWithXYWHA(0, 0, w, h, 0.85f, 0.1f, 0.1f, 0.12f);
    VectorArt& art = game.art();
    art.drawText(labelText, "DEBUG MENU (F1 closes)", w / 2, 10, 1.2f, reinterpret_cast<uintptr_t>(this));
    unsigned long long owner = reinterpret_cast<uintptr_t>(this) + 1;
    for (const Placed& p : layout()) {
        float c = p.item.enabled ? (p.item.on ? 0.55f : 0.32f) : 0.18f;
        AdvancedImage::drawRectangleWithXYWHA(p.x, p.y, p.w, p.h, 1, c, c, p.item.on ? 0.2f : c);
        art.drawText(labelText, p.item.label, p.x + p.w / 2, p.y + 5, 0.9f, owner++);
    }
}

void DebugMenu::touchUp(float x, float y) {
    if (!open) return;
    for (const Placed& p : layout()) {
        if (x < p.x || x > p.x + p.w || y < p.y || y > p.y + p.h || !p.item.enabled) continue;
        if (!p.item.keepOpen) open = false;
        if (p.item.action) p.item.action();
        return;
    }
}
