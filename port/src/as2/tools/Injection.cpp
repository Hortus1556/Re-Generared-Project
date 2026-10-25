#include "as2/tools/Injection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/Bat.h"
#include "as2/injuries/Centipede.h"
#include "as2/menus/FlashStage.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

int missionId() { return Connector::sharedConnector().current_mission_id; }
VectorArt& art() { return flashstage::art("flash"); }
constexpr unsigned long long kOwner = 0x494E4A0000000000ull;  // "INJ"
// The Flash stage's edges the syringe turns away from (Injection.as).
constexpr float kMinX = 25, kMaxX = 575, kMinY = 200, kMaxY = 600;

// The finger in screen points (the touch's: twice the unscaled point).
Point finger() { return {gameState->unscaledMouseX * 2, gameState->unscaledMouseY * 2}; }

VectorTransform stageAt(float x, float y, float turn) { return flashstage::at(x, y, turn); }

}  // namespace

Injection::Injection(ToolData* d) : ITool(d) {
    getSingleInjury = true;
    damage = 2;
    setToolLevel();
    overlay.layer = 8;
    overlay.art = [this](const float*) {
        if (syringe >= 0) art().draw(syringe, frame, shownFor, stageAt(x, y, turn), kOwner);
    };
}

Injection::~Injection() {
    showOverlay(false);
    releaseRef(injury);
}

void Injection::showOverlay(bool shown) {
    SceneManager& scene = SceneManager::sharedSceneManagerInstance();
    if (shown && !overlay.inScene) scene.insertShape(&overlay);
    else if (!shown && overlay.inScene) scene.removeShape(&overlay);
}

void Injection::setToolLevel() {
    level = SaveDetails::GetToolLevel("Injection");
    // Each level a straighter syringe and a bigger boost.
    boost = level == 2 ? 13 : level == 3 ? 16 : 10;
}

bool Injection::on(const char* part, Point p) {
    return art().hitTestPart(syringe, frame, shownFor, shownFor, stageAt(x, y, turn), nullptr, part, p.x, p.y, true);
}

void Injection::hurt() {
    gameState->createPopup("wrong", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), nullptr);
    if (gameState->inShock) return;
    int mission = missionId();
    if (mission != 19) {
        // (The graveyard: no harm; only the blood.)
        gameState->health = gameState->health - damage;
        addInjuryAtFinger(mission == 9 ? 0x52 : 1,
                          static_cast<int>(static_cast<float>(std::rand() % 100) / 100.0f * 360.0f));
    }
    addBloodAtFinger(5);
}

void Injection::turnOn() {
    if (turnedOn) return;
    // (The President's centipede attacking: nothing.)
    if (missionId() == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return;
    ITool::turnOn();
    Array<InjuryGraphic>& under = game->findCorrectInjury(getSingleInjury);
    if (under.count() != 0 && under[0]->anything) {
        under[0]->interact();
        return;
    }
    // The syringe, its needle at the finger, turned away from the edges.
    Point f = flashstage::toStage(finger());
    x = f.x;
    y = f.y;
    if (y <= kMinY) turn = x <= kMinX ? 135.0f : x >= kMaxX ? 225.0f : 180.0f;
    else if (y >= kMaxY) turn = x <= kMinX ? 45.0f : x >= kMaxX ? 315.0f : 0.0f;
    else turn = x <= kMinX ? 45.0f : x >= kMaxX ? 315.0f : 0.0f;
    syringe = art().symbol("injectionBar" + std::to_string(level >= 1 && level <= 3 ? level : 1));
    // One of its five mazes (the vampire's red). (Flash's random(1) + 1
    // always picked the first of the five the artists drew.)
    std::string maze = (missionId() == 9 ? "vlad" : "bar") + std::to_string(std::rand() % 5 + 1);
    frame = syringe >= 0 ? std::max(1, art().frameOf(syringe, maze)) : 1;
    shownFor = 0;
    showOverlay(syringe >= 0);
    releaseRef(injury);
    injury = under.count() != 0 ? retainRef(under[0]) : nullptr;
    // A big centipede: which of its eyes.
    eyeHit = -1;
    if (injury && dynamic_cast<BigCentipede*>(injury))
        for (int k = 0; k < injury->hitBoxCount; ++k)
            if (game->performHitTestOnHitBoxes(&injury->hitboxes[k], 1)) eyeHit = k;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("INJECTION_IN");
    if (platform::traceEnabled()) {
        platform::debugTrace("injection shown for %s", injury ? injury->mInjuryData->type.c_str() : "the heart");
        tracePath();
    }
}

void Injection::turnOff(bool release) {
    ITool::turnOff(release);
    if (syringe >= 0) {
        syringe = -1;
        platform::debugTrace("injection closed");
    }
    showOverlay(false);
    releaseRef(injury);
    injury = nullptr;
}

void Injection::render(int) {
    if (!turnedOn || syringe < 0) return;
    ++shownFor;
    // A bat only while it flies.
    if (auto* bat = dynamic_cast<Bat*>(injury); bat && bat->state() != Bat::kFlying) {
        wrongUsage();
        gameState->createPopup("wrong", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), nullptr);
        turnOff(true);
        return;
    }
    Point p = finger();
    if (!on("hit", p)) {
        // On a wall, or off the path: a wrong use.
        if (!on("wall", p) && on("floor", p)) return;
        wrongUsage();
        hurt();
        turnOff(true);
        return;
    }
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("INJECTION_OUT");
    int mission = missionId();
    if (!injury) {
        // Into the patient: the heart gains.
        if (!gameState->inShock && !gameState->heartStopped && !(mission == 29 && gameState->currentStage == 4) &&
            mission != 19) {
            const char* popup = "correct";
            if (mission == 9) {
                MyOpenAL::sharedMyOpenAL().playSFXWithKey("MMM");
                popup = "yummy";
            }
            gameState->createPopup(popup, static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY),
                                   nullptr);
            gameState->health = gameState->health + static_cast<float>(boost);
        }
    } else if (auto* big = dynamic_cast<BigCentipede*>(injury)) {
        big->updateEye(eyeHit);
        big->trace("treated");
    } else if (!injury->isKindOf("BrainTransplant")) {
        injury->updateInjury();
        injury->trace("treated");
    }
    turnOff(true);
}

