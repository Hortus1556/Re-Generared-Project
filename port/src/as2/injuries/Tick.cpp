#include "as2/injuries/Tick.h"

#include <cmath>
#include <cstdlib>

#include "as2/Connector.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
float percent() { return static_cast<float>(std::rand() % 100) / 100.0f; }
void sfx(const char* key) { MyOpenAL::sharedMyOpenAL().playSFXWithKey(key); }
constexpr double kDeg = 3.141592653589793 / 180.0;
constexpr int kSmallCut = 1, kPoison = 0xc, kBarnacleSplatter = 0x32;

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

// One of the eye masks (the final mission's), at a place on the body.
Sprite* eyeMask(const char* image, float x, float y) {
    Sprite* s = Sprite::initWithImageNamed(image, 64, 64);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 40;
    sh->offset = {-32, -32};
    sh->pos = {x, y};
    scene().addShape(sh);
    return s;
}

}  // namespace

Tick::Tick(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 80;
    box_height = 80;
    make = "tick";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Tick"));
    Sprite* s = Sprite::initWithImageNamed("tick_suck1", 128, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 32;
    sh->offset = {-64, -64};
    mSpriteArray.addObject(s);
    // Another tick about already, or a big tick (which brings them flying).
    bool another = false, bigTick = false;
    for (InjuryGraphic* other : gameState->_injuries) {
        if (other == this) continue;
        if (other->isKindOf("Tick")) another = true;
        else if (other->isKindOf("BigTick")) bigTick = true;
    }
    if (Connector::sharedConnector().current_mission_id == 0x1d && gameState->currentStage == 1 && !another) {
        Sprite* right = eyeMask("right_eye_mask.png", 190, 234);
        mSpriteArray.addObject(right);
        releaseRef(right);
        Sprite* left = eyeMask("left_eye_mask.png", 345, 234);
        mSpriteArray.addObject(left);
        releaseRef(left);
    }
    setup();
    scene().addShape(sh);
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    if (!bigTick) {
        // Found sucking.
        currentState = kSucking;
        flyTimer = 0;
        bite();
        invincible = true;
    } else {
        currentState = kFlying;
        flyTimer = 90;
        invincible = false;
        newTarget();
        speed = 3;
        flyTimer += std::rand() % 60;
    }
    releaseRef(s);
    traceMoved();
}

void Tick::newTarget() {
    targetX = static_cast<float>(static_cast<int>(static_cast<float>(maxX - minX) * percent() + static_cast<float>(minX)));
    targetY = static_cast<float>(static_cast<int>(static_cast<float>(maxY - minY) * percent() + static_cast<float>(minY)));
    origX = static_cast<float>(mInjury->x);
    origY = static_cast<float>(mInjury->y);
    angle = static_cast<float>(std::atan2(static_cast<double>(targetX - origX), static_cast<double>(targetY - origY)) / kDeg);
}

void Tick::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    oneHitBox(static_cast<float>(mInjury->x), static_cast<float>(mInjury->y), "hitbox_40x32.png");
    if (currentStage != 1) return;
    lives = 1;
    if (mInjury->x == -1 && mInjury->y == -1) {
        // Anywhere over the body.
        Shape* sh = body();
        sh->pos.x = static_cast<float>(maxX - minX) * percent() + static_cast<float>(minX);
        mInjury->x = static_cast<int>(sh->pos.x);
        sh->pos.y = static_cast<float>(maxY - minY) * percent() + static_cast<float>(minY);
        mInjury->y = static_cast<int>(sh->pos.y);
    }
    newTarget();
    angleChangeValue = std::rand() % 2 < 1 ? -2.0f : 2.0f;
    angleChangeTimer = std::rand() % 20 + 20;
    speed = 3;
}

void Tick::bite() {
    Shape* sh = body();
    add(kSmallCut, static_cast<int>(sh->pos.x + 3), static_cast<int>(sh->pos.y + 35), 90);
    add(kPoison, static_cast<int>(sh->pos.x + 3), static_cast<int>(sh->pos.y + 35), 0);
}

void Tick::takeOff() {
    currentState = kFlying;
    flyTimer = 90;
    invincible = false;
    newTarget();
    speed = 3;
    flyTimer += std::rand() % 60;
    trace("flying");
}

