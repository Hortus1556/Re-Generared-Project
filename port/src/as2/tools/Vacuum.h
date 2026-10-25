// Vacuum: sucks up blood and the other splatters (poison, peanuts, fat,
// ink, feathers, salt) where it is held, and treats the injuries it is the
// tool for (its boxes under the nozzle; fires and "anything" injuries
// within its reach). It never does harm.
#pragma once

#include "as2/tools/ITool.h"

namespace as2 {

class Vacuum : public ITool {
public:
    explicit Vacuum(ToolData* d);
    void turnOn() override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override {}

private:
    int radius = 75;       // drops within it are drawn in
    int suction = 200;     // how hard (over the distance, a frame)
    int nozzleRadius = 25;  // drops within it are taken
    int bloodHeld = 100;
};

}  // namespace as2
