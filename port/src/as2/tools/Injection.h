// Injection: the syringe as the Flash AS2 used it. Pressed on the patient
// (or on an injury that wants it), a syringe (`injectionBar<level>`, one of
// its five mazes, the vampire's red) appears with its needle at the finger,
// turned away from the stage's edges; held down, the finger is drawn up its
// winding path (`floor`) to the ring at the top (`hit`): there the injury is
// treated or, with none, the heart gains `boost`. Touching a `wall` or
// leaving the path is a wrong use (a cut and blood at the finger, the heart
// hurt); letting go puts the syringe away. Meanwhile the HUD's cursor gives
// way to the system's pointer. The syringe is drawn in the scene over the
// operation, under the dark of a blackout (its torch lights it as the rest).
#pragma once

#include <string>

#include "as2/SceneManager.h"
#include "as2/tools/ITool.h"

namespace as2 {

class InjuryGraphic;

class Injection : public ITool {
public:
    explicit Injection(ToolData* d);
    ~Injection() override;
    void turnOn() override;
    void turnOff(bool release) override;
    void render(int frame) override;
    void setToolLevel() override;
    bool hidesPointer() const override { return syringe >= 0; }

private:
    // Whether the finger (screen) is on the syringe's piece `part`.
    bool on(const char* part, Point p);
    // A wrong use's harm: "wrong", the heart, a cut and blood at the finger.
    void hurt();
    // For the play-tests: a way up the maze from the needle to the ring, in
    // screen points ("injection path x,y ...").
    void tracePath();

    int boost = 10;
    int syringe = -1, frame = 1;  // the syringe's symbol and frame (-1: none)
    float x = 0, y = 0, turn = 0;  // its needle on the Flash stage, its turn
    long shownFor = 0;
    InjuryGraphic* injury = nullptr;  // retained while shown
    int eyeHit = -1;  // a big centipede's eye it was pressed on
    // The syringe's place in the scene (screen space, just under the dark's
    // layer 7), in the scene while it is shown.
    Shape overlay;
    void showOverlay(bool shown);
};

}  // namespace as2
