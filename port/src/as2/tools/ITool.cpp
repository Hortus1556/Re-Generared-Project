#include "as2/tools/ITool.h"

#include <cmath>
#include <cstdlib>
#include <map>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/InjuryGraphic.h"
#include "platform/Platform.h"

namespace as2 {

namespace {
std::map<std::string, std::function<ITool*(ToolData*)>>& classes() {
    static std::map<std::string, std::function<ITool*(ToolData*)>> c;
    return c;
}
}  // namespace

void registerToolClass(const std::string& name, std::function<ITool*(ToolData*)> make) {
    classes()[name] = std::move(make);
}

ITool* ITool::create(ToolData* data) {
    if (!data) return nullptr;
    auto it = classes().find(data->type);
    if (it == classes().end()) {
        platform::debugTrace("tool: class %s is not ported", data->type.c_str());
        return nullptr;
    }
    return it->second(data);
}

int ITool::trail(float x, float y, float spacing, const std::function<void(float, float, float)>& emit) {
    double now = platform::passTime();
    bool going = trailPass >= 0 && now - trailPass < platform::passSeconds() * 1.5;
    trailPass = now;
    if (!going || spacing <= 0) {
        // A new stroke: it starts here.
        trailX = x;
        trailY = y;
        trailCarry = 0;
        return 0;
    }
    float dx = x - trailX, dy = y - trailY;
    float len = std::sqrt(dx * dx + dy * dy);
    int made = 0;
    float s = spacing - trailCarry;
    for (; s <= len; s += spacing, ++made) emit(trailX + dx * s / len, trailY + dy * s / len, s / len);
    trailCarry = made ? len - (s - spacing) : trailCarry + len;
    trailX = x;
    trailY = y;
    return made;
}

bool ITool::beginTurnOn() {
    if (turnedOn) return false;
    if (Connector::sharedConnector().current_mission_id == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return false;
    turnedOn = true;
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    if (under.count() != 0 && under[0]->anything) {
        under[0]->interact();
        return false;
    }
    if (level == 1) cursorHalfSize = 1;
    else if (level == 2) cursorHalfSize = 4;
    else if (level == 3) cursorHalfSize = 8;
    return true;
}

void ITool::treatCurrentInjuries(bool updateToolEffect, bool (*instant)(InjuryGraphic*)) {
    // The list may lose the injury being treated (healed: removed); the
    // original went on by index all the same.
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    for (int n = 0; n < under.count(); ++n) {
        InjuryGraphic* i = under[n];
        if (i->invincible) return;
        retainRef(i);
        if (game->getAllHealedInjury(i, this, instant && instant(i), updateToolEffect, false)) {
            i->updateInjury();
            i->trace("treated");
        }
        releaseRef(i);
    }
}

void ITool::addInjuryAtFinger(int id, int rotation) {
    Injury* in = new Injury;
    in->mID = id;
    in->x = static_cast<int>(gameState->mouseX);
    in->y = static_cast<int>(gameState->mouseY);
    in->rotation = rotation;
    in->scale = 1;
    gameState->addInjury(in, true, false);
    releaseRef(in);
}

void ITool::addBloodAtFinger(int velocity) {
    Injury* b = new Injury;
    b->mID = 0;
    b->y = static_cast<int>(gameState->mouseY);
    b->x = static_cast<int>(gameState->mouseX);
    b->rotation = 0;
    b->scale = 1;
    gameState->addBlood(b, velocity);
    releaseRef(b);
}

void ITool::readLevel(const char* saveName) {
    level = SaveDetails::GetToolLevel(saveName);
    if (level == 1) hitBuffer = 20;
    else if (level == 2) hitBuffer = 50;
    else if (level == 3) hitBuffer = 80;
}

void ITool::render(int) {
    // Dragged off the bottom of the play area: let go.
    if (gameState->mouseY > 460) turnOff(true);
    // setCurrentInjuries: the game's list itself (GameState::currentInjuries).
    game->findCorrectInjury(getSingleInjury);
    if (gameState->currentInjuries().count() == 0) {
        if (maxWrongUsageTimer <= ++wrongUsageTimer) wrongUsage();
    } else {
        wrongUsageTimer = 0;
    }
}

void ITool::wrongUsage() {
    scream();
    wrongUsageTimer = 0;
}

void ITool::scream() {
    int mission = Connector::sharedConnector().current_mission_id;
    if (mission == 19 || gameState->inShock) return;
    // The patient's kind of scream, and how many of it there are.
    const char* kind = "MALE_SCREAM%i";
    int count = 4;
    switch (mission) {
        case 6: kind = "CLOWN_SCREAM%i"; break;
        case 4: kind = "PLEASURE_SCREAM%i"; count = 3; break;
        case 5: case 13: case 21: kind = "FEMALE_SCREAM%i"; break;
        case 11: kind = "PIG_SCREAM%i"; count = 3; break;
        case 23: kind = "LIZARD_SCREAM%i"; count = 3; break;
        case 25: kind = "PHANT_SCREAM%i"; count = 3; break;
        case 28: kind = "Brain_scream%i"; break;
        default: break;
    }
    int n = static_cast<int>(static_cast<float>(std::rand() % 100) / 100.0f * count + 1.0f);
    char key[32];
    std::snprintf(key, sizeof key, kind, n);
    MyOpenAL::sharedMyOpenAL().playSFXWithKey(key);
}

}  // namespace as2
