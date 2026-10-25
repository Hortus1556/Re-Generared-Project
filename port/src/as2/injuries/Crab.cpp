#include "as2/injuries/Crab.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }


// An injury of `id` (blood, a cut, the shell) at (x, y).
Injury* newInjury(int id, int x, int y, int rotation) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = x;
    n->y = y;
    n->rotation = rotation;
    n->scale = 1;
    n->offset = true;
    return n;
}

}  // namespace

namespace {
float random01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }
}  // namespace

Crab::Crab(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    make = "crab";
    maxStages = 2;
    alive = true;
    initStageArray(flashStageTable("Crab"));
    Sprite* s = Sprite::initWithImageNamed("crabShellWalk_001", 128, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = 32;
    s->currentShape()->offset = {-64, -64};
    s->currentShape()->pos = flashPlace();  // (its row; along it at random)
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    releaseRef(s);
    flash = std::make_unique<FlashCreature>("Crab");
    flash->attach(s->currentShape());
    speed = minSpeed;
    setup();
    showFlash();
}

void Crab::showFlash() {
    if (!flash->ready()) return;
    flash->look = VectorLook();
    flash->look.hidden.push_back("hit");
    if (!alive) flash->show("HITTING_NOSHELL");
    else if (hitting) flash->show("HITTING_SHELL");
    else flash->show(currentStage == 1 ? "WALKING_SHELL" : "WALKING_NOSHELL");
}

bool Crab::hitDone() const {
    // (Its holder clip played to its end; 64 passes without the art.)
    if (!flash->ready()) return hitFrames >= 64;
    return flash->age() >= flash->lengthOf(flash->label(), "holder");
}

void Crab::boxFollows() {
    Shape* s = body();
    s->pos.x = flashstage::stageToWorld(x, 0).x;
    moveFlashHitBoxes(s->pos);
    traceMoved();
}

void Crab::setup() {
    InjuryGraphic::setup();
    // Its hit areas, Flash's (bare ones once its shell cracks).
    if (currentStage == 1) flashHitBoxes(*flash, "WALKING_SHELL", body()->pos, kBoxHealth);
    invincible = false;
    invincibleTimer = 45;
    damageTimer = 0;
    specialTimer = static_cast<int>(std::round(current().specialPeriod * random01()));
    hitting = false;
    if (currentStage == 1) {
        // Somewhere along its row, heading either way.
        speed = minSpeed;
        x = std::round(kMaxX * random01());
        left = random01() < 0.5f;
    }
    boxFollows();
}

void Crab::updateInjury() {
    ++currentStage;
    speed = 0;
    damageTimer = 0;
    Point at = body()->pos;
    gameState->createPopup("correct", static_cast<int>(at.x), static_cast<int>(at.y), this);
    hitFrames = 0;
    if (currentStage > maxStages) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("CRAB_DEATH");
        alive = false;
        invincible = true;
    } else if (currentStage == 2) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("CRAB_EXPLODE");
        hitting = true;
        invincible = true;
        flashHitBoxes(*flash, "WALKING_NOSHELL", body()->pos, kBoxHealth);
    }
    // Faster each time.
    minSpeed *= 1.7f;
    maxSpeed *= 1.7f;
    showFlash();
}

void Crab::createEffect(int count, int velocity) {
    // (On the shell: sparks, which the port does not draw.)
    if (currentStage == 1) return;
    Point at = body()->pos;
    Injury* b = newInjury(17, static_cast<int>(at.x), static_cast<int>(at.y), 0);
    for (int n = 0; n < count; ++n) gameState->addBlood(b, velocity);
    releaseRef(b);
}

void Crab::special() {
    // A nip: a small cut where it is (not near the sides; kept on the body).
    if (!(x > kMinX + 75 && x < kMaxX - 75) || current().effect != "smallCut") return;
    Point at = body()->pos;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("PIZZA_CUTTER");
    gameState->health -= 3;
    Injury* cut = newInjury(1, static_cast<int>(std::clamp(at.x, 100.0f, 350.0f)),
                            static_cast<int>(std::clamp(at.y, 80.0f, 280.0f)), static_cast<int>(360 * random01()));
    cut->offset = false;
    gameState->addInjury(cut, false, false);
    releaseRef(cut);
    Injury* blood = newInjury(0, static_cast<int>(at.x), static_cast<int>(at.y), 0);
    blood->offset = false;
    gameState->addBlood(blood, -1);
    releaseRef(blood);
}

void Crab::render() {
    showFlash();  // (the state the last pass left)
    Shape* s = body();
    if (hitting) {
        // Its shell cracking; when it has, the shell flies off: a piece to
        // take out (turned toward the body near the sides, kept on it).
        ++hitFrames;
        if (!hitDone()) return;
        gameState->health -= 3;
        float px = s->pos.x, py = s->pos.y;
        int rotation = x < kMinX + 150 ? 45 : x > kMaxX - 150 ? 315 : static_cast<int>(360 * random01());
        // (On the top row, a piece whose end to take lay up toward the
        // cards, under them, could not be taken: turned the other way.)
        if (py < kTopRow && std::cos(rotation * 3.14159265 / 180) < 0) rotation = (rotation + 180) % 360;
        Injury* shell = newInjury(18, static_cast<int>(std::clamp(px, 130.0f, 350.0f)), static_cast<int>(py), rotation);
        gameState->addInjury(shell, false, true);
        releaseRef(shell);
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("PIZZA_CUTTER");
        setup();
        speed = maxSpeed;
        invincible = false;
        return;
    }
    if (!alive) {
        // Dying: bleeding blue while its death plays, then gone.
        ++hitFrames;
        Injury* b = newInjury(17, static_cast<int>(s->pos.x), static_cast<int>(s->pos.y), 0);
        gameState->addBlood(b, -1);
        releaseRef(b);
        if (!hitDone()) return;
        retainRef(this);
        gameState->removeInjury(this);
        releaseRef(this);
        return;
    }
    if ((specialTimer += 2) >= current().specialPeriod) {
        specialTimer = static_cast<int>(std::round(current().specialPeriod * random01()));
        gameState->health -= current().damage * 2;
        special();
    }
    // The pointer near: away from it, faster (turning only once slowed);
    // else slowing to its least speed.
    Point p = flashstage::toStage({gameState->mouseX * 2, gameState->mouseY * 2});
    float dx = p.x - x, dy = p.y - flashstage::toStage({0, s->pos.y * 2}).y;
    if (std::sqrt(dx * dx + dy * dy) <= kRunRadius && p.x != x) {
        bool away = p.x > x;  // (left, away from a pointer on its right)
        if (away != left) {
            if (speed <= minSpeed) left = away;
            else speed *= kSlowMulti;
        } else if (speed < maxSpeed) {
            speed *= kFastMulti;
        }
    } else if (speed > minSpeed) {
        speed *= kSlowMulti;
    }
    // Along its row, round again at the sides.
    if (left) {
        x -= speed;
        if (x <= kMinX - 50) x = kMaxX + 50;
    } else {
        x += speed;
        if (x >= kMaxX + 50) x = kMinX - 50;
    }
    boxFollows();
}

}  // namespace as2
