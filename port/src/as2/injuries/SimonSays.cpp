#include "as2/injuries/SimonSays.h"

#include <cmath>
#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/tools/ITool.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

// "S5_L1".."S5_L3", "S5_R1".."S5_R3" + suffix.
std::string muscleImage(int m, const char* suffix) {
    return std::string("S5_") + (m < 3 ? "L" : "R") + std::to_string(m < 3 ? m + 1 : m - 2) + suffix;
}

void setColour(Shape* s, float c) { s->color[0] = s->color[1] = s->color[2] = s->color[3] = c; }

// Where each muscle is from the injury.
const Point kMuscles[6] = {{-74, -80}, {-73, 3}, {-70, 86}, {62, -81}, {57, 4}, {56, 86}};

}  // namespace

SimonSays::SimonSays(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 64;
    box_height = 64;
    make = "dirt";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("SimonSays"));
    // The six muscles, hidden until they twitch.
    for (int m = 0; m < 6; ++m) {
        Sprite* s = Sprite::initWithImageNamed(muscleImage(m, "_unhealthy_twitch"), 128, 128);
        s->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* sh = s->currentShape();
        sh->layer = 41;
        sh->offset = {-64, -64};
        setColour(sh, 0);
        mSpriteArray.addObject(s);
        releaseRef(s);
    }
    setup();
}

void SimonSays::setup() {
    InjuryGraphic::setup();
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    for (int m = 0; m < 6; ++m) {
        Shape* sh = shape(m);
        sh->pos = {x + kMuscles[m].x, y + kMuscles[m].y};
        scene().removeShape(sh);
        scene().addShape(sh);
    }
    if (hitboxes) return;
    // A box on each muscle (a little above its middle).
    hitBoxCount = 6;
    hitboxes = new HitBox[6];
    for (int m = 0; m < 6; ++m) {
        Point p = shape(m)->pos;
        HitBox& b = hitboxes[m];
        b.rect = {{p.x - 32, p.y - 32 - 16}, {64, 64}};
        b.sprite = Sprite::initWithImageNamed("hitbox_80x64.png", 64, 64);
        b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* g = b.sprite->currentShape();
        g->layer = 33;
        g->offset = {-32, -32};
        g->pos = {p.x, p.y - 16};
        g->rotation = static_cast<float>(mInjury->rotation);
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
    }
}

int SimonSays::sequenceLength() const { return currentRound == 6 ? 5 : std::min(currentRound + 1, 4); }

void SimonSays::lightOn(int m) {
    Shape* sh = shape(m);
    sh->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(
        muscleImage(m, healed[m] ? "_healthy_twitch" : "_unhealthy_twitch"));
    setColour(sh, 1);
}

void SimonSays::resetMuscles() {
    for (int m = 0; m < 6; ++m) {
        Shape* sh = shape(m);
        if (!healed[m]) {
            setColour(sh, 0);
            continue;
        }
        sh->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(muscleImage(m, "_healthy"));
        setColour(sh, 1);
    }
}

void SimonSays::createSeq() {
    // First a sick muscle; then (from it and the healed ones) the rest.
    int unhealed[6], n = 0;
    for (int m = 0; m < 6; ++m) {
        if (!healed[m]) unhealed[n++] = m;
        lightSequence[m] = -1;
    }
    lightSequence[0] = unhealed[std::rand() % (6 - (currentRound - 1))];
    int pickable[6], p = 0;
    pickable[p++] = lightSequence[0];
    for (int m = 0; m < 6; ++m)
        if (healed[m]) pickable[p++] = m;
    for (int k = 1; k < sequenceLength(); ++k) lightSequence[k] = pickable[std::rand() % currentRound];
}

void SimonSays::addCutsOnMuscle(int m) {
    // A burn about the muscle.
    gameState->health = gameState->health - 1;
    Point p = shape(m)->pos;
    Injury* n = new Injury;
    n->mID = 0x1c;
    n->x = static_cast<int>(p.x + static_cast<float>(std::rand() % 20 - 10));
    n->y = static_cast<int>(p.y + static_cast<float>(std::rand() % 20 - 10));
    n->rotation = 0;
    n->scale = 1;
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

void SimonSays::penalty(bool slow) {
    waitingTimer = 60;
    currentState = kPenalty;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("BUZZER_WRONG");
    if (slow) gameState->createPopup("wrong", mInjury->x, mInjury->y, nullptr);
    else gameState->createPopup("wrong", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), nullptr);
    gameState->health = gameState->health + current().damage * -2.0f;
    trace("wrong");
}

