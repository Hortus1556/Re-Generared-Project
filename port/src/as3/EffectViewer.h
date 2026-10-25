// Not in the original: a debug screen that shows the surgery effects
// (EffectList.h) on their own, for checking their port (docs/as3.md).
//
// AS_EFFECTS=<name>[@<x>:<y>][,<name>...] (debug mode, -d, only): the game
// starts on this screen instead of the title: the first surgery's chest
// (bg_mrgibletsChest.png) under the surgery HUD (common/in_game_ui.json),
// an EffectList on the screen as the surgery keeps it, and the named
// effects added at (x, y) in cocos points (default: the screen's middle).
// Names: the tools' lighter, cream, scanner, chainsaw (the first one named
// is selected, as the surgery's selectTool does; the touches go to it: the
// scanner blinks at the finger every interval; the chainsaw also gets its
// trail cursor); the events' damage, shock, electricity, blood, poison,
// bluescreen, explosion, frozen, whitescreen, poisoned, greenscreen (event
// kinds 0..11); dying[=<health>], success, outoftime, highlight, sparkle,
// tag<kind>[=<points>], cointag, arrow, gradient; highfive=<partner id>,
// giblets, saturdayheart, saturdayparticle, bot, flow, brutality, baton,
// reverand (the cursor: the touches go to it), reverandtools, reverandend,
// sheen. AS_EFFECTS_REPEAT=1 adds each again when it ends. Each
// effect's start and end is traced (platform::debugTrace).
#pragma once

namespace mt {

class Scene;

// The viewer when AS_EFFECTS is set in debug mode, else nullptr.
Scene* effectViewer();

}  // namespace mt
