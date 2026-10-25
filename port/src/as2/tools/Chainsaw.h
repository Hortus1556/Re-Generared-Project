// Chainsaw: saws bone and tough organs (a transplant's dashed line). It
// shakes the screen and throws blood while running; each box under it loses
// the heal amount, a box spent goes out, and all of an injury's (or one
// transplant part's) spent move it on. Running over nothing wears the heart
// a little every frame; too long over nothing is a wrong use (a large cut).
#pragma once

#include "as2/tools/ITool.h"

namespace as2 {

struct HitBox;

class Chainsaw : public ITool {
public:
    explicit Chainsaw(ToolData* d);
    void turnOn() override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override;

private:
    // Saws `count` boxes of the injury (part `part` of a transplant): true
    // if one was under it.
    bool saw(HitBox* boxes, int count, InjuryGraphic* injury, int part);
};

}  // namespace as2
