// ToolEffect: a short animation a tool or an injury leaves in the world (a
// flame, smoke, a splat, sparks...), updated once a mission frame until it
// is done. Made by class name (NSClassFromString), as Game addEffect: did.
#pragma once

#include <string>

#include "as2/SceneManager.h"
#include "core/Object.h"

namespace as2 {

class ToolEffect : public Object {
public:
    ~ToolEffect() override;
    // An effect of the class at (x, y) world points; nullptr for a class
    // the game does not have.
    static ToolEffect* create(const std::string& className, float x, float y);

    // True when it is over (its shape is gone).
    virtual bool update() { return false; }
    // Its frames, all told.
    virtual int life() const { return 25; }
    // A piece of a tool's trail (ITool::trail): gone in half its life,
    // fading out from the start (its frames as ever), so a stroke's trail
    // does not linger.
    void makeTrail() { trailLife = life() / 2; }
    // A pass: update(), and a trail piece's fade; true when it is over.
    bool tick();
    // setRed:Green:Blue: tints the sprite and keeps the colour for fades.
    virtual void setRed(float r, float g, float b);

    // For the effect classes (ToolEffect.cpp).
    ToolEffect() = default;
    // The sprite of `image` (w x h points) at (x, y) drawn from `offset`
    // (both coordinates), in `layer`, at `scale`, added to the scene.
    void makeSprite(const std::string& image, int w, int h, float x, float y, float offset, int layer, float scale,
                    bool above = false);
    void setTexture(const std::string& name);
    void removeShape();
    Shape* shape() { return mSprite->currentShape(); }

    Sprite* mSprite = nullptr;
    int frame = 1;
    float origRed = 1, origGreen = 1, origBlue = 1;
    int trailLife = 0, trailAge = 0;
};

}  // namespace as2
