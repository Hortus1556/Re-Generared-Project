// The bandage transition, as the Flash AS2's Controller played it
// (setupTransition): its `bandages` clip over everything, at the movie's 30
// frames a second; at frame 25, when the bandages cover the screen, what is
// under them changes (`covered`), and the rest of the clip cuts them apart
// and lets them fall. Touches wait meanwhile (AS2App::touchBegan). Used
// where Flash used it: leaving a mission for the menus (its pause's and its
// obituary's quit).
#pragma once

#include <functional>

namespace as2::bandages {

// Has its art made in the background, kept a while (its big pieces made
// as each frame came took up to 40 ms a frame): by the screens that can
// start it, as they open and now and then while they stay.
void prefetch();
// Starts it (not while one runs).
void play(std::function<void()> covered);
bool playing();
// Once a drawn frame, over everything (AS2App).
void draw();

}  // namespace as2::bandages
