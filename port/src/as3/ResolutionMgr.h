// The game's screen settings (a singleton at 0x1ea7c; its name is not in the
// binary: ResolutionMgr is the port's), and placing things on the design
// canvas.
//
// The game is laid out on a 640x384 canvas; each device shows the middle of
// it at its own size in "canvas points" (an iPhone 480x320, an iPhone 5
// 568x320, an iPad 512x384). init() looks the screen's pixels up in the
// game's table of 19 known screens: its art scale (pixels a canvas point)
// and the folder of art made for it (1.0x, 2.0x, 4.0x).
//
// (The port's) The screen follows the window's shape (fitWindow): as much of
// the canvas as fits that shape, from 512x384 canvas points (4:3, the iPad's
// view) to 626x320 (about 2:1: the widest the art goes, the canvas's 640
// less what some menus' art leaves out at its left edge), drawn with the
// 4.0x art at 4 pixels a point. A narrower window is
// letterboxed above and below, a wider one at the sides.
#pragma once

#include <string>

#include "engine/cocos/CCNode.h"

namespace mt {

class ResolutionMgr {
public:
    static ResolutionMgr* sharedResolutionMgr();  // 0x1ea7c
    // the director's window size in cocos points, and its content scale (0x1eb8c)
    void init(int width, int height, int contentScale);
    void setRect(int x, int y, int w, int h);  // 0x1ef90
    // (the port's) The screen's size in canvas points for a window of this
    // shape (width over height): 4:3 and narrower 512x384; up to 16:10 512
    // wide, as high as the shape makes it; up to 1.96:1 320 high, as wide (16:9
    // 568x320, 5:3 533x320); wider 626x320. Whole points, never more than
    // the window shows (the platform fills the window within a point).
    static void sizeForAspect(float aspect, int& width, int& height);
    // (the port's) The screen made the size for the window's shape now
    // (platform::windowAspect), if it is not already: this manager (init),
    // the director's and the platform's screen. True if it changed. Called
    // at the start and as each screen is replaced (SceneMgr), never while a
    // screen is up: screens are laid out for one size.
    bool fitWindow();
    // "<folder>/<name>.<ext>": the art made for this screen (0x2efe0). (The
    // port's) A file the screen's folder lacks comes from the package's other
    // art folders (3.5x/: the Android build's art, as the 1.02 package had for
    // the Hospital; the 1.45 package has none), drawn at their own scale
    // (CCTexture2D::getContentScale).
    std::string artPath(const char* name, const char* ext) const;
    // (the port's) The same name in another art folder while the screen's
    // folder has it too: a sheet's supplement, the frames the Android build
    // added to an iOS sheet (port/tools/extract_xapk.py); "" if none.
    std::string supplementPath(const char* name, const char* ext) const;

    int width = 0, height = 0;            // the window, cocos points (the larger first? as given)
    int pixelsWide = 0, pixelsHigh = 0;   // its pixels, the longer side first
    cocos2d::CCRect rect;                 // set by setRect
    float scale = 1;                      // pixels a canvas point (the table's)
    float canvasScale = 1;                // cocos points a canvas point: scale / content scale
    std::string folder;                   // the art's folder
};

// A node's position given on the 640x384 canvas, as the screen shows its
// middle (0x2ca70).
void setPositionOnCanvas(cocos2d::CCNode* node, float x, float y);
// The canvas point a screen position (cocos points) shows.
cocos2d::CCPoint canvasPosition(const cocos2d::CCPoint& screen);

}  // namespace mt
