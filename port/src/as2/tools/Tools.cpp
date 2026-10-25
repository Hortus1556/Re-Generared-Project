#include "as2/tools/Tools.h"

#include <cstdlib>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/Graveyard.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

namespace {

int missionId() { return Connector::sharedConnector().current_mission_id; }
int fingerX() { return static_cast<int>(gameState->mouseX); }
int fingerY() { return static_cast<int>(gameState->mouseY); }
// random() % 100 / 100 of a turn.
int randomTurn() { return static_cast<int>(static_cast<float>(std::rand() % 100) / 100.0f * 360.0f); }

// The graveyard (mission 20): the tool held on the risen patient.
void holdOnBleed(Bleed::Part part) {
    for (InjuryGraphic* i : game->findCorrectInjury(false))
        if (auto* b = dynamic_cast<Bleed*>(i)) b->held(part);
}

// The wrong use's "wrong" and the harm it does (none in shock or in the
// graveyard, where nothing hurts).
void wrongPopupAndHarm(float damage) {
    gameState->createPopup("wrong", fingerX(), fingerY(), nullptr);
    if (!gameState->inShock && missionId() != 19) gameState->health = gameState->health - damage;
}

}  // namespace

// --------------------------------------------------------------------- Cutter

Cutter::Cutter(ToolData* d) : ITool(d) {
    getSingleInjury = true;
    maxEffectTimer = 8;
    maxBloodTimer = 8;
    maxLowEffectTimer = maxEffectTimer / 3;
    bloodVel = 9;
    setToolLevel();
    damage = 3;
    effectTimer = 0;
}

void Cutter::setToolLevel() {
    readLevel("Cutter");
    if (level >= 1 && level <= 3) healAmount = level;
}

void Cutter::turnOn() { beginTurnOn(); }

void Cutter::render(int frame) {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("PIZZA-CUTTER");
    ITool::render(frame);
    if (maxBloodTimer <= ++bloodTimer) {
        bloodTimer = 0;
        addBloodAtFinger(-1);
    }
    // Fat and what is under the skin open at once.
    treatCurrentInjuries(true, [](InjuryGraphic* i) {
        return i->isKindOf("Fat") || i->isKindOf("UnderSurfaceExtraction") ||
               i->isKindOf("UnderSurfaceExtractionClown") || i->isKindOf("UnderSurfaceExtractionSilver");
    });
}

void Cutter::wrongUsage() {
    ITool::wrongUsage();
    wrongPopupAndHarm(damage);
    // A small cut where it went (mission 10's bleeding kind).
    if (missionId() != 19) addInjuryAtFinger(missionId() == 9 ? 0x52 : 1, randomTurn());
    addBloodAtFinger(bloodVel);
}

// -------------------------------------------------------------------- Lighter

Lighter::Lighter(ToolData* d) : ITool(d) {
    getSingleInjury = false;
    maxEffectTimer = 8;
    maxBloodTimer = 8;
    maxLowEffectTimer = maxEffectTimer / 3;
    setToolLevel();
    damage = 2;
    effectTimer = 0;
}

void Lighter::setToolLevel() {
    readLevel("Lighter");
    if (level >= 1 && level <= 3) healAmount = level;
}

void Lighter::turnOn() {
    if (beginTurnOn()) MyOpenAL::sharedMyOpenAL().playSFXWithKey("LIGHTER_CLICK");
}

void Lighter::render(int frame) {
    // Flames along the stroke (one every 6 points, each when the finger
    // passed there); a still finger, as ever, one every few passes.
    int made = trail(static_cast<float>(fingerX()), static_cast<float>(fingerY()), 6, [](float x, float y, float at) {
        game->addTrailEffect("Flame", static_cast<int>(x), static_cast<int>(y), at);
    });
    if (maxLowEffectTimer <= ++lowEffectTimer) {
        lowEffectTimer = 0;
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("LIGHTER");
        if (!made) game->addEffect("Flame", fingerX(), fingerY());
    }
    if (maxEffectTimer <= ++effectTimer) {
        // The spray of flame the original asked for here made nothing.
        effectTimer = 0;
        game->addEffect("flameSpray", fingerX(), fingerX());
    }
    if (maxLowEffectTimer - 1 <= lowEffectTimer && missionId() == 9) game->addEffect("SmokeEffect", fingerX(), fingerY());
    ITool::render(frame);
    if (missionId() == 9 && gameState->currentInjuries().count() == 0) {
        // Mission 10: nothing under it counts again towards a wrong use.
        if (maxWrongUsageTimer <= ++wrongUsageTimer) wrongUsage();
        return;
    }
    if (missionId() == 19) {
        holdOnBleed(Bleed::kBody);
        return;
    }
    treatCurrentInjuries(false);
}

