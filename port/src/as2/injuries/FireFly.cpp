#include "as2/injuries/FireFly.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"
#include "as2/tools/Tongs.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
float percent() { return static_cast<float>(std::rand() % 100) / 100.0f; }
void sfx(const char* key) { MyOpenAL::sharedMyOpenAL().playSFXWithKey(key); }

void add(int id, int x, int y) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = x;
    n->y = y;
    n->rotation = 0;
    n->scale = 1;
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

constexpr int kFireSplatter = 0x1b, kFire = 0x1d, kFireFly = 0x20, kBrokenEgg = 0x21, kBarnacleSplatter = 0x32;

constexpr double kDeg = 3.141592653589793 / 180.0;
// MapUtils.getAngle(x1, x2, y1, y2) - 90: the heading from one point to
// another, 0 up, clockwise.
float headingTo(Point from, Point to) {
    return static_cast<float>(std::atan2(to.y - from.y, to.x - from.x) / kDeg) + 90;
}

}  // namespace

// ---------------------------------------------------------------------- Eggs

FireFlyEgg::FireFlyEgg(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : PictureExtraction(d, i, playerCreated, force, "FireFlyEgg", "eggs.png", 128, 64) {
    make = "burn";
    slippery = true;
    flash = std::make_unique<FlashCreature>("FireFlyEgg");
    flash->attach(mSpriteArray[0]->currentShape());
    flash->look.hidden.push_back("hit");
    setup();
}

void FireFlyEgg::render() {
    // Its glow by the share of the time to hatching gone (before the
    // render that may hatch it).
    if (flash->ready()) {
        int n = std::max(1, current().specialPeriod);
        flash->look.frames["anim"] = std::clamp(static_cast<int>(std::round(100.0 * specialTimer / n)), 1, 100);
    }
    PictureExtraction::render();
}

void FireFlyEgg::setup() {
    PictureExtraction::setup();
    // It hatches between half and all of its period from now.
    int period = current().specialPeriod;
    specialTimer = std::max(0, static_cast<int>(static_cast<float>(period) * percent() - static_cast<float>(period / 2)));
}

void FireFlyEgg::special() {
    sfx("EXPLOSION");
    gameState->health = gameState->health - 2;
    int x = mInjury->x, y = mInjury->y;
    if (beingHeld) {
        // In the tongs: where the finger is, and let go.
        x = static_cast<int>(gameState->mouseX);
        y = static_cast<int>(gameState->mouseY);
        if (auto* tongs = dynamic_cast<Tongs*>(gameState->currentTool)) tongs->dropHeld();
    }
    // (The original added the firefly twice and no shell when it hatched in
    // the tongs: one of each, as when it hatches on the body.)
    add(kFireFly, x, y);
    add(kBrokenEgg, x, y);
    add(kFire, x, y);
    add(kFireSplatter, x, y);
    ++currentStage;
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

ExtractionFireFlyEggBroken::ExtractionFireFlyEggBroken(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : PictureExtraction(d, i, playerCreated, force, "ExtractionFireFlyEggBroken", "eggsHatch.png", 128, 64) {
    make = "extractionFireFlyEggBroken";
    flash = std::make_unique<FlashCreature>("ExtractionFireFlyEggBroken");
    flash->attach(mSpriteArray[0]->currentShape());
    flash->look.hidden.push_back("hit");
    setup();
}

// ------------------------------------------------------------------- FireFly

namespace {
float random01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }
}  // namespace

FireFly::FireFly(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    make = "burn";
    maxStages = 1;
    alive = true;
    initStageArray(flashStageTable("FireFly"));
    Sprite* s = Sprite::initWithImageNamed("fireflyHover0001", 128, 256);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 31;  // (FireFly.as: "creatures_higher", as the dove)
    sh->offset = {-64, -128};
    mSpriteArray.addObject(s);
    scene().addShape(sh);
    releaseRef(s);
    flash = std::make_unique<FlashCreature>("FireFly");
    flash->attach(sh);
    setup();
    showFlash();
}

void FireFly::showFlash() {
    if (!flash->ready()) return;
    flash->show(alive ? "ALIVE" : "DEAD");
    flash->look = VectorLook();
    flash->look.hidden.push_back("hit");
}

void FireFly::place() {
    Point p = flashstage::stageToWorld(at.x, at.y);
    mSpriteArray[0]->currentShape()->pos = p;
    mInjury->x = static_cast<int>(p.x);
    mInjury->y = static_cast<int>(p.y);
    moveFlashHitBoxes(p);
    traceMoved();
}

void FireFly::newTarget() {
    target.x = std::round((kMaxX - kMinX) * random01()) + kMinX;
    target.y = std::round((kMaxY - kMinY) * random01()) + kMinY;
    angle = headingTo(at, target);
}

void FireFly::setup() {
    InjuryGraphic::setup();
    invincible = false;
    damageTimer = 0;
    if (currentStage != 1) return;
    lives = 1;
    if (mInjury->x == -1 && mInjury->y == -1) {
        // Placed anywhere over the body.
        at.x = std::round((kMaxX - kMinX) * random01()) + kMinX;
        at.y = std::round((kMaxY - kMinY) * random01()) + kMinY;
    } else {
        at = flashstage::toStage({mInjury->x * 2.0f, mInjury->y * 2.0f});
    }
    flashHitBoxes(*flash, "ALIVE", flashstage::stageToWorld(at.x, at.y), 2);
    newTarget();
    angleTimer = std::max(0.0f, kMaxAngleTimer * random01() - kMaxAngleTimer / 3.0f);
    speed = kSpeed;
    place();
}

void FireFly::special() {
    // A fireball, only while well over the body.
    if (!(at.x > kMinX + 50 && at.x < kMaxX - 50 && at.y > kMinY + 50 && at.y < kMaxY - 50)) return;
    if (current().effect != "fireball") return;
    sfx("EXPLOSION");
    gameState->health -= 2;
    add(kFire, mInjury->x, mInjury->y);
    add(kFireSplatter, mInjury->x, mInjury->y);
}

void FireFly::interact() {
    if (!alive) return;
    if (--lives == 0) {
        sfx("GENERIC_CREATURE1");
        updateInjury();
    } else {
        gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    }
}

void FireFly::updateInjury() {
    ++currentStage;
    speed = 0;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) return;
    // Dead: green blood flies.
    Injury* b = new Injury;
    b->mID = kBarnacleSplatter;
    b->x = mInjury->x;
    b->y = mInjury->y;
    b->rotation = 0;
    b->scale = 1;
    for (int n = 0; n < 5; ++n) gameState->addBlood(b, -1);
    releaseRef(b);
    invincible = true;
    alive = false;
    showFlash();
}

void FireFly::render() {
    showFlash();  // (the state the last pass left)
    if (!alive) {
        // Its death plays: on its 40th frame it bursts into a fire; at its
        // end it is gone (60 passes without the art).
        long n = flash->ready() ? flash->age() : 0;
        long end = flash->ready() ? flash->lengthOf("DEAD", "anim") : 60;
        if (!burst && n >= std::min(40L, end - 1)) {
            burst = true;
            sfx("CRAB_EXPLODE");
            gameState->health -= 2;
            add(kFire, mInjury->x, mInjury->y);
            add(kFireSplatter, mInjury->x, mInjury->y);
        }
        if (n >= end || (!flash->ready() && ++angleTimer > 60)) {
            retainRef(this);
            gameState->removeInjury(this);
            releaseRef(this);
        }
        return;
    }
    sfx("WASP_FLY");  // (Flash's BUZZ; the iPad's sound for it)
    if (++specialTimer >= current().specialPeriod) {
        specialTimer = static_cast<int>(std::round(current().specialPeriod * random01()));
        gameState->health -= current().damage * 2;
        special();
    }
    // On toward its target; there, a pause, then the next.
    const double a = angle * kDeg;
    at.x += static_cast<float>(std::sin(a) * speed);
    at.y += static_cast<float>(-std::cos(a) * speed);
    if (speed != 0) {
        float dx = target.x - at.x, dy = target.y - at.y;
        if (std::sqrt(dx * dx + dy * dy) <= speed) speed = 0;
    } else if (++angleTimer >= kMaxAngleTimer) {
        angleTimer = std::max(0.0f, kMaxAngleTimer * random01() - kMaxAngleTimer / 3.0f);
        speed = kSpeed;
        newTarget();
    }
    place();
}

}  // namespace as2
