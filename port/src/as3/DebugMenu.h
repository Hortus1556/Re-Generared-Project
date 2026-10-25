// The F1 debug menu (debug mode only, -d; not in the original): the engine's
// DebugPanel (engine/ui/DebugPanel.h) drawn over the cocos scene by the
// director's overlay hook (CCDirector::setOverlay), its touches taken before
// the director's while it is open. Its items: every surgery in each mode
// straight into play (as AS_SURGERY), the menus' screens, a running
// surgery's cheats (SurgeryDebug.cpp), changes to the save in use, and the
// display's switches. With "Hit areas" on, the surgery's touch areas are
// drawn over it, menu open or not. Without -d none of it exists.
#pragma once

namespace mt {

// The overlay hook and the menu's state (AppDelegate, in debug mode only).
void debugMenuInstall();
// F1: opens or closes the menu.
void debugMenuKey();
// A touch (0 began, 1 moved, 2 ended, window points y down): true if it is
// the menu's (begun while it was open), not the game's.
bool debugMenuTouch(int phase, float x, float y);
// Escape: closes the menu if it is open (true), else it is the game's.
bool debugMenuEscape();

}  // namespace mt
