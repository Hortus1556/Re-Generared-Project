// Battery: the car battery. A tap shocks what is under it: a creature
// (the dove) is stunned (its interact); nothing to shock, the patient is
// shocked instead (a burn, the heart stops: inShock) and must be revived
// within 300 frames by another tap, which restarts the heart at no more
// than restartAmount (10/20/30 by level).
#pragma once

#include "as2/tools/ITool.h"

namespace as2 {

class Battery : public ITool {
public:
    explicit Battery(ToolData* d);
    void turnOn() override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override;

private:
    int restartAmount = 10;
};

}  // namespace as2
