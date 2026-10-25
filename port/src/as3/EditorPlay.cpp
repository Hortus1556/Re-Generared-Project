#include "as3/EditorPlay.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "as3/EditorLevelsScreen.h"
#include "as3/EditorScreen.h"
#include "as3/EditorWidgets.h"
#include "as3/Log.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryInfo.h"
#include "as3/SurgeryScreen.h"
#include "engine/cocos/CCDirector.h"
#include "platform/Platform.h"

namespace mt {
namespace editor {

using namespace cocos2d;

namespace {

const char* const kPlayName = "edited:play";

// the level in play
struct Play {
    bool active = false;
    Level level;
    json::Value json;
    Back back = Back::Levels;
    SavedGame slot;  // the slot as it was before
};
Play& play() {
    static Play p;
    return p;
}

// the slot as it was (its sound switches as they are now: the pause
// screen's, which save at once)
void restoreSlot() {
    Play& p = play();
    if (!p.active) return;
    SavedGame& slot = SaveData::shared().currentSlot();
    SavedGame kept = p.slot;
    kept.sfxOn = slot.sfxOn;
    kept.musicOn = slot.musicOn;
    kept.lastSaveTimestamp = slot.lastSaveTimestamp;
    slot = kept;
    p.active = false;
}

Scene* backScene(Back back) {
    if (back == Back::Editor) return EditorScreen::create();
    return EditorLevelsScreen::create();
}

// a star of the results (the game's are pictures of the area maps, not
// loaded here)
void drawStar(CCDrawNode* d, const CCPoint& c, float r, bool lit) {
    ccColor4F fill = lit ? ccc4f(1.0f, 0.82f, 0.12f, 1) : ccc4f(0.25f, 0.22f, 0.18f, 1);
    ccColor4F edge = ccc4f(0.11f, 0.09f, 0.08f, 1);
    CCPoint pts[10];
    for (int i = 0; i < 10; ++i) {
        float a = static_cast<float>(M_PI) * (0.5f + static_cast<float>(i) / 5.0f);
        float rr = i % 2 ? r * 0.45f : r;
        pts[i] = ccp(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr);
    }
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < 10; ++i) {
            CCPoint tri[3] = {c, pts[i], pts[(i + 1) % 10]};
            if (pass == 0) {
                // the outline: the same, a little larger
                CCPoint big[3] = {c, ccpAdd(c, ccpMult(ccpSub(pts[i], c), 1.18f)),
                                  ccpAdd(c, ccpMult(ccpSub(pts[(i + 1) % 10], c), 1.18f))};
                d->drawPolygon(big, 3, edge, 0, edge);
            } else {
                d->drawPolygon(tri, 3, fill, 0, fill);
            }
        }
}

// The editor's results over the surgery: won or lost, the score and the
// time left, the stars the level's thresholds give (shown, not kept), then
// again or back.
class ResultScreen : public PanelScene {
public:
    static ResultScreen* create(const SurgeryResult& r, bool won) {
        ResultScreen* s = new ResultScreen;
        s->setName("EditorResultScreen");
        s->init();
        s->autorelease();
        s->m_result = r;
        s->m_won = won;
        return s;
    }
    bool needsLoadingScreen() override { return false; }
    void load() override {
        PanelScene::load();
        const Level& l = play().level;
        Popup* p = Popup::create(m_won ? "SURGERY COMPLETE!" : "PATIENT LOST", 330, 210,
                                 m_won ? Tone::Green : Tone::Red);
        p->putText(l.title, 165, 160, 14, 300, true);
        char line[96];
        int t = std::max(0, m_result.timeLeft);
        std::snprintf(line, sizeof line, "Score %d    Time left %d:%02d", m_result.score, t / 60, t % 60);
        p->putText(line, 165, 136, 13, 300, true);
        // the stars as the game counts them (SurgeryScreen::finishWin): by
        // score and time used against the level's thresholds
        int stars = 0;
        if (m_won) {
            float used = static_cast<float>(l.stages.empty() ? 0 : l.stages[0].time) - static_cast<float>(m_result.timeLeft);
            stars = 1;
            if (m_result.score >= l.minScore[2] && used <= static_cast<float>(l.minTime[2])) stars = 3;
            else if (m_result.score >= l.minScore[1] && used <= static_cast<float>(l.minTime[1])) stars = 2;
        }
        CCDrawNode* d = CCDrawNode::create();
        for (int i = 0; i < 3; ++i) drawStar(d, ccp(115.0f + 50.0f * static_cast<float>(i), 98), 17, i < stars);
        p->putNode(d, 0, 0);
        p->putText("Edited surgeries give no stars or coins.", 165, 64, 11, 300, true);
        p->put(Button::create("AGAIN", 120, 32, Tone::Orange, [] { SceneMgr::sharedSceneMgr()->replaceScene(replay()); }),
               30, 14);
        bool editor = play().back == Back::Editor;
        p->put(Button::create(editor ? "EDITOR" : "BACK", 120, 32, Tone::Green,
                              [editor] { SceneMgr::sharedSceneMgr()->replaceScene(backScene(editor ? Back::Editor : Back::Levels)); }),
               180, 14);
        addPanel(p, 10);
        log("editor: results %s score %d time %d stars %d", m_won ? "won" : "lost", m_result.score, m_result.timeLeft,
            stars);
    }

private:
    void back() override { SceneMgr::sharedSceneMgr()->replaceScene(backScene(play().back)); }
    SurgeryResult m_result;
    bool m_won = false;
};

}  // namespace

Session& session() {
    static Session s;
    return s;
}

bool editorUnlocked() { return SaveData::shared().currentSlot().starsOf("surgery_bunker_hubris2", 0) > 0; }

Scene* playLevel(const Level& level, Back back) {
    Play& p = play();
    restoreSlot();  // (one still in play: put back first)
    p.level = level;
    p.back = back;
    // the surgery's file as the game reads them; never a tag partner nor a tutorial
    std::string text = toText(level, false, false);
    json::parse(text, p.json);
    json::Value no;
    no.type = json::Value::kBool;
    no.boolean = false;
    p.json.members.push_back({"tag_allowed", no});
    p.slot = SaveData::shared().currentSlot();
    p.active = true;
    const AreaDef* area = Catalog::shared().areaFor(level);
    SurgeryInfo info = area ? loadSurgeryInfo("surgery_edited", *area) : SurgeryInfo();
    info.id = "surgery_edited";
    SurgeryParams params;
    params.mode = 0;
    params.name = kPlayName;
    log("editor: play \"%s\" (%d stages)", level.title.c_str(), static_cast<int>(level.stages.size()));
    return SurgeryScreen::create(info, params);
}

bool isEditedName(const std::string& name) { return name == kPlayName; }

bool readEditedLevel(const std::string& name, json::Value& out) {
    if (!isEditedName(name) || play().json.isNull()) return false;
    out = play().json;
    return true;
}

bool isEditedPlay(const SurgeryParams& params) { return isEditedName(params.name); }

void playFinished(const SurgeryResult& result, bool won) {
    restoreSlot();
    SceneMgr::sharedSceneMgr()->pushScene(ResultScreen::create(result, won), false);
}

Scene* leavePlay() {
    restoreSlot();
    return backScene(play().back);
}

Scene* replay() { return playLevel(play().level, play().back); }

Scene* editedLevelFromEnv() {
    const char* spec = platform::debugMode() ? std::getenv("AS_EDITED") : nullptr;
    if (!spec || !*spec) return nullptr;
    std::string file = spec, error;
    Level level;
    bool ok;
    if (file.find('/') == std::string::npos && file.find('\\') == std::string::npos) {
        ok = loadLevel(file, level, error);
    } else {
        size_t n = 0;
        void* raw = SDL_LoadFile(file.c_str(), &n);
        ok = raw != nullptr;
        if (raw) {
            std::string text(static_cast<const char*>(raw), n);
            SDL_free(raw);
            ok = fromText(text, level, error);
        } else {
            error = "can't be read";
        }
    }
    if (!ok) {
        logError("AS_EDITED: %s: %s", file.c_str(), error.c_str());
        return nullptr;
    }
    return playLevel(level, Back::Levels);
}

}  // namespace editor
}  // namespace mt
