#include "as2/injuries/Snake.h"

#include <cstdlib>

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

Injury* injuryAt(int id, int x, int y) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = x;
    n->y = y;
    n->rotation = 0;
    n->scale = 1;
    return n;
}

constexpr int kPoisonSplatter = 0xb, kPoison = 0xc, kSnakeBlood = 0x11;
constexpr int kHole = 0, kBody = 1, kDeadBody = 2;

}  // namespace

Snake::Snake(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 64;
    box_height = 48;
    make = "snake";
    maxStages = 6;
    alive = true;
    initStageArray(stageTable("Snake"));
    // (It first rears out up to 20 frames later than its period.)
    stageArray[0].specialPeriod += std::rand() % 20;
    // The hole, the snake, its dead body (to take out).
    Sprite* s[] = {sprite("snakeHole", 64, 64, 43, {-32, -32}), sprite("snakeMovement1", 128, 128, 32, {-64, -64}),
                   sprite("deadsnakeRemovable", 128, 64, 32, {-64, -32})};
    for (Sprite* p : s) mSpriteArray.addObject(p);
    scene().addShape(s[kHole]->currentShape());
    setup();
    for (Sprite* p : s) {
        p->currentShape()->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
        releaseRef(p);
    }
    currentFrame = 0;
    invincible = true;
}

Snake::~Snake() { removable.removeAllObjects(); }

void Snake::setup() {
    InjuryGraphic::setup();
    if (hitboxes) return;
    // One box over the hole, sawn a long while (40).
    hitBoxCount = 1;
    hitboxes = new HitBox[1];
    HitBox& b = hitboxes[0];
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    b.rect = {{x - 32, y - 32}, {64, 64}};
    b.sprite = sprite("hitbox_20x16.png", 64, 64, 33, {-32, -32});
    b.sprite->currentShape()->pos = {x, y};
    b.sprite->currentShape()->rotation = static_cast<float>(mInjury->rotation);
    b.active = true;
    b.colour = 0x66ff33;
    b.health = 40;
}

void Snake::rearOut() {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("snakeFX");
    currentFrame = 0;
    currentLabel = kRearing;
    Shape* body = mSpriteArray[kBody]->currentShape();
    scene().removeShape(body);
    scene().addShape(body);
    body->scale = 0;
    trace("special");
}

void Snake::special() {
    specialTimer = 0;
    if (currentLabel == kHidden) rearOut();
}

void Snake::interact() {
    interactCount = 8;
    currentLabel = kHurt;
}

void Snake::render() {
    InjuryGraphic::render();
    Shape* body = mSpriteArray[kBody]->currentShape();
    auto& tm = SingletonTextureManager::sharedTextureManagerInstance();
    if (currentLabel == kHidden && gameController->mShakeDetected) {
        rearOut();
    } else if (currentLabel == kRearing) {
        body->scale = static_cast<float>(++currentFrame) * 0.0625f;
        if (currentFrame == 16) {
            currentFrame = 0;
            currentLabel = kOut;
            invincible = false;
            trace("out");
        }
    } else if (currentLabel == kHiding) {
        if (++currentFrame == 16) {
            currentFrame = 0;
            specialTimer = 0;
            currentLabel = kHidden;
            body->scale = 0;
        } else {
            body->scale = static_cast<float>(16 - currentFrame) * 0.0625f;
        }
    } else if (currentLabel == kOut || currentLabel == kHurt) {
        if (--interactCount < 1) {
            interactCount = 0;
            currentLabel = kOut;
        }
        int f = ++currentFrame;
        // Writhing, or (sawn at) in pain.
        std::string look = currentLabel == kHurt && f != 90 ? "snakeDamage" + std::to_string(f % 2 + 1)
                                                             : "snakeMovement" + std::to_string(f / 2 % 10 + 1);
        body->tex = tm.getTexture2D(look);
        if (currentFrame == 90) {
            // Back into its hole, leaving poison.
            currentFrame = 0;
            currentLabel = kHiding;
            invincible = true;
            Injury* n = injuryAt(kPoisonSplatter, mInjury->x, mInjury->y);
            gameState->addInjury(n, false, false);
            releaseRef(n);
            trace("hiding");
        }
    } else if (currentLabel == kBleeding) {
        ++currentFrame;
        Shape* dead = mSpriteArray[kDeadBody]->currentShape();
        Injury* n = injuryAt(kSnakeBlood, static_cast<int>(dead->pos.x), static_cast<int>(dead->pos.y));
        n->offset = true;
        gameState->addBlood(n, -1);
        releaseRef(n);
        if (currentFrame > 63) {
            currentLabel = kDead;
            invincible = false;
        }
    }
}

void Snake::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    Shape* hole = mSpriteArray[kHole]->currentShape();
    gameState->createPopup("correct", static_cast<int>(hole->pos.x), static_cast<int>(hole->pos.y), this);
    if (currentStage > maxStages) {
        alive = false;
        retainRef(this);
        gameState->removeInjury(this);
        releaseRef(this);
        invincible = false;
        return;
    }
    if (hitboxes) hitboxes[0].health = 20;
    auto& tm = SingletonTextureManager::sharedTextureManagerInstance();
    if (currentStage == 2) {
        // Sawn through: its body (to take out) where it was; a poison blob.
        scene().removeShape(mSpriteArray[kBody]->currentShape());
        Shape* dead = mSpriteArray[kDeadBody]->currentShape();
        scene().addShape(dead);
        if (removable.count() == 0) {
            DraggableWrapper* w = new DraggableWrapper;
            w->injury = this;
            w->mSprite = mSpriteArray[kDeadBody];
            w->hitboxes = hitboxes;
            w->hitBoxCount = hitBoxCount;
            w->xOrigin = dead->pos.x;
            w->yOrigin = dead->pos.y;
            removable.addObject(w);
            releaseRef(w);
        }
        Injury* n = injuryAt(kPoison, static_cast<int>(hole->pos.x), static_cast<int>(hole->pos.y));
        gameState->addInjury(n, false, false);
        releaseRef(n);
        currentLabel = kBleeding;
        currentFrame = 0;
    } else if (currentStage == 5) {
        hole->tex = tm.getTexture2D("snakeHoleStitched");
    } else if (currentStage == 6) {
        hole->tex = tm.getTexture2D("snakeHoleCaut");
    }
}

std::string Snake::toolNow() const {
    if (currentStage == 1 && currentLabel != kOut && currentLabel != kHurt) return "wait";
    return InjuryGraphic::toolNow();
}

}  // namespace as2
