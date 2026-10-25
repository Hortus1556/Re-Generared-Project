#include "as2/injuries/BigTick.h"

#include <cmath>
#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/tools/ITool.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
float percent() { return static_cast<float>(std::rand() % 100) / 100.0f; }
void sfx(const char* key) { MyOpenAL::sharedMyOpenAL().playSFXWithKey(key); }
constexpr double kDeg = 3.141592653589793 / 180.0;
constexpr int kBlood = 0, kSmallCut = 1, kLargeCut = 3, kPoison = 0xc, kBlueBlood = 0x11, kBarnacleSplatter = 0x32,
              kTick = 0x8d, kChainsaw = 7;

void add(int id, float x, float y, int rotation, bool playerCreated = false) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = static_cast<int>(x);
    n->y = static_cast<int>(y);
    n->rotation = rotation;
    n->scale = 1;
    gameState->addInjury(n, playerCreated, false);
    releaseRef(n);
}

// Blood from (x, y), `rounds` times each of the velocities.
void burst(int id, float x, float y, int rounds, std::initializer_list<int> velocities) {
    Injury* b = new Injury;
    b->mID = id;
    b->x = static_cast<int>(x);
    b->y = static_cast<int>(y);
    b->rotation = 0;
    b->scale = 1;
    for (int n = 0; n < rounds; ++n)
        for (int v : velocities) gameState->addBlood(b, v);
    releaseRef(b);
}

}  // namespace

BigTick::BigTick(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 128;
    box_height = 128;
    make = "bigtick";
    maxStages = 7;
    alive = true;
    initStageArray(stageTable("BigTick"));
    Sprite* s = Sprite::initWithImageNamed("overlord11", 256, 256);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 34;
    sh->offset = {-128, -128};
    mSpriteArray.addObject(s);
    releaseRef(s);
    setup();
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    origX = static_cast<float>(mInjury->x);
    origY = static_cast<float>(mInjury->y);
    // Under the skin until it is cut open.
    hidden = true;
}

void BigTick::newTarget() {
    targetX = static_cast<float>(static_cast<int>(static_cast<float>(maxX - minX) * percent() + static_cast<float>(minX)));
    targetY = static_cast<float>(static_cast<int>(static_cast<float>(maxY - minY) * percent() + static_cast<float>(minY)));
    origX = static_cast<float>(mInjury->x);
    origY = static_cast<float>(mInjury->y);
}

void BigTick::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    freeHitBoxes();
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    if (currentStage == 2 || currentStage == 4 || currentStage == 6) {
        // Sucking: the line round it to saw it free (and its course ready
        // for when it flies).
        newTarget();
        angle = static_cast<float>(std::atan2(static_cast<double>(targetX - x), static_cast<double>(targetY - y)) / kDeg);
        angleChangeValue = std::rand() % 2 < 1 ? -2.0f : 2.0f;
        angleChangeTimer = std::rand() % 20 + 20;
        speed = 3;
        box_width = 20;
        box_height = 16;
        hitBoxCount = 22;
        hitboxes = new HitBox[22];
        lineHitBoxes(hitboxes, GameData::bigTickHitPoints, 22, x, y);
        return;
    }
    box_width = 128;
    box_height = 128;
    oneHitBox(x, y, "hitbox_40x32.png");
}

void BigTick::bite(bool playerCreated) {
    Shape* sh = body();
    add(kLargeCut, sh->pos.x + 3, sh->pos.y + 35, 90, playerCreated);
    add(kPoison, sh->pos.x + 3, sh->pos.y + 35, 0, playerCreated);
}

void BigTick::returnHome() {
    bool hurt = currentState == kHurt;
    if (!hurt) {
        --currentStage;
        setup();
    }
    Shape* sh = body();
    scene().removeShape(sh);
    sh->layer = 34;
    scene().addShape(sh);
    currentState = kSucking;
    // (Its first bites are the level's: no score for them.)
    bite(--playerCreatedCounter < 1);
    invincible = false;
    // (Shocked, its next stage comes with the landing: traced then.)
    if (!hurt) trace("sucking");
}

void BigTick::interact() {
    if (gameState->currentTool && gameState->currentTool->toolData->mId == kChainsaw) return;
    if (currentState != kStunned) return;
    --lives;
    updateInjury();
    currentState = kHurt;
    currentFrame = 0;
    invincible = true;
    sfx("CRAB_DEATH");
    trace("hurt");
}

