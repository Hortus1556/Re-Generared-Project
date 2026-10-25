// FlashScreen: a screen that is one of the Flash AS2's clips (the pause and
// death clipboards), drawn as vector art over the game on the Flash stage
// (flashstage), in place of the iPad's nib screen of the same name.
//
// Its clips play at the Flash movie's 30 frames a second, whatever the
// port's pass: each `Clip` (the screen's own timeline, or a named inner
// clip the screen drives as its scripts did) is shown on its frame, plays on
// while playing, and stops where its frame scripts stopped it (the ActionScript
// 3 `stop()`s the art does not carry, given as `stops`). Clicks go to named
// pieces (a press and a let-go on the same one, as Flash's CLICK; or the press
// alone, as MOUSE_DOWN); its buttons show their over and down looks.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "engine/VectorArt.h"
#include "engine/ui/UIKit.h"

namespace as2 {

class FlashScreen : public ui::ViewController {
public:
    // The clip `symbol` of package `package` ("flash-shell"), on a plain
    // full-screen view (no nib).
    FlashScreen(const std::string& package, const std::string& symbol);

    // Has the clip's frames (at stage point `placeAt`, with `look`) made in
    // the background and kept 40 seconds: a screen's big art rasterised as it
    // opened stalled it (a tenth of a second for the pause).
    static void prefetch(const std::string& package, const std::string& symbol, Point placeAt,
                         const VectorLook* look = nullptr);

    void touchesBegan(const ui::Touches& t) override;
    void touchesEnded(const ui::Touches& t) override;

protected:
    // A timeline the screen drives: its frame, whether it plays, where it
    // stops by itself, how long it has shown its frame.
    struct Clip {
        int frame = 1;
        bool playing = false;
        std::set<int> stops;
        long inFrame = 0;
        int frames = 1;  // its length (set from the art)
        void gotoAndStop(int f) { frame = f, playing = false, inFrame = 0; }
        void gotoAndPlay(int f) { frame = f, playing = true, inFrame = 0; }
    };

    void viewDidLoad() override;
    // Before and after the clip is drawn, under and on it (`at`: where the
    // clip is on the screen): what the screen draws beyond the art.
    virtual void drawUnder(const VectorTransform& at) { (void)at; }
    virtual void drawOver(const VectorTransform& at) { (void)at; }
    // Writes `text` in the hand of the text field at path `field`, `size`
    // times its height, on the line where `where` (screen pixels) starts
    // it, or ends it for a right-aligned field: a name the art has no page
    // for (the CFO's patients).
    void writeIn(const std::string& field, const std::string& text, const flash::Rect& where, float size,
                 const VectorTransform& at, const VectorLook& look);
    // Once a Flash frame, after the clips moved on: the screen's checks (its
    // ENTER_FRAME handlers).
    virtual void onFrame() {}
    // Per draw: the pieces' looks (texts, inner frames, hidden) beyond the
    // driven clips'.
    virtual void dress(VectorLook& look) { (void)look; }
    // A click on the named piece (path of instance names, "holder.resume"),
    // and a press on it.
    virtual void onClick(const std::string& part) { (void)part; }
    virtual void onPress(const std::string& part) { (void)part; }
    // The pieces that take clicks, in order (the first under the finger has
    // it); a screen lists those shown now.
    virtual std::vector<std::string> clickable() const { return {}; }
    // An inner clip by path, driven by the screen (its length from the art).
    Clip& inner(const std::string& path);

    VectorArt& art;
    int symbol = -1;
    Point placeAt{0, 0};  // where on the stage the clip is put
    Clip main;          // the screen's own timeline
    long age = 0;       // Flash frames since it was shown
    std::string pressed;  // the piece pressed on, while held

private:
    void draw();
    void step();
    void traceRest();
    VectorLook look();
    std::string pieceAt(Point p);
    std::map<std::string, Clip> inners;
    double last = -1, carry = 0;
    bool hovering = false;  // a button under the pointer
    std::string traced;     // the pieces last traced (debug)
};

}  // namespace as2
