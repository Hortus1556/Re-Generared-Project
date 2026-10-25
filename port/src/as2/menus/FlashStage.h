// The Flash AS2's stage (600x460) on the port's screen: drawn at the
// screen's height, centred, as the HUD, the injection's syringe, the pause
// and death clipboards, the discharge form, the bandages and the loading
// screen are. Its movies are packages under games/as2 ("flash",
// "flash-shell", "flash-preloader", "flash-cutscene": CMakeLists.txt), read
// in the background from the start (preload).
#pragma once

#include <string>

#include "as2/SceneManager.h"
#include "engine/VectorArt.h"

namespace as2::flashstage {

// A package's art by its folder ("flash", "flash-shell", ...).
VectorArt& art(const std::string& package);
// Starts reading the packages in the background (at start: the game's
// "flash" took a third of a second, read by the first frame of a mission).
void preload();
// Once per drawn frame: each loaded package's art made in the background
// swapped in, and the pointer a hand if a screen asked for it this frame.
void endFrame();
// A Flash button is under the pointer: the pointer shows Flash's hand.
void wantHand();
// The stage's scale and left edge on the screen.
float scale();
float left();
// A transform placing stage point (x, y), turned by `turn` degrees.
VectorTransform at(float x = 0, float y = 0, float turn = 0);
// A screen point on the stage.
Point toStage(Point screen);
// The mission's world (the iPad's points: half the screen's) and the stage
// as the HUD places it: a Flash script's stage point or length there (the
// monsters' logic from the Flash AS2).
Point stageToWorld(float x, float y);
float worldPerStage();
// Flash's MathUtils: makeScorePretty (thousands set off by commas) and
// formatTime (minutes and two-digit seconds).
std::string prettyScore(long long score);
std::string formatTime(int seconds);
// The Flash movie's frame rate: its clips play at 30 frames a second
// whatever the port's pass is.
constexpr double kFps = 30;

}  // namespace as2::flashstage
