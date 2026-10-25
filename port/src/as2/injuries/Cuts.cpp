#include "as2/injuries/Cuts.h"

#include <cmath>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
int missionId() { return Connector::sharedConnector().current_mission_id; }

Sprite* sprite(const std::string& image, int w, int h, int layer, float ox, float oy) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = layer;
    sh->offset = {ox, oy};
    return s;
}

// The pupil the patient of mission 17 follows the finger with (16 points
// at twice the size, over the cut).
Sprite* pupil() {
    Sprite* p = sprite("pupil.png", 16, 16, 45, -8, -8);
    p->currentShape()->scale = 2;
    return p;
}

void placeAt(Sprite* s, const Injury* in) {
    Shape* sh = s->currentShape();
    sh->pos = {static_cast<float>(in->x), static_cast<float>(in->y)};
    sh->rotation = static_cast<float>(in->rotation);
}

// A stage treated: the next, and "correct".
void stageDone(InjuryGraphic* i) {
    ++i->currentStage;
    i->damageTimer = 0;
    gameState->createPopup("correct", i->mInjury->x, i->mInjury->y, i);
}

void setTexture(Sprite* s, const char* name) {
    s->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(name);
}

}  // namespace

// The stage done ("correct"), and the next one's boxes, or the fade.
#define CUT_NEXT_STAGE()                                                              \
    ++currentStage;                                                                   \
    damageTimer = 0;                                                                  \
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);

// ------------------------------------------------------------------- SmallCut

SmallCut::SmallCut(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 32;
    box_height = 16;
    make = "smallCut";
    maxStages = 2;
    alive = true;
    initStageArray(stageTable("SmallCut"));
    Sprite* s = sprite("smallcut.png", 32, 64, 38, -16, 0);
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    releaseRef(s);
    setup();
}

void SmallCut::setup() {
    InjuryGraphic::setup();
    columnHitBoxes(4, "hitbox_20x16.png", false);
}

void SmallCut::render() {
    if (!alive) fadeOut(mSpriteArray[0]);
    else InjuryGraphic::render();
}

void SmallCut::updateInjury() {
    stageDone(this);
    // Burnt shut: the scar.
    if (stage(currentStage - 1).c28) setTexture(mSpriteArray[0], "smallcut2.png");
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        hideGuides();
        setup();
    }
}

// ------------------------------------------------------------------- Incision

Incision::Incision(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "incision";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Incision"));
    // The opened cut: shown (fading in) once cut.
    Sprite* cut = sprite("largecut.png", 64, 256, 38, -32, 0);
    float* c = cut->currentShape()->color;
    c[0] = c[1] = c[2] = c[3] = 0;
    mSpriteArray.addObject(cut);
    Sprite* p = pupil();
    mSpriteArray.addObject(p);
    setup();
    placeAt(cut, mInjury);
    releaseRef(cut);
    releaseRef(p);
    mx = gameState->mouseX;
    my = gameState->mouseY;
    bMovedYet = false;
}

void Incision::setup() {
    InjuryGraphic::setup();
    // The yellow dotted line to cut along.
    columnHitBoxes(7, "hitbox2_20x16.png", true);
    if (missionId() == 16) {
        Sprite* p = mSpriteArray[1];
        scene().removeShape(p->currentShape());
        p->currentShape()->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y + 60)};
        scene().addShape(p->currentShape());
    }
}

void Incision::render() {
    if (!alive) {
        fadeOut(mSpriteArray[0], true);
        return;
    }
    InjuryGraphic::render();
    if (!bMovedYet && (mx != gameState->mouseX || my != gameState->mouseY)) bMovedYet = true;
    if (bMovedYet)
        followFinger(mSpriteArray[1], static_cast<float>(mInjury->x), static_cast<float>(mInjury->y + 50), 0.5f, 70);
}

void Incision::updateInjury() {
    stageDone(this);
    if (stage(currentStage - 1).c28 && currentStage == 2) scene().addShape(mSpriteArray[0]->currentShape());
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        hideGuides();
        setup();
    }
}

// ------------------------------------------------------------------- LargeCut

LargeCut::LargeCut(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 60;
    box_height = 32;
    make = "largeCut";
    maxStages = 3;
    alive = true;
    initStageArray(stageTable("LargeCut"));
    Sprite* cut = sprite("largecut.png", 64, 256, 38, -32, 0);
    mSpriteArray.addObject(cut);
    scene().addShape(cut->currentShape());
    Sprite* p = pupil();
    mSpriteArray.addObject(p);
    releaseRef(p);
    setup();
    placeAt(cut, mInjury);
    releaseRef(cut);
    mx = gameState->mouseX;
    my = gameState->mouseY;
    bMovedYet = false;
    if (missionId() == 16 && !playerCreated) {
        Shape* ps = mSpriteArray[1]->currentShape();
        scene().removeShape(ps);
        ps->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y + 60)};
        scene().addShape(ps);
    }
}

