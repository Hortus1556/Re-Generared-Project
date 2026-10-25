#include "as2/injuries/Splatter.h"

#include <cmath>
#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
float percent() { return static_cast<float>(std::rand() % 100) / 100.0f; }

// The trace's "drop landed|gone <blood|splatter> <x>,<y> #<n>" (screen points
// of where it lies, or where the vacuum takes it; n numbers the drop), for
// the play-tests: "splatter" for the drops
// of a splatter that is one of the stage's injuries (it holds the stage up
// until vacuumed), "blood" for the rest.
bool blocksStage(InjuryGraphic* splatter) {
    for (InjuryGraphic* i : gameState->_injuries)
        if (i == splatter) return true;
    return false;
}

void traceDrop(const char* event, bool blood, const Shape* s, unsigned n) {
    if (!platform::traceEnabled()) return;
    Point p = gameState->worldToScreen(s->pos);
    platform::debugTrace("drop %s %s %.0f,%.0f #%u", event, blood ? "blood" : "splatter", p.x, p.y, n);
}

unsigned dropCount = 0;

// The splatters whose own picture is the drop and the stain.
bool ownPicture(const std::string& make) {
    return make == "peanutSplatter" || make == "featherSplatter" || make == "yellowPoisonSplatter" ||
           make == "poisonSplatter" || make == "dirt";
}

}  // namespace

Splatter::Splatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity)
    : InjuryGraphic(d, i, playerCreated, force) {
    // performInitStuff
    box_width = 20;
    box_height = 16;
    make = "splatter_higher";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Splatter"));
    baseVel = velocity == -1 ? 7.0f : static_cast<float>(velocity);
}

Splatter::~Splatter() {
    for (Particle& p : particles) releaseRef(p.sprite);
}

void Splatter::setup() {
    damageTimer = 0;
    specialTimer = 0;
    std::string picture = "Splatter";
    float scale = 0.3f;
    if (ownPicture(make)) {
        // (The peanuts' picture, "peanutSplatter", is in no build: there
        // they never showed. The bundle's loose peanut pieces instead, used
        // by nothing else: a kernel, a broken shell, the open half-shell.)
        picture = make == "peanutSplatter" ? "nut" + std::to_string(std::rand() % 5 + 1) : make;
        scale = make == "poisonSplatter" || make == "yellowPoisonSplatter" ? 0.75f : 0.5f;
        // (The peanut pieces at the whole peanut's own size.)
        if (make == "peanutSplatter") scale = 1;
    }
    for (int n = 0; static_cast<float>(n) < std::ceil(static_cast<float>(numParticles) * particle_mod); ++n) {
        Sprite* s = Sprite::initWithImageNamed(picture, origWidth, origHeight);
        s->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* sh = s->currentShape();
        sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
        sh->offset = {static_cast<float>(-(origWidth / 2)), static_cast<float>(-(origHeight / 2))};
        sh->layer = make == "peanutSplatter" ? 36 : 31;
        sh->scale = scale;
        sh->color[0] = red;
        sh->color[1] = green;
        sh->color[2] = blue;
        sh->rotation = static_cast<float>(static_cast<int>(percent() * 360.0f));
        scene().addShape(sh);
        // Thrown at half to one and a half times the speed; it flies until
        // the friction brings it under 1.
        float vel = baseVel * 0.5f + baseVel * percent();
        int steps = 1;
        for (float v = vel; v > 1; v *= friction) ++steps;
        particles.push_back({s, vel, 0, static_cast<float>(steps)});
    }
}

void Splatter::removeParticle(size_t index) {
    Particle p = particles[index];
    if (p.landed && p.sprite->currentShape()->inScene) traceDrop("gone", !blocksStage(this), p.sprite->currentShape(), p.landed);
    scene().removeShape(p.sprite->currentShape());
    releaseRef(p.sprite);
    particles.erase(particles.begin() + static_cast<long>(index));
    bool kinds = isKindOf("PoisonSplatter") || isKindOf("YellowSplatter") || isKindOf("PeanutSplatter") ||
                 isKindOf("InkSplatter") || isKindOf("FatSplatter") || isKindOf("DoveSplatter") ||
                 isKindOf("SaltSplatter");
    if (!particles.empty()) return;
    if (kinds) updateInjury();
    else gameState->checkTutorial(7, mInjury->mID);
}

void Splatter::render() {
    retainRef(this);  // the last drop gone ends it
    fly();
    if (saps && alive) {
        if (++damageTimer >= current().damagePeriod) {
            damageTimer = 0;
            gameState->health = gameState->health - static_cast<float>(static_cast<int>(particles.size()) / 3);
        }
        if ((specialTimer += 2) >= current().specialPeriod) special();
    }
    releaseRef(this);
}

