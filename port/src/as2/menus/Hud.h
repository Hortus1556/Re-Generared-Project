// The operation's HUD and pointer as the Flash AS2 drew them: its `ui` and
// `cursor` symbols (re/flash/as2's assets.swf, extracted into
// games/as2/flash by the build), in place of the iPad's panel buttons and
// HUD sprites.
//
// - The tools: numbered cards across the top (their keys, 1-9 and 0), each
//   at the tool's level, dimmed while the mission does not offer it, its
//   highlight lit while in hand; a tutorial's arrow under a card.
// - The JogLog dial top left: its face's colour and runner by the heart
//   (GameState's jogger state), the BPM, the pause button in its corner.
//   The port's own runner animations (shocked, the brain out) are the
//   iPad's pictures, in the runner's place.
// - The time bottom left (its drip running down with the mission's clock;
//   a brain out's countdown instead while it runs), the score bottom right,
//   Bleed's countdown.
// - The pointer: Flash's cursor with the tool in hand, at the mouse as it is
//   at each display frame (platform::PointerArt); the system's is hidden
//   meanwhile.
//
// The Flash stage (600x460) is drawn at the screen's height, centred
// (flashstage).
#pragma once

#include "as2/SceneManager.h"
#include "engine/VectorArt.h"

namespace as2 {

class Hud {
public:
    static Hud& shared();

    // A tutorial's arrow under tool `tool` (1-10), played from its start.
    void showArrow(int tool);
    // The arrows taken away (a new stage).
    void clearArrows();
    // The tool card (1-10) at screen point p, 0 if none; whether p is on
    // the pause button.
    int toolAt(Point p);
    bool pauseAt(Point p);
    // The HUD drawn over the operation (GameController::renderScene).
    void draw();
    // Whether the operation's pointer is the HUD's cursor now (the system's
    // hidden): in an operation, not paused, no popup or alert over it.
    bool drawsPointer() const;

private:
    Hud() = default;
    bool ready();
    // Whether screen point p is on the named piece's art as last drawn (its
    // look: each card on its level's frame; Flash's MOUSE_DOWN on the clip:
    // its shapes, not its box, which for a tilted card reached well below
    // it).
    bool partAt(const char* part, Point p);

    int ui = -1, cursor = -1, countdown = -1;
    VectorLook drawn;  // the look the HUD was last drawn with
    long age = 0;
    int arrowAge[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    int joggerShown = 0;  // the runner's frame, and since when (age)
    long joggerSince = 0;
};

}  // namespace as2
