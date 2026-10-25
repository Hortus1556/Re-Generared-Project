#include "as2/tools/Scanner.h"

#include <cmath>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

Sprite* pulseSprite() {
    Sprite* s = Sprite::initWithImageNamed("detector.png", 128, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 38;
    sh->offset = {-64, -64};
    sh->scale = 0.5f;
    return s;
}

// Where a hidden injury is to the scanner: a fat's or (without its
// sprites) a centipede's own place, a centipede's first sprite, any other's
// 64 points along its turn.
Point whereHidden(InjuryGraphic* i) {
    bool fat = i->isKindOf("Fat"), centipede = !fat && i->isKindOf("Centipede");
    if (centipede && i->mSpriteArray.count() != 0) return i->mSpriteArray[0]->currentShape()->pos;
    float x = static_cast<float>(i->mInjury->x), y = static_cast<float>(i->mInjury->y);
    if (fat || centipede) return {x, y};
    double r = static_cast<float>(i->mInjury->rotation * 3.141592653589793 / 180.0);
    return {static_cast<float>(x + std::sin(r) * -64.0), static_cast<float>(y + std::cos(r) * 64.0)};
}

}  // namespace

Scanner::Scanner(ToolData* d) : ITool(d) {
    getSingleInjury = false;
    setToolLevel();
    mSprite = pulseSprite();
    mSpriteArrow = Sprite::initWithImageNamed("scannerArrow.png", 32, 64);
    mSpriteArrow->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* a = mSpriteArrow->currentShape();
    a->layer = 38;
    a->offset = {-16, -32};
    a->scale = 2;
}

Scanner::~Scanner() {
    releaseRef(mSprite);
    releaseRef(mSpriteArrow);
}

void Scanner::setToolLevel() { level = SaveDetails::GetToolLevel("Scanner"); }

void Scanner::turnOn() {
    if (turnedOn) return;
    if (Connector::sharedConnector().current_mission_id == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("BEEP");
    ITool::turnOn();
    mSprite->currentShape()->pos = {gameState->mouseX, gameState->mouseY};
    arrow_visible = level > 1;
    rings.removeAllObjects();
    // The injuries it is over (an "anything" one reacts): the last tool's,
    // or at level 3 the hidden ones (the original made those the game's
    // current list; the port keeps that list the tools' own).
    Array<InjuryGraphic>* under = &gameState->currentInjuries();
    if (level > 2) {
        Array<InjuryGraphic>& hidden = game->findHiddenInjury(getSingleInjury);
        under = &hidden;
        for (InjuryGraphic* i : hidden) {
            Sprite* ring = pulseSprite();
            ring->currentShape()->pos = whereHidden(i);
            scene().addShape(ring->currentShape());
            rings.addObject(ring);
            releaseRef(ring);
        }
    }
    if (arrow_visible) scene().addShape(mSpriteArrow->currentShape());
    scene().addShape(mSprite->currentShape());
    if (under->count() != 0 && (*under)[0]->anything) (*under)[0]->interact();
}

void Scanner::turnOff(bool release) {
    ITool::turnOff(release);
    scene().removeShape(mSprite->currentShape());
    scene().removeShape(mSpriteArrow->currentShape());
    rings.removeAllObjects();
}

void Scanner::render(int) {
    // The pulse grows from a quarter to 1.75 and starts again, fading.
    Shape* pulse = mSprite->currentShape();
    pulse->scale += scaleOffset;
    if (!(pulse->scale < 1.75f)) pulse->scale = 0.25f;
    for (Sprite* ring : rings) {
        Shape* r = ring->currentShape();
        r->scale = pulse->scale * 0.5f;
        r->color[0] = r->color[1] = r->color[2] = 0;
        r->color[3] = (r->scale - 0.25f) / -0.75f + 1.0f;
    }
    Shape* arrow = mSpriteArrow->currentShape();
    pulse->pos = arrow->pos = {gameState->mouseX, gameState->mouseY};
    float fade = (pulse->scale - 0.25f) / -1.5f + 1.0f;
    // The nearest hidden injury (the original kept a list it never emptied
    // while on, the nearest of each frame put first).
    InjuryGraphic* nearest = nullptr;
    float nearestDist = -1;
    Point at{};
    for (InjuryGraphic* i : game->findHiddenInjury(getSingleInjury)) {
        Point p = whereHidden(i);
        float dx = gameState->mouseX - p.x, dy = gameState->mouseY - p.y;
        float d = std::sqrt(dx * dx + dy * dy);
        i->distance = d;
        if (!nearest || d < static_cast<float>(static_cast<int>(nearestDist))) {
            nearest = i;
            nearestDist = d;
            at = p;
        }
    }
    float* c = pulse->color;
    if (!nearest) {
        c[0] = c[1] = c[2] = 0;
        c[3] = fade;
        return;
    }
    arrow->rotation = static_cast<float>(
        std::atan2(static_cast<double>(at.y - gameState->mouseY), static_cast<double>(at.x - gameState->mouseX)) *
            180.0 / 3.141592653589793 + 90.0);
    float d = nearest->distance;
    if (d <= 50) {
        c[0] = 0;
        c[1] = fade;
    } else if (d <= 100) {
        c[0] = c[1] = fade;
    } else if (d <= 150) {
        c[0] = fade;
        c[1] = fade * 0.5f;
    } else {
        c[0] = fade;
        c[1] = 0;
    }
    c[2] = 0;
    c[3] = fade;
}

}  // namespace as2
