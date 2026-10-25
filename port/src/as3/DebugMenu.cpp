#include "as3/DebugMenu.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "as3/AchievementsScreen.h"
#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/Content.h"
#include "as3/CreditsScreen.h"
#include "as3/Log.h"
#include "as3/PartnerInfoUtils.h"
#include "as3/PartnerSelectScreen.h"
#include "as3/RestoredContent.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryData.h"
#include "as3/SurgeryInfo.h"
#include "as3/SurgeryScreen.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/UpgradeScreen.h"
#include "as3/VideoPlayer.h"
#include "as3/WelcomeScreen.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCDrawNode.h"
#include "engine/ui/DebugPanel.h"
#include "platform/GL.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

ui::DebugPanel* gPanel = nullptr;
bool gPanelTouch = false;       // the touch down began on the open menu
int gPage = 0;                  // 0 the go-to page, 1 the surgery's and the save's, 2 the cheats
int gMode = 0;                  // the surgeries' mode: 0 main, 1 and 2 the challenges
bool gHitAreas = false;         // the surgery's touch areas drawn
CCDrawNode* gHitNode = nullptr;  // (retained) what draws them

// ---- going to a screen ----

// The screen in play is left for another: a running surgery stopped (no more
// updates, touches or sounds, as its pause's quit), a movie stopped.
void leaveScreen() {
    if (VideoPlayer::sharedVideoPlayer()->isPlaying()) VideoPlayer::sharedVideoPlayer()->stop();
    if (SurgeryScreen* s = SurgeryScreen::running()) s->debugLeave();
}

void goTo(Scene* next) {
    leaveScreen();
    SceneMgr::sharedSceneMgr()->replaceScene(next);
}

void goToSurgery(const AreaDef& area, const std::string& id, int mode) {
    // as the area map and the case file would leave the slot (on the save's
    // current slot, as AS_SURGERY)
    SavedGame& slot = SaveData::shared().currentSlot();
    slot.lastSelectedArea = area.areaId;
    slot.selectedSurgery = id;
    slot.selectedMode = mode + 1;
    SurgeryParams params;
    params.mode = mode;
    log("debug: go to %s mode %d", id.c_str(), mode);
    goTo(SurgeryScreen::create(loadSurgeryInfo(id, area), params));
}

// ---- the save in use ----

bool haveSlot() { return !SaveData::shared().savedGames.empty(); }

std::string slotName() {
    const SaveData& save = SaveData::shared();
    for (size_t i = 0; i < save.savedGames.size(); ++i)
        if (save.savedGames[i].uniqueId == save.lastLoaded)
            return "slot " + std::to_string(i + 1) + " of " + std::to_string(save.savedGames.size());
    return "no slot yet";
}

void unlockAll() {
    // every surgery won with three stars and both its challenges, every
    // surgery's tutorial done
    SavedGame& slot = SaveData::shared().currentSlot();
    for (const AreaDef& a : AreasDefUtils::shared()->areas)
        for (const std::string& id : a.surgeries) {
            std::array<unsigned char, 3>& st = slot.stars[id];
            st[0] = std::max<unsigned char>(st[0], 3);
            st[1] = st[2] = 1;
            for (int mode = 0; mode < 3; ++mode) {
                json::Value root;
                if (!readContent(surgeryFile(id, mode, ""), root)) continue;
                const std::string t = root["tutorial_filename"].str();
                if (!t.empty() && std::find(slot.completedTutorials.begin(), slot.completedTutorials.end(), t) ==
                                      slot.completedTutorials.end())
                    slot.completedTutorials.push_back(t);
            }
        }
    SaveData::shared().saveGame();
    log("debug: unlocked all (%d stars)", slot.totalStars());
}

void giveUpgrades() {
    SavedGame& slot = SaveData::shared().currentSlot();
    for (const UpgradeDef& u : UpgradeInfoUtils::shared()->upgrades) {
        int i = upgradeIndex(u.id);
        if (i >= 0 && !slot.owns(SavedGame::kItemUpgrade, i)) slot.addTransaction(0, SavedGame::kItemUpgrade, i, 1);
    }
    SaveData::shared().saveGame();
    log("debug: every upgrade given");
}

