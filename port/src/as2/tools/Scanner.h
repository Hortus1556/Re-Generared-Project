// Scanner: the ULTRASOUND 5000. While on, a ring pulses where the finger is,
// coloured by how near the nearest hidden injury is (green within 50 points,
// yellow, orange, red past 150; black with none hidden). At level 2 an arrow
// points to it; at level 3 rings mark every hidden one.
#pragma once

#include "as2/SceneManager.h"
#include "as2/tools/ITool.h"

namespace as2 {

class Scanner : public ITool {
public:
    explicit Scanner(ToolData* d);
    ~Scanner() override;
    void turnOn() override;
    void turnOff(bool release) override;
    void render(int frame) override;
    void setToolLevel() override;

private:
    Sprite* mSprite = nullptr;       // the pulse
    Sprite* mSpriteArrow = nullptr;  // level 2 up
    Array<Sprite> rings;             // level 3: one on each hidden injury
    float scaleOffset = 0.05f;       // the pulse's growth a frame
    bool arrow_visible = false;
};

}  // namespace as2
