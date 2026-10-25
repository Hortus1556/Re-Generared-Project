#include "as2/injuries/Centipede.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
void sfx(const char* key) { MyOpenAL::sharedMyOpenAL().playSFXWithKey(key); }
constexpr double kDeg = 3.141592653589793 / 180.0;

Injury* injuryAt(int id, int x, int y, int rotation = 0) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = x;
    n->y = y;
    n->rotation = rotation;
    n->scale = 1;
    return n;
}

void add(int id, int x, int y, int rotation = 0) {
    Injury* n = injuryAt(id, x, y, rotation);
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

void blood(int x, int y, int count = 1) {
    Injury* n = injuryAt(0, x, y);
    for (int k = 0; k < count; ++k) gameState->addBlood(n, -1);
    releaseRef(n);
}

constexpr int kSmallCut = 1, kLargeCut = 3, kPoisonSplatter = 0xb, kPoison = 0xc, kCentipede = 0x4a;
// The small centipede's remains, its head (the port's own: library.json,
// as2_data.py).
constexpr int kDeadCentipede = 204;

Sprite* sprite(const std::string& image, int w, int h, Point offset, int layer) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->offset = offset;
    s->currentShape()->layer = layer;
    return s;
}


}  // namespace

// ------------------------------------------------------------------ Centipede

namespace {
float rand01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }
// MapUtils.getAngle(x1, x2, y1, y2) - 90: the heading from one point to
// another, 0 up, clockwise.
float headingTo(Point from, Point to) {
    return static_cast<float>(std::atan2(to.y - from.y, to.x - from.x) / kDeg) + 90;
}
}  // namespace

Centipede::Centipede(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    make = "centipede";
    maxStages = 1;
    alive = true;
    dirTimer = static_cast<int>(rand01() * kMaxDirTimer / 2);
    initStageArray(flashStageTable("Centipede"));
    // The head, the body segments, the tail: the Flash AS2's (the iPad's
    // pictures under them, were the art missing), each where it is.
    const Point at = flashstage::toStage({flashPlace().x * 2, flashPlace().y * 2});
    for (int k = 0; k < kSegments; ++k) {
        Sprite* s = k == 0       ? sprite("centipede_Head0001", 128, 128, {-64, -64}, 32)
                    : k == kTail ? sprite("centipede_tail0001", 128, 256, {-64, -128}, 32)
                                 : sprite("centipede_Body0001", 128, 64, {-64, -32}, 32);
        mSpriteArray.addObject(s);
        scene().addShape(s->currentShape());
        art[k] = std::make_unique<FlashCreature>(k == 0 ? "head" : k == kTail ? "tail" : "body");
        art[k]->attach(s->currentShape());
        releaseRef(s);
        // All at its place, the tail 50 a segment below (Centipede.setup).
        from[k] = to[k] = k == kTail ? Point{at.x, at.y + left * 50.0f} : at;
    }
    setup();
    specialTimer = current().specialPeriod / 2;
    tweenBodyParts();
    place();
}

void Centipede::setup() {
    InjuryGraphic::setup();
    // The box on the tinted segment (its own clip's: 5 to saw), placed with
    // it each frame.
    freeHitBoxes();
    std::vector<flash::Rect> areas = art[left - 1]->hitAreas("", "");
    hitArea = areas.empty() ? flash::Rect{-30, -20, 30, 20} : areas[0];
    hitBoxCount = 1;
    hitboxes = new HitBox[1];
    HitBox& b = hitboxes[0];
    b.sprite = sprite("hitbox_80x64.png", 64, 40, {-32, -20}, 33);
    b.active = true;
    b.colour = 0x66ff33;
    b.health = 5;
    // The tinted one red (ColourUtils.tint(0xff0000, 0.3)); the rest plain.
    for (int k = 0; k < kSegments; ++k) {
        art[k]->tint = flash::ColorTransform();
        if (k == left - 1) {
            for (int c = 0; c < 3; ++c) art[k]->tint.mult[c] = 0.7f;
            art[k]->tint.add[0] = 0.3f * 255;
        }
    }
}

