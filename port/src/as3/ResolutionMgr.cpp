#include "as3/ResolutionMgr.h"

#include <algorithm>

#include "as3/Log.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCFileUtils.h"
#include "platform/Platform.h"

namespace mt {

namespace {

// The design canvas (0x2caf0, 0x2caf4).
constexpr float kCanvasWidth = 640, kCanvasHeight = 384;

// (the port's) The content scale the port runs at (AppDelegate: 4 pixels a
// cocos point, the 4.0x art), and the screen's shapes (sizeForAspect): no
// narrower than 4:3, 512 points wide up to 16:10, then 320 high.
constexpr int kPortContentScale = 4;
constexpr float kNarrowest = 4.0f / 3.0f, kTallBreak = 1.6f;
constexpr int kNarrowestWidth = 512, kLowest = 320;
// The widest the art goes: the surgeries', dialogues' and maps' backgrounds
// are the canvas's 640 points, but some menus' art stops short of its left
// edge, which no original device showed (the upgrades' and achievements'
// machine is black up to canvas x 6, the world map's background starts at
// 2.5, the title's and the partners' and credits' at about 1.8), so a screen
// shows at most canvas x 7 to 633.
constexpr int kWidest = 626;

// The game's known screens in pixels, the longer side first, with their art
// scale and folder (0x3f2850, 0x3f28e8; the folders as 0x1eb8c lists them).
struct KnownScreen {
    int w, h;
    float scale;
    const char* folder;
};
const KnownScreen kScreens[] = {
    {480, 320, 1.0f, "1.0x"},  {960, 640, 2.0f, "2.0x"},   {1136, 640, 2.0f, "2.0x"},  {1024, 768, 2.0f, "2.0x"},
    {2048, 1536, 4.0f, "4.0x"}, {400, 240, 1.0f, "1.0x"},  {432, 240, 1.0f, "1.0x"},   {480, 320, 1.0f, "1.0x"},
    {800, 480, 1.5f, "1.0x"},  {854, 480, 1.5f, "2.0x"},   {960, 540, 1.5f, "2.0x"},   {1024, 600, 2.0f, "2.0x"},
    {1280, 800, 2.5f, "4.0x"}, {1280, 768, 2.0f, "4.0x"},  {1536, 1152, 3.0f, "4.0x"}, {1920, 1152, 3.5f, "4.0x"},
    {1920, 1200, 3.5f, "4.0x"}, {2560, 1536, 4.5f, "4.0x"}, {2560, 1600, 5.0f, "4.0x"},
};
constexpr int kUnknownScreen = 2;  // what an unknown screen is taken for

// (the port's) The package's art folders besides the screen's, looked in for
// what the screen's lacks. The package from iOS 1.45 has none: 3.5x/ held the
// Hospital's art from the Android build 1.43 while the package was iOS 1.02's
// (port/tools/extract_xapk.py, now a reference only); a package made so would
// still work.
const char* const kOtherArtFolders[] = {"3.5x"};

}  // namespace

ResolutionMgr* ResolutionMgr::sharedResolutionMgr() {
    static ResolutionMgr* r = [] {
        ResolutionMgr* m = new ResolutionMgr;
        m->init(480, 320, 1);
        m->rect = CCRectMake(0, 0, 480, 320);
        return m;
    }();
    return r;
}

void ResolutionMgr::init(int w, int h, int contentScale) {
    int longer = w < h ? h : w, shorter = w < h ? w : h;
    int pw = longer * contentScale, ph = shorter * contentScale;
    int index = -1;
    for (int i = 0; i < static_cast<int>(sizeof kScreens / sizeof kScreens[0]); ++i)
        if (kScreens[i].w == pw && kScreens[i].h == ph) {
            index = i;
            break;
        }
    if (index < 0 && contentScale == kPortContentScale) {
        // (the port's) A screen of any shape (fitWindow) at the port's content
        // scale: the iPad retina's art (4.0x), a canvas point a cocos point.
        scale = static_cast<float>(kPortContentScale);
        folder = "4.0x";
    } else {
        if (index < 0) {
            logError("Unknown resolution %d %d", pw, ph);
            index = kUnknownScreen;
        }
        scale = kScreens[index].scale;
        folder = kScreens[index].folder;
    }
    canvasScale = scale / static_cast<float>(contentScale);
    width = w;
    height = h;
    pixelsWide = h < w ? pw : ph;
    pixelsHigh = h < w ? ph : pw;
}

void ResolutionMgr::sizeForAspect(float aspect, int& w, int& h) {
    // (the original's table runs from the iPad's 4:3 to the iPhone 5's 16:9;
    // the port goes on to the canvas's whole width)
    const float a = std::max(aspect, kNarrowest);
    if (a <= kTallBreak) {
        // 4:3 to 16:10: the 512 points the iPad shows, as high as the shape
        // makes it (the canvas's 384 at 4:3)
        w = kNarrowestWidth;
        h = std::min(static_cast<int>(kCanvasHeight), static_cast<int>(kNarrowestWidth / a + 0.01f));
    } else {
        // 16:10 to about 2:1: the 320 points every phone shows, as wide as
        // the shape makes it, up to the widest the art goes
        h = kLowest;
        w = std::min(kWidest, static_cast<int>(kLowest * a + 0.01f));
    }
}

bool ResolutionMgr::fitWindow() {
    const float aspect = platform::windowAspect();
    int w = 0, h = 0;
    sizeForAspect(aspect, w, h);
    cocos2d::CCDirector* director = cocos2d::CCDirector::sharedDirector();
    const cocos2d::CCSize win = director->getWinSize();
    if (w == width && h == height && static_cast<int>(win.width) == w && static_cast<int>(win.height) == h) return false;
    director->setDesignResolutionSize(static_cast<float>(w), static_cast<float>(h));
    init(w, h, static_cast<int>(director->getContentScaleFactor()));
    log("screen %dx%d canvas points (window shape %.3f)", w, h, aspect);
    return true;
}

void ResolutionMgr::setRect(int x, int y, int w, int h) {
    rect = CCRectMake(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h));
}

std::string ResolutionMgr::artPath(const char* name, const char* ext) const {
    std::string own = folder + "/" + name + "." + ext;
    if (cocos2d::CCFileUtils::sharedFileUtils()->isFileExist(own)) return own;
    std::string other = supplementPath(name, ext);
    return other.empty() ? own : other;
}

std::string ResolutionMgr::supplementPath(const char* name, const char* ext) const {
    for (const char* f : kOtherArtFolders) {
        if (folder == f) continue;
        std::string p = std::string(f) + "/" + name + "." + ext;
        if (cocos2d::CCFileUtils::sharedFileUtils()->isFileExist(p)) return p;
    }
    return "";
}

void setPositionOnCanvas(cocos2d::CCNode* node, float x, float y) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float s = r->canvasScale;
    // the canvas is shown about its middle: what the screen leaves out of it, on each side
    float marginX = (kCanvasWidth - r->width / s) * 0.5f;
    float marginY = (kCanvasHeight - r->height / s) * 0.5f;
    node->setPosition(cocos2d::ccp((x - marginX) * s, (y - marginY) * s));
}

cocos2d::CCPoint canvasPosition(const cocos2d::CCPoint& screen) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float s = r->canvasScale;
    return cocos2d::ccp(screen.x / s + (kCanvasWidth - r->width / s) * 0.5f, screen.y / s + (kCanvasHeight - r->height / s) * 0.5f);
}

}  // namespace mt
