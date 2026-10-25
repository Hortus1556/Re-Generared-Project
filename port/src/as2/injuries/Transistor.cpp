#include "as2/injuries/Transistor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/tools/ITool.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
constexpr int kSmallCut = 1, kLargeCutScar = 0x3f;

// Sprites: the two scars' healed looks, the transistor, its needle, its
// body's middle, its gauge.
enum { kHealed1, kHealed2, kBody, kNeedle, kMiddle, kGauge };

Sprite* picture(const char* image, int w, int h, int layer, Point offset) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = layer;
    s->currentShape()->offset = offset;
    return s;
}

bool present(const InjuryGraphic* i) {
    for (InjuryGraphic* other : gameState->_injuries)
        if (other == i) return true;
    return false;
}

void addCut(float x, float y, int rotation) {
    Injury* c = new Injury;
    c->mID = kSmallCut;
    c->x = static_cast<int>(x);
    c->y = static_cast<int>(y);
    c->rotation = rotation;
    c->scale = 1;
    gameState->addInjury(c, true, false);
    releaseRef(c);
}

}  // namespace

Transistor::Transistor(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "transistor";
    maxStages = 4;
    alive = true;
    initStageArray(stageTable("Transistor"));
    for (Sprite* s : {picture("largecut3.png", 64, 256, 41, {-32, 0}), picture("largecut3.png", 64, 256, 41, {-32, 0}),
                      picture("transistor1.png", 256, 64, 28, {-128, -32}),
                      picture("transistor2.png", 32, 16, 28, {-16, -8}),
                      picture("transistor3.png", 64, 32, 28, {-32, -16}),
                      picture("transistor4.png", 32, 32, 28, {-16, -16})}) {
        mSpriteArray.addObject(s);
        releaseRef(s);
    }
    setup();
}

bool Transistor::otherWaiting() const {
    for (InjuryGraphic* other : gameState->_injuries)
        if (other != this && other->isKindOf("Transistor") && other->currentStage != 4) return true;
    return false;
}

void Transistor::setup() {
    InjuryGraphic::setup();
    if (currentStage == 1 && otherWaiting()) return;
    if (currentStage == 1 || currentStage == 2) {
        // A line down one side, then the other.
        float side = currentStage == 1 ? 94.0f : -94.0f;
        Point from = mInjury->rotation == 90 ? Point{0, side} : Point{side, 0};
        columnHitBoxes(7, "hitbox2_20x16.png", true, 44, from);
    } else {
        freeHitBoxes();
    }
}

void Transistor::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    int rot = mInjury->rotation;
    float side = currentStage == 2 ? 94.0f : -94.0f;
    float ox = rot == 90 ? -80.0f : side, oy = rot == 90 ? side : 80.0f;
    if (currentStage == 2 || currentStage == 3) {
        // The side cut: a scar there (closed until the transistor is in),
        // and its healed look waiting.
        Injury* n = new Injury;
        n->mID = kLargeCutScar;
        n->x = static_cast<int>(x + ox);
        n->y = static_cast<int>(y + oy);
        n->scale = 1;
        n->rotation = rot;
        InjuryGraphic* scar = gameState->addInjury(n, false, false);
        releaseRef(n);
        if (scar) scar->invincible = true;
        (currentStage == 2 ? scar1 : scar2) = scar;
        Shape* healed = shape(currentStage == 2 ? kHealed1 : kHealed2);
        healed->pos = rot == 0 ? Point{x + side, y} : Point{x, y + side};
        healed->rotation = static_cast<float>(rot);
    } else if (currentStage == 4) {
        // In: shown over the brain, its gauge and needle on it, the scars
        // open to treat.
        Shape* body = shape(kBody);
        scene().removeShape(body);
        body->layer = 41;
        scene().addShape(body);
        addCut(body->pos.x - oy, body->pos.y + ox, rot == 0 ? std::rand() % 160 + 100 : std::rand() % 160 + 190);
        for (int k : {kMiddle, kGauge, kNeedle}) {
            Shape* s = shape(k);
            s->pos = body->pos;
            s->rotation = static_cast<float>(rot);
            s->layer = k == kNeedle ? 40 : 39;
            scene().addShape(s);
        }
        for (InjuryGraphic* scar : {scar1, scar2})
            if (scar && present(scar)) scar->invincible = false;
    }
    if (currentStage > maxStages) {
        alive = false;
        gameState->removeInjury(this);
        return;
    }
    setup();
    if (currentStage == 3) {
        // The transistor, on the tongs' tray (turned as it goes in).
        shape(kBody)->rotation = static_cast<float>(rot);
        waitOnTray(mSpriteArray[kBody]);
    }
}

