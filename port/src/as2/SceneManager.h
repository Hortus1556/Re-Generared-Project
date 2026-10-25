// The game's GL scene: Shapes (a textured quad with its place, turn, scale
// and colour) in 47 layers, drawn by SceneManager each frame, and Sprite,
// which owns a Shape showing one tile of a texture.
//
// Layers are drawn from 46 (the back) to 0. Layers 9..46 are the world:
// scrolled, zoomed around (240, 160) and shaken by GameState; 9..27 is the
// "old layer", where the previous stage's art fades out (MoveAllToOldLayer).
// Layer 0 is the loading screen's: while it holds anything, only it is
// drawn.
//
// The iPad build draws the iPhone's 480x320 point layout on its 1024x768
// screen at twice the size by applying each shape's position and offset
// twice (verified in the instructions: the translations are issued twice),
// while the quads are in the HD art's pixels (twice the points).
#pragma once

#include <functional>
#include <list>

#include "as2/Texture2D.h"
#include "core/Object.h"

namespace as2 {

struct Shape {
    Point pos;             // +0x00
    Point offset;          // +0x08: the quad's corner from pos, before the turn's scale
    float rotation = 0;    // +0x10, degrees
    Texture2D* tex = nullptr;  // +0x14 (not retained)
    float verts[8] = {};   // +0x18: (0,0) (w,0) (0,h) (w,h)
    float uv[8] = {};      // +0x1c: the same corners' texture coordinates
    int layer = 0;         // +0x24
    float scale = 1;       // +0x28
    float color[4] = {1, 1, 1, 1};  // +0x2c: r, g, b, a
    // The port's: made at the finger (a tool's flame or cream): shown as soon
    // as it is made (platform::appearAtOnce), keeping up with the pointer.
    bool atFinger = false;
    float appearAt = 0;  // when within its first pass (a trail's piece)
    float fade = 1;      // its opacity on top of its colour (a trail's piece)
    // The port's: drawn in place of its texture, under its place, turn and
    // scale (the iPad's pixels), with its colour (r, g, b, a): a creature's
    // Flash clip (FlashCreature).
    std::function<void(const float color[4])> art;
    // The port's: where it sits in its layer, and its identity for the
    // recorded drawing.
    bool inScene = false;
    int nodeLayer = 0;  // the layer list it is in (layer may change meanwhile)
    std::list<Shape*>::iterator node;
    unsigned long long id = Object::newSerial();
};

class SceneManager {
public:
    static constexpr int kLayers = 47;
    static SceneManager& sharedSceneManagerInstance();

    // addShape puts it in its layer below what is there (drawn first),
    // insertShape above (drawn last).
    void addShape(Shape* s);
    void insertShape(Shape* s);
    void removeShape(Shape* s);
    // Moves layers 28..46 to 9..27 and starts their fade (1 -> 0 by 0.05 a frame).
    void MoveAllToOldLayer();
    void renderScene();

    float mOldLayerFade = 0, mOldLayerFadeSpeed = 0;
    // The red flash (GameState) tints everything drawn.
    float redEffect = 1, greenEffect = 1, blueEffect = 1, alphaEffect = 1;
    // The shape the finger holds this pass (the tongs' catch), drawn where
    // the pointer is at each display frame (platform::PointerArt), so it
    // stays under the cursor; set by its tool each pass, cleared once drawn.
    const Shape* onPointer = nullptr;

private:
    std::list<Shape*> layers[kLayers];  // in drawing order
    void draw(const Shape& s, bool world);
};

class Sprite : public Object {
public:
    // A shape showing tiles of spriteWidth x spriteHeight points (pixels:
    // twice that on the iPad) of the texture.
    static Sprite* initWithImageNamed(const std::string& name, int spriteWidth, int spriteHeight);
    static Sprite* initWithTexture2D(Texture2D* tex, int spriteWidth, int spriteHeight);
    ~Sprite() override;

    Shape* currentShape() { return &shape_; }
    // Shows the tile at column x, row y.
    void calculateTextureCoordsAtOffsetX(int x, int y);
    void setBasePositionXY(float x, float y) { basePositionOffset = {x, y}; }
    // Cuts the shown tile at startX (points) for a piece drawn from x: false
    // when nothing of it is left.
    bool clipToStartX(int startX, int endX, int forX, int forY);
    void justFlipItH();
    void justFlipItV();
    bool flip_h = false, flip_v = false;
    float spritePixelWidth() const { return spritePixelWidth_; }
    float spritePixelHeight() const { return spritePixelHeight_; }

    // Sprite sheets (LoadSpriteSheet:Layer:, set_seq_id:, update) are not
    // ported: no code of the game uses them.

private:
    Sprite() = default;
    void setup(Texture2D* tex, int w, int h);
    Shape shape_;
    Point basePositionOffset;
    float spritePixelWidth_ = 0, spritePixelHeight_ = 0;
    float spriteWidthRatio = 0, spriteHeightRatio = 0;
    int xTiles = 0;
    Point lastTextureOffset{-1, -1};
};

}  // namespace as2
