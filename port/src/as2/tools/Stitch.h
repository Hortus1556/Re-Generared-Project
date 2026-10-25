// Stitch: the needle. Each injury under it is followed (a StitchInjury):
// every time the finger crosses the cut's middle line within its boxes, a
// stitch is made where it crossed; stitches in enough boxes (4, 3 or 2 by
// level) close that stage. The thread drawn is the finger's path since it
// went down (mStitches, Panel; drawn by GameController renderScene).
#pragma once

#include <vector>

#include "as2/tools/ITool.h"

namespace as2 {

class InjuryGraphic;
struct HitBox;

class Stitch : public ITool {
public:
    explicit Stitch(ToolData* d);
    ~Stitch() override;
    void turnOn() override;
    void turnOff(bool release) override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override;

    Point* mStitches = nullptr;  // 2048 points: the thread, panel coordinates
    int mStitchCount = 0;
    int mNeeded = 4;

private:
    // StitchInjury: an injury being sewn: the stitches made, which side of
    // the line the finger was on (-1, 1; 0 at first), and where it was (in
    // the injury's frame).
    struct Sewn {
        InjuryGraphic* injury;  // retained
        std::vector<Point> stitches;
        int side = 0;
        float x = 0, y = 0;
    };
    std::vector<Sewn> mInjuries;
    void clearSewn();
    // finishedStitching:...: a stitch in `needed` of the boxes (one counted
    // a box, but every one on the snake).
    static bool finishedStitching(const HitBox* boxes, int count, const std::vector<Point>& stitches,
                                  const InjuryGraphic* injury, int needed);
};

}  // namespace as2
