#include "as2/injuries/Puslet.h"

#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameState.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
constexpr int kSmallCut = 1, kPoisonSplatter = 0xb, kYellowSplatter = 0x73;

void add(int id, int x, int y, int rotation) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = x;
    n->y = y;
    n->rotation = rotation;
    n->scale = 1;
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

}  // namespace

Puslet::Puslet(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 48;
    box_height = 48;
    make = "pussy";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Puslet"));
    Sprite* s = Sprite::initWithImageNamed("pustule", 64, 64);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 38;
    sh->offset = {-32, -32};
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    mSpriteArray.addObject(s);
    setup();
    scene().addShape(sh);
    releaseRef(s);
}

void Puslet::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_40x32.png");
}

void Puslet::updateInjury() {
    if (!alive) return;
    // Lanced: pus everywhere, a cut left.
    for (int n = 0; n < 10; ++n) add(kYellowSplatter, mInjury->x, mInjury->y, 0);
    add(kSmallCut, mInjury->x, mInjury->y, std::rand() % 360);
    alive = false;
    removeCounter = 10;
}

void Puslet::render() {
    if (alive) {
        InjuryGraphic::render();
        return;
    }
    // Fading away over 10 frames.
    retainRef(this);
    fadeOut(mSpriteArray[0]);
    releaseRef(this);
}

PoisonParticle::PoisonParticle(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 80;
    box_height = 80;
    make = "PoisonParticle";
    maxStages = 1;
    initStageArray(stageTable("PoisonParticle"));
    setup();
}

void PoisonParticle::render() {
    // At once three drops of poison, and it is gone.
    for (int n = 0; n < 3; ++n) add(kPoisonSplatter, mInjury->x, mInjury->y, 0);
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

}  // namespace as2
