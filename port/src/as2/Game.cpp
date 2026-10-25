#include "as2/Game.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/Loader.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/SceneManager.h"
#include "as2/effects/ToolEffect.h"
#include "as2/injuries/InjuryGraphic.h"
#include "as2/injuries/Bat.h"
#include "as2/injuries/Centipede.h"
#include "as2/injuries/Squid.h"
#include "as2/injuries/Transplant.h"
#include "as2/tools/ITool.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
int missionId() { return Connector::sharedConnector().current_mission_id; }

// The finger turned into the frame of a guide sprite (by -its turn about
// its position).
Point toBox(const Shape* s) {
    float dx = gameState->mouseX - s->pos.x, dy = gameState->mouseY - s->pos.y;
    double a = static_cast<float>(-s->rotation * 3.141592653589793 / 180.0);
    double c = std::cos(a), sn = std::sin(a);
    return {static_cast<float>(dx * c - dy * sn) + s->pos.x, s->pos.y + static_cast<float>(c * dy + dx * sn)};
}

}  // namespace

Game* Game::initWithMissionData(Mission* m) {
    Game* g = new Game;
    game = g;
    // The torch's light may go this far from the middle (the screen less
    // its 256-point square, halved).
    Size screen = gameController->ActualScreenSize;
    g->torchXBound = static_cast<float>(static_cast<int>((screen.width + gameController->GfxScaleFactor * -256) * 0.5f));
    g->torchYBound = static_cast<float>(static_cast<int>((screen.height + gameController->GfxScaleFactor * -256) * 0.5f));
    g->mission_data = retainRef(m);
    g->mission_stage = MissionStage::initFromDictionary(m->data[0]);
    int id = missionId();
    if (g->mission_stage->torch || id == 21 || id == 26 || id == 28 || id == 29) gameController->resetShake();
    if (g->mission_stage->torch) {
        g->torch = true;
        g->sprTorch[0] = Sprite::initWithImageNamed("torch", 256, 256);
        g->sprTorch[0]->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* t = g->sprTorch[0]->currentShape();
        t->layer = 7;
        t->offset = {-128, -128};
        t->pos = {240, 160};
        scene().addShape(t);
        for (int i = 1; i < 5; ++i) {
            g->sprTorch[i] = Sprite::initWithImageNamed("torch2", 1, 1);
            g->sprTorch[i]->calculateTextureCoordsAtOffsetX(0, 0);
            Shape* b = g->sprTorch[i]->currentShape();
            b->layer = 7;
            b->offset = {0, 0};
            b->pos = {0, 0};
            scene().addShape(b);
        }
    }
    new GameState;  // sets gameState
    return g;
}

Game::~Game() {
    mFindCorrectInjury.removeAllObjects();
    mFindHiddenInjury.removeAllObjects();
    effectsList.removeAllObjects();
    for (Sprite*& s : sprTorch) {
        releaseRef(s);
        s = nullptr;
    }
    releaseRef(mission_stage);
    releaseRef(mission_data);
    if (game == this) game = nullptr;
}

void Game::updateInjuries() {
    if (!gameState) return;
    for (int i = 0; i < gameState->_injuries.count(); ++i) gameState->_injuries[i]->render();
}

void Game::missionQuit() {
    MenuManager::sharedMenuManager().killViewController();
    Loader::sharedLoader().startQuitGame();
    mKillGame = true;
    gameState->paused = true;
}

void Game::missionFailed(bool timeUp) {
    platform::debugTrace("mission failed (%s)", timeUp ? "time" : "heart");
    // As the Flash AS2: the tool put down, the flatline, and the obituary
    // over the stopped mission (MissionFailed: replay or quit).
    if (gameState->currentTool && gameState->currentTool->isTurnedOn()) gameState->currentTool->turnOff(false);
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    al.stopSoundWithKey("ingame");
    al.playSFXWithKey("FLATLINE");
    gameController->winTime = gameState->time;
    gameController->winScore = static_cast<int>(gameState->score);
    gameController->winLevel = missionId();
    if (gameController->mLevelPack == 1) {
        // The CFO pack: as the iPad's, the mission unloaded first, then its
        // own summary (Loader).
        MenuManager::sharedMenuManager().killViewController();
        Loader::sharedLoader().startMissionFailed();
    } else {
        MenuManager::sharedMenuManager().setupNewViewController("MissionFailed", "MissionFailed");
    }
    gameState->paused = true;
    mFailed = true;
    // Bango analytics (the failure's kind) are removed.
}

