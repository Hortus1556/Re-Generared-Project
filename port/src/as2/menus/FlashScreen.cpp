#include "as2/menus/FlashScreen.h"

#include "as2/menus/FlashStage.h"
#include "platform/Platform.h"

#include <cstdio>

namespace as2 {

namespace {

constexpr unsigned long long kOwner = 0x464C534300000000ull;  // "FLSC"

bool inside(const flash::Rect& r, Point p) {
    return !r.empty() && p.x >= r.xMin && p.x <= r.xMax && p.y >= r.yMin && p.y <= r.yMax;
}

}  // namespace

FlashScreen::FlashScreen(const std::string& package, const std::string& symbolName)
    : ViewController(std::string()), art(flashstage::art(package)) {
    symbol = art.symbol(symbolName);
    main.frames = symbol >= 0 ? art.frameCount(symbol) : 1;
    main.playing = true;
}

void FlashScreen::prefetch(const std::string& package, const std::string& symbolName, Point placeAt,
                           const VectorLook* look) {
    VectorArt& art = flashstage::art(package);
    int symbol = art.symbol(symbolName);
    if (symbol < 0) return;
    for (int f = 1, n = art.frameCount(symbol); f <= n; ++f)
        art.prefetch(symbol, f, f, 0, flashstage::at(placeAt.x, placeAt.y), kOwner, look, 40 * 60);
}

void FlashScreen::viewDidLoad() {
    // The clip is drawn by a view over the whole screen (as the EAGLView
    // draws the game's scene).
    ui::GLView* g = new ui::GLView;
    g->bounds = view()->bounds;
    g->center = {view()->bounds.width / 2, view()->bounds.height / 2};
    g->userInteractionEnabled = false;
    g->drawHook = [this] { draw(); };
    view()->addSubview(g);
    releaseRef(g);
}

FlashScreen::Clip& FlashScreen::inner(const std::string& path) {
    auto it = inners.find(path);
    if (it != inners.end()) return it->second;
    Clip& c = inners[path];
    int s = art.innerSymbol(symbol, main.frame, age, "/" + path);
    c.frames = s >= 0 ? art.frameCount(s) : 1;
    return c;
}

void FlashScreen::step() {
    // Flash frames at 30 a second of the pass clock (at most a few at once
    // after a stall).
    double now = platform::passTime();
    if (last < 0) last = now;
    carry = std::min(carry + (now - last) * flashstage::kFps, 4.0);
    last = now;
    auto advance = [](Clip& c) {
        ++c.inFrame;
        if (!c.playing) return;
        if (++c.frame > c.frames) c.frame = 1;
        c.inFrame = 0;
        if (c.stops.count(c.frame)) c.playing = false;
    };
    while (carry >= 1) {
        carry -= 1;
        ++age;
        advance(main);
        for (auto& kv : inners) advance(kv.second);
        onFrame();
    }
    traceRest();
}

void FlashScreen::traceRest() {
    // (Debug: once its clips rest, where each piece that takes clicks is,
    // for the play-tests: "flash piece <path> at x,y", again as they change.)
    if (!platform::debugMode()) return;
    bool rest = !main.playing || main.frames == 1;
    for (auto& kv : inners) rest = rest && !kv.second.playing;
    if (!rest) return;
    std::string pieces;
    VectorLook l = look();
    VectorTransform at = flashstage::at(placeAt.x, placeAt.y);
    for (const std::string& part : clickable()) {
        flash::Rect r = art.partBounds(symbol, main.frame, age, main.inFrame, at, &l, part);
        if (r.empty()) continue;
        char b[64];
        std::snprintf(b, sizeof b, " at %.0f,%.0f", (r.xMin + r.xMax) / 2, (r.yMin + r.yMax) / 2);
        pieces += "flash piece " + part + b + "\n";
    }
    if (pieces == traced) return;
    traced = pieces;
    size_t p = 0;
    for (size_t q; (q = pieces.find('\n', p)) != std::string::npos; p = q + 1)
        platform::debugTrace("%s", pieces.substr(p, q - p).c_str());
}

VectorLook FlashScreen::look() {
    VectorLook l;
    // (By their exact paths: "help" is not "holder.help".)
    for (auto& kv : inners) {
        l.frames["/" + kv.first] = kv.second.frame;
        l.inFrames["/" + kv.first] = kv.second.inFrame;
    }
    dress(l);
    // The button under the pointer: over, or down while pressed.
    float px, py;
    platform::livePointer(px, py);
    int b = art.buttonAt(symbol, main.frame, age, main.inFrame, flashstage::at(placeAt.x, placeAt.y), px, py, &l);
    if (b >= 0) l.buttons[b] = pressed.empty() ? 2 : 4;
    hovering = b >= 0;
    return l;
}

void FlashScreen::draw() {
    if (symbol < 0) return;
    step();
    VectorLook l = look();
    if (hovering) flashstage::wantHand();
    VectorTransform at = flashstage::at(placeAt.x, placeAt.y);
    drawUnder(at);
    art.draw(symbol, main.frame, age, main.inFrame, at, kOwner, &l);
    drawOver(at);
}

void FlashScreen::writeIn(const std::string& field, const std::string& text, const flash::Rect& where, float size,
                          const VectorTransform& at, const VectorLook& look) {
    flash::Matrix local, inSymbol;
    int id = -1;
    if (where.empty() || !art.partInfo(symbol, main.frame, age, main.inFrame, &look, field, local, inSymbol, id)) return;
    auto it = art.movie().editTexts.find(id);
    if (it == art.movie().editTexts.end()) return;
    const float scale = size * at.scaleX;
    // (In a box wide enough for any name, the field's own being for its
    // text; drawText centres the box, its text at the box's left, or
    // right.)
    constexpr float kBox = 320;  // the field's pixels
    const float half = kBox * scale / 2;
    float x = it->second.align == 1 ? where.xMax - half : where.xMin + half;
    art.drawText(id, text, x, where.yMin, scale, kOwner + 7, kBox);
}

std::string FlashScreen::pieceAt(Point p) {
    VectorLook l = look();
    for (const std::string& part : clickable())
        if (inside(art.partBounds(symbol, main.frame, age, main.inFrame, flashstage::at(placeAt.x, placeAt.y), &l, part), p)) return part;
    return std::string();
}

void FlashScreen::touchesBegan(const ui::Touches& t) {
    if (t.empty()) return;
    pressed = pieceAt(t[0].locationInView(view()));
    if (!pressed.empty()) onPress(pressed);
}

void FlashScreen::touchesEnded(const ui::Touches& t) {
    std::string was = pressed;
    pressed.clear();
    if (!t.empty() && !was.empty() && pieceAt(t[0].locationInView(view())) == was) onClick(was);
}

}  // namespace as2
