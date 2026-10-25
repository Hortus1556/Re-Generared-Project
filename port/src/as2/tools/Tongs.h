// Tongs: take what is stuck in the patient. Pressed on an object, it is
// pulled along its own direction (a pull off by more than the angle
// buffers is a wrong use); far enough and it comes out, held under the
// finger, and the tray appears; let go over the tray it is done, anywhere
// else it is dropped (a wrong use) and goes back in. A transplant's piece
// taken to the tray leaves the healthy one there (trayHolding): taken from
// the tray and let go on its place it goes in. Turning on and off happen at
// the next frame (mDoTurnOn, mDoTurnOff).
#pragma once

#include <vector>

#include "as2/SceneManager.h"
#include "as2/tools/ITool.h"

namespace as2 {

class InjuryGraphic;
class DraggableWrapper;
struct HitBox;

class Tongs : public ITool {
public:
    explicit Tongs(ToolData* d);
    ~Tongs() override;
    void turnOn() override;
    void turnOff(bool release) override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override;

    float angleBuffer = 0;
    int removeBuffer = 0;
    int grip = 0;

    // An insertion opened: its pin waits on the tray.
    void setupInsertion(InjuryGraphic* insertion);
    // EmptyHolding, then let go: what it held is gone from its grip (an egg
    // hatching in it).
    void dropHeld();
    // getHoldingInjury: whose piece it holds (or null).
    InjuryGraphic* holdingNow() const { return holdingInjury; }

private:
    void doTurnOn();
    void doTurnOff(bool release);
    // Takes the injury's removable `index` out: holding it, the tray up.
    void setHolding(int index);
    void addTray();
    void removeTray();
    // The healthy replacement shown on the tray, to take (setTrayHolding::).
    void setTrayHolding(InjuryGraphic* owner, DraggableWrapper* w);
    void removeTrayHolding();
    // Pressed on the tray with something on it: holding that.
    void setHoldingFromTray();
    // Where a replacement from the tray goes, and how near (`size`) it
    // must be let go.
    static Point trayTarget(InjuryGraphic* owner, float* size);
    // The injury's removable `index` (a transplant's part's piece).
    static DraggableWrapper* removableOf(InjuryGraphic* i, int index);
    // Taken up whole by its object, then dropped on the tray (Snake,
    // SkullCut).
    static bool takenWhole(const InjuryGraphic* i);
    bool hitTest(const HitBox* boxes, int count) const;
    // A wrong use with something held: "dropped".
    void droppedPopup();

    // holding: the injury and which of its removables (none: nothing held).
    InjuryGraphic* holdingInjury = nullptr;  // retained
    int holdingIndex = 0;
    bool holdingFromTray = false;
    bool holding() const { return holdingInjury != nullptr; }
    InjuryGraphic* trayInjury = nullptr;      // retained: whose piece is on the tray
    DraggableWrapper* trayDraggable = nullptr;  // retained: that piece
    void clearHolding();
    std::vector<Point> mouseArray;  // where the finger was, last 4 at least
    Sprite* mTray = nullptr;
    InjuryGraphic* injury = nullptr;      // retained: the one being taken
    DraggableWrapper* draggable = nullptr;  // the thing held (its injury's)
    float movedDist = 0;
    Point grabOrigin;
};

}  // namespace as2
