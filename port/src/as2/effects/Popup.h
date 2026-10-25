// Popup: the word a treatment earns ("awesome", "healed", "wrong"...) with
// the combo count ("x12"), shrinking and fading over 20 frames.
#pragma once

#include <string>

#include "as2/SceneManager.h"
#include "core/Object.h"

namespace as2 {

class Popup : public Object {
public:
    static Popup* initWithXY(int x, int y, const std::string& image, bool playerCreated, int combo);
    ~Popup() override;
    // False once it has gone.
    bool update();

private:
    Popup() = default;
    Sprite* mSprite = nullptr;
    // The combo: "x" and up to three digits, each a yellow picture over its
    // black outline.
    Sprite* comboB[4] = {};
    Sprite* comboY[4] = {};
};

}  // namespace as2