void Centipede::tweenBodyParts() {
    // The head: a step on (now and then turned 20 either way), or back the
    // way it came at an edge.
    const float r = rand01() * 3;
    if (++dirTimer == kMaxDirTimer) {
        dirTimer = static_cast<int>(rand01() * kMaxDirTimer / 2);
        ranRot = r < 1 ? -20 : r < 2 ? 20 : 0;
    }
    for (int k = 0; k < kSegments; ++k) from[k] = to[k];
    const Point head = from[0];
    const double a = (ranRot + turn[0]) * kDeg;
    Point next = {head.x + static_cast<float>(std::sin(a)) * kMaxMoveDist, head.y - static_cast<float>(std::cos(a)) * kMaxMoveDist};
    if (!(next.x > kMinX && next.x < kMaxX && next.y > kMinY && next.y < kMaxY)) {
        const double back = (turn[0] + 180) * kDeg;
        next = {head.x + static_cast<float>(std::sin(back)) * kMaxMoveDist,
                head.y - static_cast<float>(std::cos(back)) * kMaxMoveDist};
        dirTimer = kMaxDirTimer - 1;
    }
    to[0] = next;
    turn[0] = headingTo(head, next);
    // Each segment left to where the one before is; the tail to the last.
    for (int k = 1; k < left; ++k) {
        turn[k] = headingTo(from[k], from[k - 1]);
        to[k] = from[k - 1];
    }
    turn[kTail] = headingTo(from[kTail], from[left - 1]);
    to[kTail] = from[left - 1];
    stepFrame = 0;
}

void Centipede::place() {
    const float t = std::min(1.0f, static_cast<float>(stepFrame) / kStepFrames);
    Point now[kSegments];
    for (int k = 0; k < kSegments; ++k) {
        now[k] = {from[k].x + (to[k].x - from[k].x) * t, from[k].y + (to[k].y - from[k].y) * t};
        Shape* s = mSpriteArray[k]->currentShape();
        s->pos = flashstage::stageToWorld(now[k].x, now[k].y);
        s->rotation = 0;
        art[k]->turn = turn[k];
    }
    // The tinted segment's box: its clip's box turned with it (as Flash's
    // hitTestObject, the box about the turned clip).
    const int k = left - 1;
    const double a = turn[k] * kDeg;
    const float c = static_cast<float>(std::cos(a)), sn = static_cast<float>(std::sin(a));
    float xMin = 1e9f, yMin = 1e9f, xMax = -1e9f, yMax = -1e9f;
    for (float x : {hitArea.xMin, hitArea.xMax})
        for (float y : {hitArea.yMin, hitArea.yMax}) {
            float rx = x * c - y * sn, ry = x * sn + y * c;
            xMin = std::min(xMin, rx), xMax = std::max(xMax, rx), yMin = std::min(yMin, ry), yMax = std::max(yMax, ry);
        }
    const Point lo = flashstage::stageToWorld(now[k].x + xMin, now[k].y + yMin);
    const Point hi = flashstage::stageToWorld(now[k].x + xMax, now[k].y + yMax);
    HitBox& b = hitboxes[0];
    b.rect = {lo, {hi.x - lo.x, hi.y - lo.y}};
    b.sprite->currentShape()->pos = {(lo.x + hi.x) / 2, (lo.y + hi.y) / 2};
    traceMoved();
}

void Centipede::special() {
    // A bite: a small cut under its head, turned with it.
    specialTimer = current().specialPeriod / 2;
    const Point head = mSpriteArray[0]->currentShape()->pos;
    gameState->health -= 3;
    add(kSmallCut, static_cast<int>(head.x), static_cast<int>(head.y), static_cast<int>(turn[0]));
    sfx("PIZZA_CUTTER");
    blood(static_cast<int>(head.x), static_cast<int>(head.y));
}

void Centipede::createEffect(int count, int) {
    blood(static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), count);
}

void Centipede::updateInjury() {
    const Point head = mSpriteArray[0]->currentShape()->pos;
    gameState->createPopup("correct", static_cast<int>(head.x), static_cast<int>(head.y), this);
    const Point seg = mSpriteArray[left - 1]->currentShape()->pos;
    const int x = static_cast<int>(seg.x), y = static_cast<int>(seg.y);
    if (left == 1) add(kPoison, x, y);
    add(kPoisonSplatter, x, y);
    blood(x, y);
    removeHit();
}

