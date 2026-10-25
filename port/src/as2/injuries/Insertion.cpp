#include "as2/injuries/Insertion.h"

#include "as2/GameController.h"
#include "as2/GameState.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

}  // namespace

Insertion::Insertion(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "insertion";
    maxStages = 2;
    alive = true;
    initStageArray(stageTable("Insertion"));
    // The cut (shown once opened) and the pin.
    Sprite* cut = Sprite::initWithImageNamed("largecut.png", 64, 256);
    cut->calculateTextureCoordsAtOffsetX(0, 0);
    cut->currentShape()->layer = 38;
    cut->currentShape()->offset.x = -32;
    mSpriteArray.addObject(cut);
    releaseRef(cut);
    Sprite* pin = Sprite::initWithImageNamed("pin.png", 128, 256);
    pin->calculateTextureCoordsAtOffsetX(0, 0);
    pin->currentShape()->layer = 28;
    pin->currentShape()->offset = {-72, -254};
    mSpriteArray.addObject(pin);
    releaseRef(pin);
    setup();
}

void Insertion::setup() {
    InjuryGraphic::setup();
    if (currentStage == 1) {
        // The line to cut, shown.
        columnHitBoxes(7, "hitbox2_20x16.png", true);
        return;
    }
    freeHitBoxes();
    Shape* cut = mSpriteArray[0]->currentShape();
    cut->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    cut->rotation = static_cast<float>(mInjury->rotation);
    scene().addShape(cut);
}

void Insertion::setInsertionLength(float length) {
    Sprite* pin = mSpriteArray[1];
    Shape* s = pin->currentShape();
    float g = gameController->GfxScaleFactor;
    float v = pin->spritePixelHeight() + g * length;
    s->uv[5] = s->uv[7] = v / pin->spritePixelHeight();
    if (s->uv[5] > 1 && s->tex) s->tex->tile();  // longer than its picture
    s->verts[5] = s->verts[7] = v;
    s->offset.y = -(v / g);
    s->rotation = static_cast<float>(mInjury->rotation - 90);
}

void Insertion::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (maxStages < currentStage) {
        // In: pushed home.
        setInsertionLength(-75);
        alive = false;
        gameState->removeInjury(this);
        return;
    }
    setup();
    // The pin, on the tongs' tray.
    waitOnTray(mSpriteArray[1], hitboxes, hitBoxCount);
}

}  // namespace as2
