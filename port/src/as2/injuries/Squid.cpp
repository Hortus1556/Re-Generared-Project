#include "as2/injuries/Squid.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"
#include "as2/tools/ITool.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
void sfx(const char* key) { MyOpenAL::sharedMyOpenAL().playSFXWithKey(key); }

// InkSplatter at (x, y).
void ink(float x, float y) {
    Injury* n = new Injury;
    n->mID = 48;
    n->x = static_cast<int>(x);
    n->y = static_cast<int>(y);
    n->rotation = 0;
    n->scale = 1;
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

float random01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }
float worldLength(float stage) { return stage * flashstage::worldPerStage(); }

// A black splat somewhere on the screen (Flash: anywhere on its 650 x 460).
void splat() {
    Point p = flashstage::stageToWorld(random01() * 650, random01() * 460);
    game->addSplatAtXY(p.x, p.y, 0, 0, 0);
}

// Squid.as's arm_positions: where each arm clip is placed about it.
// The dead squid's id (the port's own Remains: library.json, as2_data.py).
constexpr int kDeadSquid = 202;

constexpr float kArmAt[5][2] = {{-245.5f, -142}, {-223.5f, 80.7f}, {244, -187.3f}, {247, 18.3f}, {-17.9f, 156}};
// The iPad's pictures, under the art were it missing.
const char* const kArmPictures[5] = {"tentacleLeg4", "tentacleLeg5", "tentacleLeg1", "tentacleLeg2", "tentacleLeg3"};

Sprite* sprite(const std::string& image, int w, int h, int layer) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = layer;
    s->currentShape()->offset = {-w / 2.0f, -h / 2.0f};
    return s;
}

HitBox box(const flash::Rect& r, Point about, float health) {
    const float w = worldLength(r.xMax - r.xMin), h = worldLength(r.yMax - r.yMin);
    const float x = about.x + worldLength((r.xMin + r.xMax) / 2), y = about.y + worldLength((r.yMin + r.yMax) / 2);
    HitBox b;
    b.rect = {{x - w / 2, y - h / 2}, {w, h}};
    b.sprite = Sprite::initWithImageNamed("hitbox_20x16.png", static_cast<int>(w), static_cast<int>(h));
    b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* g = b.sprite->currentShape();
    g->layer = 33;
    g->offset = {-w / 2, -h / 2};
    g->pos = {x, y};
    b.health = health;
    b.active = health > 0;
    b.colour = 0x66ff33;
    return b;
}

}  // namespace

Squid::Squid(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    make = "squid";
    maxStages = 6;
    alive = true;
    initStageArray(flashStageTable("Squid"));
    const Point p = at();
    // Its arms under it (Squid.as put them at the bottom of the injuries),
    // each the Flash AS2's arm clip.
    for (int k = 0; k < 5; ++k) {
        Sprite* s = sprite(kArmPictures[k], 128, 128, 43);
        armAt[k] = {kArmAt[k][0], kArmAt[k][1]};
        s->currentShape()->pos = {p.x + worldLength(armAt[k].x), p.y + worldLength(armAt[k].y)};
        mSpriteArray.addObject(s);
        scene().addShape(s->currentShape());
        arms[k] = std::make_unique<FlashCreature>("arm" + std::to_string(k + 1));
        arms[k]->attach(s->currentShape());
        releaseRef(s);
    }
    // Its body and head: the Flash AS2's squid.
    Sprite* body = sprite("squidNormal", 512, 512, 43);
    body->currentShape()->pos = p;
    mSpriteArray.addObject(body);
    scene().insertShape(body->currentShape());
    flash = std::make_unique<FlashCreature>("Squid");
    flash->attach(body->currentShape());
    releaseRef(body);
    dyingFrames = flash->innerLength("head", "dying", "dying");
    setup();
}

Squid::~Squid() {
    for (Group& g : groups) freeGroup(g);
}

