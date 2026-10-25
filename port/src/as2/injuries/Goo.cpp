#include "as2/injuries/Goo.h"

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

Sprite* sprite(const std::string& image, int w, int h, int layer, Point offset) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = layer;
    s->currentShape()->offset = offset;
    return s;
}

constexpr int kYellowSplatter = 0x73;

// The props, by linkage: picture, offset.
struct Prop {
    const char* linkage;
    const char* image;
    Point offset;
};
const Prop kProps[] = {{"GooWand", "wandGoo.png", {-64, -192}},
                       {"GooHonker", "honkerGoo.png", {-64, -228}},
                       {"GooJuggly", "jugglyGoo.png", {-64, -256}},
                       {"GooFlute", "snakefluteGoo.png", {-52, -240}}};

}  // namespace

Goo::Goo(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "Goo";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Goo"));
    Sprite* s = sprite("flesh.png", 128, 128, 38, {-64, -64});
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    setup();
    Shape* sh = s->currentShape();
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    sh->rotation = static_cast<float>(mInjury->rotation + 90);
    releaseRef(s);
}

void Goo::setup() {
    InjuryGraphic::setup();
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    float turn = static_cast<float>(mInjury->rotation);
    if (mSpriteArray.count() == 1) {
        // The prop in it.
        for (const Prop& p : kProps) {
            if (mInjuryData->linkage != p.linkage) continue;
            Sprite* s = sprite(p.image, 128, 256, 38, p.offset);
            s->currentShape()->pos = {x, y};
            s->currentShape()->rotation = turn + 90;
            mSpriteArray.addObject(s);
            scene().addShape(s->currentShape());
            releaseRef(s);
        }
    }
    // Four boxes down its middle (from two above), their guides shown.
    if (!hitboxes) {
        hitBoxCount = 4;
        hitboxes = new HitBox[4];
    }
    for (int k = 0; k < hitBoxCount; ++k) {
        HitBox& b = hitboxes[k];
        b.rect = {{x - static_cast<float>(box_width / 2), y + static_cast<float>(box_height * (k - 2))},
                  {static_cast<float>(box_width), static_cast<float>(box_height)}};
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
        if (!b.sprite)
            b.sprite = sprite("hitbox2_20x16.png", 20, 16, 33,
                              {static_cast<float>(-(box_width / 2)), static_cast<float>(box_height * (k - 2))});
        Shape* g = b.sprite->currentShape();
        g->pos = {x, y};
        g->rotation = turn;
        scene().removeShape(g);
        scene().addShape(g);
    }
}

void Goo::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage > maxStages) {
        // Burst: it fades away.
        alive = false;
        removeCounter = 10;
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("NOSE_POP");
        return;
    }
    for (int k = 0; k < hitBoxCount; ++k) scene().removeShape(hitboxes[k].sprite->currentShape());
    setup();
}

void Goo::render() {
    if (alive) {
        InjuryGraphic::render();
        return;
    }
    if (--removeCounter < 1) {
        retainRef(this);
        gameState->removeInjury(this);
        releaseRef(this);
        return;
    }
    // Fading, goo flying from it.
    float f = static_cast<float>(removeCounter) / 10.0f;
    float* c = mSpriteArray[0]->currentShape()->color;
    c[0] = c[1] = c[2] = c[3] = f;
    Injury* n = new Injury;
    n->mID = kYellowSplatter;
    n->x = mInjury->x;
    n->y = mInjury->y;
    n->rotation = 0;
    n->scale = 1;
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

}  // namespace as2
