// Not in the original: a debug screen that shows particle systems on their
// own, for checking the particle port (docs/as3.md).
//
// AS_PARTICLES=<name>[@<x>:<y>][,<name>...] (debug mode, -d, only): the game
// starts on this screen instead of the title. Each system is
// mt::ParticleSystem::create("particle_<name>") (the prefix may be given or
// left out), placed at (x, y) in cocos points, else spread across the
// screen's middle, over a dark grey. Once a second (and when made) it
// traces each system's emission rate, life, total and duration as read,
// its living particles, and how many died in the second (it keeps its dead
// particles, ParticleSystem::setKeepDeadParticles) (platform::debugTrace).
#pragma once

namespace mt {

class Scene;

// The viewer when AS_PARTICLES is set in debug mode, else nullptr.
Scene* particleViewer();

}  // namespace mt
