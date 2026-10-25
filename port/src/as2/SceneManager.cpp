#include "as2/SceneManager.h"

#include <algorithm>
#include <optional>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "platform/DrawList.h"

namespace as2 {

SceneManager& SceneManager::sharedSceneManagerInstance() {
    static SceneManager instance;
    return instance;
}

// The original kept one node per shape: adding a shape that is already in
// the scene would have linked it twice; the port moves it instead.
void SceneManager::addShape(Shape* s) {
    if (s->inScene) removeShape(s);
    s->nodeLayer = std::clamp(s->layer, 0, kLayers - 1);
    auto& l = layers[s->nodeLayer];
    l.push_front(s);
    s->node = l.begin();
    s->inScene = true;
}

void SceneManager::insertShape(Shape* s) {
    if (s->inScene) removeShape(s);
    s->nodeLayer = std::clamp(s->layer, 0, kLayers - 1);
    auto& l = layers[s->nodeLayer];
    l.push_back(s);
    s->node = std::prev(l.end());
    s->inScene = true;
}

void SceneManager::removeShape(Shape* s) {
    if (!s->inScene) return;
    layers[s->nodeLayer].erase(s->node);
    s->inScene = false;
}

void SceneManager::MoveAllToOldLayer() {
    for (int layer = 46; layer > 27; --layer)
        while (!layers[layer].empty()) {
            Shape* s = layers[layer].front();
            removeShape(s);
            s->layer = layer - 19;
            addShape(s);
        }
    mOldLayerFade = 1;
    mOldLayerFadeSpeed = 0.05f;
}

void SceneManager::draw(const Shape& s, bool world) {
    if (!s.tex && !(world && s.art)) return;
    const float inv = gameController->InverseScaleFactor;
    // (Held at the finger: placed where the logic had the pointer, the
    // screen point that made its world point.)
    std::optional<platform::PointerArt> held;
    if (&s == onPointer && gameState) held.emplace(gameState->unscaledMouseX * 2, gameState->unscaledMouseY * 2);
    glPushMatrix();
    if (world) {
        glTranslatef(s.pos.x, s.pos.y, 0);
        glTranslatef(s.pos.x, s.pos.y, 0);
        glRotatef(s.rotation, 0, 0, 1);
        glScalef(s.scale, s.scale, 1);
        if (s.art) {
            // Its art about its place (the texture's offset centred a picture).
            s.art(drawColor());
            glPopMatrix();
            return;
        }
        glTranslatef(s.offset.x, s.offset.y, 0);
        glTranslatef(s.offset.x, s.offset.y, 0);
    } else {
        // Layer 0 (loading): once, with no scale of its own.
        glTranslatef(s.pos.x + s.offset.x, s.pos.y + s.offset.y, 0);
        glRotatef(s.rotation, 0, 0, 1);
    }
    glScalef(inv, inv, 1);
    platform::Sprite sp;
    std::copy(s.verts, s.verts + 8, sp.xy);
    if (s.atFinger) platform::appearAtOnce(s.appearAt);
    platform::drawSprite(s.id, s.tex->name(), sp, s.uv, drawColor(), GL_ONE);
    glPopMatrix();
}

void SceneManager::renderScene() {
    if (!layers[0].empty()) {
        // Drawn with whatever colour was set last (the loader's white).
        for (Shape* s : layers[0]) draw(*s, false);
        return;
    }
    for (int layer = kLayers - 1; layer >= 0; --layer) {
        // A copy: nothing should change the layers while drawing, but the
        // list must not be invalidated if a draw ever did.
        std::vector<Shape*> list(layers[layer].begin(), layers[layer].end());
        for (Shape* s : list) {
            float fade = (layer >= 9 && layer <= 27 ? mOldLayerFade : 1) * s->fade;
            glPushMatrix();
            if (layer > 8) {
                // The world: GameState's scroll, shake and zoom around the
                // middle of the iPhone layout. Without a game (messages to
                // nil answer 0) the zoom is 0 and nothing of it shows.
                if (!gameState) {
                    glPopMatrix();
                    continue;
                }
                glTranslatef(gameState->_xScroll + 240 + gameState->_xWorld,
                             gameState->_yScroll + 160 + gameState->_yWorld, 0);
                glScalef(gameState->scale, gameState->scale, 1);
                glTranslatef(-240, -160, 0);
            }
            glColor(fade * s->color[0] * redEffect, fade * s->color[1] * greenEffect,
                    fade * s->color[2] * blueEffect, fade * s->color[3] * alphaEffect);
            draw(*s, true);
            glPopMatrix();
        }
    }
    onPointer = nullptr;
}

// ------------------------------------------------------------------ Sprite

void Sprite::setup(Texture2D* tex, int w, int h) {
    // The size is in points: twice that in the iPad's pixels (an iPhone 4
    // took its screen scale).
    int pw = gameController->isIPad ? w << 1 : static_cast<int>(w * gameController->ScreenScaleFactor);
    int ph = gameController->isIPad ? h << 1 : static_cast<int>(h * gameController->ScreenScaleFactor);
    shape_.tex = tex;
    shape_.layer = 0;
    spritePixelWidth_ = static_cast<float>(pw);
    spritePixelHeight_ = static_cast<float>(ph);
    if (tex) {
        spriteWidthRatio = spritePixelWidth_ / tex->pixelsWide();
        spriteHeightRatio = spritePixelHeight_ / tex->pixelsHigh();
    }
    lastTextureOffset = {-1, -1};
    shape_.scale = 1;
}

Sprite* Sprite::initWithImageNamed(const std::string& name, int w, int h) {
    Sprite* s = new Sprite;
    Texture2D* tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(name);
    s->setup(tex, w, h);
    s->shape_.offset = {0, 0};
    s->xTiles = tex && s->spritePixelWidth_ > 0 ? static_cast<int>(tex->pixelsWide()) / static_cast<int>(s->spritePixelWidth_) : 0;
    for (float& c : s->shape_.color) c = 1;
    return s;
}

Sprite* Sprite::initWithTexture2D(Texture2D* tex, int w, int h) {
    // The original left the colour unset (calloc: zero); every caller sets it.
    Sprite* s = new Sprite;
    s->setup(tex, w, h);
    for (float& c : s->shape_.color) c = 0;
    return s;
}

Sprite::~Sprite() { SceneManager::sharedSceneManagerInstance().removeShape(&shape_); }

void Sprite::calculateTextureCoordsAtOffsetX(int x, int y) {
    float fx = static_cast<float>(x), fy = static_cast<float>(y);
    if (fx == lastTextureOffset.x && fy == lastTextureOffset.y) return;
    float u0 = fx * spriteWidthRatio, u1 = spriteWidthRatio + u0;
    float v0 = spriteHeightRatio * fy, v1 = spriteHeightRatio + v0;
    float* uv = shape_.uv;
    uv[0] = uv[4] = u0;
    uv[1] = uv[3] = v0;
    uv[2] = uv[6] = u1;
    uv[5] = uv[7] = v1;
    float* v = shape_.verts;
    v[0] = v[1] = 0;
    v[2] = spritePixelWidth_;
    v[3] = 0;
    v[4] = 0;
    v[5] = spritePixelHeight_;
    v[6] = spritePixelWidth_;
    v[7] = spritePixelHeight_;
    lastTextureOffset = {fx, fy};
}

bool Sprite::clipToStartX(int startX, int endX, int forX, int forY) {
    calculateTextureCoordsAtOffsetX(forX, forY);
    float x = shape_.pos.x;
    if (!(x < static_cast<float>(startX))) {
        float right = static_cast<float>(endX) - spritePixelWidth_ / gameController->GfxScaleFactor;
        if (x > right) {
            unsigned texW = shape_.tex ? shape_.tex->pixelsWide() : 1;
            int w = static_cast<int>(static_cast<float>(static_cast<int>(static_cast<float>(endX) - x)) *
                                     gameController->GfxScaleFactor);
            if (w < 1) return false;
            float fw = static_cast<float>(w);
            shape_.uv[2] = shape_.uv[6] = fw * (1.0f / texW) + shape_.uv[0];
            shape_.verts[2] = shape_.verts[6] = fw;
        }
    }
    return true;
}

void Sprite::justFlipItH() {
    float u0 = shape_.uv[0];
    shape_.uv[4] = shape_.uv[2];
    shape_.uv[0] = shape_.uv[2];
    shape_.uv[6] = u0;
    shape_.uv[2] = u0;
}

void Sprite::justFlipItV() {
    float v0 = shape_.uv[1];
    shape_.uv[3] = shape_.uv[5];
    shape_.uv[1] = shape_.uv[5];
    shape_.uv[7] = v0;
    shape_.uv[5] = v0;
}

}  // namespace as2