void Game::missionCompleted() {
    if (gameState->currentTool && gameState->currentTool->isTurnedOn()) gameState->currentTool->turnOff(false);
    int id = missionId();
    int previous = SaveDetails::GetScoreForLevel(id + 1);
    gameState->checkTutorial(3, 0);
    // The time left adds to the score (a tenth of it, but all of it in the
    // graveyard).
    float bonus = static_cast<float>(gameState->time);
    if (id != 19) bonus = bonus / 10.0f;
    gameState->score = static_cast<long long>(bonus + static_cast<float>(gameState->score));
    // Game Center achievements are removed.
    int taken = mission_stage->time - gameState->time;
    int best = SaveDetails::GetTimeForLevel(id + 1);
    if (best == 0 || taken < best) SaveDetails::SaveTimeForLevel(taken, id + 1);
    // An upgrade is won on a first completion while upgrades are left (and
    // not in the CFO missions).
    int upgrades = 0;
    for (const char* tool : GameData::toolNames4) upgrades += SaveDetails::GetToolLevel(tool) - 1;
    gameController->winUpgradeTool = previous == 0 && upgrades < 20 && id < 20;
    if (gameState->score > previous) SaveDetails::SaveScoreForLevel(static_cast<int>(gameState->score), id + 1);
    for (ITool* t : gameState->_tools)
        if (!SaveDetails::GetToolUsedToolID(t->toolData->mId)) SaveDetails::SaveToolUsed(true, t->toolData->mId);
    gameController->winTime = gameState->time;
    gameController->winScore = static_cast<int>(gameState->score);
    gameController->winLevel = id;
    platform::debugTrace("mission won score %lld time %d", gameState->score, gameState->time);
    MenuManager::sharedMenuManager().killViewController();
    Loader::sharedLoader().startMissionComplete();
    gameState->paused = true;
    mWon = true;
    mKillGame = true;
}

bool Game::needToFade() const {
    int stage = gameState->currentStage;
    if (stage < 2 || stage > static_cast<int>(mission_data->data.size())) return false;
    MissionStage* before = MissionStage::initFromDictionary(mission_data->data[static_cast<size_t>(stage - 2)]);
    MissionStage* now = MissionStage::initFromDictionary(mission_data->data[static_cast<size_t>(stage - 1)]);
    bool fade = now->body_level != before->body_level;
    releaseRef(before);
    releaseRef(now);
    return fade;
}

bool Game::finishedMission() const {
    return static_cast<int>(mission_data->data.size()) <= gameState->currentStage - 1;
}

void Game::checkInjuries(int) {
    if (!gameState || gameState->_injuries.count() != 0) return;
    ++gameState->currentStage;
    if (finishedMission()) {
        missionCompleted();
        return;
    }
    if (needToFade()) {
        scene().MoveAllToOldLayer();
    } else {
        // removeOrgans and removeBlood (on a new background) are empty.
        gameState->removeOrgans();
        gameState->removeBlood();
    }
    gameState->setupStage(gameState->currentStage);
}

Array<InjuryGraphic>& Game::findHiddenInjury(bool single) {
    mFindHiddenInjury.removeAllObjects();
    for (InjuryGraphic* i : gameState->_injuries)
        if (i->alive && i->hidden) {
            mFindHiddenInjury.addObject(i);
            if (single) break;
        }
    return mFindHiddenInjury;
}

void Game::addSplatAtXY(float x, float y, float r, float g, float b) {
    ToolEffect* e = ToolEffect::create("SplatEffect", x, y);
    if (!e) return;
    e->setRed(r, g, b);
    effectsList.addObject(e);
    releaseRef(e);
}

void Game::addEffect(const std::string& name, int x, int y, float r, float g, float b) {
    if (name == "flameSpray" || name == "SmokeEffect") return;
    ToolEffect* e = ToolEffect::create(name, static_cast<float>(x), static_cast<float>(y));
    if (!e) return;
    e->setRed(r, g, b);
    effectsList.addObject(e);
    releaseRef(e);
}

void Game::addEffect(const std::string& name, int x, int y) {
    if (name == "flameSpray") return;
    ToolEffect* e = ToolEffect::create(name, static_cast<float>(x), static_cast<float>(y));
    if (!e) return;
    effectsList.addObject(e);
    releaseRef(e);
}

void Game::addTrailEffect(const std::string& name, int x, int y, float at) {
    ToolEffect* e = ToolEffect::create(name, static_cast<float>(x), static_cast<float>(y));
    if (!e) return;
    e->shape()->appearAt = at;
    e->makeTrail();
    effectsList.addObject(e);
    releaseRef(e);
}

void Game::addTrailEffect(const std::string& name, int x, int y, float r, float g, float b, float at) {
    ToolEffect* e = ToolEffect::create(name, static_cast<float>(x), static_cast<float>(y));
    if (!e) return;
    e->setRed(r, g, b);
    e->shape()->appearAt = at;
    e->makeTrail();
    effectsList.addObject(e);
    releaseRef(e);
}

