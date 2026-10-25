#include "as2/injuries/Dove.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

// The dead dove's and wasp's ids (the port's own Remains: library.json,
// as2_data.py).
constexpr int kDeadDove = 200, kDeadWasp = 201;

Injury* newInjury(int id, float x, float y, int rotation) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = static_cast<int>(x);
    n->y = static_cast<int>(y);
    n->rotation = rotation;
    n->scale = 1;
    return n;
}

void addInjury(int id, float x, float y, int rotation) {
    Injury* n = newInjury(id, x, y, rotation);
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

void addBlood(int id, float x, float y, int count) {
    Injury* n = newInjury(id, x, y, 0);
    for (int k = 0; k < count; ++k) gameState->addBlood(n, -1);
    releaseRef(n);
}

// Which of a pair of doves comes in from the left. (The original reset its
// count at every dove, so all of them came from the left on top of each
// other.)
int gNumCreated = 0;

}  // namespace

// ---------------------------------------------------------------------- Dove

namespace {
// A stage point and length in the world.
Point world(Point stage) { return flashstage::stageToWorld(stage.x, stage.y); }
float worldLength(float stage) { return stage * flashstage::worldPerStage(); }
// MapUtils.getAngle(x1, x2, y1, y2) - 90: the heading from one point to
// another, 0 up, clockwise.
float headingTo(Point from, Point to) {
    return static_cast<float>(std::atan2(to.y - from.y, to.x - from.x) * 180.0 / 3.141592653589793) + 90;
}
float random01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }
}  // namespace

Dove::Dove(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    make = "dove";
    alive = true;
    maxStages = 1;
    initStageArray(flashStageTable("Dove"));
    // Its picture: the Flash AS2's clip (the iPad's two frames under it,
    // were the art missing).
    Sprite* s = Sprite::initWithImageNamed("doveGlide001", 256, 256);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    // Dove.as's "creatures_higher": over the splatter in flight (its own
    // blood and feathers, which hid its death), as Flash's layers were.
    s->currentShape()->layer = 31;
    s->currentShape()->offset = {-128, -128};
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    releaseRef(s);
    flash = std::make_unique<FlashCreature>("Dove");
    flash->attach(s->currentShape());
    // (Dove.as counted every dove made, so only the first came from the
    // left: a pair comes from opposite sides.)
    numIs = ++gNumCreated % 2 == 1 ? 1 : 2;
    setup();
    showFlash();
}

void Dove::showFlash() {
    if (!flash->ready()) return;
    // (The shape holds no turn: the clip turns by Flash's angle.)
    flash->turn = angle;
    flash->look = VectorLook();
    flash->look.hidden.push_back("hit");
    if (!alive || shocked) {
        // (Shot on a low pass: its shadow hidden, as it was then.)
        flash->show(alive ? "SHOCKED" : "DYING");
        flash->look.hidden.push_back("shadow");
    } else {
        flash->show("ALIVE");
        flash->look.hidden.push_back(high ? "alive" : "shadow");
        // (shadow.gotoAndStop(crossed + 1) before it counted the crossing.)
        flash->look.frames["shadow"] = std::clamp(crossed, 1, 2);
    }
}

void Dove::boxFollows() {
    Shape* s = body();
    s->pos = world(at);
    s->rotation = 0;
    moveFlashHitBoxes(s->pos);
    traceMoved();
}

void Dove::headAcross() {
    right = !right;
    target.x = right ? kMaxX + kOff : kMinX - kOff;
    target.y = std::round((kMaxY - kMinY) * random01()) + kMinY;
    angle = headingTo(at, target);
    angleTimer = 0;
}