void Splatter::fly() {
    // Newest first, as the original walked its list.
    for (size_t k = particles.size(); k-- > 0;) {
        if (k >= particles.size()) continue;
        Particle& p = particles[k];
        if (p.vel == -1) continue;
        Shape* s = p.sprite->currentShape();
        float v = p.vel * friction;
        if (!(v >= 1)) v = 0;
        p.vel = v;
        double a = s->rotation * 3.141592653589793 / 180.0;
        s->pos.x = static_cast<float>(s->pos.x + std::sin(a) * v);
        s->pos.y = static_cast<float>(s->pos.y + std::cos(a) * -v);
        if (p.steps != p.step) {
            // In flight: it grows a little at first, then shrinks.
            float next = p.step + 1;
            float t = std::round(100.0f / p.steps * next);
            float f = (isKindOf("FeatherSplatter") ? 100.0f - t : 50.0f - t) / 1000.0f + 1.0f;
            s->scale = s->scale * f;
            p.step = next;
            continue;
        }
        if (s->layer != 31) continue;  // landed already
        if (!remove) {
            scene().removeShape(s);
            if (minX <= s->pos.x && s->pos.x <= maxX && minY <= s->pos.y && s->pos.y <= maxY) {
                // A stain where it stopped.
                std::string picture = make;
                bool own = ownPicture(make);
                if (!own) picture = "Splatter" + std::to_string(std::min(ran, static_cast<int>(percent() * ran) + 1));
                s->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(picture);
                s->layer = 36;
                if (isKindOf("BloodSplatter") && !own) s->offset.y = -96;
                scene().addShape(s);
                p.landed = ++dropCount;
                traceDrop("landed", !blocksStage(this), s, p.landed);
            } else {
                removeParticle(k);
            }
            continue;
        }
        // The fire splatters: each drop a burn.
        int burn = isKindOf("FireSplatter") ? 0x1c : isKindOf("BloodFireSplatter") ? 0x54 : 0;
        if (burn) {
            Injury* in = new Injury;
            in->mID = burn;
            in->x = static_cast<int>(s->pos.x);
            in->y = static_cast<int>(s->pos.y);
            in->rotation = static_cast<int>(percent() * 360.0f);
            in->scale = 1;
            gameState->addInjury(in, mPlayerCreated, false);
            releaseRef(in);
        }
        removeParticle(k);
        if (particles.empty()) updateInjury();
    }
}

int Splatter::suck(Point at, int nozzleRadius, int radius, int suction) {
    int taken = 0;
    retainRef(this);  // the last drop taken may end it
    for (size_t k = particles.size(); k-- > 0;) {
        if (k >= particles.size()) continue;
        Particle& p = particles[k];
        if (p.step < p.steps) continue;  // still flying
        Shape* s = p.sprite->currentShape();
        float dx = s->pos.x - at.x, dy = s->pos.y - at.y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < static_cast<float>(nozzleRadius)) {
            if (isKindOf("PeanutSplatter")) MyOpenAL::sharedMyOpenAL().playSFXWithKey("PEANUT_SFX");
            removeParticle(k);
            ++taken;
            continue;
        }
        if (d > static_cast<float>(radius)) continue;
        // Drawn in: no longer a stain (its own drop picture; the poison's
        // own), it moves no more by itself.
        if (isKindOf("PoisonSplatter"))
            s->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D("poisonSplatter");
        else if (!isKindOf("PeanutSplatter"))
            s->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(make);
        p.vel = -1;
        // (Drawn in the frame it stopped, before it lay as a stain: it lies
        // still now all the same.)
        if (!p.landed) {
            p.landed = ++dropCount;
            traceDrop("landed", !blocksStage(this), s, p.landed);
        }
        double a = (static_cast<float>(std::atan2(static_cast<double>(dx), static_cast<double>(dy)) * 180.0 /
                                       3.141592653589793) - 180.0f) * 3.141592653589793 / 180.0;
        float pull = static_cast<float>(suction) / d;
        s->pos.x = static_cast<float>(s->pos.x + std::sin(a) * pull);
        s->pos.y = static_cast<float>(s->pos.y + std::cos(a) * pull);
    }
    releaseRef(this);
    return taken;
}

void Splatter::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (maxStages < currentStage) {
        alive = false;
        gameState->removeInjury(this);
    } else {
        setup();
    }
}

BloodSplatter::BloodSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity)
    : BloodSplatter(d, i, playerCreated, force, velocity, {0.27f, 0, 0, "Splatter", 64, 128, 1}) {}

BloodSplatter::BloodSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity,
                             const Kind& kind)
    : Splatter(d, i, playerCreated, force, velocity) {
    make = kind.make;
    red = kind.r;
    green = kind.g;
    blue = kind.b;
    origWidth = kind.width;
    origHeight = kind.height;
    numParticles = kind.particles;
    setup();
}

PoisonSplatter::PoisonSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int)
    : PoisonSplatter(d, i, playerCreated, force, "poisonSplatter", 1, 1, 1, 32, 32, 13) {}

PoisonSplatter::PoisonSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, const char* picture,
                               float r, float g, float b, int width, int height, int particles)
    : Splatter(d, i, playerCreated, force) {
    ran = 1;
    baseVel = 6;
    numParticles = particles;
    make = picture;
    red = r;
    green = g;
    blue = b;
    origWidth = width;
    origHeight = height;
    stageArray[0].damagePeriod = 350;
    saps = true;
    setup();
}

}  // namespace as2
