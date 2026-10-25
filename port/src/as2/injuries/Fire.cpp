#include "as2/injuries/Fire.h"

#include <cstdlib>
#include <string>

#include "as2/GameController.h"
#include "as2/GameState.h"

namespace as2 {

Fire::Fire(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 48;
    box_height = 48;
    make = "fire";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Fire"));
    Sprite* s = Sprite::initWithImageNamed("flameAnim1", 128, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 36;
    sh->offset = {-64, -115};
    mSpriteArray.addObject(s);
    setup();
    SceneManager::sharedSceneManagerInstance().addShape(sh);
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    releaseRef(s);
}

void Fire::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    // One box over it, not drawn.
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_40x32.png");
}

void Fire::render() {
    // The flames: 1 to 6 and back, four frames each.
    ++currentFrame;
    if (!alive) return;
    int n = currentFrame % 44 / 4 + 1;
    if (n > 6) n = 12 - n;
    mSpriteArray[0]->currentShape()->tex =
        SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("flameAnim" + std::to_string(n));
}

void Fire::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) {
        setup();
        return;
    }
    // Out: a burn where it was.
    Point at = mSpriteArray[0]->currentShape()->pos;
    Injury* burn = new Injury;
    burn->mID = 0x1c;
    burn->x = static_cast<int>(at.x);
    burn->y = static_cast<int>(at.y);
    burn->rotation = static_cast<int>(static_cast<float>(std::rand() % 100) / 100.0f * 360.0f);
    burn->scale = 1;
    gameState->addInjury(burn, mPlayerCreated, false);
    releaseRef(burn);
    alive = false;
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

}  // namespace as2
