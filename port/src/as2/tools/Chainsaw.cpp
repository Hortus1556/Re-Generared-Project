#include "as2/tools/Chainsaw.h"

#include <cmath>
#include <cstdlib>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/Squid.h"
#include "as2/injuries/Transplant.h"

namespace as2 {

namespace {

int missionId() { return Connector::sharedConnector().current_mission_id; }

}  // namespace

Chainsaw::Chainsaw(ToolData* d) : ITool(d) {
    getSingleInjury = true;
    maxEffectTimer = 8;
    maxBloodTimer = 8;
    maxLowEffectTimer = maxEffectTimer / 3;
    bloodVel = 14;
    setToolLevel();
    effectTimer = 0;
}

void Chainsaw::setToolLevel() {
    level = SaveDetails::GetToolLevel("Chainsaw");
    if (level < 1 || level > 3) return;
    static const float harm[3] = {5, 3, 1};
    damage = harm[level - 1];
    healAmount = level;
}

void Chainsaw::turnOn() {
    if (turnedOn) return;
    if (missionId() == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return;
    ITool::turnOn();
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    if (under.count() != 0 && under[0]->anything) {
        under[0]->interact();
        return;
    }
    effectTimer = maxEffectTimer - 1;
    if (level == 1) cursorHalfSize = 1;
    else if (level == 2) cursorHalfSize = 4;
    else if (level == 3) cursorHalfSize = 8;
}

bool Chainsaw::saw(HitBox* boxes, int count, InjuryGraphic* injury, int part) {
    bool hit = false, allSpent = true;
    for (int k = 0; k < count; ++k) {
        HitBox& b = boxes[k];
        // The finger turned into the box's frame (about its guide).
        const Shape* s = b.sprite->currentShape();
        float dx = gameState->mouseX - s->pos.x, dy = gameState->mouseY - s->pos.y;
        double a = static_cast<float>(-s->rotation * 3.141592653589793 / 180.0);
        double c = std::cos(a), sn = std::sin(a);
        float px = static_cast<float>(dx * c - dy * sn) + s->pos.x;
        float py = s->pos.y + static_cast<float>(c * dy + dx * sn);
        float halfW = b.rect.size.width * 0.5f, halfH = b.rect.size.height * 0.5f;
        float reach = static_cast<float>(cursorHalfSize);
        int ox = std::abs(static_cast<int>(px - (halfW + b.rect.origin.x)));
        int oy = std::abs(static_cast<int>(py - (b.rect.origin.y + halfH)));
        if (static_cast<float>(ox) < halfW + reach && static_cast<float>(oy) < halfH + reach) {
            injury->interact();
            if (maxEffectTimer <= ++effectTimer) {
                effectTimer = 0;
                injury->createEffect(1, bloodVel);
            }
            b.health -= static_cast<float>(healAmount);
            injury->worked = true;
            if (b.health <= 0) {
                b.active = false;
                SceneManager::sharedSceneManagerInstance().removeShape(b.sprite->currentShape());
            }
            hit = true;
        }
        if (b.active) allSpent = false;
    }
    if (!allSpent) return hit;
    // All spent: a transplant's part is cut; the injury moves on.
    retainRef(injury);
    if (auto* t = dynamic_cast<Transplant*>(injury)) t->removeHit(part);
    else if (auto* s = dynamic_cast<Squid*>(injury)) s->removeHit(part);
    // (ThighTransplant2's piece: with it.)
    injury->updateInjury();
    injury->trace("treated");
    releaseRef(injury);
    return hit;
}

void Chainsaw::render(int frame) {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("CHAINSAW");
    ITool::render(frame);
    gameState->setShakeAsHigh(1);
    if (maxBloodTimer <= ++bloodTimer) {
        bloodTimer = 0;
        for (int n = 0; n < 3; ++n) addBloodAtFinger(bloodVel);
    }
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    bool hit = false;
    for (int n = 0; n < under.count(); ++n) {
        InjuryGraphic* i = under[n];
        if (i->invincible) return;
        if (auto* t = dynamic_cast<Transplant*>(i)) {
            // Each part not yet sawn through.
            for (size_t p = 0; p < t->parts.size(); ++p) {
                Transplant::Part& part = t->parts[p];
                if (!part.complete && part.hits && saw(part.hits, part.count, t, static_cast<int>(p))) hit = true;
            }
        } else if (auto* squid = dynamic_cast<Squid*>(i)) {
            // A squid, stunned: one of its tentacles (each three boxes), all
            // of them off, its body.
            if (!squid->allTentaclesRemoved()) {
                for (int g = 1; g < 6; ++g) {
                    HitBox* boxes = squid->groupBoxes(g);
                    if (boxes && saw(boxes, squid->groupCount(g), squid, g)) {
                        hit = true;
                        break;
                    }
                }
            } else if (HitBox* body = squid->groupBoxes(6)) {
                if (saw(body, squid->groupCount(6), squid, 6)) return;
            }
        } else if (i->hitboxes) {
            // (IncisionCoffin: with it.)
            if (saw(i->hitboxes, i->hitBoxCount, i, 0)) hit = true;
        }
    }
    if (hit) return;
    // Running over nothing.
    if (!gameState->inShock && missionId() != 19)
        gameState->health = gameState->health + damage / -70.0f;
}

void Chainsaw::wrongUsage() {
    ITool::wrongUsage();
    int mission = missionId();
    if (!gameState->inShock && mission != 19)
        gameState->health = gameState->health - damage;
    // A large cut where it went (mission 10's bleeding kind).
    if (mission != 19)
        addInjuryAtFinger(mission == 9 ? 0x53 : 3, static_cast<int>(static_cast<float>(std::rand() % 100) / 100.0f * 360.0f));
    addBloodAtFinger(bloodVel);
}

}  // namespace as2