void LargeCut::setup() {
    InjuryGraphic::setup();
    columnHitBoxes(5, "hitbox_40x32.png", false);
}

void LargeCut::render() {
    if (!alive) {
        fadeOut(mSpriteArray[0]);
        return;
    }
    InjuryGraphic::render();
    if (!bMovedYet && (mx != gameState->mouseX || my != gameState->mouseY)) bMovedYet = true;
    if (bMovedYet)
        followFinger(mSpriteArray[1], static_cast<float>(mInjury->x), static_cast<float>(mInjury->y + 70), 0.125f, 20);
}

void LargeCut::updateInjury() {
    stageDone(this);
    // Stitched, then burnt: its look at each.
    if (stage(currentStage - 1).c28) {
        if (currentStage == 2) setTexture(mSpriteArray[0], "largecut2.png");
        else if (currentStage == 3) setTexture(mSpriteArray[0], "largecut3.png");
    }
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        hideGuides();
        setup();
    }
}

// -------------------------------------------------------------- LargeCutScar

LargeCutScar::LargeCutScar(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 60;
    box_height = 32;
    make = "largeCutScar";
    maxStages = 2;
    alive = true;
    initStageArray(stageTable("LargeCutScar"));
    Sprite* cut = sprite("largecut.png", 64, 256, 38, -32, 0);
    mSpriteArray.addObject(cut);
    scene().addShape(cut->currentShape());
    setup();
    releaseRef(cut);
}

void LargeCutScar::setup() {
    InjuryGraphic::setup();
    columnHitBoxes(5, "hitbox_40x32.png", false);
}

void LargeCutScar::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    if (stage(currentStage - 1).c28) {
        if (currentStage == 2) setTexture(mSpriteArray[0], "largecut2.png");
        else if (currentStage == 3) setTexture(mSpriteArray[0], "largecut3.png");
    }
    if (maxStages < currentStage) {
        alive = false;
        gameState->removeInjury(this);
    } else {
        hideGuides();
        setup();
    }
}

// ----------------------------------------------------------------- ClownNose

ClownNose::ClownNose(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "clownNose";
    maxStages = 4;
    alive = true;
    initStageArray(stageTable("ClownNose"));
    // The cut (shown once the nose is off), the nose, the clown's pupils.
    Sprite* cut = sprite("largecut.png", 64, 256, 38, -32, 0);
    mSpriteArray.addObject(cut);
    releaseRef(cut);
    Sprite* nose = sprite("clownNose.png", 256, 256, 38, -128, -64);
    mSpriteArray.addObject(nose);
    scene().addShape(nose->currentShape());
    releaseRef(nose);
    for (float x : {90.0f, 414.0f}) {
        Sprite* eye = sprite("pupil.png", 16, 16, 38, -8, -8);
        eye->currentShape()->pos = {x, 122};
        mSpriteArray.addObject(eye);
        scene().addShape(eye->currentShape());
        releaseRef(eye);
    }
    setup();
    nose->currentShape()->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    mx = gameState->mouseX;
    my = gameState->mouseY;
    bMovedYet = false;
}

void ClownNose::eyeFollows(Sprite* eye, float x, float y) {
    float dx = gameState->mouseX - x, dy = gameState->mouseY - y;
    float d = std::sqrt(dx * dx + dy * dy) * 0.125f;
    if (d > 30) d = 30;
    double a = static_cast<float>(std::atan2(static_cast<double>(dy), static_cast<double>(dx)));
    Shape* s = eye->currentShape();
    s->pos = {static_cast<float>(x + std::cos(a) * d), static_cast<float>(y + d * std::sin(a))};
}

void ClownNose::render() {
    if (!bMovedYet && (mx != gameState->mouseX || my != gameState->mouseY)) bMovedYet = true;
    if (bMovedYet) {
        eyeFollows(mSpriteArray[2], 90, 122);
        eyeFollows(mSpriteArray[3], 414, 122);
    }
    if (alive) {
        InjuryGraphic::render();
        return;
    }
    // Done: the cut fades out over ten frames.
    if (--removeCounter < 1) {
        gameState->removeInjury(this);
        return;
    }
    float v = static_cast<float>(removeCounter) / 10.0f;
    float* c = mSpriteArray[0]->currentShape()->color;
    c[0] = c[1] = c[2] = c[3] = v;
}