void Centipede::removeHit() {
    const Point seg = mSpriteArray[left - 1]->currentShape()->pos;
    sfx("SPLAT1");
    blood(static_cast<int>(seg.x), static_cast<int>(seg.y), 3);
    scene().removeShape(mSpriteArray[left - 1]->currentShape());
    if (--left == 0) {
        // The head off: dead, the head left lying where it was.
        add(kDeadCentipede, static_cast<int>(seg.x), static_cast<int>(seg.y), static_cast<int>(std::lround(turn[0])));
        int n = std::min(3, static_cast<int>(rand01() * 3 + 1));
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("GENERIC_CREATURE" + std::to_string(n));
        ++currentStage;
        alive = false;
        scene().removeShape(mSpriteArray[kTail]->currentShape());
        retainRef(this);
        gameState->removeInjury(this);
        releaseRef(this);
        return;
    }
    // The next one tinted (the tail follows it from the next step).
    setup();
    place();
}

void Centipede::render() {
    if (!alive) return;
    if (++specialTimer >= current().specialPeriod) special();
    if (++stepFrame >= kStepFrames) {
        stepFrame = kStepFrames;
        place();
        tweenBodyParts();
    }
    place();
}

// --------------------------------------------------------------- BigCentipede

namespace {
float random01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }
float worldLength(float stage) { return stage * flashstage::worldPerStage(); }
// A splat somewhere on the screen (Flash: anywhere on its 650 x 460).
void splatAnywhere(float r, float g, float b) {
    Point p = flashstage::stageToWorld(random01() * 650, random01() * 460);
    game->addSplatAtXY(p.x, p.y, r, g, b);
}
}  // namespace

BigCentipede::BigCentipede(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    make = "bigCentipede";
    maxStages = 1;
    alive = true;
    lastX = gameController->pointer().x;
    lastY = gameController->pointer().y;
    initStageArray(flashStageTable("BigCentipede"));
    // Its picture: the Flash AS2's clip, in every state (the iPad's head
    // under it, were the art missing).
    Sprite* s = sprite("large_centipede0001", 512, 256, {-256, -128}, 32);
    s->currentShape()->pos = at();
    mSpriteArray.addObject(s);
    scene().addShape(s->currentShape());
    flash = std::make_unique<FlashCreature>("BigCentipede");
    flash->attach(s->currentShape());
    releaseRef(s);
    setup();
}

void BigCentipede::setup() {
    InjuryGraphic::setup();
    // A box on each eye (placed on them each frame).
    freeHitBoxes();
    hitBoxCount = 3;
    hitboxes = new HitBox[3];
    for (int k = 0; k < 3; ++k) {
        HitBox& b = hitboxes[k];
        b.sprite = sprite("hitbox_80x64.png", 38, 30, {-19, -15}, 33);
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
    }
    specialTimer = 0;
    mouseShake = 0;
    cutTimer = 0;
    state = kHidden;
    isStunned = false;
    showFlash();
}

const char* BigCentipede::label() const {
    static const char* const kLabels[] = {"hidden", "appear", "attack", "stunned", "craze", "vanish", "dying", "retract"};
    return kLabels[state];
}

bool BigCentipede::played() const {
    return !flash->ready() || flash->age() >= flash->lengthOf(label(), label());
}

void BigCentipede::showFlash() {
    flash->show(label());
    flash->look = VectorLook();
    // Its eyes: open, wide while it is dazed, burst once put out.
    for (int k = 0; k < 3; ++k) {
        const std::string eye = "eye" + std::to_string(k + 1);
        const std::string path = std::string(label()) + ".holder.holder." + eye;
        int f = hits[k] ? flash->frameIn(label(), path, "burst") : state == kStunned ? flash->frameIn(label(), path, "normal") : 1;
        flash->look.frames[eye] = f;
        // Its box on it (as drawn now).
        flash::Rect r = flash->boundsNow(path);
        if (r.empty() || !hitboxes) continue;
        const float w = std::max(20.0f, worldLength(r.xMax - r.xMin)), h = std::max(20.0f, worldLength(r.yMax - r.yMin));
        const float x = at().x + worldLength((r.xMin + r.xMax) / 2), y = at().y + worldLength((r.yMin + r.yMax) / 2);
        hitboxes[k].rect = {{x - w / 2, y - h / 2}, {w, h}};
        hitboxes[k].sprite->currentShape()->pos = {x, y};
    }
}

void BigCentipede::setState(State s) {
    state = s;
    showFlash();
    trace("special");
}

void BigCentipede::special() {
    int period = current().specialPeriod;
    isStunned = false;
    if (state == kHidden) {
        // Out it comes.
        specialTimer = static_cast<int>(period / 1.5);
        setState(kAppear);
    } else if (state == kCraze) {
        specialTimer = 0;
        setState(kVanish);
    } else if (state == kStunned) {
        // Come to: a craze.
        specialTimer = period / 2;
        setState(kCraze);
    }
}

