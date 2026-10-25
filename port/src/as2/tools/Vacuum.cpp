#include "as2/tools/Vacuum.h"

#include <cmath>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/InjuryGraphic.h"
#include "as2/injuries/Splatter.h"

namespace as2 {

Vacuum::Vacuum(ToolData* d) : ITool(d) {
    getSingleInjury = false;
    setToolLevel();
}

void Vacuum::setToolLevel() {
    level = SaveDetails::GetToolLevel("Vacuum");
    if (level < 1 || level > 3) return;
    static const int heal[3] = {1, 2, 3}, buffer[3] = {20, 50, 80}, pull[3] = {250, 350, 450},
                     reach[3] = {75, 150, 300};
    healAmount = heal[level - 1];
    hitBuffer = buffer[level - 1];
    suction = pull[level - 1];
    radius = reach[level - 1];
}

void Vacuum::turnOn() {
    if (turnedOn) return;
    if (Connector::sharedConnector().current_mission_id == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return;
    ITool::turnOn();
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    if (under.count() != 0 && under[0]->anything) under[0]->interact();
}

void Vacuum::render(int) {
    Point at{gameState->mouseX, gameState->mouseY};
    if (maxLowEffectTimer <= ++lowEffectTimer) {
        lowEffectTimer = 0;
        if (Connector::sharedConnector().current_mission_id == 9)
            game->addEffect("BloodEffect", static_cast<int>(at.x), static_cast<int>(at.y));
    }
    game->findCorrectInjury(getSingleInjury);
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("VACUUM");
    // The injuries it treats. A hit on any stops the sucking this frame.
    // (The original's blood drops here, GameState addEffect, drew nothing.)
    bool hit = false;
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    for (int n = 0; n < under.count(); ++n) {
        InjuryGraphic* i = under[n];
        retainRef(i);
        if (!i->anything && !i->isKindOf("Fire") && !i->isKindOf("BloodFire")) {
            // Its boxes under the nozzle lose the heal amount; all of them
            // healed moves it on.
            bool allDone = true;
            for (int b = 0; b < i->hitBoxCount; ++b) {
                HitBox& box = i->hitboxes[b];
                if (game->performHitTestOnHitBoxes(&box, 1)) {
                    hit = true;
                    box.health -= static_cast<float>(healAmount);
                    i->worked = true;
                    if (box.health <= 0) {
                        box.active = false;
                        if (box.sprite) SceneManager::sharedSceneManagerInstance().removeShape(box.sprite->currentShape());
                    }
                }
                if (box.active) allDone = false;
            }
            if (allDone) {
                MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION3");
                i->updateInjury();
                i->trace("treated");
            }
        } else {
            // Within reach of it (while it holds any): one less to suck.
            float dx = static_cast<float>(i->mInjury->x) - at.x, dy = static_cast<float>(i->mInjury->y) - at.y;
            if (std::sqrt(dx * dx + dy * dy) <= static_cast<float>(hitBuffer) && bloodHeld > 0) {
                hit = true;
                if (--i->health < 1) {
                    i->updateInjury();
                    i->trace("treated");
                }
            }
        }
        releaseRef(i);
    }
    if (hit) return;
    // The blood (not dirt) and the splatters' landed drops.
    for (int n = 0; n < gameState->_blood.count(); ++n) {
        Splatter* s = dynamic_cast<Splatter*>(gameState->_blood[n]);
        if (s && !s->isKindOf("Dirt")) bloodHeld += 10 * s->suck(at, nozzleRadius, radius, suction);
    }
    static const char* const kinds[] = {"PoisonSplatter", "YellowSplatter", "PeanutSplatter", "FatSplatter",
                                        "DoveSplatter",   "InkSplatter",    "SaltSplatter"};
    for (int n = 0; n < gameState->_injuries.count(); ++n) {
        Splatter* s = dynamic_cast<Splatter*>(gameState->_injuries[n]);
        if (!s) continue;
        for (const char* k : kinds)
            if (s->isKindOf(k)) {
                bloodHeld += 10 * s->suck(at, nozzleRadius, radius, suction);
                break;
            }
    }
}

}  // namespace as2