void ClownNose::setup() {
    InjuryGraphic::setup();
    if (currentStage == 1) {
        // The line round the nose, shown.
        columnHitBoxes(7, "hitbox2_20x16.png", true, 33);
        return;
    }
    // The cut: stitched, burnt, creamed (5 boxes of 40 by 32).
    int keepW = box_width, keepH = box_height;
    box_width = 40;
    box_height = 32;
    columnHitBoxes(5, "hitbox_40x32.png", false);
    box_width = keepW;
    box_height = keepH;
}

void ClownNose::createEffect(int count, int velocity) {
    Injury* b = new Injury;
    b->mID = 0;
    b->x = static_cast<int>(gameState->mouseX);
    b->y = static_cast<int>(gameState->mouseY);
    b->rotation = 0;
    b->scale = 1;
    for (int n = 0; n < count * 8; ++n) gameState->addBlood(b, velocity);
    releaseRef(b);
}

void ClownNose::updateInjury() {
    stageDone(this);
    if (stage(currentStage - 1).c28) {
        if (currentStage == 2) {
            // The nose off: the cut under it.
            scene().removeShape(mSpriteArray[1]->currentShape());
            hideGuides();
            placeAt(mSpriteArray[0], mInjury);
            mSpriteArray[0]->currentShape()->rotation = 0;
            scene().addShape(mSpriteArray[0]->currentShape());
        }
        if (currentStage == 3) setTexture(mSpriteArray[0], "largecut2.png");
        else if (currentStage == 4) setTexture(mSpriteArray[0], "largecut3.png");
    }
    if (currentStage == 2) {
        // Pop: a burst of blood below the nose.
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("NOSE_POP");
        Shape* nose = mSpriteArray[1]->currentShape();
        Injury* b = new Injury;
        b->mID = 0;
        b->x = static_cast<int>(nose->pos.x);
        b->y = static_cast<int>(nose->pos.y + 64);
        b->rotation = 0;
        b->scale = 1;
        gameState->addBlood(b, -1);
        static const int kVelocities[] = {-1, 1, 2, 4, 6, 7, 8, 10, 11, 13, 14, 15, 16, 18};
        for (int round = 0; round < 15; ++round)
            for (int v : kVelocities) gameState->addBlood(b, v);
        releaseRef(b);
    }
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        setup();
    }
}

// ------------------------------------------------------------------ BloodCut

BloodCut::BloodCut(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    bool large = isKindOf("BloodLargeCut");
    box_width = large ? 60 : 40;
    box_height = large ? 32 : 16;
    boxes = large ? 5 : 4;
    guide = large ? "hitbox_40x32.png" : "hitbox_20x16.png";
    make = large ? "bloodLargeCut" : "bloodSmallCut";
    alive = true;
    initOwnStages(large ? "BloodLargeCut" : "BloodSmallCut");
    Sprite* s = large ? sprite("largecut.png", 64, 256, 38, -32, 0) : sprite("smallcut.png", 32, 64, 38, -16, 0);
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    releaseRef(s);
    setup();
}

void BloodCut::setup() {
    InjuryGraphic::setup();
    columnHitBoxes(boxes, guide, false);
}

void BloodCut::render() {
    if (!alive) fadeOut(mSpriteArray[0]);
    else InjuryGraphic::render();
}

void BloodCut::updateInjury() {
    stageDone(this);
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        hideGuides();
        setup();
    }
}

// ---------------------------------------------------------------------- Burn

Burn::Burn(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 48;
    box_height = 48;
    make = isKindOf("BloodBurn") ? "bloodBurn" : "burn";
    alive = true;
    initOwnStages("Burn");
    Sprite* s = sprite("burn.png", 64, 64, 38, -32, -32);
    mSpriteArray.addObject(s);
    setup();
    scene().addShape(s->currentShape());
    releaseRef(s);
}

void Burn::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    freeHitBoxes();
    // One box over it.
    hitBoxCount = 1;
    hitboxes = new HitBox[1];
    HitBox& b = hitboxes[0];
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    b.rect = {{x - box_width / 2, y - box_height / 2}, {static_cast<float>(box_width), static_cast<float>(box_height)}};
    b.active = true;
    b.colour = 0x66ff33;
    b.health = 2;
    b.sprite = Sprite::initWithImageNamed("hitbox_40x32.png", box_width, box_height);
    b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* g = b.sprite->currentShape();
    g->layer = 44;
    g->offset = {static_cast<float>(-(box_width / 2)), static_cast<float>(-(box_height / 2))};
    g->pos = {x, y};
    g->rotation = static_cast<float>(mInjury->rotation);
}

void Burn::updateInjury() {
    if (!alive) return;
    alive = false;
    removeCounter = 10;
}

void Burn::render() {
    if (alive) {
        InjuryGraphic::render();
        return;
    }
    fadeOut(mSpriteArray[0]);
}

}  // namespace as2

