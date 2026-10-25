#include "as2/injuries/Cosmetic.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
float percent() { return static_cast<float>(std::rand() % 100) / 100.0f; }

// `count` injuries of `id` at (x, y).
void add(int id, float x, float y, int count = 1) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = static_cast<int>(x);
    n->y = static_cast<int>(y);
    n->rotation = 0;
    n->scale = 1;
    for (int k = 0; k < count; ++k) gameState->addInjury(n, false, false);
    releaseRef(n);
}

constexpr int kFire = 0x1d, kFireSplatter = 0x1b, kFatSplatter = 0x45;

Sprite* picture(const std::string& image, int w, int h) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 38;
    sh->offset = {static_cast<float>(-w / 2), static_cast<float>(-h / 2)};
    return s;
}

void removeSelf(InjuryGraphic* i) {
    retainRef(i);
    gameState->removeInjury(i);
    releaseRef(i);
}

}  // namespace

Fat::Fat(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 64;
    box_height = 64;
    make = "fat";
    maxStages = 2;
    alive = true;
    hidden = true;
    initStageArray(stageTable("Fat"));
    // Shown once it is cut open.
    Sprite* s = picture("fat.png", 256, 256);
    mSpriteArray.addObject(s);
    setup();
    releaseRef(s);
}

void Fat::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_40x32.png");
}

void Fat::updateInjury() {
    Shape* s = mSpriteArray[0]->currentShape();
    if (currentStage == 1) {
        // Cut open: it shows, throwing fat.
        int n = std::min(2, static_cast<int>(percent() * 2.0f + 1.0f));
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SPLAT" + std::to_string(n));
        hidden = false;
        add(kFatSplatter, s->pos.x, s->pos.y, 3);
        scene().addShape(s);
    }
    ++currentStage;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) {
        setup();
        return;
    }
    // Burnt: it bursts.
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("NOSE_POP");
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXPLOSION");
    alive = false;
    add(kFatSplatter, s->pos.x, s->pos.y, 3);
    add(kFatSplatter, s->pos.x - 30, s->pos.y - 30, 3);
    add(kFatSplatter, s->pos.x + 30, s->pos.y + 30, 3);
    add(kFire, s->pos.x, s->pos.y);
    gameState->health = gameState->health - 2;
    add(kFireSplatter, s->pos.x, s->pos.y);
    gameState->health = gameState->health - 2;
    removeSelf(this);
}

Lip::Lip(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 96;
    box_height = 40;
    make = "lip";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Lip"));
    // The full lip, shown once plumped: the top one, or the bottom; the
    // right-hand ones flipped.
    Sprite* s = picture("topLip.png", 256, 128);
    mSpriteArray.addObject(s);
    setup();
    const std::string& at = mInjuryData->linkage;
    if (at == "BottomLipLeft" || at == "BottomLipRight")
        s->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("bottomLip");
    if (at == "BottomLipRight" || at == "TopLipRight") s->justFlipItH();
    releaseRef(s);
}

void Lip::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    // The box a little off the injury's place, toward the mouth's middle.
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    const std::string& at = mInjuryData->linkage;
    if (at == "BottomLipLeft") x += 6, y -= 4;
    if (at == "TopLipLeft") x += 15, y += 4;
    if (at == "BottomLipRight") x -= 16, y -= 4;
    if (at == "TopLipRight") x -= 24, y += 4;
    oneHitBox(x, y, "hitbox_40x32.png");
}

void Lip::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) return;
    // Plumped: the full lip shows.
    scene().addShape(mSpriteArray[0]->currentShape());
    alive = false;
    invincible = true;
    removeSelf(this);
}

Unibrow::Unibrow(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 64;
    box_height = 32;
    make = "unibrow";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Unibrow"));
    Sprite* s = picture("monobrow.png", 64, 32);
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    setup();
    releaseRef(s);
}

void Unibrow::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_40x32.png");
}

void Unibrow::updateInjury() {
    // Burnt off: a fire where it was.
    Shape* s = mSpriteArray[0]->currentShape();
    add(kFire, s->pos.x, s->pos.y);
    alive = false;
    removeSelf(this);
    gameState->health = gameState->health - 2;
}

}  // namespace as2