void BigCentipede::createEffect(int count, int velocity) {
    Injury* n = injuryAt(0, static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY));
    for (int k = 0; k < count; ++k) gameState->addBlood(n, velocity);
    releaseRef(n);
}

void BigCentipede::updateEye(int eye) {
    eyeHit = eye;
    updateInjury();
}

void BigCentipede::removeHit(int eye) {
    sfx("SPLAT1");
    hits[eye] = true;
    const HitBox& b = hitboxes[eye];
    int x = static_cast<int>(b.rect.origin.x + b.rect.size.width / 2), y = static_cast<int>(b.rect.origin.y + b.rect.size.height / 2);
    blood(x, y, 4);
    for (int k = 0; k < 4; ++k) add(kPoisonSplatter, x, y);
}

void BigCentipede::updateInjury() {
    if (eyeHit >= 0 && eyeHit < 3 && !hits[eyeHit]) removeHit(eyeHit);
    if (eyesLeft() == 0) ++currentStage;
    int mx = static_cast<int>(gameState->mouseX), my = static_cast<int>(gameState->mouseY);
    isStunned = false;
    if (currentStage > maxStages) {
        sfx("CENT_SCREECH");
        gameState->createPopup("correct", mx, my, this);
        alive = false;
        invincible = true;
        setState(kDying);
        return;
    }
    sfx("SNARL");
    gameState->createPopup("correct", mx, my, this);
    specialTimer = current().specialPeriod / 2;
    setState(kCraze);
}