void givePartners() {
    SavedGame& slot = SaveData::shared().currentSlot();
    for (const PartnerInfo& p : PartnerInfoUtils::shared()->partners) {
        int i = partnerIndex(p.id);
        if (i >= 0 && !slot.owns(SavedGame::kItemPartner, i)) slot.addTransaction(0, SavedGame::kItemPartner, i, 1);
    }
    SaveData::shared().saveGame();
    log("debug: every partner given");
}

// ---- the items ----

ui::DebugItem item(const std::string& label, float width, std::function<void()> action, bool enabled = true) {
    ui::DebugItem it;
    it.label = label;
    it.width = width;
    it.action = std::move(action);
    it.enabled = enabled;
    return it;
}

ui::DebugItem toggle(const std::string& label, float width, bool on, std::function<void()> action) {
    ui::DebugItem it = item(label, width, std::move(action));
    it.isSwitch = true;
    it.on = on;
    return it;
}

ui::DebugItem caption(const std::string& label, float width, bool newRow = true) {
    ui::DebugItem it;
    it.label = label;
    it.width = width;
    it.caption = true;
    it.newRow = newRow;
    return it;
}

std::vector<ui::DebugItem> items() {
    std::vector<ui::DebugItem> v;
    // the pages
    v.push_back(toggle("Go to", 120, gPage == 0, [] { gPage = 0; }));
    v.push_back(toggle("Surgery and save", 120, gPage == 1, [] { gPage = 1; }));
    v.push_back(toggle("Cheats", 120, gPage == 2, [] { gPage = 2; }));
    if (gPage == 2) {
        // the original's developer switches (Cheats.h), each asked where the
        // original asked it; off at start, none without -d
        v.push_back(caption("The original's cheats (Cheats::IsCheatEnabled), where it checked them:", 400));
        const Cheats::CheatCodes codes[] = {
            Cheats::kHitAnywhere,     Cheats::kNoMisuseDamage,  Cheats::kInvincible,     Cheats::kReverendForever,
            Cheats::kTagAlwaysFull,   Cheats::kPressWins,       Cheats::kPressLoses,     Cheats::kUnlockAll,
            Cheats::kAllUpgradeRows,  Cheats::kAllPartners,     Cheats::kStars250,       Cheats::kSkipTutorials,
            Cheats::kReplayTutorials, Cheats::kSkipIntroMovie,  Cheats::kLoadTestFull,   Cheats::kLoadTestNothing,
            Cheats::kLoadTestSheets};
        int n = 0;
        for (Cheats::CheatCodes c : codes) {
            ui::DebugItem it = toggle(std::to_string(static_cast<int>(c)) + " " + Cheats::label(c), 122,
                                      Cheats::IsCheatEnabled(c), [c] { Cheats::Toggle(c); });
            it.newRow = n++ % 4 == 0;
            v.push_back(it);
        }
        return v;
    }
    if (gPage == 0) {
        // every surgery, by area, in the mode chosen; the area's button its map
        v.push_back(caption("Surgeries in:", 120));
        const char* modes[3] = {"Main", "Challenge 1", "Challenge 2"};
        for (int m = 0; m < 3; ++m) v.push_back(toggle(modes[m], 100, gMode == m, [m] { gMode = m; }));
        for (const AreaDef& a : AreasDefUtils::shared()->areas) {
            ui::DebugItem map = item(a.title + " map", 120, [a] {
                SaveData::shared().currentSlot().lastSelectedArea = a.areaId;
                goTo(AreaScreen::create(a, 0));
            });
            map.newRow = true;
            v.push_back(map);
            for (const std::string& id : a.surgeries)
                v.push_back(item(id.substr(id.rfind('_') + 1), 100, [a, id] { goToSurgery(a, id, gMode); }));
        }
        ui::DebugItem title = item("Title", 120, [] { goTo(WelcomeScreen::create()); });
        title.newRow = true;
        v.push_back(title);
        v.push_back(item("World map", 100, [] { goTo(WorldScreen::create()); }));
        v.push_back(item("Upgrades", 100, [] { goTo(UpgradeScreen::create("world")); }));
        v.push_back(item("Partners", 100, [] { goTo(PartnerSelectScreen::create("world", SurgeryInfo(), SurgeryParams())); }));
        v.push_back(item("Credits", 100, [] { goTo(CreditsScreen::create()); }));
        v.push_back(item("Achievements", 100, [] { goTo(AchievementsScreen::create()); }));
        return v;
    }
    // the running surgery (shown on top, not over)
    SurgeryScreen* s = SurgeryScreen::running();
    const bool playing = s && s->debugPlayable();
    v.push_back(caption("Surgery:", 66));
    v.push_back(item("Win now", 94, [] { if (SurgeryScreen* s = SurgeryScreen::running()) s->debugWin(); }, playing));
    v.push_back(item("Lose now", 94, [] { if (SurgeryScreen* s = SurgeryScreen::running()) s->debugLose(); }, playing));
    v.push_back(item("Next stage", 94, [] { if (SurgeryScreen* s = SurgeryScreen::running()) s->debugNextStage(); }, playing));
    v.push_back(item("Heal", 94, [] { if (SurgeryScreen* s = SurgeryScreen::running()) s->debugHeal(); }, playing));
    v.push_back(item("+60 s", 94, [] { if (SurgeryScreen* s = SurgeryScreen::running()) s->debugAddTime(60); }, playing));
    v.push_back(caption("", 66));
    v.push_back(toggle("God mode", 94, SurgeryScreen::s_debugGodMode, [] { SurgeryScreen::s_debugGodMode = !SurgeryScreen::s_debugGodMode; }));
    v.push_back(item("Tag power full", 94, [] { if (SurgeryScreen* s = SurgeryScreen::running()) s->debugFillTag(); },
                     playing && s->debugHasPartner()));
    v.push_back(item("Free tools", 94, [] { if (SurgeryScreen* s = SurgeryScreen::running()) s->debugFreeTools(); }, playing));
    v.push_back(toggle("Hit areas", 94, gHitAreas, [] { gHitAreas = !gHitAreas; }));
    // the save in use: these write it
    const bool slot = haveSlot();
    v.push_back(caption("Changes the save in use (" + slotName() + "), at once:", 400));
    ui::DebugItem coins = item("Save: +10000 coins", 108, [] {
        SaveData::shared().currentSlot().addTransaction(10000, 0, 0, 10000);  // (earned, as a win's)
        SaveData::shared().saveGame();
        log("debug: +10000 coins (%d)", SaveData::shared().currentSlot().coins());
    }, slot);
    coins.newRow = true;
    v.push_back(coins);
    v.push_back(item("Save: unlock all", 108, unlockAll, slot));
    v.push_back(item("Save: all upgrades", 108, giveUpgrades, slot));
    v.push_back(item("Save: all partners", 108, givePartners, slot));
    v.push_back(item("Save: end cooldowns", 108, [] {
        SavedGame& g = SaveData::shared().currentSlot();
        for (const char* p : kPartnerIds) g.tagCooldowns[p] = 0;
        SaveData::shared().saveGame();
        log("debug: partner cooldowns reset");
    }, slot));
    // the blood packs (BloodPacks.h): full, none, or the next one due in 10 s
    // (the count as one would come back: the timestamp set a recharge less
    // 10 s ago)
    v.push_back(item("Save: packs full", 108, [] {
        GameStateUtils::RefillBloodPacks();
        log("debug: blood packs full (%d)", SaveData::shared().currentSlot().numBloodPacks);
    }, slot));
    v.push_back(item("Save: no packs", 108, [] {
        SavedGame& g = SaveData::shared().currentSlot();
        g.numBloodPacks = 0;
        g.lastBloodPackTimestamp = static_cast<uint64_t>(std::time(nullptr));
        SaveData::shared().saveGame();
        log("debug: blood packs none");
    }, slot));
    v.push_back(item("Save: pack in 10 s", 108, [] {
        SavedGame& g = SaveData::shared().currentSlot();
        GameStateUtils::UpdateBloodPacksAndTime(g);
        if (g.numBloodPacks >= GameStateUtils::MaxBloodPacks()) g.numBloodPacks = GameStateUtils::MaxBloodPacks() - 1;
        g.lastBloodPackTimestamp =
            static_cast<uint64_t>(std::time(nullptr) - GameStateUtils::RechargeSecs() + 10);
        SaveData::shared().saveGame();
        log("debug: blood packs %d, the next in 10 s", g.numBloodPacks);
    }, slot));
    ui::DebugItem cut = toggle("Save: Director's cut", 108, slot && restored::directorsCut(), [] {
        if (haveSlot()) restored::setDirectorsCut(!restored::directorsCut());
    });
    cut.enabled = slot;
    v.push_back(cut);
    v.push_back(caption("Display:", 66));
    v.push_back(toggle("Smooth motion", 108, platform::smoothMotion(), [] { platform::setSmoothMotion(!platform::smoothMotion()); }));
    // the surgery's HUD layout (SurgeryHud.cpp): by the screen's size, the
    // tool bar across the top, the original's sides; followed at once
    {
        static const char* const kLayouts[] = {"HUD: by screen", "HUD: tool bar", "HUD: sides"};
        const int at = std::max(0, SurgeryScreen::s_hudLayoutOverride);
        v.push_back(item(kLayouts[at % 3], 108, [] {
            SurgeryScreen::s_hudLayoutOverride = (std::max(0, SurgeryScreen::s_hudLayoutOverride) + 1) % 3;
            log("debug: hud layout %d", SurgeryScreen::s_hudLayoutOverride);
        }));
    }
    // the screen's shape (ResolutionMgr::fitWindow): the window's, or one
    // forced as AS_ASPECT does; taken at the next screen change
    {
        static int shape = 0;
        static const char* const kShapes[] = {"Shape: window", "Shape: 4:3", "Shape: 16:10", "Shape: 16:9",
                                              "Shape: 20:9", "Shape: 21:9"};
        static const float kAspects[] = {0, 4.0f / 3, 16.0f / 10, 16.0f / 9, 20.0f / 9, 21.0f / 9};
        v.push_back(item(kShapes[shape], 108, [] {
            shape = (shape + 1) % 6;
            platform::setAspectOverride(kAspects[shape]);
            log("debug: screen shape %s (at the next screen)", kShapes[shape]);
        }));
    }
    return v;
}