void Dove::setup() {
    InjuryGraphic::setup();
    // Its hit area, Flash's, 2 to work.
    if (!hitboxes) flashHitBoxes(*flash, "ALIVE", world(at), 2);
    if (currentStage != 1) return;
    // In from off one side, low, to off the other.
    invincible = false;
    shocked = false;
    high = false;
    crossed = 1;
    right = numIs != 1;  // (headAcross turns it: the first comes in rightward)
    at.x = numIs == 1 ? kMinX - kOff : kMaxX + kOff;
    at.y = std::round((kMaxY - kMinY) * random01()) + kMinY;
    headAcross();
    lives = 3;
    speed = kSpeed / (kMaxCrossed - crossed + 1);
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("DOVE_COOING");
    crossed = 0;
    boxFollows();
}

void Dove::updateInjury() {
    ++currentStage;
    speed = 0;
    damageTimer = 0;
    Point p = world(at);
    gameState->createPopup("correct", static_cast<int>(p.x), static_cast<int>(p.y), this);
    if (currentStage > maxStages) {
        invincible = true;
        alive = false;
    }
}

void Dove::special() {
    if (!(at.x > kMinX && at.x < kMaxX && at.y > kMinY && at.y < kMaxY)) return;
    Point p = world(at);
    if (!high) {
        // Low: a nip.
        int period = current().specialPeriod;
        specialTimer = period / 2 + static_cast<int>(std::round(period / 2 * random01()));
        gameState->health -= current().damage * 2 + 3;
        addInjury(1, p.x, p.y, static_cast<int>(360 * random01()));
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("PIZZA_CUTTER");
        addBlood(0, p.x, p.y, 1);
    } else {
        // Up high: a dropping.
        specialTimer = 0;
        addInjury(57, p.x, p.y, 0);
    }
}

void Dove::interact() {
    // (Dying already: no more.)
    if (!alive || lives <= 0) return;
    // Shocked: feathers and blood fly.
    Point p = world(at);
    const float d = worldLength(50);
    addInjury(58, p.x, p.y, 0);
    addInjury(58, p.x, p.y, 0);
    addBlood(0, p.x, p.y, 2);
    addBlood(95, p.x, p.y, 3);
    addBlood(95, p.x + d, p.y + d, 3);
    addBlood(95, p.x - d, p.y - d, 3);
    --lives;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("DOVE_SQUEAL1");
    if (lives == 0) {
        speed = 0;
        updateInjury();
        return;
    }
    // Its zap plays; out of reach until it next comes in low.
    gameState->createPopup("correct", static_cast<int>(p.x), static_cast<int>(p.y), this);
    invincibleTimer = 0;
    invincible = true;
    shocked = true;
    showFlash();
}

void Dove::render() {
    showFlash();  // (the state the last pass left)
    if (!alive) {
        // Dying: it bleeds while its burst plays, then its body lies where
        // it fell (on the body, in the tongs' reach), and it is gone.
        Point p = world(at);
        addBlood(0, p.x, p.y, 4);
        if (!flash->ready() || flash->age() >= flash->lengthOf("DYING", "anim")) {
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("DOVE_EXPLODE");
            const Point fell = world({std::clamp(at.x, kMinX, kMaxX), std::clamp(at.y, kMinY, kMaxY)});
            Injury* body = newInjury(kDeadDove, fell.x, fell.y, static_cast<int>(std::lround(angle)));
            gameState->addInjury(body, false, false);
            releaseRef(body);
            retainRef(this);
            gameState->removeInjury(this);
            releaseRef(this);
        }
        return;
    }
    // Flying on its heading (shocked too).
    const double a = angle * 3.141592653589793 / 180.0;
    at.x += static_cast<float>(std::sin(a) * speed);
    at.y += static_cast<float>(-std::cos(a) * speed);
    if (shocked && (!flash->ready() || flash->age() >= flash->lengthOf("SHOCKED", "shocked"))) shocked = false;
    if (speed != 0) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey(high ? "DOVE_FLAP" : "SOARING");
        if (++specialTimer >= current().specialPeriod) special();
        float dx = at.x - target.x, dy = at.y - target.y;
        if (std::sqrt(dx * dx + dy * dy) <= kSettle) speed = 0;
    } else if (++angleTimer >= kMaxAngleTimer) {
        // Waited off the side: across again, low (and in reach) once it has
        // crossed high.
        if (crossed == kMaxCrossed) {
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("DOVE_COOING");
            invincible = false;
            high = false;
            headAcross();
            speed = kSpeed / (kMaxCrossed - crossed + 1);
            crossed = 0;
        } else {
            invincible = true;
            high = true;
            headAcross();
            speed = kSpeed / (kMaxCrossed - crossed + 1);
            ++crossed;
        }
    }
    boxFollows();
}