void BigTick::updateInjury() {
    if (!alive) return;
    if (currentStage == 3 || currentStage == 5 || currentStage == 7) {
        // (Shocked: render sees to it.)
        if (currentState != kHurt) currentFrame = 0;
        return;
    }
    ++currentStage;
    speed = 0;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    Shape* sh = body();
    if (currentStage == 2) {
        // Cut open: there it is.
        gameState->ChangeBG("S10_bg2");
        sfx("CRAB_EXPLODE");
        scene().addShape(sh);
        burst(kBlood, sh->pos.x, sh->pos.y, 1, {-1});
        burst(kBlood, sh->pos.x, sh->pos.y, 10, {-1, 1, 1, 2, 3, 4, 4, 5, 5, 6, 7, 8, 8, 9});
        bite(false);
        hidden = false;
    } else if (currentStage == 3 || currentStage == 5 || currentStage == 7) {
        // Sawn free: it flies.
        scene().removeShape(sh);
        sh->layer = 32;
        scene().addShape(sh);
        currentState = kFlying;
        flyTimer = 90;
        newTarget();
        speed = 3;
        flyTimer += std::rand() % 60;
        trace("flying");
    } else if (currentStage > maxStages) {
        currentState = kHurt;
        currentFrame = 0;
        burst(kBarnacleSplatter, sh->pos.x, sh->pos.y, 5, {-1});
        invincible = true;
        alive = false;
        return;
    }
    setup();
    // (In the air it is out of reach until stunned.)
    if (currentState == kFlying) invincible = true;
}

void BigTick::render() {
    Shape* sh = body();
    auto& textures = SingletonTextureManager::sharedTextureManagerInstance();
    ++currentFrame;
    if (currentState == kSucking) {
        sh->tex = textures.getTexture2D("overlord" + std::to_string(currentFrame % 10 + 11));
        return;
    }
    if (currentState == kStunned) {
        sfx("DAZED");
        sh->tex = textures.getTexture2D("overlord" + std::to_string(currentFrame % 10 + 21));
        if (--flyTimer <= 0) returnHome();
        traceMoved();
        return;
    }
    if (currentState == kHurt) {
        sh->tex = textures.getTexture2D("overlord" + std::to_string(currentFrame % 10 + 31));
        if (lives < 1) {
            // Dying: blue blood seeping.
            Injury* b = new Injury;
            b->mID = kBlueBlood;
            b->x = static_cast<int>(sh->pos.x);
            b->y = static_cast<int>(sh->pos.y);
            b->rotation = 0;
            b->scale = 1;
            b->offset = true;
            gameState->addBlood(b, -1);
            releaseRef(b);
            if (currentFrame == 1) sfx("overlorddeath");
        }
        if (currentFrame == 40 && lives < 1)
            burst(kBlood, sh->pos.x, sh->pos.y, 12, {-1, 1, 2, 4, 6, 7, 8, 10, 11, 13, 14, 15, 16, 18});
        if (currentFrame == 60) {
            // Two more ticks from it.
            add(kTick, sh->pos.x, sh->pos.y, 0);
            add(kTick, sh->pos.x + 3, sh->pos.y + 35, 0);
            if (lives < 1) {
                currentState = kSucking;
                scene().removeShape(sh);
                retainRef(this);
                gameState->removeInjury(this);
                releaseRef(this);
                return;
            }
            returnHome();
            ++currentStage;
            setup();
            trace("sucking");
        }
        return;
    }
    // Flying. Shaken: stunned.
    if (gameController->mShakeDetected) {
        currentState = kStunned;
        flyTimer = 90;
        invincible = false;
        newTarget();
        speed = 3;
        trace("stunned");
        traceMoved();
        return;
    }
    sh->tex = textures.getTexture2D("overlord" + std::to_string(currentFrame % 10 + 1));
    if (currentFrame % 20 == 0) add(kSmallCut, sh->pos.x + 3, sh->pos.y + 35, std::rand() % 360, true);
    sfx("WASP_FLY");
    if (++specialTimer >= current().specialPeriod) {
        specialTimer = static_cast<int>(static_cast<float>(current().specialPeriod) * percent());
        gameState->health = gameState->health + current().damage * -2.0f;
        special();
    }
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
    traceMoved();
    if (--flyTimer > 0) {
        // Off the body, or past its target: a new one from here.
        float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
        bool inside = minX < mInjury->x && mInjury->x < maxX && minY < mInjury->y && mInjury->y < maxY;
        float gone = (x - origX) * (x - origX) + (y - origY) * (y - origY);
        float course = (targetX - origX) * (targetX - origX) + (targetY - origY) * (targetY - origY);
        if (!inside || gone >= course) {
            speed = 3;
            newTarget();
            angle = static_cast<float>(std::atan2(static_cast<double>(targetX - x), static_cast<double>(targetY - y)) / kDeg);
        }
        return;
    }
    returnHome();
}

std::string BigTick::toolNow() const {
    if (currentStage == 3 || currentStage == 5 || currentStage == 7) {
        if (currentState == kFlying) return "shake";
        if (currentState == kStunned) return "battery";
        return "wait";
    }
    return currentState == kHurt ? "wait" : InjuryGraphic::toolNow();
}

}  // namespace as2