void Squid::freeGroup(Group& g) {
    if (!g.hits) return;
    for (int k = 0; k < g.count; ++k) {
        scene().removeShape(g.hits[k].sprite->currentShape());
        releaseRef(g.hits[k].sprite);
    }
    delete[] g.hits;
    g.hits = nullptr;
}

void Squid::showHead() {
    static const char* const kLabels[] = {"", "dying", "before_spray", "", "craze", "mouth_open", "spray", "stunned", "",
                                          "dead"};
    const char* label = kLabels[std::clamp(state, 0, 9)];
    flash->look = VectorLook();
    for (int g = 1; g <= 6; ++g) flash->look.hidden.push_back("hit" + std::to_string(g));
    flash->look.frames["head"] = flash->innerFrameOf("head", label);
    // (Its inner clips from when the state came; the dying one stepped a
    // frame a pass.)
    flash->look.inFrames["head"] = stateFrames;
    if (state == kDying) flash->look.frames["head.dying"] = std::min(dyingFrame + 1, dyingFrames);
    for (int k = 0; k < 5; ++k) {
        Shape* s = mSpriteArray[k]->currentShape();
        s->pos = {at().x + worldLength(armAt[k].x), at().y + worldLength(armAt[k].y)};
    }
}

void Squid::setup() {
    InjuryGraphic::setup();
    for (Group& g : groups) freeGroup(g);
    const Point p = at();
    // The battery's target: its tonsils, in its open mouth (nothing to heal).
    VectorLook open;
    open.frames["head"] = flash->innerFrameOf("head", "mouth_open");
    flash::Rect tonsils = flash->boundsOf("", "head.mouth_open.tonsils", &open);
    if (tonsils.empty()) tonsils = {-40, 40, 40, 120};
    groups[0].count = 1;
    groups[0].hits = new HitBox[1]{box(tonsils, p, 0)};
    // Its arms' boxes and its body's (Flash's hit1..hit6), 6 each, yellow
    // (shown while it is stunned).
    for (int g = 1; g <= 6; ++g) {
        std::vector<flash::Rect> areas = flash->hitAreas("", "hit" + std::to_string(g));
        Group& grp = groups[g];
        grp.count = static_cast<int>(areas.size());
        grp.hits = grp.count ? new HitBox[areas.size()] : nullptr;
        for (size_t k = 0; k < areas.size(); ++k) {
            grp.hits[k] = box(areas[k], p, 6);
            float* c = grp.hits[k].sprite->currentShape()->color;
            c[0] = c[1] = c[3] = 1;
            c[2] = 0.2f;  // (16777011: yellow)
        }
    }
    specialTimer = 0;
    state = kResting;
    stateFrames = 0;
    stunned = false;
    showHead();
    trace("boxed");
}

bool Squid::allTentaclesRemoved() const {
    for (int g = 1; g < 6; ++g)
        if (!groups[g].complete) return false;
    return true;
}

void Squid::removeHit(int group) {
    freeGroup(groups[group]);
    groups[group].complete = true;
    // An arm sawn off: drawn back a little (5 to 15 each way).
    if (group >= 1 && group <= 5) {
        armAt[group - 1].x -= 5 + random01() * 10;
        armAt[group - 1].y -= 5 + random01() * 10;
    }
}

void Squid::interact() {
    if (gameState->currentTool->toolData->type != "Battery") return;
    gameState->createPopup("correct", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), this);
    MyOpenAL::sharedMyOpenAL().stopSoundWithKey("SUCK");
    state = kStunned;
    stateFrames = 0;
    specialTimer = 0;
    stunned = true;
    // The boxes still to saw shown.
    for (int g = 1; g <= 6; ++g)
        for (int k = 0; groups[g].hits && k < groups[g].count; ++k)
            if (groups[g].hits[k].health > 0) scene().addShape(groups[g].hits[k].sprite->currentShape());
    showHead();
    trace("stunned");
}

