// The engine's own splash, "Powered by Bleed Engine", shown before every
// game's first screen (runGame): it fades in, holds and fades out over
// about three seconds; a click or Esc cuts it short. Not shown while debug
// automation drives the game (automationActive()), so tests see the game's
// frames from the first.
#pragma once

#include <functional>

namespace platform {

// Returns false if the player closed the window during it. `ready` (if
// given): it stays up, at full brightness and its window answering, until
// that says the game's content is read.
bool showEngineSplash(const std::function<bool()>& ready = {});

}  // namespace platform