void BigCentipede::render() {
    showFlash();  // (its eyes and their boxes as drawn now)
    const Point p = at();
    float damage = 0;  // the heart's loss this frame
    if (!alive) {
        if (state != kDying) return;
        // Bursting: poison and blood about it every 10 frames, green splats;
        // when its death has played, three small centipedes.
        sfx("CRAB_EXPLODE");  // (Flash's CENT_EXPLOSION: not in the iPad's bundle)
        if (++bloodTimer >= 10) {
            bloodTimer = 0;
            const float d = worldLength(100);
            const int x = static_cast<int>(p.x), y = static_cast<int>(p.y), di = static_cast<int>(d);
            for (Point o : {Point{0, 0}, Point{d, -d}, Point{-d, d}, Point{d, d}, Point{-d, -d}}) {
                add(kPoisonSplatter, x + static_cast<int>(o.x), y + static_cast<int>(o.y));
                blood(x + static_cast<int>(o.x), y + static_cast<int>(o.y));
            }
            (void)di;
        }
        splatAnywhere(0, 0.58f, 0.27f);
        if (played()) {
            // (Flash's three lay on each other: turned apart.)
            for (int rot : {240, 120, 0}) add(kCentipede, static_cast<int>(p.x), static_cast<int>(p.y), rot);
            retainRef(this);
            gameState->removeInjury(this);
            releaseRef(this);
        }
        return;
    }
    switch (state) {
    case kHidden:
        // It waits while anything else is left to treat; then the body
        // rumbles (most of the wait) before it comes out.
        if (specialTimer == 0 && gameState->_injuries.count() > 1) return;
        if (specialTimer <= current().specialPeriod * 0.7) {
            sfx("EXPLOSION");  // (Flash's ERUPT: not in the iPad's bundle)
            gameState->setShakeAsHigh(1);
        }
        if (++specialTimer >= current().specialPeriod) special();
        break;
    case kAppear:
        // Out, roaring, blood flying; at the end a bite: a large cut.
        if (flash->age() >= 16) {
            sfx("SNARL");
            splatAnywhere(0.27f, 0, 0);
        }
        if (played()) {
            flash::Rect r = flash->boundsOf("appear", "appear");
            const float h = r.empty() ? 0 : worldLength(r.yMax - r.yMin);
            gameState->health -= 5;
            add(kLargeCut, static_cast<int>(p.x - worldLength(20)), static_cast<int>(std::max(80.0f, p.y - h / 2)));
            sfx("PIZZA-CUTTER");
            specialTimer = 0;
            mouseShake = 0;
            setState(kAttack);
            if (!hinted) {
                // (Nothing in the original said what to do: no tool works
                // while it attacks.)
                hinted = true;
                float tx, ty;
                gameState->showHint(platform::deviceTilt(tx, ty)
                                        ? "It's attacking! Rub it fast to stun it, then inject its eyes!"
                                        : "It's attacking! Shake the mouse fast over it to stun it, then inject its eyes!");
            }
        }
        break;
    case kAttack: {
        // Rubbed: each frame the pointer moves 10 stage pixels or more
        // counts; 45 of them (15 more for each eye left) and it is stunned.
        sfx("SNARL");  // (Flash's ROAR_LOOP: not in the iPad's bundle)
        Point q = gameController->pointer();
        float dx = (lastX - q.x) * 0.5f, dy = (lastY - q.y) * 0.5f;  // (the world's points)
        lastX = q.x;
        lastY = q.y;
        if (std::sqrt(dx * dx + dy * dy) >= worldLength(kNeededDist)) {
            ++mouseShake;
            gameState->setShakeAsHigh(1);
        }
        if (mouseShake >= kOMaxMouseShake - kBaseMaxShake * eyesLeft()) {
            isStunned = true;
            mouseShake = 0;
            setState(kRetract);
        }
        if (random01() < 0.5f) splatAnywhere(0, 0.58f, 0.27f);
        damage = current().damage;
        break;
    }
    case kRetract:
        // Down, dazed; then stunned.
        sfx("DAZED");
        if (played()) {
            specialTimer = 0;
            setState(kStunned);
        }
        break;
    case kStunned:
        sfx("DAZED");
        if (++specialTimer >= current().specialPeriod) special();
        // (For the play-tests: where its swaying eyes are now, while they
        // can be injected.)
        if (flash->age() % 6 == 0) trace("stunned");
        break;
    case kCraze: {
        // Raging: the screen red, the heart hurt, and every 10 frames a
        // cut bitten about it (beyond its body, on the stage).
        sfx("EXPLOSION");
        gameState->flashRed();
        damage = current().damage;
        if (++cutTimer >= kMaxCutTimer) {
            cutTimer = 0;
            flash::Rect r = flash->boundsOf("craze", "craze");
            const float w = r.empty() ? 200 : r.xMax - r.xMin;  // stage pixels
            const Point c = flashstage::toStage({p.x * 2, p.y * 2});
            const float start = 360 * random01();
            for (float a = 0; a < start + 360; a += 20) {
                const float d = w / 2 + random01() * w / 2;
                const float x = c.x + std::sin((start + a) * kDeg) * d, y = c.y - std::cos((start + a) * kDeg) * d;
                if (!(x > 0 && x < 650 && y > 0 && y < 460)) continue;
                Point q = flashstage::stageToWorld(x, y);
                gameState->health -= 3;
                add(kSmallCut, static_cast<int>(q.x), static_cast<int>(q.y), static_cast<int>(360 * random01()));
                sfx("PIZZA-CUTTER");
                blood(static_cast<int>(q.x), static_cast<int>(q.y));
                break;
            }
        }
        if (++specialTimer >= current().specialPeriod) special();
        break;
    }
    case kVanish:
        // Back under; two small centipedes crawl out.
        if (played()) {
            add(kCentipede, static_cast<int>(p.x), static_cast<int>(p.y));
            add(kCentipede, static_cast<int>(p.x + worldLength(75)), static_cast<int>(p.y + worldLength(75)));
            blood(static_cast<int>(p.x), static_cast<int>(p.y));
            specialTimer = 0;
            setState(kHidden);
            if (!hintedHide) {
                // (It waits, hidden, until nothing else is left: unsaid too.)
                hintedHide = true;
                gameState->showHint("It's hiding! Treat everything else (the centipedes, the cuts, the poison) and it "
                                    "will come back out.");
            }
        }
        break;
    default:
        break;
    }
    gameState->health = gameState->health - damage;
}

std::string BigCentipede::toolNow() const {
    if (!alive) return "wait";
    if (state == kAttack) return "rub";
    if (state == kStunned || state == kRetract) return "injection";
    return "wait";
}

void BigCentipede::traceBoxes() const {
    if (state == kStunned || state == kRetract) {
        // The eyes still to inject.
        for (int k = 0; k < 3; ++k)
            if (!hits[k]) InjuryGraphic::traceBoxes(&hitboxes[k], 1, "");
    } else if (state == kAttack) {
        // Where to rub: its middle eye's box.
        InjuryGraphic::traceBoxes(&hitboxes[1], 1, "");
    }
}

}  // namespace as2