void Injection::tracePath() {
    // A grid over the syringe, 4 points a cell; a breadth-first search from
    // the needle through the path to the ring; every third cell traced.
    flash::Rect b = art().partBounds(syringe, frame, 0, 0, stageAt(x, y, turn), nullptr, "");  // pixels
    if (b.empty()) return;
    const float step = 4;
    int w = static_cast<int>((b.xMax - b.xMin) / step) + 1, h = static_cast<int>((b.yMax - b.yMin) / step) + 1;
    auto at = [&](int c) { return Point{b.xMin + static_cast<float>(c % w) * step, b.yMin + static_cast<float>(c / w) * step}; };
    std::vector<signed char> kind(static_cast<size_t>(w * h), -1);  // -1 unknown, 0 blocked, 1 path, 2 ring
    auto kindOf = [&](int c) {
        signed char& k = kind[static_cast<size_t>(c)];
        if (k < 0) {
            Point p = at(c);
            k = on("hit", p) ? 2 : (on("floor", p) && !on("wall", p)) ? 1 : 0;
        }
        return k;
    };
    Point f = finger();
    int start = std::clamp(static_cast<int>((f.y - b.yMin) / step), 0, h - 1) * w +
                std::clamp(static_cast<int>((f.x - b.xMin) / step), 0, w - 1);
    std::vector<int> from(static_cast<size_t>(w * h), -2);
    std::deque<int> open{start};
    from[static_cast<size_t>(start)] = -1;
    int goal = -1;
    while (!open.empty() && goal < 0) {
        int c = open.front();
        open.pop_front();
        int cx = c % w, cy = c / w;
        const int nx[4] = {cx + 1, cx - 1, cx, cx}, ny[4] = {cy, cy, cy + 1, cy - 1};
        for (int k = 0; k < 4; ++k) {
            if (nx[k] < 0 || ny[k] < 0 || nx[k] >= w || ny[k] >= h) continue;
            int n = ny[k] * w + nx[k];
            if (from[static_cast<size_t>(n)] != -2) continue;
            signed char kn = kindOf(n);
            if (kn == 0) continue;
            from[static_cast<size_t>(n)] = c;
            if (kn == 2) {
                goal = n;
                break;
            }
            open.push_back(n);
        }
    }
    if (goal < 0) {
        platform::debugTrace("injection path none");
        return;
    }
    std::vector<int> cells;
    for (int c = goal; c >= 0; c = from[static_cast<size_t>(c)]) cells.push_back(c);
    std::string line = "injection path";
    for (size_t i = cells.size(); i-- > 0;) {
        if (i % 3 != 0 && i != 0) continue;
        Point p = at(cells[i]);
        char buf[32];
        std::snprintf(buf, sizeof buf, " %.0f,%.0f", p.x, p.y);
        line += buf;
    }
    platform::debugTrace("%s", line.c_str());
}

}  // namespace as2