// ---- the hit areas ----

// A rect about `at` turned by the injury's rotation (degrees, clockwise as
// cocos's), outlined; `local` is its rect as the injury would be unturned.
void turnedRect(const InjuryState* inj, const CCRect& local, const ccColor4F& c) {
    const float a = -inj->rotation * 3.14159265f / 180.0f;
    const float cs = std::cos(a), sn = std::sin(a);
    const CCPoint o = inj->position;
    auto turn = [&](float x, float y) {
        float dx = x - o.x, dy = y - o.y;
        return ccp(o.x + dx * cs - dy * sn, o.y + dx * sn + dy * cs);
    };
    const float x0 = local.getMinX(), x1 = local.getMaxX(), y0 = local.getMinY(), y1 = local.getMaxY();
    CCPoint p[4] = {turn(x0, y0), turn(x1, y0), turn(x1, y1), turn(x0, y1)};
    for (int i = 0; i < 4; ++i) gHitNode->drawSegment(p[i], p[(i + 1) % 4], 0.5f, c);
}

void drawHitAreas() {
    SurgeryScreen* s = SurgeryScreen::running();
    if (!s || !s->debugPlayable()) return;
    if (!gHitNode) {
        gHitNode = CCDrawNode::create();
        gHitNode->retain();
    }
    gHitNode->clear();
    // (premultiplied colours, as the draw node blends)
    const ccColor4F green = {0, 0.9f, 0, 0.9f}, grey = {0.35f, 0.35f, 0.35f, 0.6f}, cyan = {0, 0.6f, 0.7f, 0.7f},
                    yellow = {0.9f, 0.8f, 0, 0.9f}, magenta = {0.9f, 0, 0.9f, 1};
    for (InjuryState* inj : s->debugInjuries()) {
        if (inj->finished || !inj->root || !inj->root->isVisible()) continue;
        // its rect (the creatures' and a size-rect stage's touch area), turned with it
        turnedRect(inj, inj->rect, cyan);
        // its pieces (what the tongs take), in its unturned frame
        for (const PieceState& p : inj->pieces)
            if (!p.onTray && p.rect.size.width > 0) turnedRect(inj, p.rect, yellow);
        // an extraction's touch area as it is (its art: ArtHit.h), dotted
        // every 3 points about its place
        if ((inj->def->base == kAngledExtraction || inj->def->base == kSurfaceExtraction) && !inj->pieces.empty()) {
            const float reach = std::max(inj->pieces[0].rect.size.width, inj->pieces[0].rect.size.height) + 12;
            for (float y = -reach; y <= reach; y += 3)
                for (float x = -reach; x <= reach; x += 3) {
                    const CCPoint at = ccp(inj->position.x + x, inj->position.y + y);
                    if (inj->pieceContains(inj->pieces[0], at)) gHitNode->drawDot(at, 1.2f, magenta);
                }
        }
        // the hit areas' points: green to treat, grey done
        for (const HitAreaState& a : inj->areas)
            for (const HitPointState& p : a.points) {
                const CCRect& r = p.rect;
                const ccColor4F& c = p.done || a.done ? grey : green;
                CCPoint q[4] = {ccp(r.getMinX(), r.getMinY()), ccp(r.getMaxX(), r.getMinY()),
                                ccp(r.getMaxX(), r.getMaxY()), ccp(r.getMinX(), r.getMaxY())};
                for (int i = 0; i < 4; ++i) gHitNode->drawSegment(q[i], q[(i + 1) % 4], 0.5f, c);
            }
        gHitNode->drawDot(inj->position, 1.5f, magenta);
    }
    // in cocos's projection (points, y up)
    const CCSize win = CCDirector::sharedDirector()->getWinSize();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, win.width, 0, win.height, -1024, 1024);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gHitNode->visit();
}

