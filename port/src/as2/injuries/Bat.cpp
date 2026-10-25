#include "as2/injuries/Bat.h"

#include <cstdlib>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

// Blood thrown from (x, y).
void blood(float x, float y, int velocity) {
    Injury* b = new Injury;
    b->mID = 0;
    b->x = static_cast<int>(x);
    b->y = static_cast<int>(y);
    b->rotation = 0;
    b->scale = 1;
    b->offset = true;
    gameState->addBlood(b, velocity);
    releaseRef(b);
}

float worldLength(float stage) { return stage * flashstage::worldPerStage(); }
float random01() { return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1); }

}  // namespace

Bat::Bat(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    make = "bat";
    maxStages = 1;
    alive = true;
    initStageArray(flashStageTable("Bat"));
    // Its picture: the Flash AS2's clip (the iPad's hanging bat under it,
    // were the art missing).
    Sprite* s = Sprite::initWithImageNamed("bat1", 128, 256);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 32;
    sh->offset = {-64, -128};
    sh->pos = at();
    mSpriteArray.addObject(s);
    scene().addShape(sh);
    releaseRef(s);
    flash = std::make_unique<FlashCreature>("Bat");
    flash->attach(sh);
    bodyEnd = flash->lengthOf("flapping", "body");
    setup();
    showFlash();
}

const char* Bat::label() const {
    return !alive ? "dying" : state_ == kFlying ? "flapping" : state_ == kSplatting ? "spitting" : "sleeping";
}

void Bat::showFlash() {
    if (!flash->ready()) return;
    flash->show(label());
    flash->look = VectorLook();
    flash->look.hidden.push_back("hit");
    flash->look.frames["body"] = bodyFrame;
}

void Bat::boxes(int health) { flashHitBoxes(*flash, label(), at(), static_cast<float>(health)); }

void Bat::setup() {
    InjuryGraphic::setup();
    // Asleep, hanging.
    bodyTarget = 1;
    shots = 0;
    specialTimer = 0;
    state_ = kHanging;
    boxes(4);
}

void Bat::updateInjury() {
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (state_ == kHanging) {
        // Lit: it flies, flapping over the spot.
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("GENERIC_CREATURE3");
        state_ = kFlying;
        specialTimer = 0;
        boxes(2);
        return;
    }
    if (state_ != kFlying) return;
    // Injected: a step paler each time; the third shot kills it.
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("GENERIC_CREATURE2");
    static const char* const kPaler[3] = {"amount2", "amount3", "amount4"};
    bodyTarget = flash->frameIn("flapping", "body", kPaler[std::min(shots, 2)]);
    if (++shots >= 3) {
        alive = false;
        state_ = kDead;
    }
}

void Bat::special() {
    if (state_ == kFlying) {
        // Not killed in time: it heals, and spits blood about a while.
        bodyFrame = bodyTarget = 1;
        shots = 0;
        specialTimer = static_cast<int>(current().specialPeriod / 1.5);
        state_ = kSplatting;
        boxes(4);
    } else if (state_ == kSplatting) {
        // Then asleep again (the lighter again).
        specialTimer = 0;
        state_ = kHanging;
        boxes(4);
    }
}

void Bat::createEffect(int count, int velocity) {
    for (int n = 0; n < count; ++n) blood(gameState->mouseX, gameState->mouseY, velocity);
}

void Bat::render() {
    showFlash();  // (the state the last pass left)
    const Point p = at();
    if (!alive) {
        // Dying: its body plays to its end, then it bursts in blood and is
        // gone.
        if (bodyFrame < bodyEnd) {
            ++bodyFrame;
            return;
        }
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("CRAB_EXPLODE");
        const float d = worldLength(100);
        for (int n = 0; n < 5; ++n) {
            blood(p.x, p.y, -1);
            blood(p.x + d, p.y + d, -1);
            blood(p.x - d, p.y - d, -1);
        }
        Point splat = flashstage::stageToWorld(random01() * 650, random01() * 460);
        game->addSplatAtXY(splat.x, splat.y, 0.27f, 0, 0);
        ++currentStage;
        retainRef(this);
        gameState->removeInjury(this);
        releaseRef(this);
        return;
    }
    // Its body a step toward its look a pass (paler as it is shot).
    if (bodyFrame < bodyTarget) {
        ++bodyFrame;
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION1");
    } else if (bodyFrame > bodyTarget) {
        --bodyFrame;
    }
    if (state_ == kFlying || state_ == kSplatting) {
        if (state_ == kFlying) MyOpenAL::sharedMyOpenAL().playSFXWithKey("BAT_WINGS");
        if (++specialTimer >= current().specialPeriod) special();
    }
    if (state_ == kSplatting) {
        // Spitting: blood splats about its mouth, half the frames.
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SPITTING");
        if (random01() < 0.5f) {
            flash::Rect mouth = flash->boundsOf("spitting", "spitting");
            float mx = mouth.empty() ? 0 : (mouth.xMin + mouth.xMax) / 2, my = mouth.empty() ? 0 : (mouth.yMin + mouth.yMax) / 2;
            game->addSplatAtXY(p.x + worldLength(mx + random01() * 300 - 150), p.y + worldLength(my + random01() * 300 - 150),
                               0.27f, 0, 0);
        }
    }
}

}  // namespace as2