void Lighter::wrongUsage() {
    ITool::wrongUsage();
    wrongPopupAndHarm(damage);
    // A burn where it went (mission 10's: a fire that bleeds).
    if (missionId() != 19) addInjuryAtFinger(missionId() == 9 ? 0x59 : 0x1c, 0);
}

// ---------------------------------------------------------------------- Cream

Cream::Cream(ToolData* d) : ITool(d) {
    getSingleInjury = false;
    maxEffectTimer = 8;
    maxBloodTimer = 8;
    maxLowEffectTimer = maxEffectTimer / 3;
    setToolLevel();
    effectTimer = 0;
}

void Cream::setToolLevel() {
    readLevel("Cream");
    if (level == 1) damage = 0.1f;
    else if (level == 2) damage = 0.2f;
    else if (level == 3) damage = 0.3f;
    if (level >= 1 && level <= 3) healAmount = level;
}

void Cream::turnOn() { beginTurnOn(); }

void Cream::render(int) {
    // The graveyard's cream is a dark mud.
    bool grave = missionId() == 19;
    auto puff = [grave](const char* name, float x, float y) {
        if (grave) game->addEffect(name, static_cast<int>(x), static_cast<int>(y), 0.8f, 0.4f, 0);
        else game->addEffect(name, static_cast<int>(x), static_cast<int>(y));
    };
    auto trailPuff = [grave](const char* name, float x, float y, float at) {
        if (grave) game->addTrailEffect(name, static_cast<int>(x), static_cast<int>(y), 0.8f, 0.4f, 0, at);
        else game->addTrailEffect(name, static_cast<int>(x), static_cast<int>(y), at);
    };
    // Cream along the stroke (a puff every 5 points, a spray every third,
    // each when the finger passed there); a still finger, as ever, every
    // few passes.
    int made = trail(static_cast<float>(fingerX()), static_cast<float>(fingerY()), 5, [&](float x, float y, float at) {
        trailPuff("CreamEffect", x, y, at);
        if (++trailSprays % 3 == 0) trailPuff("CreamSpray", x, y, at);
    });
    if (maxLowEffectTimer <= ++lowEffectTimer) {
        lowEffectTimer = 0;
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("CREAM");
        if (!made) puff("CreamEffect", static_cast<float>(fingerX()), static_cast<float>(fingerY()));
    }
    if (maxEffectTimer <= ++effectTimer) {
        effectTimer = 0;
        if (!made) puff("CreamSpray", static_cast<float>(fingerX()), static_cast<float>(fingerY()));
    }
    if (maxLowEffectTimer - 1 <= lowEffectTimer && missionId() == 9) {
        // Mission 10: smoke too.
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("LIGHTER");
        game->addEffect("SmokeEffect", fingerX(), fingerY());
    }
    // Mission 10's cream only ever counts towards a wrong use.
    if (missionId() == 9) {
        if (maxWrongUsageTimer <= ++wrongUsageTimer) wrongUsage();
        return;
    }
    if (grave) {
        holdOnBleed(Bleed::kMouth);
        return;
    }
    // The cream never counts a wrong use elsewhere: it finds and treats.
    game->findCorrectInjury(getSingleInjury);
    treatCurrentInjuries(false);
}

void Cream::wrongUsage() {
    ITool::wrongUsage();
    gameState->createPopup("wrong", fingerX(), fingerY(), nullptr);
    // A burn where it went (the bleeding kind, BloodBurn).
    addInjuryAtFinger(0x54, 0);
}

}  // namespace as2
