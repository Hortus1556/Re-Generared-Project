#include "as2/tools/Battery.h"

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/Dove.h"
#include "as2/injuries/Squid.h"

namespace as2 {

namespace {

int missionId() { return Connector::sharedConnector().current_mission_id; }

// A burn (or mission 10's kind) where the finger is, the player's doing.
void burnAtFinger(int id) {
    Injury* b = new Injury;
    b->mID = id;
    b->x = static_cast<int>(gameState->mouseX);
    b->y = static_cast<int>(gameState->mouseY);
    b->rotation = 0;
    b->scale = 1;
    gameState->addInjury(b, true, false);
    releaseRef(b);
}

}  // namespace

Battery::Battery(ToolData* d) : ITool(d) {
    getSingleInjury = false;
    setToolLevel();
}

void Battery::setToolLevel() {
    level = SaveDetails::GetToolLevel("Battery");
    if (level >= 1 && level <= 3) restartAmount = level * 10;
}

void Battery::wrongUsage() {
    // The patient shocked: "wrong", a burn and the heart stopped.
    ITool::wrongUsage();
    gameState->createPopup("wrong", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), nullptr);
    int mission = missionId();
    if (mission == 9) {
        burnAtFinger(0x59);
    } else if (mission != 19) {
        gameState->inShock = true;
        gameState->inShockTimer = 0;
        burnAtFinger(0x1c);
    }
    turnOff(true);
}

void Battery::turnOn() {
    if (turnedOn) return;
    int mission = missionId();
    if (mission == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return;
    ITool::turnOn();
    MyOpenAL::sharedMyOpenAL().forcePlaySFXWithKey("BATTERY");
    if (mission == 9) {
        wrongUsage();
        return;
    }
    if (mission != 19) {
        gameState->showBatteryAnimAtXY(static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY));
        Array<InjuryGraphic>& under = gameState->currentInjuries();
        if (under.count() != 0 && under[0]->anything) under[0]->interact();
        return;
    }
    // The graveyard: what it is for is moved on at once.
    Array<InjuryGraphic>& under = game->findCorrectInjury(getSingleInjury);
    if (under.count() == 0) {
        wrongUsage();
        return;
    }
    for (int n = 0; n < under.count(); ++n) under[n]->updateInjury();
}

void Battery::render(int) {
    Array<InjuryGraphic>& under = game->findCorrectInjury(getSingleInjury);
    if (under.count() == 0) {
        if (gameState->inShock) {
            // The heart restarted: "revived", at no more than the battery's
            // restart amount (a burn where it touched).
            burnAtFinger(0x1c);
            gameState->createPopup("revived", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY),
                                   nullptr);
            gameState->inShock = false;
            gameState->inShockTimer = 300;
            if (static_cast<float>(restartAmount) < gameState->health)
                gameState->health = static_cast<float>(restartAmount);
            turnOff(true);
            return;
        }
        // A hidden centipede shocked out; else the patient shocked.
        bool centipede = false;
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isKindOf("Centipede") && i->hidden) {
                centipede = true;
                i->interact();
                break;
            }
        if (missionId() == 15 && centipede) {
            gameState->inShock = true;
            gameState->inShockTimer = 0;
            turnOff(false);
            burnAtFinger(0x1c);
        } else {
            wrongUsage();
        }
        return;
    }
    for (int n = 0; n < under.count(); ++n) {
        InjuryGraphic* i = under[n];
        if (i->isKindOf("BrainTransplant")) {
            // Only with the old brain out (the heart running down): the
            // heart restarted, at the battery's amount or more.
            if (!gameState->brainOut()) {
                wrongUsage();
                return;
            }
            // (Too late: the heart stopped for good.)
            if (gameState->heartStopped) return;
            gameState->restoreBrain(static_cast<float>(restartAmount));
            i->updateInjury();
            // (Off now: held on, the next frame found nothing under it and
            // shocked the patient it had just revived.)
            turnOff(true);
            return;
        }
        if (auto* dove = dynamic_cast<Dove*>(i)) {
            // Stunned already: nothing; flown up to its shadow, the patient
            // is hit.
            if (dove->invincible && !dove->showShadow()) return;
            if (dove->invincible && dove->showShadow()) {
                wrongUsage();
                return;
            }
        } else if (i->invincible) {
            return;
        }
        // A squid: only its mouth, while it sucks.
        HitBox* boxes = i->hitboxes;
        int count = i->hitBoxCount;
        if (auto* squid = dynamic_cast<Squid*>(i)) {
            boxes = squid->mouthBox();
            count = 1;
        }
        if (boxes && game->performHitTestOnHitBoxes(boxes, count)) {
            i->interact();
            turnOff(true);
            return;
        }
    }
}

}  // namespace as2
