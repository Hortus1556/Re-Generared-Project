// The pointer on a PC (not the original's, which was played by touch;
// docs/as3.md, "The surgery's HUD on a big screen"): in a surgery the same
// yellow Flash cursor as the other Amateur Surgeon games draw (AS2's
// `cursor` symbol, from its Flash assets.swf, extracted by the build into
// the package's flash-pointer/: CMakeLists.txt's as3_pointer), its plain
// frame, with the tool in hand inside its circle as AS2's frames have
// theirs: AS3's own tool without its card (ToolArt.h; the injection's out
// one while it draws out). It is drawn as platform::PointerArt, so it is where the mouse
// is at each display frame, over everything; the system's pointer is
// hidden meanwhile and comes back over every other screen (the pause, the
// results, the menus). On a touch device there is no pointer at all
// (docs/rules.md: the finger is it).
#pragma once

namespace mt {

// The director's pointer overlay (AppDelegate, at start).
void pointerInstall();

}  // namespace mt
