#include "as2/injuries/Barnacle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"
#include "platform/Platform.h"

namespace as2 {

namespace {
float random01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }
}  // namespace

Barnacle::Barnacle(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    make = "barnacle";
    maxStages = 1;
    alive = true;
    initStageArray(flashStageTable("Barnicle"));
    Sprite* s = Sprite::initWithImageNamed("barnacle.png", 128, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = 38;
    s->currentShape()->offset = {-64, -64};
    s->currentShape()->pos = at();
    mSpriteArray.addObject(s);
    SceneManager::sharedSceneManagerInstance().addShape(s->currentShape());
    releaseRef(s);
    flash = std::make_unique<FlashCreature>("Barnicle");
    flash->attach(s->currentShape());
    setup();
    showFlash();
}

void Barnacle::setup() {
    InjuryGraphic::setup();
    flashHitBoxes(*flash, "ALIVE", at(), 5);
}

void Barnacle::showFlash() {
    if (!flash->ready()) return;
    flash->show(alive ? "ALIVE" : "DYING");
    flash->look = VectorLook();
    flash->look.hidden.push_back("hit");
    if (!alive) return;
    // Its pupil on the pointer: toward it by a fifth of the distance, at
    // most 5 (1.2 times sideways), about (0, -2.7).
    float px, py;
    platform::livePointer(px, py);
    Point m = flashstage::toStage({px, py});
    Point c = flashstage::toStage({at().x * 2, at().y * 2});
    float dx = m.x - c.x, dy = m.y - c.y + 2.7f;
    float d = std::min(std::sqrt(dx * dx + dy * dy) / 5, 5.0f);
    float a = std::atan2(dy, dx);
    flash->look.positions["pupil"] = {std::cos(a) * d * 1.2f, std::sin(a) * d - 2.7f};
}

void Barnacle::render() {
    showFlash();  // (the state the last pass left)
    if (alive) {
        // Now and then it hurts.
        if ((specialTimer += 2) >= current().specialPeriod) {
            specialTimer = static_cast<int>(std::round(current().specialPeriod * random01()));
            gameState->health -= current().damage * 2;
            special();
        }
        return;
    }
    // Dying: it bleeds while its death plays, then it is gone.
    Injury* b = new Injury;
    b->mID = 50;
    b->x = mInjury->x;
    b->y = mInjury->y;
    b->rotation = 0;
    b->scale = 1;
    gameState->addBlood(b, -1);
    releaseRef(b);
    if (flash->ready() && flash->age() < flash->lengthOf("DYING", "dying")) return;
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

void Barnacle::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages || !alive) return;
    int n = std::min(3, static_cast<int>(random01() * 3 + 1));
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("GENERIC_CREATURE" + std::to_string(n));
    alive = false;
    invincible = true;
    showFlash();
}

}  // namespace as2