// ------------------------------------------------------------------- Droplet

Droplet::Droplet(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "droplet";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Droplet"));
    Sprite* s = Sprite::initWithImageNamed("Splatter.png", 64, 128);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 31;
    sh->offset = {-32, -64};
    sh->scale = 1.5f;
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    mSpriteArray.addObject(s);
    scene().addShape(sh);
    releaseRef(s);
}

void Droplet::render() {
    // Falling (shrinking) for 60 frames, then landed.
    ++mCurrentFrame;
    mSpriteArray[0]->currentShape()->scale = 1.5f - static_cast<float>(mCurrentFrame) / 60.0f;
    if (mCurrentFrame > 59) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION2");
        updateInjury();
    }
}

void Droplet::updateInjury() {
    if (++currentStage <= maxStages) return;
    Point at = mSpriteArray[0]->currentShape()->pos;
    addInjury(58, at.x, at.y, 0);
    addInjury(58, at.x, at.y, 0);
    invincible = true;
    alive = false;
    retainRef(this);
    gameState->removeInjury(this);
    releaseRef(this);
}

// ---------------------------------------------------------------- InsectBoss

InsectBoss::InsectBoss(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    make = "insectBoss";
    alive = true;
    maxStages = 1;
    initStageArray(flashStageTable("InsectBoss"));
    Sprite* s = Sprite::initWithImageNamed("waspMove_0001", 256, 256);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = 31;  // ("creatures_higher", as the dove)
    s->currentShape()->offset = {-128, -128};
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    releaseRef(s);
    flash = std::make_unique<FlashCreature>("InsectBoss");
    flash->attach(s->currentShape());
    setup();
    showFlash();
}

void InsectBoss::showFlash() {
    if (!flash->ready()) return;
    flash->turn = angle;
    flash->look = VectorLook();
    flash->look.hidden.push_back("hit");
    if (!alive) {
        flash->show("DYING");
    } else if (shocked) {
        flash->show("SHOCKED");
    } else {
        flash->show("ALIVE");
        if (speed == 0) flash->look.frames["alive"] = 1;  // (resting: its wings still)
    }
}

void InsectBoss::boxFollows() {
    Shape* s = body();
    s->pos = world(at);
    s->rotation = 0;
    moveFlashHitBoxes(s->pos);
    traceMoved();
}

void InsectBoss::headFor(float x, float y) {
    target = {x, y};
    angle = headingTo(at, target);
}

void InsectBoss::setup() {
    InjuryGraphic::setup();
    // Its hit area, Flash's, 2 to work.
    if (!hitboxes) flashHitBoxes(*flash, "ALIVE", world(at), 2);
    if (currentStage != 1) return;
    // In from off one side (at random) to off the other.
    shocked = false;
    right = std::rand() % 2 == 0;
    at.x = right ? kMinX - kOff : kMaxX + kOff;
    at.y = std::round((kMaxY - kMinY) * random01()) + kMinY;
    headFor(right ? kMaxX + kOff : kMinX - kOff, std::round((kMaxY - kMinY) * random01()) + kMinY);
    angleTimer = 0;
    lives = 3;
    speed = kSpeed;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("WASP_FLY");
    crossed = 0;
    boxFollows();
}

void InsectBoss::updateInjury() {
    ++currentStage;
    speed = 0;
    damageTimer = 0;
    Point p = world(at);
    gameState->createPopup("correct", static_cast<int>(p.x), static_cast<int>(p.y), this);
    if (currentStage > maxStages) {
        invincible = true;
        alive = false;
    }
}

