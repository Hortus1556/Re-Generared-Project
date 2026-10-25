#include "as2/injuries/Eyelid.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "as2/GameController.h"
#include "as2/GameState.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

Sprite* sprite(const std::string& image, int w, int h, int layer, Point offset) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = layer;
    s->currentShape()->offset = offset;
    return s;
}

}  // namespace

Eyelid::Eyelid(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 96;
    box_height = 48;
    make = "eyelid";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Eyelid"));
    // The top lid, the bottom one (its mirror; only on an eye not turned),
    // the pupil.
    Sprite* top = sprite("eyeLidAnim1.png", 512, 256, 38, {-256, -183});
    mSpriteArray.addObject(top);
    Sprite* bottom = sprite("eyeLidAnim1.png", 512, 256, 38, {-256, -60});
    bottom->justFlipItV();
    mSpriteArray.addObject(bottom);
    Sprite* pupil = sprite("pupil.png", 16, 16, 45, {-8, -8});
    mSpriteArray.addObject(pupil);
    setup();
    Point at{static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    float turn = static_cast<float>(mInjury->rotation);
    top->currentShape()->pos = at;
    top->currentShape()->rotation = turn;
    scene().addShape(top->currentShape());
    if (mInjury->rotation == 0) {
        bottom->currentShape()->pos = at;
        bottom->currentShape()->rotation = turn;
        scene().addShape(bottom->currentShape());
    }
    pupil->currentShape()->pos = {at.x, at.y - 20};
    pupil->currentShape()->scale = 2;
    scene().addShape(pupil->currentShape());
    releaseRef(top);
    releaseRef(bottom);
    releaseRef(pupil);
    mx = gameState->mouseX;
    my = gameState->mouseY;
}

void Eyelid::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    // The box back on the lid, its size again; the lid shut.
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    oneHitBox(x, y, "hitbox_40x32.png");
    hitboxes[0].sprite->currentShape()->layer = 33;
    opened = 0;
    mSpriteArray[0]->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("eyeLidAnim1");
}

void Eyelid::extendHitBox() {
    HitBox& b = hitboxes[0];
    if (b.rect.size.width != static_cast<float>(box_width)) return;
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    b.rect.size = {static_cast<float>(box_width + 32), static_cast<float>(box_height + 32)};
    b.rect.origin = {x - 16 + static_cast<float>(-(box_width / 2)), y - 16 + static_cast<float>(-(box_height / 2))};
}

bool Eyelid::pull(float dist) {
    // The box goes with the finger, away from the eye.
    double a = static_cast<double>(mInjury->rotation + 180) * 3.141592653589793 / 180.0;
    float dx = static_cast<float>(std::sin(a) * dist), dy = static_cast<float>(dist * std::cos(a));
    HitBox& b = hitboxes[0];
    b.sprite->currentShape()->pos.x += dx;
    b.sprite->currentShape()->pos.y += dy;
    b.rect.origin.x += dx;
    b.rect.origin.y += dy;
    // Open by a fifth each 30 points.
    opened += dist;
    int frame = std::clamp(static_cast<int>(opened / 150.0f * 5.0f), 0, 4);
    mSpriteArray[0]->currentShape()->tex =
        SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("eyeLidAnim" + std::to_string(frame + 1));
    return frame == 4;
}

void Eyelid::render() {
    // The pupil follows the finger once it has moved.
    if (!movedYet && (mx != gameState->mouseX || my != gameState->mouseY)) movedYet = true;
    if (movedYet) followFinger(mSpriteArray[2], static_cast<float>(mInjury->x), static_cast<float>(mInjury->y - 20), 0.5f, 70);
    if (alive) InjuryGraphic::render();
}

void Eyelid::updateInjury() {
    ++currentStage;
    gameState->createPopup("correct", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), this);
    if (currentStage <= maxStages) {
        setup();
        return;
    }
    // Open: its lids and pupil go (the eye beneath is the stage's art).
    for (int k = 0; k < 3; ++k) scene().removeShape(mSpriteArray[k]->currentShape());
    alive = false;
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

}  // namespace as2