void drawOverlay() {
    if (gHitAreas) drawHitAreas();
    if (gPanel && gPanel->isOpen()) {
        ui::Window& w = ui::Window::shared();
        w.update();
        w.drawAll();
    }
}

}  // namespace

void debugMenuInstall() {
    if (!platform::debugMode()) return;
    CCDirector::sharedDirector()->setOverlay(drawOverlay);
    Cheats::Init();  // (AS_CHEATS: Cheats.h)
    // (AS_HIT_AREAS=1: the hit areas drawn from the start, for test runs'
    // screenshots)
    if (std::getenv("AS_HIT_AREAS")) gHitAreas = true;
}

void debugMenuKey() {
    if (!platform::debugMode()) return;
    if (!gPanel) {
        // the measures for the game's 568x320 points
        ui::DebugPanelStyle style;
        style.margin = 8;
        style.gap = 3;
        style.rowHeight = 20;
        style.fontSize = 10;
        style.titleSize = 11;
        gPanel = new ui::DebugPanel(items, style);
    }
    // opened on the page for where the game is: a surgery's, else the go-to one
    if (!gPanel->isOpen()) gPage = SurgeryScreen::running() ? 1 : 0;
    gPanel->toggle();
    log("debug menu %s", gPanel->isOpen() ? "open" : "closed");
}

bool debugMenuTouch(int phase, float x, float y) {
    if (phase == 0) gPanelTouch = gPanel && gPanel->isOpen();
    if (!gPanelTouch) return false;
    ui::Window& w = ui::Window::shared();
    if (phase == 0) {
        w.touchBegan({x, y});
    } else if (phase == 1) {
        w.touchMoved({x, y}, {x, y});
    } else {
        gPanelTouch = false;
        w.touchEnded({x, y}, {x, y});
    }
    return true;
}

bool debugMenuEscape() {
    if (!gPanel || !gPanel->isOpen()) return false;
    gPanel->toggle();
    return true;
}

}  // namespace mt