void Game::StartGame() {
    MenuManager::sharedMenuManager().setupNewViewController("Panel", "Panel");
    gameState->setupStage(gameState->currentStage);
}

void Game::render(int) {
    if (!gameState) return;
    if (gameState->paused) {
        MyOpenAL::sharedMyOpenAL().stopSoundWithKey("EXPLOSION_FIRE");
        return;
    }
    // The shock (a battery jolt) lasts 300 frames; ending it ends the mission.
    if (gameState->inShockTimer < 300 && ++gameState->inShockTimer > 299) {
        gameState->inShockTimer = 0;
        gameState->inShock = false;
        missionFailed(false);
        return;
    }
    ++gameState->injuryTimer;
    if (--gameState->time > 0) {
        gameState->render();
        return;
    }
    gameState->time = 0;
    missionFailed(true);
}

void Game::checkRender() {
    if (!gameState) return;
    SceneManager& sm = scene();
    if (sm.mOldLayerFade > 0 && sm.mOldLayerFadeSpeed > 0) return;
    render(0);
    for (int i = 0; i < effectsList.count(); ++i)
        if (effectsList[i]->tick()) effectsList.removeObjectAtIndex(i--);
    ITool* tool = gameState->currentTool;
    if (!tool) return;
    // The injection works while off too (its plunger).
    if (!tool->isTurnedOn() && tool->toolData->type != "Injection") return;
    tool->render(6);
}

namespace {

// A stage's tool, or its list ("injection,lighter,stitch..."): does it
// take `tool`?
bool takes(const std::string& stageTool, const std::string& tool) {
    for (size_t a = 0, b; a <= stageTool.size(); a = b + 1) {
        b = stageTool.find(',', a);
        if (b == std::string::npos) b = stageTool.size();
        if (stageTool.compare(a, b - a, tool) == 0) return true;
    }
    return false;
}

}  // namespace

Array<InjuryGraphic>& Game::findCorrectInjury(bool single) {
    mFindCorrectInjury.removeAllObjects();
    const std::string& tool = gameState->currentTool->toolData->name;
    for (InjuryGraphic* i : gameState->_injuries) {
        if (!i->alive) continue;
        if (auto* t = dynamic_cast<Transplant*>(i)) {
            // A transplant: its stage's tools (a list, "chainsaw,tongs");
            // after the first stage its own boxes as any injury's.
            std::vector<std::string> tools;
            for (size_t a = 0, b; a <= i->current().tool.size(); a = b + 1) {
                b = i->current().tool.find(',', a);
                if (b == std::string::npos) b = i->current().tool.size();
                tools.push_back(i->current().tool.substr(a, b - a));
            }
            if (i->currentStage != 1 && i->hitboxes) {
                bool over = performHitTestOnHitBoxes(i->hitboxes, i->hitBoxCount);
                if (tools[0] == "anything" && over) {
                    i->anything = true;
                    mFindCorrectInjury.addObject(i);
                }
                if (over && i->current().tool == tool) {
                    mFindCorrectInjury.addObject(i);
                    break;
                }
            }
            for (const std::string& name : tools) {
                if (name != tool) continue;
                // A piece cut loose (to take) under the finger, or a line
                // still to saw.
                bool found = false;
                for (const Transplant::Part& p : t->parts)
                    if (p.complete && !p.removed && p.hits && performHitTestOnHitBoxes(p.hits, p.count)) found = true;
                for (const Transplant::Part& p : t->parts)
                    if (!p.complete && p.hits && performHitTestOnHitBoxes(p.hits, p.count)) found = true;
                if (!found) continue;
                mFindCorrectInjury.addObject(i);
                if (single) return mFindCorrectInjury;
            }
            continue;
        }
        if (auto* squid = dynamic_cast<Squid*>(i)) {
            // A squid: the battery on its mouth while it sucks; the chainsaw
            // on a tentacle (or, all off, its body) while it is stunned.
            bool over = false;
            if (tool == "battery") {
                over = squid->mouthBox() && performHitTestOnHitBoxes(squid->mouthBox(), 1);
            } else if (tool == "chainsaw") {
                for (int g = squid->allTentaclesRemoved() ? 6 : 1; g < (squid->allTentaclesRemoved() ? 7 : 6); ++g)
                    if (squid->groupBoxes(g) && performHitTestOnHitBoxes(squid->groupBoxes(g), squid->groupCount(g)))
                        over = true;
            }
            if (!over) continue;
            mFindCorrectInjury.addObject(i);
            if (single) break;
            continue;
        }
        // A big centipede takes the injection only while stunned.
        if (auto* big = dynamic_cast<BigCentipede*>(i))
            if (!big->stunned()) continue;
        if (auto* bat = dynamic_cast<Bat*>(i)) {
            // A bat: the lighter while it hangs, the injection while it
            // flies (its stage names both).
            bool fits = (tool == "lighter" && bat->state() == Bat::kHanging) ||
                        (tool == "injection" && bat->state() == Bat::kFlying);
            if (!fits || !i->hitboxes) continue;
        } else if (!takes(i->current().tool, tool) || !i->hitboxes) {
            // The ordinary injury: its stage's tool (or one of them), a
            // guide under the finger.
            continue;
        }
        if (performHitTestOnHitBoxes(i->hitboxes, i->hitBoxCount)) {
            mFindCorrectInjury.addObject(i);
            if (single) break;
        }
    }
    return mFindCorrectInjury;
}