void Squid::special() {
    int period = current().specialPeriod;
    switch (state) {
    case kResting:
        // It opens its mouth and sucks (the shorter the later the stage).
        specialTimer = period / 10 * currentStage;
        state = kSucking;
        break;
    case kSucking:
        // It sprays.
        specialTimer = period / 2;
        state = kSpitting;
        MyOpenAL::sharedMyOpenAL().stopSoundWithKey("SUCK");
        break;
    case kSpitting:
        specialTimer = 0;
        state = kResting;
        break;
    case kStunned:
        // Come to: a craze.
        specialTimer = period / 2;
        state = kAngry;
        break;
    case kAngry:
        specialTimer = 0;
        state = kResting;
        break;
    default:
        break;
    }
    stateFrames = 0;
    // (Its boxes hidden again.)
    for (int g = 1; g <= 6; ++g)
        for (int k = 0; groups[g].hits && k < groups[g].count; ++k)
            scene().removeShape(groups[g].hits[k].sprite->currentShape());
    stunned = false;
    showHead();
    trace("special");
}

void Squid::updateInjury() {
    ++currentStage;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (currentStage <= maxStages) {
        stunned = false;
        specialTimer = current().specialPeriod;
        return;
    }
    // The body sawn through: dead, ink bursting from it.
    sfx("SNARL");
    alive = false;
    invincible = true;
    state = kDying;
    stateFrames = 0;
    dyingFrame = 0;
    showHead();
}

void Squid::createEffect(int count, int) {
    for (int n = 0; n < count; ++n) ink(gameState->mouseX, gameState->mouseY);
}

void Squid::render() {
    ++stateFrames;
    showHead();
    const Point p = at();
    if (!alive) {
        if (state != kDying) return;
        sfx("SQUID_EXPLOSION");
        if (++inkTimer >= 25) {
            // Ink about it every 25 frames.
            inkTimer = 0;
            const float d = worldLength(100);
            ink(p.x, p.y);
            ink(p.x + d, p.y - d);
            ink(p.x - d, p.y + d);
            ink(p.x + d, p.y + d);
            ink(p.x - d, p.y - d);
        }
        splat();
        // Its death a frame a pass; at its end, its body is left (dead, to
        // lift away) and it is gone.
        if (++dyingFrame >= dyingFrames) {
            state = kGone;
            Injury* body = new Injury;
            body->mID = kDeadSquid;
            body->x = static_cast<int>(p.x);
            body->y = static_cast<int>(p.y);
            body->rotation = 0;
            body->scale = 1;
            gameState->addInjury(body, false, false);
            releaseRef(body);
            retainRef(this);
            gameState->removeInjury(this);
            releaseRef(this);
        }
        return;
    }
    if (++specialTimer >= current().specialPeriod) special();
    switch (state) {
    case kAngry:
        sfx("SNARL");  // (Flash's ERUPT: not in the iPad's bundle)
        gameState->flashRed();
        gameState->health -= current().damage * 2;
        break;
    case kSpitting:
        sfx("SPITTING");
        if (random01() < 0.5f) splat();
        gameState->health -= current().damage * 2;
        break;
    case kSucking:
        if (stateFrames > 15) sfx("SUCK");
        break;
    case kStunned:
        sfx("DAZED");
        break;
    default:
        break;
    }
}

std::string Squid::toolNow() const {
    if (!alive) return state == kDying ? "wait" : "none";
    if (state == kSucking) return "battery";
    if (state == kStunned) return "chainsaw";
    return "wait";
}

void Squid::traceBoxes() const {
    if (state == kSucking) InjuryGraphic::traceBoxes(groups[0].hits, 1, "");
    if (state != kStunned) return;
    if (!allTentaclesRemoved()) {
        for (int g = 1; g < 6; ++g)
            if (groups[g].hits) InjuryGraphic::traceBoxes(groups[g].hits, groups[g].count, "");
    } else if (groups[6].hits) {
        InjuryGraphic::traceBoxes(groups[6].hits, groups[6].count, "");
    }
}

}  // namespace as2
