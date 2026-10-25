// FlashCreature: a creature drawn as the Flash AS2's clip in its sprite's
// place (the iPad's few pictures, two to flap, replaced by Flash's whole
// animation), its behaviour still the port's. The sprite's shape draws the
// clip instead of its texture (Shape::art), under the shape's place, turn
// and scale: the clip on a labelled frame (gotoAndStop), what plays inside
// it on its own clock at the movie's 30 frames a second from when the
// label was set, its named pieces' looks; Flash's pixels to the iPad's at
// the stage's on the screen (as the HUD: flashstage::scale).
#pragma once

#include <string>
#include <vector>

#include "as2/SceneManager.h"
#include "engine/VectorArt.h"

namespace as2 {

class FlashCreature {
public:
    // A symbol of the Flash AS2's "flash" package (its linkage name).
    explicit FlashCreature(const std::string& symbol);
    ~FlashCreature();
    FlashCreature(const FlashCreature&) = delete;
    FlashCreature& operator=(const FlashCreature&) = delete;

    // Whether the art has it (else the creature keeps its pictures).
    bool ready() const { return symbol >= 0; }
    // The shape draws the clip from now on (until this goes).
    void attach(Shape* shape);
    // On a label's frame; what plays in it starts over when it changes.
    void show(const std::string& label);
    const std::string& label() const { return shown; }
    // Flash frames since the label was set.
    long age() const;
    // A piece's box as the clip is drawn now (its frame, age and look),
    // as boundsOf.
    flash::Rect boundsNow(const std::string& part);
    // The frame of `label` in the inner clip at `path` on the frame of
    // `onLabel` (1 if none), and that clip's length.
    int frameIn(const std::string& onLabel, const std::string& path, const std::string& label);
    int lengthOf(const std::string& onLabel, const std::string& path);
    // For a clip whose labels are an inner clip's (the squid's `head`): that
    // clip's frame of `label`, and the length of `child` on that frame.
    int innerFrameOf(const std::string& clip, const std::string& label);
    int innerLength(const std::string& clip, const std::string& label, const std::string& child);

    VectorLook look;   // its named pieces (alive, shadow...)
    // What plays inside held on its first frame (a dead thing lying still).
    bool still = false;
    float turn = 0;    // degrees, on top of the shape's
    float scale = 1;   // on top of the Flash-to-iPad scale
    flash::ColorTransform tint;  // its own colour (ColourUtils.tint)

    // A piece's box in the clip (Flash pixels about its origin, unturned)
    // on a label's frame: its hit area ("hit"), where the tools work.
    flash::Rect boundsOf(const std::string& onLabel, const std::string& part, const VectorLook* with = nullptr);
    // The hit areas Flash's InjuryGraphic took (each movie clip in `hit`, or
    // in another group: the squid's hit1..hit6) on a label's frame: their
    // boxes, as boundsOf.
    std::vector<flash::Rect> hitAreas(const std::string& onLabel, const std::string& group = "hit");

private:
    void draw(const float color[4]);

    VectorArt& art;
    int symbol = -1;
    int frame = 1;
    std::string shown;
    double since = 0;  // pass time the label was set
    Shape* attached = nullptr;
};

}  // namespace as2
