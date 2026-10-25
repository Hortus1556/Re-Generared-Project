#include "as2/menus/Hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "as2/Connector.h"
#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/Graveyard.h"
#include "as2/Loader.h"
#include "as2/menus/Bandages.h"
#include "as2/menus/CutScene.h"
#include "as2/menus/InGame.h"
#include "as2/menus/Panel.h"
#include "as2/menus/Results.h"
#include "as2/tools/ITool.h"
#include "as2/menus/FlashStage.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

// The tools' names in the art (each card, its highlight and arrow), in
// their ids' order (the keys 1-9, 0).
const char* const kTools[10] = {"cutter", "tongs",    "lighter",   "cream",   "stitch",
                                "vacuum", "chainsaw", "injection", "battery", "scanner"};
constexpr unsigned long long kOwner = 0x4855440000000000ull;  // "HUD"

VectorArt& art() { return flashstage::art("flash"); }
int missionId() { return Connector::sharedConnector().current_mission_id; }
Panel* panel() { return dynamic_cast<Panel*>(MenuManager::sharedMenuManager().mCurrentViewController); }

ITool* toolWithId(int id) {
    for (ITool* t : gameState->_tools)
        if (t->toolData->mId == id) return t;
    return nullptr;
}

}  // namespace

Hud& Hud::shared() {
    static Hud h;
    return h;
}

bool Hud::ready() {
    if (ui >= 0) return true;
    VectorArt& a = art();
    ui = a.symbol("ui");
    cursor = a.symbol("cursor");
    countdown = a.symbol("bleedCountdown");
    // The runner's death plays once and stays on its last frame (its class's
    // stop() on frame 33).
    a.addStop(a.symbol("assets_fla.manny_death_1174"), 33);
    return ui >= 0;
}


void Hud::showArrow(int tool) {
    if (tool >= 1 && tool <= 10) arrowAge[tool - 1] = 0;
}

void Hud::clearArrows() { std::fill(std::begin(arrowAge), std::end(arrowAge), -1); }

bool Hud::partAt(const char* part, Point p) {
    VectorTransform t = flashstage::at();
    return art().hitTestPart(ui, 1, age, age, t, &drawn, part, p.x, p.y, true);
}

int Hud::toolAt(Point p) {
    if (!gameState || !ready()) return 0;
    // Its picture or its card (`<tool>highlight`, white, orange while held:
    // Flash listened on the picture's clip alone, whose own card shape its
    // level-3 frames lack, so the card's paper took a press or not by the
    // tool's level).
    for (int i = 0; i < 10; ++i)
        if (partAt(kTools[i], p) || partAt((std::string(kTools[i]) + "highlight").c_str(), p)) return i + 1;
    return 0;
}

bool Hud::pauseAt(Point p) { return gameState && ready() && partAt("pause_but", p); }

bool Hud::drawsPointer() const {
    // (On a touch device there is no pointer: the finger is it.)
    if (platform::touchDevice()) return false;
    if (!game || !gameState || gameState->paused || game->mWon || game->mFailed) return false;
    Panel* p = panel();
    // (Not while a tool takes the pointer over: the injection's syringe
    // shows the system's, as Flash's did.)
    if (gameState->currentTool && gameState->currentTool->hidesPointer()) return false;
    return p && !p->hasPopup();
}