void InsectBoss::special() {
    // A sting over the body: a small cut.
    if (!(at.x > kMinX && at.x < kMaxX && at.y > kMinY && at.y < kMaxY) || current().effect != "smallCut") return;
    Point p = world(at);
    gameState->health -= 3;
    addInjury(1, p.x, p.y, static_cast<int>(360 * random01()));
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("PIZZA_CUTTER");
    addBlood(0, p.x, p.y, 1);
}

void InsectBoss::interact() {
    if (!alive || lives <= 0) return;
    // Shocked: poison spills.
    Point p = world(at);
    addInjury(11, p.x, p.y, 0);
    addInjury(11, p.x, p.y, 0);
    if (--lives == 0) {
        updateInjury();
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("GENERIC_CREATURE1");
        return;
    }
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("CRAB_EXPLODE");
    addInjury(12, p.x, p.y, 0);
    gameState->createPopup("correct", static_cast<int>(p.x), static_cast<int>(p.y), this);
    // Off the side at once.
    right = !right;
    headFor(right ? kMaxX + kFlee : kMinX - kFlee, std::round((kMaxY - kMinY) * random01()) + kMinY);
    angleTimer = kMaxAngleTimer - 1;
    invincibleTimer = 0;
    invincible = true;
    shocked = true;
    showFlash();
}

void InsectBoss::render() {
    showFlash();  // (the state the last pass left)
    Point p = world(at);
    if (!alive) {
        // Dying: poison every 10 frames, a last blob when its death has
        // played, and it is gone.
        if (++bloodTimer >= 10) {
            bloodTimer = 0;
            addInjury(11, p.x, p.y, 0);
        }
        if (!flash->ready() || flash->age() >= flash->lengthOf("DYING", "anim")) {
            addInjury(12, p.x, p.y, 0);
            // Its body where it fell (on the body, in the tongs' reach).
            const Point fell = world({std::clamp(at.x, kMinX, kMaxX), std::clamp(at.y, kMinY, kMaxY)});
            addInjury(kDeadWasp, fell.x, fell.y, static_cast<int>(std::lround(angle)));
            retainRef(this);
            gameState->removeInjury(this);
            releaseRef(this);
        }
        return;
    }
    const double a = angle * 3.141592653589793 / 180.0;
    at.x += static_cast<float>(std::sin(a) * speed);
    at.y += static_cast<float>(-std::cos(a) * speed);
    if (speed != 0) {
        // In flight: out of reach, stinging.
        invincible = true;
        if (++specialTimer >= current().specialPeriod) {
            specialTimer = static_cast<int>(std::round(current().specialPeriod * random01()));
            gameState->health -= current().damage * 2;
            special();
        }
        float dx = at.x - target.x, dy = at.y - target.y;
        if (std::sqrt(dx * dx + dy * dy) <= kSettle) speed = 0;
    } else {
        // Resting: in reach.
        invincible = false;
        if (++angleTimer >= kMaxAngleTimer) {
            angleTimer = std::max(0.0f, kMaxAngleTimer * random01() - kMaxAngleTimer / 3.0f);
            speed = kSpeed;
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("WASP_FLY");
            if (crossed == kMaxCrossed) {
                // Onto the body, to rest a moment there.
                crossed = 0;
                headFor(std::round((kMaxX - kMinX) * random01()) + kMinX, std::round((kMaxY - kMinY) * random01()) + kMinY);
                angleTimer = kMaxAngleTimer / 1.2f;
            } else {
                right = !right;
                headFor(right ? kMaxX + kOff : kMinX - kOff, std::round((kMaxY - kMinY) * random01()) + kMinY);
                angleTimer = 0;
                ++crossed;
            }
        }
    }
    if (shocked && (!flash->ready() || flash->age() >= flash->lengthOf("SHOCKED", "shocked"))) shocked = false;
    boxFollows();
}

}  // namespace as2
