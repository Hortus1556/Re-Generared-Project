#include "as2/injuries/AmpPlug.h"

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"

namespace as2 {

namespace {

void colour(Sprite* s, float r, float g, float b, float a) {
    if (!s) return;
    float* c = s->currentShape()->color;
    c[0] = r;
    c[1] = g;
    c[2] = b;
    c[3] = a;
}

// The background's pink (the brain with the amp in).
void pinkBackground() { colour(gameState->spriteBg, 0.71875f, 0.3125f, 0.71875f, 1); }

Sprite* picture(const char* image, int w, int h, Point offset) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 28;
    sh->offset = offset;
    return s;
}

}  // namespace

AmpPlug::AmpPlug(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "AmpPlug";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("AmpPlug"));
    // The plug, by its linkage.
    const std::string& kind = d->linkage;
    Sprite* s = nullptr;
    if (kind == "AmpPlug1") s = picture("S9_plug1_2.png", 32, 512, {-12, -93});
    else if (kind == "AmpPlug2") s = picture("S9_plug2_2.png", 64, 512, {-32, -76});
    else if (kind == "AmpPlug3") s = picture("S9_plug3_2.png", 64, 512, {-32, -76});
    else if (kind == "AmpPlug4") s = picture("cap", 512, 128, {-256, -64});
    if (s) {
        mSpriteArray.addObject(s);
        releaseRef(s);
    }
    setup();
    if (s) waitOnTray(s);
    pinkBackground();
}

void AmpPlug::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) return;
    // In: its plugged picture.
    const std::string& kind = mInjuryData->linkage;
    const char* in = kind == "AmpPlug1" ? "S9_plug1" : kind == "AmpPlug2" ? "S9_plug2" : kind == "AmpPlug3" ? "S9_plug3" : nullptr;
    if (in && mSpriteArray.count() > 0)
        mSpriteArray[0]->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(in);
    alive = false;
}

void AmpPlug::render() {
    if (alive) return;
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

BrainPill::BrainPill(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "insertion";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("BrainPill"));
    Sprite* s = picture("pill.png", 128, 256, {-64, -128});
    mSpriteArray.addObject(s);
    releaseRef(s);
    setup();
    waitOnTray(s);
    pinkBackground();
}

void BrainPill::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) return;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("Aspirin_Splash");
    alive = false;
    currentFrame = 0;
}

void BrainPill::render() {
    if (alive) return;
    Shape* sh = mSpriteArray[0]->currentShape();
    if (++currentFrame == 1) sh->pos.y -= 44;
    // It fizzes, rising.
    sh->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("fizz_" +
                                                                                  std::to_string(currentFrame % 15 + 1));
    sh->pos.y += 0.5f;
    if (currentFrame < 91) {
        // The brain's pink easing back over three seconds.
        float f = static_cast<float>(currentFrame) / 90.0f;
        colour(gameState->spriteBg, 0.71875f - f * 0.28125f, 0.3125f + f * 0.495f, 0.71875f - f * 0.09375f, 1);
        if (gameState->spriteBg && gameState->spriteFg) {
            const float* bg = gameState->spriteBg->currentShape()->color;
            colour(gameState->spriteFg, bg[0] * 0.1f, bg[1] * 0.1f, bg[2] * 0.1f, 0.1f);
        }
    } else {
        // Then the fizz fades.
        float a = static_cast<float>(120 - currentFrame) / 30.0f;
        sh->color[0] = sh->color[1] = sh->color[2] = sh->color[3] = a;
    }
    if (currentFrame > 119) {
        retainRef(this);
        gameState->removeInjury(this);
        releaseRef(this);
    }
}

}  // namespace as2
