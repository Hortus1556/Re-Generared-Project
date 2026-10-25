#include "as2/injuries/Extraction.h"

#include <cmath>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

// SurfaceExtraction setup: the object of each kind (by its library linkage;
// the Under... kinds share theirs): its picture (128 points wide, `height`
// high), the pixels of it showing at first (startLen, times the screen
// scale; 0 keeps the 56 points of init), how far it is pulled to come out
// (removableLength), its sprite's length, and whether it is poisoned.
struct ObjectKind {
    const char* linkage;
    const char* image;
    int height;
    int startLen, removableLength, spriteLength;
    bool poison;
};
const ObjectKind kKinds[] = {
    {"UnderSurfaceExtractionPeanut", "nutty.png", 128, 44, 40, 0, true},
    {"SurfaceExtractionPeanut", "nutty.png", 128, 44, 40, 0, true},
    {"UnderSurfaceExtractionBattery", "batteryextraction.png", 128, 44, 40, 0, true},
    {"UnderSurfaceExtractionLipstick", "lipstick.png", 128, 60, 50, 0, true},
    {"UnderSurfaceExtractionPizza", "pizza.png", 256, 70, 80, 224, true},
    {"UnderSurfaceExtractionSyringe", "syringeextraction.png", 256, 70, 90, 224, true},
    {"UnderSurfaceExtraction8Ball", "8ball.png", 128, 50, 50, 0, false},
    {"UnderSurfaceExtractionLionKey", "lionkey.png", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionTopHat", "tophat.png", 128, 0, 0, 0, false},
    {"SurfaceExtractionChickun", "chcikun.png", 256, 72, 80, 224, false},
    {"SurfaceExtractionSpecs", "glasses.png", 256, 80, 100, 224, false},
    {"SurfaceExtractionWand", "wandGoo.png", 256, 0, 0, 224, false},
    {"SurfaceExtractionHonker", "honkerGoo.png", 256, 86, 90, 224, false},
    {"SurfaceExtractionJuggly", "jugglyGoo.png", 256, 86, 110, 224, false},
    {"SurfaceExtractionFlute", "snakefluteGoo.png", 256, 86, 110, 224, false},
    {"UnderSurfaceExtractionGlass", "glass.png", 128, 0, 0, 0, false},
    {"SurfaceExtractionGlass", "glass.png", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionWire", "barbed.png", 128, 0, 0, 0, false},
    {"SurfaceExtractionWire", "barbed.png", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionMoney", "money.png", 128, 0, 0, 0, false},
    {"SurfaceExtractionMoney", "money.png", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionCard", "license.png", 128, 0, 0, 0, false},
    {"SurfaceExtractionCard", "license.png", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionPassport", "passport.png", 128, 0, 0, 0, false},
    {"SurfaceExtractionPassport", "passport.png", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionKnee1", "boneExtraction1", 128, 0, 0, 0, false},
    {"SurfaceExtractionKnee1", "boneExtraction1", 128, 0, 0, 0, false},
    {"SurfaceExtractionThigh1", "boneExtraction1", 128, 0, 0, 0, false},
    {"SurfaceExtractionThigh2", "boneExtraction1", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionKnee2", "boneExtraction1", 128, 0, 0, 0, false},
    {"SurfaceExtractionKnee2", "boneExtraction1", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionCrab", "boneExtraction1", 128, 0, 0, 0, false},
    {"SurfaceExtractionCrab", "boneExtraction1", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionTooth1", "tooth1", 128, 0, 0, 0, false},
    {"SurfaceExtractionTooth1", "tooth1", 128, 0, 0, 0, false},
    {"UnderSurfaceExtractionTooth2", "tooth2", 128, 0, 0, 0, false},
    {"SurfaceExtractionTooth2", "tooth2", 128, 0, 0, 0, false},
    {"SurfaceExtractionStake", "steak", 256, 0, 0, 180, false},
    {"SurfaceExtractionGarlic", "garlic", 128, 0, 40, 110, false},  // (its class's 40)
    {"UnderSurfaceExtractionSilver", "bullet", 128, 0, 0, 110, false},
    {"SurfaceExtractionTentacle1", "tentacle1", 256, 0, 0, 256, false},
    {"SurfaceExtractionTentacle2", "tentacle2", 256, 0, 0, 256, false},
    {"SurfaceExtractionTentacle3", "tentacle1", 256, 0, 0, 256, false},
    {"SurfaceExtractionTentacle4", "tentacle4", 128, 0, 0, 0, false},
    {"SurfaceExtractionTentacle5", "tentacle2", 256, 0, 0, 256, false},
    {"UnderSurfaceExtractionClown", "bunny", 1024, 0, 0, 0, false},
    {"SurfaceExtractionNeedle", "needle", 128, 0, 0, 0, false},
    {"SurfaceExtractionFang", "fangRemovable", 128, 0, 0, 0, false},
};

const ObjectKind* kindOf(const std::string& linkage) {
    for (const ObjectKind& k : kKinds)
        if (linkage == k.linkage) return &k;
    return nullptr;
}

// The object's quad shows `pixels` of it (from its top), its texture cut
// to match.
void showLength(Sprite* s, float pixels, float uvBottom) {
    Shape* sh = s->currentShape();
    sh->uv[5] = sh->uv[7] = uvBottom;
    if (uvBottom > 1 && sh->tex) sh->tex->tile();  // longer than its picture
    sh->verts[5] = sh->verts[7] = pixels;
}

}  // namespace

SurfaceExtraction::SurfaceExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    startLen = static_cast<float>(static_cast<int>(gameController->GfxScaleFactor * 56));
    box_width = 80;
    box_height = 64;
    angleBuffer = 35;
    removableLength = 60;
    spriteLength = 128;
    hidden = false;
    make = "surfaceExtraction";
    alive = true;
    // (The stake and the garlic: three stages, the last vacuumed.)
    initOwnStages("SurfaceExtraction");
    origRX = origHX = -1;
    // The wound, then (setup) the object in it.
    Sprite* wound = makeWound();
    setup();
    if (mSpriteArray.count() > 1) scene().addShape(mSpriteArray[1]->currentShape());
    scene().addShape(wound->currentShape());
    addRemovable();
}

SurfaceExtraction::SurfaceExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force, bool)
    : InjuryGraphic(d, i, playerCreated, force) {}

Sprite* SurfaceExtraction::makeWound() {
    Sprite* wound = Sprite::initWithImageNamed("largecut.png", 64, 256);
    wound->calculateTextureCoordsAtOffsetX(0, 0);
    wound->currentShape()->layer = 38;
    wound->currentShape()->offset.x = -32;
    mSpriteArray.addObject(wound);
    releaseRef(wound);
    return wound;
}

void SurfaceExtraction::addRemovable() {
    // The object the tongs take.
    DraggableWrapper* w = new DraggableWrapper;
    w->injury = this;
    w->mSprite = mSpriteArray.count() > 1 ? mSpriteArray[1] : nullptr;
    w->hitboxes = hitboxes;
    w->hitBoxCount = hitBoxCount;
    if (w->mSprite) {
        w->xOrigin = w->mSprite->currentShape()->pos.x;
        w->yOrigin = w->mSprite->currentShape()->pos.y;
    }
    removable.addObject(w);
    releaseRef(w);
}

void SurfaceExtraction::placeObject(float uvBottom) {
    Sprite* s = mSpriteArray[1];
    showLength(s, startLen, uvBottom);
    Shape* sh = s->currentShape();
    sh->offset = {8, -(startLen / gameController->GfxScaleFactor)};
    sh->rotation = static_cast<float>(90 + mInjury->rotation);
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
}

SurfaceExtraction::~SurfaceExtraction() = default;

void SurfaceExtraction::render() {
    if (!alive) fadeOut(mSpriteArray[0]);
    else InjuryGraphic::render();
}

void SurfaceExtraction::extendHitBox() {
    bool under = isKindOf("UnderSurfaceExtraction");
    if (currentStage == 1 || (currentStage == 2 && under)) {
        for (int i = 0; i < hitBoxCount; ++i) hitboxes[i].rect.size = {96, 112};
    } else if (currentStage == 2 && isKindOf("UnderSurfaceExtractionSilver")) {
        for (int i = 0; i < hitBoxCount; ++i) hitboxes[i].rect.size = {96, 132};
    }
}

void SurfaceExtraction::setExtractionLength(float length) {
    if (mSpriteArray.count() < 2) return;
    Sprite* s = mSpriteArray[1];
    float g = gameController->GfxScaleFactor;
    float v = g * length + startLen;
    showLength(s, v, v / s->spritePixelHeight());
    s->currentShape()->offset.y = -(v / g);
    bool grows = currentStage == 1 ||
                 (currentStage == 2 && (isKindOf("UnderSurfaceExtraction") || isKindOf("UnderSurfaceExtractionSilver")));
    if (grows)
        for (int i = 0; i < hitBoxCount; ++i) hitboxes[i].rect.size.width = g * length + 64 + 32;
}

void SurfaceExtraction::setup() {
    InjuryGraphic::setup();
    hasPoison = false;
    beingHeld = false;
    float ssf = gameController->ScreenScaleFactor;
    if (mSpriteArray.count() == 1) {
        // The object, once.
        const ObjectKind* k = mInjuryData ? kindOf(mInjuryData->linkage) : nullptr;
        if (k) {
            if (k->startLen) startLen = static_cast<float>(static_cast<int>(ssf * static_cast<float>(k->startLen)));
            if (k->removableLength) removableLength = k->removableLength;
            if (k->spriteLength) spriteLength = k->spriteLength;
            if (k->poison) hasPoison = true;
            Sprite* s = Sprite::initWithImageNamed(k->image, 128, k->height);
            s->calculateTextureCoordsAtOffsetX(0, 0);
            s->currentShape()->layer = 35;
            mSpriteArray.addObject(s);
            releaseRef(s);
            placeObject(startLen / s->spritePixelHeight());
        }
    }
    if (isKindOf("UnderSurfaceExtraction") || isKindOf("UnderSurfaceExtractionClown") ||
        isKindOf("UnderSurfaceExtractionSilver"))
        return;  // their own setup does the rest
    if (currentStage == 1 && mSpriteArray.count() > 1) {
        // Back in its wound, as at first.
        placeObject(startLen * 0.5f / static_cast<float>(spriteLength));
        setExtractionLength(0);
    }
    freeHitBoxes();
    if (currentStage < 3) objectBox(80);
    else stitchBoxes();
}

void SurfaceExtraction::objectBox(int height) {
    // The object's box, where the tongs take it.
    hitBoxCount = 1;
    hitboxes = new HitBox[1];
    HitBox& b = hitboxes[0];
    b.rect = {{static_cast<float>(mInjury->x - 16), static_cast<float>(mInjury->y + 32)},
              {64, static_cast<float>(height)}};
    b.active = true;
    b.colour = 0x66ff33;
    b.health = 2;
    b.sprite = Sprite::initWithImageNamed("hitbox_64x80.png", 64, height);
    b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = b.sprite->currentShape();
    sh->layer = 44;
    sh->offset = {-16, 32};
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    sh->rotation = static_cast<float>(mInjury->rotation);
    if (removable.count() > 0) {
        removable[0]->hitboxes = hitboxes;
        removable[0]->hitBoxCount = hitBoxCount;
    }
}

void SurfaceExtraction::stitchBoxes() {
    // The wound left: stitched like a large cut.
    int keepW = box_width, keepH = box_height;
    box_width = 40;
    box_height = 32;
    columnHitBoxes(5, "hitbox_40x32.png", false);
    box_width = keepW;
    box_height = keepH;
    if (removable.count() > 0) {
        removable[0]->hitboxes = nullptr;
        removable[0]->hitBoxCount = 0;
    }
}

void SurfaceExtraction::updateInjury() {
    Sprite* wound = mSpriteArray[0];
    double r = mInjury->rotation * 3.141592653589793 / 180.0;
    // 70 points out along it.
    float ox = static_cast<float>(std::sin(r) * -70.0), oy = static_cast<float>(std::cos(r) * 70.0);
    if (currentStage == 1) {
        // Pulled out: it bleeds.
        Injury* b = new Injury;
        b->mID = 0;
        b->x = static_cast<int>(ox + wound->currentShape()->pos.x);
        b->y = static_cast<int>(oy + wound->currentShape()->pos.y);
        b->rotation = 0;
        b->scale = 1;
        gameState->addBlood(b, -1);
        releaseRef(b);
    }
    ++currentStage;
    damageTimer = 0;
    if (stage(currentStage - 1).c28) {
        const char* look = currentStage == 3 ? "largecut.png" : currentStage == 4 ? "largecut2.png"
                           : currentStage == 5 ? "largecut3.png" : nullptr;
        if (look) wound->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(look);
    }
    if (currentStage == 3)
        gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    else
        gameState->createPopup("correct", static_cast<int>(static_cast<float>(mInjury->x) + ox),
                               static_cast<int>(static_cast<float>(mInjury->y) + oy), this);
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        hideGuides();
        setup();
    }
}

// ----------------------------------------------------- UnderSurfaceExtraction

UnderSurfaceExtraction::UnderSurfaceExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : SurfaceExtraction(d, i, playerCreated, force, true) {
    startLen = static_cast<float>(static_cast<int>(gameController->GfxScaleFactor * 56));
    box_width = 80;
    box_height = 64;
    angleBuffer = 35;
    removableLength = 46;
    spriteLength = 128;
    make = "underSurfaceExtraction";
    alive = true;
    // Under the skin: nothing shows until it is cut open.
    hidden = true;
    // (The silver bullet: four stages, the last vacuumed.)
    initOwnStages("UnderSurfaceExtraction");
    origRX = origHX = -1;
    makeWound();
    setup();
    addRemovable();
}

void UnderSurfaceExtraction::setup() {
    SurfaceExtraction::setup();
    if (currentStage == 2 && mSpriteArray.count() == 2 && mInjuryData)
        placeObject(startLen / mSpriteArray[1]->spritePixelHeight());
    if (currentStage < 4) {
        // One box for the cut and the pull, kept through both.
        if (!hitboxes) objectBox(100);
    } else {
        freeHitBoxes();
        stitchBoxes();
    }
}

void UnderSurfaceExtraction::updateInjury() {
    Sprite* wound = mSpriteArray[0];
    double r = mInjury->rotation * 3.141592653589793 / 180.0;
    // 70 points out along it.
    float ox = static_cast<float>(std::sin(r) * -70.0), oy = static_cast<float>(std::cos(r) * 70.0);
    auto add = [](int id, float x, float y, bool blood) {
        Injury* n = new Injury;
        n->mID = id;
        n->x = static_cast<int>(x);
        n->y = static_cast<int>(y);
        n->rotation = 0;
        n->scale = 1;
        if (blood) gameState->addBlood(n, -1);
        else gameState->addInjury(n, false, false);
        releaseRef(n);
    };
    if (currentStage == 1) {
        // Cut open: the wound and the object show; it bleeds, and a
        // poisonous one splashes poison and leaves some.
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SPLAT1");
        hidden = false;
        scene().addShape(mSpriteArray[1]->currentShape());
        scene().addShape(wound->currentShape());
        Point at = wound->currentShape()->pos;
        add(0, ox + at.x, oy + at.y, true);
        if (hasPoison) {
            add(11, static_cast<float>(mInjury->x) + ox, static_cast<float>(mInjury->y) + oy, false);
            add(12, ox + at.x, oy + at.y, false);
        }
    }
    ++currentStage;
    gameState->createPopup("correct", static_cast<int>(static_cast<float>(mInjury->x) + ox),
                           static_cast<int>(static_cast<float>(mInjury->y) + oy), this);
    if (stage(currentStage - 1).c28) {
        const char* look = currentStage == 4 ? "largecut.png" : currentStage == 5 ? "largecut2.png"
                           : currentStage == 6 ? "largecut3.png" : nullptr;
        if (look) wound->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(look);
    }
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        hideGuides();
        setup();
    }
}

// ------------------------------------------------ UnderSurfaceExtractionClown

UnderSurfaceExtractionClown::UnderSurfaceExtractionClown(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : SurfaceExtraction(d, i, playerCreated, force, true) {
    startLen = static_cast<float>(static_cast<int>(gameController->GfxScaleFactor * 56));
    box_width = 80;
    box_height = 64;
    angleBuffer = 35;
    removableLength = 800;
    make = "underSurfaceExtraction";
    maxStages = 5;
    alive = true;
    hidden = true;
    moved = 0;
    initStageArray(stageTable("UnderSurfaceExtractionClown"));
    origRX = origHX = -1;
    makeWound();
    setup();
    addRemovable();
}

void UnderSurfaceExtractionClown::setExtractionLength(float length) {
    if (mSpriteArray.count() < 2) return;
    Sprite* s = mSpriteArray[1];
    float g = gameController->GfxScaleFactor;
    float v = g * length + startLen;
    showLength(s, v, v / s->spritePixelHeight());
    s->currentShape()->offset.y = -(v / g);
    if (currentStage == 2)
        for (int k = 0; k < hitBoxCount; ++k) hitboxes[k].rect.size.width = g * length + 64;
}

void UnderSurfaceExtractionClown::render() {
    SurfaceExtraction::render();
    // Let go of, it slips back in.
    if (alive && !beingHeld && currentStage == 2 && moved > 0) {
        moved -= 1;
        setExtractionLength(moved);
    }
}

void UnderSurfaceExtractionClown::setup() {
    SurfaceExtraction::setup();
    if (currentStage == 2 && mSpriteArray.count() == 2 && mInjuryData)
        placeObject(startLen / mSpriteArray[1]->spritePixelHeight());
    if (currentStage < 3) {
        // One box for the cut and the pull.
        if (!hitboxes) objectBox(100);
    } else {
        freeHitBoxes();
        stitchBoxes();
    }
}

void UnderSurfaceExtractionClown::updateInjury() {
    Sprite* wound = mSpriteArray[0];
    double r = mInjury->rotation * 3.141592653589793 / 180.0;
    float ox = static_cast<float>(std::sin(r) * -70.0), oy = static_cast<float>(std::cos(r) * 70.0);
    if (currentStage == 1) {
        // Cut open: it shows, and bleeds.
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SPLAT1");
        hidden = false;
        scene().addShape(mSpriteArray[1]->currentShape());
        scene().addShape(wound->currentShape());
        Injury* b = new Injury;
        b->mID = 0;
        b->x = static_cast<int>(ox + wound->currentShape()->pos.x);
        b->y = static_cast<int>(oy + wound->currentShape()->pos.y);
        b->rotation = 0;
        b->scale = 1;
        gameState->addBlood(b, -1);
        releaseRef(b);
    }
    ++currentStage;
    gameState->createPopup("correct", static_cast<int>(static_cast<float>(mInjury->x) + ox),
                           static_cast<int>(static_cast<float>(mInjury->y) + oy), this);
    if (stage(currentStage - 1).c28) {
        const char* look = currentStage == 2 ? "largecut.png" : currentStage == 4 ? "largecut2.png"
                           : currentStage == 5 ? "largecut3.png" : nullptr;
        if (look) wound->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(look);
    }
    if (currentStage == 3) {
        // All out: a bunny with it.
        Shape* w = wound->currentShape();
        Injury* bunny = new Injury;
        bunny->mID = 66;
        bunny->x = static_cast<int>(w->pos.x - 64);
        bunny->y = static_cast<int>(w->pos.y - 64);
        bunny->rotation = static_cast<int>(w->rotation + 270);
        bunny->scale = 1;
        bunny->offset = true;
        gameState->addInjury(bunny, false, true);
        releaseRef(bunny);
    }
    if (maxStages < currentStage) {
        alive = false;
        removeCounter = 10;
    } else {
        setup();
    }
}

// ---------------------------------------------------------------- Extraction

Extraction::Extraction(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    make = "extraction";
}

void Extraction::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), this);
    alive = false;
    gameState->removeInjury(this);
}

// ---------------------------------------------------------- PictureExtraction

PictureExtraction::PictureExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force, const char* stages,
                                     const char* picture, int size, int box, int layer)
    : Extraction(d, i, playerCreated, force) {
    box_width = box;
    box_height = box;
    maxStages = 1;
    alive = true;
    initStageArray(stageTable(stages));
    Sprite* s = Sprite::initWithImageNamed(picture, size, size);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = layer;
    sh->offset = {static_cast<float>(-size / 2), static_cast<float>(-size / 2)};
    mSpriteArray.addObject(s);
    scene().addShape(sh);
    releaseRef(s);
}

void PictureExtraction::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    if (hitboxes) return;
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_40x32.png");
    // What the tongs hold.
    Shape* sh = mSpriteArray[0]->currentShape();
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    DraggableWrapper* w = new DraggableWrapper;
    w->injury = this;
    w->mSprite = mSpriteArray[0];
    w->hitboxes = hitboxes;
    w->hitBoxCount = hitBoxCount;
    w->xOrigin = sh->pos.x;
    w->yOrigin = sh->pos.y;
    removable.addObject(w);
    releaseRef(w);
}