void Tick::interact() {
    if (--lives == 0) {
        sfx("GENERIC_CREATURE1");
        updateInjury();
    } else {
        gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    }
}

void Tick::updateInjury() {
    ++currentStage;
    speed = 0;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) return;
    // Dead: green blood flies.
    Shape* sh = body();
    Injury* b = new Injury;
    b->mID = kBarnacleSplatter;
    b->x = static_cast<int>(sh->pos.x);
    b->y = static_cast<int>(sh->pos.y);
    b->rotation = 0;
    b->scale = 1;
    for (int n = 0; n < 5; ++n) gameState->addBlood(b, -1);
    releaseRef(b);
    invincible = true;
    alive = false;
    currentState = kDying;
    currentFrame = 0;
    traceMoved();
}

void Tick::render() {
    Shape* sh = body();
    auto& textures = SingletonTextureManager::sharedTextureManagerInstance();
    ++currentFrame;
    if (currentState == kDying) {
        // It fizzes, then bursts.
        sh->tex = textures.getTexture2D("tick_elec" + std::to_string(currentFrame % 10 + 1));
        if (currentFrame == 40) {
            sfx("CRAB_EXPLODE");
            Injury* b = new Injury;
            b->mID = 0;
            b->x = static_cast<int>(sh->pos.x);
            b->y = static_cast<int>(sh->pos.y);
            b->rotation = 0;
            b->scale = 1;
            static const int kVelocities[] = {-1, 1, 2, 4, 6, 7, 8};
            for (int n = 0; n < 10; ++n)
                for (int v : kVelocities) gameState->addBlood(b, v);
            releaseRef(b);
        }
        if (currentFrame == 60) {
            scene().removeShape(sh);
            retainRef(this);
            gameState->removeInjury(this);
            releaseRef(this);
        }
        return;
    }
    if (currentState == kSucking) {
        sh->tex = textures.getTexture2D("tick_suck" + std::to_string(currentFrame % 10 + 1));
        if (gameController->mShakeDetected) takeOff();
        traceMoved();
        return;
    }
    // In the air; shaking keeps it there longer (up to five seconds).
    if (gameController->mShakeDetected) flyTimer += 5;
    if (flyTimer > 150) flyTimer = 150;
    sh->tex = textures.getTexture2D("tick_fly" + std::to_string(currentFrame % 12 + 1));
    sfx("WASP_FLY");
    if (++specialTimer >= current().specialPeriod) {
        specialTimer = static_cast<int>(static_cast<float>(current().specialPeriod) * percent());
        gameState->health = gameState->health + current().damage * -2.0f;
        special();
    }
    // On, its course wobbling (the turn flips every 20 to 40 frames).
    sh->pos.x = static_cast<float>(sh->pos.x + std::sin(angle * kDeg) * speed);
    sh->pos.y = static_cast<float>(sh->pos.y + std::cos(angle * kDeg) * speed);
    angle += angleChangeValue;
    if (--angleChangeTimer < 1) {
        angleChangeTimer = std::rand() % 20 + 20;
        angleChangeValue = angleChangeValue == -2.0f ? 2.0f : -2.0f;
    }
    mInjury->x = static_cast<int>(sh->pos.x);
    mInjury->y = static_cast<int>(sh->pos.y);
    if (hitboxes) {
        HitBox& b = hitboxes[0];
        b.rect.origin = {static_cast<float>(mInjury->x - box_width / 2), static_cast<float>(mInjury->y - box_height / 2)};
        b.sprite->currentShape()->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    }
    if (--flyTimer < 1) {
        // Down: it bites.
        currentState = kSucking;
        invincible = true;
        bite();
        trace("sucking");
    } else {
        // Off the body, or past its target: a new one from here.
        float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
        bool inside = minX < mInjury->x && mInjury->x < maxX && minY < mInjury->y && mInjury->y < maxY;
        float gone = (x - origX) * (x - origX) + (y - origY) * (y - origY);
        float course = (targetX - origX) * (targetX - origX) + (targetY - origY) * (targetY - origY);
        if (!inside || gone >= course) {
            speed = 3;
            newTarget();
        }
    }
    traceMoved();
}

std::string Tick::toolNow() const {
    if (currentState == kSucking) return "shake";
    return currentState == kFlying ? "battery" : "wait";
}

}  // namespace as2
