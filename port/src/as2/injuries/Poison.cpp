#include "as2/injuries/Poison.h"

#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameState.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

// PoisonSplatter, `count` times at (x, y).
void splatters(int x, int y, int count) {
    Injury* s = new Injury;
    s->mID = 11;
    s->x = x;
    s->y = y;
    s->rotation = 0;
    s->scale = 1;
    for (int n = 0; n < count; ++n) gameState->addInjury(s, false, false);
    releaseRef(s);
}

}  // namespace

Poison::Poison(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 40;
    box_height = 32;
    make = "poison";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Poison"));
    Sprite* s = Sprite::initWithImageNamed("poison.png", 128, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 34;
    sh->offset = {-64, -64};
    sh->scale = static_cast<float>(0.9 + static_cast<double>(static_cast<float>(std::rand() % 100) / 100.0f) * 0.2);
    mSpriteArray.addObject(s);
    scene().addShape(sh);
    setup();
    releaseRef(s);
}

void Poison::setup() {
    InjuryGraphic::setup();
    // One box over it, not drawn (nothing to heal: the injection's).
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_20x16.png", 0);
}

void Poison::render() {
    Shape* s = mSpriteArray[0]->currentShape();
    if (!(s->scale < 1.1f)) scaleDirection = -0.0125f;
    else if (!(s->scale > 0.9f)) scaleDirection = 0.0125f;
    s->scale += scaleDirection;
}

void Poison::updateInjury() {
    if (currentStage != 1) return;
    splatters(mInjury->x, mInjury->y, 3);
    alive = false;
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

void Poison::createEffect(int count, int) {
    splatters(static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), count);
}

}  // namespace as2