void Transistor::render() {
    if (currentStage == 1) {
        // Its turn come: the line to cut.
        if (!hitboxes && !otherWaiting()) {
            setup();
            trace("ready");
        }
        return;
    }
    if (currentStage != 4) return;
    // A scar healed: its healed look in its place.
    if (scar1 && !present(scar1)) {
        scar1 = nullptr;
        scene().addShape(shape(kHealed1));
    }
    if (scar2 && !present(scar2)) {
        scar2 = nullptr;
        scene().addShape(shape(kHealed2));
    }
    // Nothing else left (the other transistors in too): done.
    bool others = false;
    for (InjuryGraphic* i : gameState->_injuries)
        if (!(i->isKindOf("Transistor") && i->currentStage == 4)) others = true;
    if (!others) {
        retainRef(this);
        gameState->removeInjury(this);
        releaseRef(this);
        return;
    }
    balance();
}

void Transistor::balance() {
    // (Its own two scars: the original counted every scar turned as it is,
    // so a second transistor's set the first one's gauge going again.)
    int scars = (scar1 ? 1 : 0) + (scar2 ? 1 : 0);
    if (scars == 0) {
        if (!stillActive) return;
        scene().removeShape(shape(kNeedle));
        scene().removeShape(shape(kGauge));
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("BUZZER_RIGHT");
        stillActive = false;
        trace("steady");
        return;
    }
    bool across = mInjury->rotation == 0;
    float needle = std::clamp(static_cast<float>(gameController->balance(across ? 1 : 0) * 128.0), -16.0f, 16.0f);
    if (scars > 1) gaugeOffset += gaugeDirection;
    if (gaugeOffset >= 16) {
        gaugeOffset = 16;
        gaugeDirection = -0.25f;
    } else if (gaugeOffset < -16) {
        gaugeOffset = -16;
        gaugeDirection = 0.25f;
    }
    if (std::abs(static_cast<int>(needle - gaugeOffset)) >= 6) {
        // Off the gauge: hurt, and now and then cut.
        if (--cutTimer % 5 == 0) {
            gameState->health = gameState->health - 1;
            ITool::scream();
        }
        if (cutTimer < 1) {
            gameState->health = gameState->health - 1;
            ITool::scream();
            cutTimer = 30;
            float dx, dy;
            if (!across) {
                if (std::rand() % 10 < 4) {
                    dx = static_cast<float>(std::rand() % 100 - 50);
                    dy = static_cast<float>(std::rand() % 60 + 120);
                } else {
                    dx = static_cast<float>(std::rand() % 200 + 30);
                    dy = static_cast<float>(std::rand() % 180 - 90);
                }
            } else if (std::rand() % 10 < 4) {
                dx = static_cast<float>(-120 - std::rand() % 100);
                dy = static_cast<float>(std::rand() % 80 - 70);
            } else {
                dx = static_cast<float>(std::rand() % 180 - 100);
                dy = static_cast<float>(-30 - std::rand() % 160);
            }
            Point at = shape(kBody)->pos;
            addCut(at.x + dx, at.y + dy, std::rand() % 360);
        }
    }
    Point at = shape(kBody)->pos;
    if (across) {
        shape(kGauge)->pos.x = static_cast<float>(static_cast<int>(at.x + gaugeOffset));
        shape(kNeedle)->pos.x = static_cast<float>(static_cast<int>(at.x + needle));
    } else {
        shape(kGauge)->pos.y = static_cast<float>(static_cast<int>(at.y + gaugeOffset));
        shape(kNeedle)->pos.y = static_cast<float>(static_cast<int>(at.y + needle));
    }
    // "injury balance #<n> gauge <g> needle <n> dir <d> axis <across|down>"
    // (dir 0 while the gauge stands).
    int g = static_cast<int>(std::lround(gaugeOffset)), n = static_cast<int>(std::lround(needle));
    if (platform::traceEnabled() && (g != tracedGauge || n != tracedNeedle)) {
        tracedGauge = g;
        tracedNeedle = n;
        platform::debugTrace("injury balance #%llu gauge %d needle %d dir %.2f axis %s", serial(), g, n,
                             scars > 1 ? gaugeDirection : 0.0f, across ? "across" : "down");
    }
}

std::string Transistor::toolNow() const {
    if (currentStage == 4 || (currentStage == 1 && !hitboxes)) return "wait";
    return InjuryGraphic::toolNow();
}

}  // namespace as2
