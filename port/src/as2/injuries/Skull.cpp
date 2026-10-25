#include "as2/injuries/Skull.h"

#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

}  // namespace

SkullCut::SkullCut(InjuryData* d, Injury* i, bool playerCreated, bool force) : BoneTransplant(d, i, playerCreated, force) {
    make = "skull";
    // The eye masks: the right one is the kind's "healthy" sprite.
    Shape* right = mSpriteArray[1]->currentShape();
    right->pos = {190, 234};
    scene().addShape(right);
    Sprite* left = Sprite::initWithImageNamed("left_eye_mask.png", 64, 64);
    left->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = left->currentShape();
    sh->layer = 40;
    sh->offset = {-32, -32};
    sh->pos = {345, 234};
    mSpriteArray.addObject(left);
    scene().addShape(sh);
    releaseRef(left);
}

void SkullCut::updateInjury() {
    // The cap gone for good: the masks with it.
    bool all = true;
    for (const Part& p : parts)
        if (!p.removed) all = false;
    if (all && currentStage + 1 > maxStages)
        for (int k = 1; k < mSpriteArray.count(); ++k) scene().removeShape(mSpriteArray[k]->currentShape());
    BoneTransplant::updateInjury();
}

ReplaceSkull::ReplaceSkull(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "insertion";
    maxStages = 3;
    alive = true;
    initStageArray(stageTable("ReplaceSkull"));
    Sprite* cap = Sprite::initWithImageNamed("cap.png", 512, 128);
    cap->calculateTextureCoordsAtOffsetX(0, 0);
    cap->currentShape()->layer = 28;
    cap->currentShape()->offset = {-256, -64};
    mSpriteArray.addObject(cap);
    releaseRef(cap);
    setup();
    waitOnTray(cap);
}

void ReplaceSkull::setup() {
    InjuryGraphic::setup();
    if (currentStage < 2) return;
    // Back on: its line to burn, then cream.
    freeHitBoxes();
    hitBoxCount = 11;
    hitboxes = new HitBox[11];
    lineHitBoxes(hitboxes, GameData::skullHitPoints, 11, static_cast<float>(mInjury->x),
                 static_cast<float>(mInjury->y));
}

void ReplaceSkull::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage == 2) {
        // In its place, over the head.
        Shape* sh = mSpriteArray[0]->currentShape();
        scene().removeShape(sh);
        sh->layer = 41;
        scene().addShape(sh);
    }
    if (currentStage > maxStages) {
        alive = false;
        gameState->removeInjury(this);
        return;
    }
    setup();
}

}  // namespace as2