void Hud::draw() {
    if (!gameState || !game || !ready()) return;
    ++age;
    // The screens a mission can open (the pause, the obituary, the
    // bandages) made ahead, so they open without a stall (one a frame, as
    // walking their frames takes a few milliseconds).
    switch (age % (20 * 60)) {
        case 1: PauseMenu::prefetch(); break;
        case 2: MissionFailed::prefetch(); break;
        case 3: bandages::prefetch(); break;
        case 4: Loader::sharedLoader().prefetchArt(); break;
        case 5: cutscene::prefetchStory(missionId(), true); break;  // (the end story)
        case 6: MissionWin::prefetch(); break;
        default: break;
    }
    VectorArt& a = art();
    const float s = flashstage::scale(), x0 = flashstage::left();
    VectorTransform t = flashstage::at();
    int mission = missionId();
    VectorLook look;

    // The tools: in the mission's offer at their level (2 x level), else
    // dimmed (one before); the graveyard's cream and needle and the
    // vampire's injection have their own look.
    for (int i = 0; i < 10; ++i) {
        int level = std::clamp(SaveDetails::GetToolLevel(GameData::toolNames2[i]), 1, 3);
        ITool* tool = toolWithId(i + 1);
        std::string name = kTools[i];
        int frame = tool ? 2 * level : 2 * level - 1;
        if ((mission == 19 && (i == 3 || i == 4)) || (mission == 9 && i == 7)) {
            int card = a.innerSymbol(ui, 1, age, name);
            int special = card >= 0 ? a.frameOf(card, mission == 19 ? "bleed" : "vlad") : 0;
            if (special > 0) frame = special;
        }
        look.frames[name] = frame;
        bool held = tool && gameState->currentTool == tool;
        look.frames[name + "highlight"] = held ? 2 * level : 2 * level - 1;
        // A tutorial's arrow under it: its 80 frames played once.
        int& arrow = arrowAge[i];
        if (arrow >= 0 && arrow + 2 > 81) arrow = -1;
        look.frames["effect" + name] = arrow >= 0 ? arrow++ + 2 : 1;
    }
    look.frames["effecttime"] = look.frames["effecthealth"] = 1;

    // The dial: its face by the heart, the runner by GameState's state (1-5;
    // the shock and the brain out are the port's own pictures, drawn after).
    int type = gameState->joggerFrameType;
    float h = gameState->health;
    int face = mission == 9 || gameState->inShock || h <= 0 ? 5 : h <= 15 ? 4 : h <= 25 ? 3 : h <= 45 ? 2 : 1;
    look.frames["bg"] = face;
    bool ownRunner = type == 6 || type == 7;
    int runner = std::clamp(type, 1, 5);
    if (runner != joggerShown) {
        joggerShown = runner;
        joggerSince = age;
    }
    look.frames["jogger"] = runner;
    look.inFrames["jogger"] = age - joggerSince;
    if (ownRunner) look.hidden.push_back("jogger");
    int heart = gameState->inShock || mission == 9 ? 0 : static_cast<int>(h);
    look.texts["health"] = std::to_string(heart);

    // The time (a brain out's countdown while it runs) and its drip, the
    // score.
    int seconds = gameState->brainOut() ? (gameState->brainOutLeft + 29) / 30 : gameState->time / 30;
    look.texts["time"] = flashstage::formatTime(seconds);
    int full = game->mission_stage ? game->mission_stage->time : 0;
    int drip = full > 0 ? 100 - static_cast<int>(static_cast<float>(gameState->time) / full * 100) : 1;
    look.frames["drip"] = std::clamp(drip, 1, 100);
    look.texts["score"] = flashstage::prettyScore(gameState->score);

    a.draw(ui, 1, age, t, kOwner, &look);
    drawn = look;

    // The port's runner pictures (the shock, the brain out), in the runner's
    // place and at its size.
    if (ownRunner && !gameState->joggerPicture.empty()) {
        // (Measured on the running jogger's first frame: its bounds as it
        // runs would move the picture with every stride.)
        VectorLook still;
        still.frames["jogger"] = 1;
        still.inFrames["jogger"] = 0;
        flash::Rect r = a.partBounds(ui, 1, 0, 0, t, &still, "jogger");  // pixels
        if (!r.empty()) {
            if (Texture2D* tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(gameState->joggerPicture)) {
                // (The picture's jogger stands 37 of its 50 pixels high.)
                float ph = (r.yMax - r.yMin) * 50.0f / 37.0f * gameState->joggerScale;
                float pw = ph * tex->pixelsWide() / std::max(1.0f, static_cast<float>(tex->pixelsHigh()));
                float cx = (r.xMin + r.xMax) * 0.5f, cy = (r.yMin + r.yMax) * 0.5f;
                float st[4] = {0, 0, tex->maxS(), tex->maxT()};
                glColor(1, 1, 1, 1);
                tex->drawInRect(cx - pw / 2, cy - ph / 2, pw, ph, st, kOwner + 1);
            }
        }
    }

    // Bleed's countdown (the graveyard), while it runs.
    if (mission == 19 && countdown >= 0) {
        for (InjuryGraphic* i : gameState->_injuries) {
            auto* b = dynamic_cast<Bleed*>(i);
            if (!b || b->aliveLeft() <= 0) continue;
            VectorLook c;
            c.texts["txt"] = std::to_string(static_cast<int>(std::lround(b->aliveLeft() / 30.0)));
            VectorTransform ct = t;
            ct.x = x0 + 525 * s;
            ct.y = 150 * s;
            a.draw(countdown, 1, age, ct, kOwner + 2, &c);
            break;
        }
    }

    // The pointer: the tool in hand (the vampire's injection is blood, the
    // graveyard's needle and cream a vial and syrup), at the mouse.
    if (drawsPointer() && cursor >= 0) {
        std::string label = gameState->currentTool ? gameState->currentTool->toolData->name : "nothing";
        if (mission == 9 && label == "injection") label = "blood";
        if (mission == 19 && label == "stitch") label = "vial";
        if (mission == 19 && label == "cream") label = "syrup";
        int frame = std::max(1, a.frameOf(cursor, label));
        float px, py;
        platform::livePointer(px, py);
        platform::PointerArt pointer(px, py);
        VectorTransform ct;
        ct.x = px;
        ct.y = py;
        ct.scaleX = ct.scaleY = s;
        a.draw(cursor, frame, age, ct, kOwner + 3);
    }
}

}  // namespace as2
