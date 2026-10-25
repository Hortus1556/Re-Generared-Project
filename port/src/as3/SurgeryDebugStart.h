// AS_SURGERY=<surgery id>[:<mode>[:<stage>]] (debug mode only): opens on
// that surgery itself (mode 0 the main surgery, 1 and 2 its challenges),
// without the menus, at its first stage or the one given (with the first
// stage's heart and clock), for test runs. Not in the original.
#pragma once

namespace mt {

class Scene;

// the surgery screen AS_SURGERY asks for, or nullptr
Scene* surgeryFromEnv();
// AS_SCREEN=<name>[:<surgery>[:<mode>]] (debug mode only): opens on a menu
// screen, as the F1 menu's Go to does, for screenshots of each screen (the
// screen's shape survey, docs/as3.md): title, saveslots, world, upgrades,
// partners, achievements, credits, editor (the list), editor-new (a new
// surgery in the editor), area:<surgery> (its area's map), info:<surgery>
// (its case file), dialogue:<surgery> (its intro), preop:<surgery>[:<mode>].
// The screen asked for, or nullptr.
Scene* screenFromEnv();
// the stage AS_SURGERY starts on (2 to `stages`), 0 for the usual start
int surgeryDebugStage(int stages);

}  // namespace mt
