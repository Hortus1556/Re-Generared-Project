// The simpler tools: Cutter (the pizza cutter: cuts along a line; wrong use
// cuts the patient), Lighter (burns cuts shut; wrong use burns) and Cream
// (heals burns and scars).
#pragma once

#include "as2/tools/ITool.h"

namespace as2 {

class Cutter : public ITool {
public:
    explicit Cutter(ToolData* d);
    void turnOn() override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override;
};

class Lighter : public ITool {
public:
    explicit Lighter(ToolData* d);
    void turnOn() override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override;
};

class Cream : public ITool {
public:
    explicit Cream(ToolData* d);
    void turnOn() override;
    void render(int frame) override;
    void setToolLevel() override;
    void wrongUsage() override;
};

}  // namespace as2
