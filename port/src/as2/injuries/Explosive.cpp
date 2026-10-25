#include "as2/injuries/Explosive.h"

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"

namespace as2 {

namespace {

void add(int id, int x, int y) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = x;
    n->y = y;
    n->rotation = 0;
    n->scale = 1;
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

constexpr int kPoison = 0xc, kFireSplatter = 0x1b, kFire = 0x1d;

}  // namespace

ExplosiveExtraction::ExplosiveExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : PictureExtraction(d, i, playerCreated, force, "ExplosiveExtraction", "cyst.png", 64, 48) {
    make = "explosiveExtraction";
    setup();
}

void ExplosiveExtraction::updateInjury() {
    if (currentStage == 1) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXPLOSION");
        add(kFireSplatter, mInjury->x, mInjury->y);
        add(kFire, mInjury->x, mInjury->y);
        gameState->health = gameState->health - 2;
    }
    Extraction::updateInjury();
}

ExplosiveExtractionBomb::ExplosiveExtractionBomb(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 48;
    box_height = 48;
    make = "explosiveExtractionBomb";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("ExplosiveExtractionBomb"));
    Sprite* s = Sprite::initWithImageNamed("minibomb.png", 128, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 38;
    sh->offset = {-64, -64};
    mSpriteArray.addObject(s);
    setup();
    SceneManager::sharedSceneManagerInstance().addShape(sh);
    releaseRef(s);
}

void ExplosiveExtractionBomb::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_40x32.png");
}

void ExplosiveExtractionBomb::updateInjury() {
    Shape* s = mSpriteArray[0]->currentShape();
    if (currentStage == 1) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXPLOSION");
        int x = static_cast<int>(s->pos.x), y = static_cast<int>(s->pos.y);
        add(kFire, x, y);
        add(kFireSplatter, x, y);
        add(kPoison, x, y);
        gameState->health = gameState->health - 2;
    }
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

}  // namespace as2
