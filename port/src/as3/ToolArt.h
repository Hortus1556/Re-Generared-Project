// The port's tool art (not the original's; tools/as3_toolart.py, docs/as3.md
// "The surgery's HUD on a big screen"): AS3's tools without their cards and
// the card without a tool, made from the HUD's own tool buttons into the
// package's toolart/ (straight PNGs at the 4.0x art's scale):
//   toolart/card-<state>.png   the clean card made longer (the top bar's:
//                              its stripes going on above its bottom edge),
//                              its top left the cards' picture's top left
//   toolart/<tool>-<state>.png the tool alone in its button's untrimmed box
//   toolart/tools.json         how much longer the card is and its inside
//                              (within its edges); per tool, in pixels (y
//                              down) in its box: the card picture's top
//                              left, the tool's bounds, the circle about its
//                              solid part
// <tool> is the button's sprite name without "ingame_" and "_button"
// (pizzacutter, gel, battery, clamps, metaldetector, injection_out...);
// <state> 0 normal, 1 selected, 2 disabled (the tool buttons' toggle states).
#pragma once

#include <string>

#include "engine/cocos/CCGeometry.h"

namespace cocos2d {
class CCTexture2D;
}

namespace mt {
namespace toolart {

// A tool button's tool name from its sprite prefix ("ingame_gel_button" ->
// "gel"); a tool type's (the pointer's: the injection's out one when it
// draws out).
std::string fromSpritePrefix(const std::string& prefix);
std::string ofTool(int tool, bool drawOut);

// The pictures (nullptr if the package has none: an older package).
cocos2d::CCTexture2D* card(int state);
cocos2d::CCTexture2D* tool(const std::string& name, int state);

// A tool's places, in pixels of its button's untrimmed box (y down).
struct ToolLayout {
    cocos2d::CCPoint cardTopLeft;    // the card picture's top left
    cocos2d::CCRect bounds;          // the tool's
    cocos2d::CCPoint circleMiddle;   // the circle about its solid part
    float circleRadius = 0;
};
bool layout(const std::string& name, ToolLayout& out);  // false if unknown
// The long card: how many pixels longer than the button's card, and its
// inside (within its edges; pixels, y down, from its top left).
bool cardLayout(float& longer, cocos2d::CCRect& inside);

}  // namespace toolart
}  // namespace mt