// --------------------------------------------------------------------- Bunny

Bunny::Bunny(InjuryData* d, Injury* i, bool playerCreated, bool force) : Extraction(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "bunny";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Bunny"));
    Sprite* s = Sprite::initWithImageNamed("bunny.png", 128, 1024);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 32;
    sh->offset = {-64, -840};
    mSpriteArray.addObject(s);
    scene().addShape(sh);
    setup();
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    DraggableWrapper* w = new DraggableWrapper;
    w->injury = this;
    w->mSprite = s;
    w->hitboxes = hitboxes;
    w->hitBoxCount = hitBoxCount;
    w->xOrigin = sh->pos.x;
    w->yOrigin = sh->pos.y;
    removable.addObject(w);
    releaseRef(w);
    releaseRef(s);
}

void Bunny::render() {
    if (!alive) return;
    if (++damageTimer >= current().damagePeriod) {
        damageTimer = 0;
        gameState->health = gameState->health + current().damage * -2.0f;
    }
    // (Its special, "firefly", does nothing.)
    if ((specialTimer += 2) >= current().specialPeriod) special();
}

void Bunny::setup() {
    InjuryGraphic::setup();
    if (hitboxes) return;
    // One box at its foot, where the tongs take it.
    hitBoxCount = 1;
    hitboxes = new HitBox[1];
    HitBox& b = hitboxes[0];
    b.rect = {{static_cast<float>(mInjury->x - 26), static_cast<float>(mInjury->y - 16)}, {52, 44}};
    b.active = true;
    b.colour = 0x66ff33;
    b.health = 2;
    b.sprite = Sprite::initWithImageNamed("hitbox_80x64.png", 56, 44);
    b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* g = b.sprite->currentShape();
    g->layer = 33;
    g->offset = {-26, -16};
    g->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    g->rotation = static_cast<float>(mInjury->rotation);
}

}  // namespace as2