bool Game::performHitTestOnHitBoxes(const HitBox* boxes, int count) const {
    for (int i = 0; i < count; ++i) {
        const HitBox& b = boxes[i];
        Point p = toBox(b.sprite->currentShape());
        if (b.rect.origin.x <= p.x && p.x <= b.rect.origin.x + b.rect.size.width && b.rect.origin.y <= p.y &&
            p.y <= b.rect.origin.y + b.rect.size.height)
            return true;
    }
    return false;
}

bool Game::getAllHealedInjury(InjuryGraphic* injury, ITool* tool, bool instantHeal, bool updateToolEffect,
                              bool hitTestOnly) {
    bool allHealed = true;
    for (int i = 0; i < injury->hitBoxCount; ++i) {
        HitBox& b = injury->hitboxes[i];
        if (b.health > 0) {
            Point p = toBox(b.sprite->currentShape());
            float halfW = b.rect.size.width * 0.5f, halfH = b.rect.size.height * 0.5f;
            int dx = std::abs(static_cast<int>(p.x - (halfW + b.rect.origin.x)));
            int dy = std::abs(static_cast<int>(p.y - (b.rect.origin.y + halfH)));
            bool hit = halfW + static_cast<float>(tool->cursorHalfSize) > static_cast<float>(dx) &&
                       halfH + static_cast<float>(tool->cursorHalfSize) > static_cast<float>(dy);
            if (hit) {
                if (hitTestOnly) return true;
                if (updateToolEffect && ++tool->effectTimer >= tool->maxEffectTimer) tool->effectTimer = 0;
                if (instantHeal) b.health = 0;
                else b.health = b.health - static_cast<float>(tool->healAmount);
                injury->worked = true;
            }
        }
        if (b.health > 0) {
            allHealed = false;
            continue;
        }
        // Healed: its guide goes.
        b.active = false;
        if (b.sprite) scene().removeShape(b.sprite->currentShape());
    }
    return hitTestOnly ? false : allHealed;
}

void Game::drawTorchWithAccelerometer(const double* accel) {
    if (!torch) return;
    // The light's offset from the middle: the tilt across the screen,
    // kept on it.
    float w = gameController->ActualScreenSize.width * gameController->InverseScaleFactor;
    float h = gameController->ActualScreenSize.height * gameController->InverseScaleFactor;
    float ox = static_cast<float>(static_cast<int>(accel[1] * w));
    float oy = static_cast<float>(static_cast<int>(accel[0] * h));
    ox = std::clamp(ox, -w * 0.5f, w * 0.5f);
    oy = std::clamp(oy, -h * 0.5f, h * 0.5f);
    float g = gameController->GfxScaleFactor;
    Shape* t = sprTorch[0]->currentShape();
    // Positions count twice on the iPad (SceneManager): the light in points.
    t->pos = {ox / g + w * 0.5f / g, oy / g + h * 0.5f / g};
    // The black edges around its square (left, right, top, bottom), each a
    // quad from (x0, y0) to (x1, y1) about the light, reaching well past the
    // screen. (The original placed them on the screen by their corners;
    // here they move with the light, so between passes they glide with it
    // and leave no gap while it moves.)
    float half = 128 * g, out = 4096;
    float edges[4][4] = {{-out, -out, -half + 1, out},
                         {half - 1, -out, out, out},
                         {-half, -out, half, -half + 1},
                         {-half, half - 1, half, out}};
    for (int i = 0; i < 4; ++i) {
        sprTorch[i + 1]->currentShape()->pos = t->pos;
        float* v = sprTorch[i + 1]->currentShape()->verts;
        float x0 = edges[i][0], y0 = edges[i][1], x1 = edges[i][2], y1 = edges[i][3];
        v[0] = x0; v[1] = y0;
        v[2] = x1; v[3] = y0;
        v[4] = x0; v[5] = y1;
        v[6] = x1; v[7] = y1;
    }
}

}  // namespace as2