void SimonSays::interact() {
    // The muscle nearest the finger.
    int nearest = -1, best = -1;
    int fx = static_cast<int>(gameState->mouseX), fy = static_cast<int>(gameState->mouseY);
    for (int m = 0; m < 6; ++m) {
        Point p = shape(m)->pos;
        int dx = fx - static_cast<int>(p.x), dy = fy - static_cast<int>(p.y);
        int d = static_cast<int>(std::sqrt(static_cast<double>(dy * dy + dx * dx)));
        if (d < best || best == -1) {
            best = d;
            nearest = m;
        }
    }
    if (nearest == lightSequence[playersSeq] && best < 65 && currentState == kPlayer) {
        ++playersSeq;
        hitTimer = 10;
        if (playersSeq == sequenceLength()) {
            waitingTimer = 60;
            currentState = kRoundWon;
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("BUZZER_RIGHT");
            gameState->createPopup("correct", mInjury->x, mInjury->y, this);
        }
        trace("tapped");
        return;
    }
    penalty(false);
    resetMuscles();
}

void SimonSays::render() {
    if (hitTimer > 0) {
        // The muscle just shocked twitches.
        resetMuscles();
        if (--hitTimer > 0) lightOn(lightSequence[playersSeq - 1]);
    }
    switch (currentState) {
    case kStart:
        waitingTimer = 45;
        currentState = kWaiting;
        break;
    case kWaiting:
        if (--waitingTimer < 1) {
            createSeq();
            waitingTimer = 21;
            currentState = kShowing;
            currentSeq = 0;
        }
        break;
    case kShowing: {
        // Each of the sequence twitches in turn.
        int m = lightSequence[currentSeq];
        --waitingTimer;
        if (waitingTimer == 20) {
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("PULSE" + std::to_string(m + 1));
            setColour(shape(m), 1);
            lightOn(m);
        } else if (waitingTimer == 10) {
            if (!healed[m]) {
                setColour(shape(m), 0);
            } else {
                setColour(shape(m), 1);
                shape(m)->tex =
                    SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(muscleImage(m, "_healthy"));
            }
        } else if (waitingTimer <= 0) {
            ++currentSeq;
            if (currentSeq == 6 || (currentSeq < 6 && lightSequence[currentSeq] == -1)) {
                // The player's turn: 6 seconds.
                currentState = kPlayer;
                waitingTimer = 180;
                playersSeq = 0;
                hitTimer = 0;
                trace("turn");
            } else {
                waitingTimer = 21;
            }
        }
        break;
    }
    case kPlayer:
        if (--waitingTimer < 1) {
            penalty(true);
            resetMuscles();
        }
        break;
    case kPenalty:
        // A burn on each muscle in turn, then the sequence again.
        if (waitingTimer % 10 == 0) addCutsOnMuscle(waitingTimer / 10 - 1);
        if (--waitingTimer <= 0) {
            waitingTimer = 60;
            currentState = kWaiting;
        }
        break;
    case kRoundWon: {
        if (hitTimer > 0) break;
        // The sick one is healed, fading in.
        int m = lightSequence[0];
        healed[m] = true;
        --waitingTimer;
        Shape* sh = shape(m);
        sh->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(muscleImage(m, "_healthy"));
        setColour(sh, 1.0f - static_cast<float>(waitingTimer) / 60.0f);
        if (waitingTimer < 1) {
            waitingTimer = 60;
            currentState = kWaiting;
            ++currentRound;
        }
        if (currentRound >= 7) {
            alive = false;
            retainRef(this);
            gameState->removeInjury(this);
            releaseRef(this);
        }
        break;
    }
    }
}

std::string SimonSays::toolNow() const { return currentState == kPlayer ? "battery" : "wait"; }

void SimonSays::traceBoxes() const {
    if (currentState != kPlayer || playersSeq >= 6 || lightSequence[playersSeq] < 0) return;
    // The next muscle to shock.
    InjuryGraphic::traceBoxes(&hitboxes[lightSequence[playersSeq]], 1, "");
}

}  // namespace as2
