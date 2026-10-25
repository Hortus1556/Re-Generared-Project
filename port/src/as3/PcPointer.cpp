#include "as3/PcPointer.h"

#include <algorithm>

#include "as3/Log.h"
#include "as3/SurgeryScreen.h"
#include "as3/ToolArt.h"
#include "engine/VectorArt.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCTexture2D.h"
#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/GL.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

const char* const kPackage = "flash-pointer/library.json";
constexpr unsigned long long kOwner = 0x50434b50ull << 8;  // (its draws, for smooth motion)
// The Flash stage's height in AS2 (600x460, shown at the screen's height):
// the cursor is as big on AS3's screen as on AS2's.
constexpr float kFlashStageHeight = 460;

// The tool held, inside the cursor's circle as AS2's cursor frames have
// theirs (its plain frame, "nothing", is the circle and the tip): AS3's own
// tool, without its card (ToolArt.h), the circle about its solid part
// (its shadow left out) on the cursor's circle, kToolRadius Flash pixels
// (shape 2910: the tip at 0,0, the circle's middle 16.75,14.5, its radius
// 12, its outline 1.25 of it), so every tool is centred and none pokes out.
constexpr float kCircleX = 16.75f, kCircleY = 14.5f, kToolRadius = 10.25f;

// A tool's picture (its bounds), its circle's middle at (x, y) and its
// radius `radius`, y down.
void drawTool(CCTexture2D* tex, const toolart::ToolLayout& place, float x, float y, float radius) {
    if (!tex || place.circleRadius <= 0) return;
    const float k = radius / place.circleRadius;
    const CCRect& b = place.bounds;
    const float x0 = x + (b.origin.x - place.circleMiddle.x) * k, y0 = y + (b.origin.y - place.circleMiddle.y) * k;
    const float x1 = x0 + b.size.width * k, y1 = y0 + b.size.height * k;
    const float W = static_cast<float>(tex->getPixelsWide()), H = static_cast<float>(tex->getPixelsHigh());
    const float l = b.origin.x / W, r = b.getMaxX() / W, t = b.origin.y / H, bt = b.getMaxY() / H;
    const float xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    const float uv[8] = {l, t, r, t, l, bt, r, bt};
    const float white[4] = {1, 1, 1, 1};
    platform::drawQuad(kOwner + 1, tex->getName(), xy, uv, white, tex->hasPremultipliedAlpha() ? GL_ONE : GL_SRC_ALPHA);
}

void drawPointer() {
    static int state = 0;  // 0 not tried, 1 the art there, -1 missing
    static int cursor = -1;
    static long age = 0;
    int tool = -1;
    bool drawOut = false;
    SurgeryScreen* s = platform::touchDevice() ? nullptr : SurgeryScreen::running();
    bool show = s && s->pointerShown(tool, drawOut);
    if (show && state == 0) {
        state = assets::exists(kPackage) ? 1 : -1;
        if (state < 0) logError("pointer art %s missing: the system's pointer instead", kPackage);
        else cursor = VectorArt::library(kPackage).symbol("cursor");
    }
    show = show && state > 0 && cursor >= 0;
    // (the system's pointer hidden while the game's shows)
    platform::setSystemCursorVisible(!show);
    if (!show) return;
    VectorArt& art = VectorArt::library(kPackage);
    const int frame = std::max(1, art.frameOf(cursor, "nothing"));
    float px, py;
    platform::livePointer(px, py);
    // (the game's points, y down, as the pointer's)
    const CCSize win = CCDirector::sharedDirector()->getWinSize();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, win.width, win.height, 0, -1024, 1024);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    {
        platform::PointerArt pointer(px, py);
        VectorTransform t;
        t.x = px;
        t.y = py;
        t.scaleX = t.scaleY = win.height / kFlashStageHeight;
        art.draw(cursor, frame, ++age, t, kOwner);
        const std::string name = toolart::ofTool(tool, drawOut);
        toolart::ToolLayout place;
        if (!name.empty() && toolart::layout(name, place))
            drawTool(toolart::tool(name, 0), place, px + kCircleX * t.scaleX, py + kCircleY * t.scaleY, kToolRadius * t.scaleX);
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace

void pointerInstall() {
    if (platform::touchDevice()) return;  // (no pointer: the finger is it)
    CCDirector::sharedDirector()->setPointerOverlay(drawPointer);
}

}  // namespace mt
