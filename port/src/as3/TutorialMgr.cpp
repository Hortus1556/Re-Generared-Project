#include "as3/TutorialMgr.h"

#include <algorithm>

#include "as3/Content.h"
#include "as3/Extractions.h"
#include "as3/InjuryState.h"
#include "as3/Log.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryData.h"
#include "as3/ToolState.h"

namespace mt {

InjuryState* TutorialSurgery::findInjury(const std::string& name, int stageNumber) const {
    // (0x77884) stage -1: every stage, else that one (1-based)
    int first = 0, last = stageCount ? stageCount() - 1 : -1;
    if (stageNumber != -1) first = last = stageNumber - 1;
    for (int i = first; i <= last; ++i) {
        const std::vector<InjuryState*>* list = injuries ? injuries(i) : nullptr;
        if (!list) continue;
        for (InjuryState* inj : *list)
            if (inj->name == name) return inj;
    }
    return nullptr;
}

namespace {

// ---------------------------------------------------------------- conditions

// The surgery is on a stage (0x1ca8d0; "stage", 1-based).
class InSurgeryStageTC : public TutorialCondition {
public:
    InSurgeryStageTC(const TutorialSurgery* s, const json::Value& v) : TutorialCondition(s), m_stage(v["stage"].integer()) {}
    bool holds() override { return m_surgery->stage() == m_stage; }  // 0x1ca904

private:
    int m_stage;  // +8
};

// A stage was completed (0x1ca6cc; "stage", kept 0-based).
class FinishedSurgeryStageTC : public TutorialCondition {
public:
    FinishedSurgeryStageTC(const TutorialSurgery* s, const json::Value& v)
        : TutorialCondition(s), m_index(v["stage"].integer() - 1) {}
    bool holds() override {  // 0x1ca700
        // (guarded: the original read past its stages for a stage it hasn't)
        if (m_index < 0 || m_index >= m_surgery->stageCount()) return false;
        return m_surgery->stageDone(m_index);
    }

private:
    int m_index;  // +8
};

// The tutorial box is closed (0x1ca914).
class PopupIsClosedTC : public TutorialCondition {
public:
    explicit PopupIsClosedTC(const TutorialSurgery* s) : TutorialCondition(s) {}
    bool holds() override { return !m_surgery->popupShown(); }  // 0x1ca928
};

// The patient's health went up since the lowest it was seen at (0x1ca898:
// it starts from the health when the tutorial is read).
class HealthIncreasesTC : public TutorialCondition {
public:
    explicit HealthIncreasesTC(const TutorialSurgery* s) : TutorialCondition(s), m_lowest(s->health()) {}
    bool holds() override {  // 0x1ca8b0
        float was = m_lowest, now = m_surgery->health();
        if (now <= was) m_lowest = now;
        return was < now;
    }

private:
    float m_lowest;  // +8
};

// A tool is the one selected (0x1ca948; "tool_name").
class SelectedToolTC : public TutorialCondition {
public:
    SelectedToolTC(const TutorialSurgery* s, const json::Value& v)
        : TutorialCondition(s), m_tool(toolByName(v["tool_name"].str())) {}
    bool holds() override {  // 0x1caaa8
        const ToolState* t = m_surgery->tool();
        return t && t->type == m_tool;
    }

private:
    int m_tool;  // +8
};

// The tag partner's power was used: ready one frame, not the next (0x1caac4).
class UsedTagPowerTC : public TutorialCondition {
public:
    explicit UsedTagPowerTC(const TutorialSurgery* s) : TutorialCondition(s) {}
    bool holds() override {  // 0x1caadc
        bool was = m_ready;
        m_ready = m_surgery->tagPowerReady && m_surgery->tagPowerReady();
        return !m_ready && was;
    }

private:
    bool m_ready = false;  // +8
};

// An injury is finished (0x1ca720; "injury_name"): found when the tutorial
// is read, in any stage.
class HealedInjuryTC : public TutorialCondition {
public:
    HealedInjuryTC(const TutorialSurgery* s, const json::Value& v) : TutorialCondition(s) {
        const std::string& name = v["injury_name"].str();
        m_injury = s->findInjury(name);
        if (!m_injury) logError("Injury named %s not found", name.c_str());
    }
    // (guarded: the original read through the missing injury)
    bool holds() override { return m_injury && m_injury->finished; }  // 0x1ca890

private:
    InjuryState* m_injury = nullptr;  // +8
};

// The injury of a name in the stages up to the current one: the first
// found, if it is past its stage of a state (0x1ca0cc; "injury_name",
// "injury_state"), or finished.
class FinishedInjuryStateTC : public TutorialCondition {
public:
    FinishedInjuryStateTC(const TutorialSurgery* s, const json::Value& v)
        : TutorialCondition(s), m_name(v["injury_name"].str()), m_state(stateByName(v["injury_state"].str())) {
        // the index of the state's stage in the injury's definition, from
        // the first injury of the name in any stage
        bool found = false;
        for (int i = 0; i < s->stageCount() && !found; ++i) {
            const std::vector<InjuryState*>* list = s->injuries(i);
            if (!list) continue;
            for (InjuryState* inj : *list) {
                if (inj->name != m_name) continue;
                found = true;
                for (size_t k = 0; k < inj->def->stages.size(); ++k)
                    if (inj->def->stages[k].state == m_state) {
                        m_stageIndex = static_cast<int>(k);
                        break;
                    }
                if (m_stageIndex == -1)
                    logError("State %s for injury %s not found", v["injury_state"].str().c_str(), m_name.c_str());
                break;
            }
        }
        if (!found) logError("Injury %s not found", m_name.c_str());
    }
    bool holds() override {  // 0x1ca438
        int stages = m_surgery->stageCount();
        for (int i = 0; i < m_surgery->stage() && i < stages; ++i) {
            for (int list = 0; list < 2; ++list) {
                // Fixed: the original looked only through the injuries still
                // to treat, so an injury finished (moved to the treated list)
                // was never found and the step waited for ever; its own test
                // says a finished injury is past every state.
                const std::vector<InjuryState*>* l = list == 0 ? m_surgery->injuries(i)
                                                    : m_surgery->treated ? m_surgery->treated(i) : nullptr;
                if (!l) continue;
                for (InjuryState* inj : *l)
                    if (inj->name == m_name) return inj->finished || inj->m_stage > m_stageIndex;
            }
        }
        return false;
    }

private:
    std::string m_name;   // +8
    int m_state;          // +0xc
    int m_stageIndex = -1;  // +0x10 (0-based; the injury's stage is 1-based)
};

// The injury of a name in the stages up to the current one has its pieces
// freed (a Transplant's +0xfdb8, allCutFree; 0x1ca4d8; "injury_name").
class FreedInjuryPiecesTC : public TutorialCondition {
public:
    FreedInjuryPiecesTC(const TutorialSurgery* s, const json::Value& v)
        : TutorialCondition(s), m_name(v["injury_name"].str()) {}
    // (0x1ca644) the first injury of the name still to treat in the stages
    // up to the current one: all the cuts freeing its pieces done
    bool holds() override {
        const int stages = std::min(m_surgery->stage(), m_surgery->stageCount());
        for (int i = 0; i < stages; ++i)
            if (const std::vector<InjuryState*>* list = m_surgery->injuries(i))
                for (InjuryState* inj : *list)
                    if (inj->name == m_name)
                        return inj->def->base == kTransplant && static_cast<Transplant*>(inj)->allCutFree;
        return false;
    }

private:
    std::string m_name;  // +8
};

// The stage shown has run some time (iOS 1.45: ctor 0x2700dc, "time";
// check 0x270118 `time <= SurgeryState +0x70c`). Used by 1.45's revised
// tutorials (exileclinic_usefulbro1, lunacyfalls_batshift,
// robotopia_revsorgan, pocketgods_fire and _locusts).
class TimeElapsedOnStageTC : public TutorialCondition {
public:
    TimeElapsedOnStageTC(const TutorialSurgery* s, const json::Value& v)
        : TutorialCondition(s), m_time(static_cast<float>(v["time"].num())) {}
    bool holds() override { return m_surgery->stageTime && m_time <= m_surgery->stageTime(); }

private:
    float m_time;  // +8
};

// ---------------------------------------------------------------- actions

// An action that only asks for an event of its type (with no more).
class PlainTA : public TutorialAction {
public:
    PlainTA(const TutorialSurgery* s, int type) : TutorialAction(s), m_type(type) {}
    TutorialEvent run() override {
        TutorialEvent e;
        e.type = m_type;
        return e;
    }

private:
    int m_type;
};

// An action naming a tool, a HUD item or a sound (0xc bytes: the name at +8).
class NamedTA : public TutorialAction {
public:
    NamedTA(const TutorialSurgery* s, int type, const std::string& name) : TutorialAction(s), m_type(type), m_name(name) {}
    TutorialEvent run() override {
        TutorialEvent e;
        e.type = m_type;
        e.text = m_name;
        return e;
    }

private:
    int m_type;
    std::string m_name;  // +8
};

// An action on an injury, found by its name when it runs (HighlightInjuryTA
// 0x1c8f6c, ShowInjuryHitAreasTA 0x1c96a0).
class InjuryTA : public TutorialAction {
public:
    InjuryTA(const TutorialSurgery* s, int type, const std::string& name) : TutorialAction(s), m_type(type), m_name(name) {}
    TutorialEvent run() override {
        TutorialEvent e;
        e.type = m_type;
        e.injury = m_surgery->findInjury(m_name);
        return e;
    }

private:
    int m_type;
    std::string m_name;  // +8
};

// The tag partner's power regenerates, or not (0x1c893c, 0x1c8478): done at
// once, no event.
class TagPowerRegenerationTA : public TutorialAction {
public:
    TagPowerRegenerationTA(const TutorialSurgery* s, bool on) : TutorialAction(s), m_on(on) {}
    TutorialEvent run() override {
        if (m_surgery->setTagPowerRegeneration) m_surgery->setTagPowerRegeneration(m_on);
        return {};
    }

private:
    bool m_on;
};

// The tutorial box with a text (0x24 bytes; 0x1c9780): "text_id" (the text
// itself), "character_name" (the picture beside it, "normal" by default),
// "show_close_button", "time_to_relaunch" (seconds, -1: never), "pause_game".
class ShowPopupTA : public TutorialAction {
public:
    ShowPopupTA(const TutorialSurgery* s, const json::Value& v) : TutorialAction(s) {
        m_text = v["text_id"].str();
        m_character = v.has("character_name") ? v["character_name"].str() : "normal";
        m_closeButton = v["show_close_button"].flag();
        m_relaunch = static_cast<float>(v["time_to_relaunch"].num(m_none));
        m_pauseGame = v["pause_game"].flag(false);
        m_timer = m_relaunch;
    }
    TutorialEvent run() override {  // 0x1c9aec
        TutorialEvent e;
        e.type = TutorialEvent::kShowPopup;
        e.text = m_text;
        e.character = m_character;
        e.closeButton = m_closeButton;
        e.pauseGame = m_pauseGame;
        return e;
    }
    // while its step waits, the box closed comes back every
    // time_to_relaunch seconds (0x1c9bd8)
    TutorialEvent update(float dt) override {
        if (m_surgery->popupShown() || m_relaunch == m_none) return {};
        m_timer -= dt;
        if (m_timer > 0) return {};
        m_timer = m_relaunch;
        return run();
    }

private:
    float m_none = -1;          // +8: "no relaunch"
    std::string m_text;         // +0xc
    std::string m_character;    // +0x10
    bool m_closeButton = false; // +0x14
    float m_relaunch = -1;      // +0x18
    float m_timer = -1;         // +0x1c
    bool m_pauseGame = false;   // +0x20
};

}  // namespace

TutorialMgr::TutorialMgr(const TutorialSurgery& surgery) : m_surgery(surgery) {}

TutorialMgr::~TutorialMgr() = default;

void TutorialMgr::start(const std::string& name) {
    // (0x1c122c) Downloaded/Tutorials/<name>.json (Data/Tutorials when the
    // content was never copied: the same files)
    json::Value root;
    m_name = name;
    if (!readContent("Tutorials/" + name + ".json", root)) return;
    for (const json::Value& s : root["steps"].items) {
        if (static_cast<int>(m_steps.size()) >= kMaxSteps) {
            logError("Tutorial %s: more than %d steps", name.c_str(), kMaxSteps);
            break;
        }
        Step step;
        auto conditions = [&](const char* key, std::vector<std::unique_ptr<TutorialCondition>>& out) {
            for (const json::Value& c : s[key].items)
                if (static_cast<int>(out.size()) < kMaxEntries)
                    if (auto p = makeCondition(c)) out.push_back(std::move(p));
        };
        auto actions = [&](const char* key, std::vector<std::unique_ptr<TutorialAction>>& out) {
            for (const json::Value& a : s[key].items)
                if (static_cast<int>(out.size()) < kMaxEntries)
                    if (auto p = makeAction(a)) out.push_back(std::move(p));
        };
        conditions("initial_conditions", step.initialConditions);
        actions("initial_actions", step.initialActions);
        conditions("end_conditions", step.endConditions);
        actions("end_actions", step.endActions);
        m_steps.push_back(std::move(step));
    }
    m_started = true;
    m_running = true;
    log("tutorial %s: %d steps", name.c_str(), static_cast<int>(m_steps.size()));
}

std::unique_ptr<TutorialCondition> TutorialMgr::makeCondition(const json::Value& v) {
    // (0x1c1c2c)
    const TutorialSurgery* s = &m_surgery;
    const std::string& type = v["type"].str();
    if (type == "FinishedInjuryState") return std::make_unique<FinishedInjuryStateTC>(s, v);
    if (type == "FinishedSurgeryStage") return std::make_unique<FinishedSurgeryStageTC>(s, v);
    if (type == "HealedInjury") return std::make_unique<HealedInjuryTC>(s, v);
    if (type == "HealthIncreases") return std::make_unique<HealthIncreasesTC>(s);
    if (type == "InSurgeryStage") return std::make_unique<InSurgeryStageTC>(s, v);
    if (type == "PopupIsClosed") return std::make_unique<PopupIsClosedTC>(s);
    if (type == "SelectedTool") return std::make_unique<SelectedToolTC>(s, v);
    if (type == "UsedTagPower") return std::make_unique<UsedTagPowerTC>(s);
    if (type == "FreedInjuryPieces") return std::make_unique<FreedInjuryPiecesTC>(s, v);
    if (type == "TimeElapsedOnStage") return std::make_unique<TimeElapsedOnStageTC>(s, v);  // (1.45 0x264d98)
    logError("Unrecognized tutorial condition %s", type.c_str());
    return nullptr;
}

std::unique_ptr<TutorialAction> TutorialMgr::makeAction(const json::Value& v) {
    // (0x1c201c)
    const TutorialSurgery* s = &m_surgery;
    const std::string& type = v["type"].str();
    using E = TutorialEvent;
    if (type == "ClosePopup") return std::make_unique<PlainTA>(s, E::kClosePopup);
    if (type == "DisableTagPowerRegeneration") return std::make_unique<TagPowerRegenerationTA>(s, false);
    if (type == "DisableTool") return std::make_unique<NamedTA>(s, E::kDisableTool, v["tool_name"].str());
    if (type == "DisableUIItem") return std::make_unique<NamedTA>(s, E::kDisableUIItem, v["ui_item_name"].str());
    if (type == "EnableTagPowerRegeneration") return std::make_unique<TagPowerRegenerationTA>(s, true);
    if (type == "EnableTool") return std::make_unique<NamedTA>(s, E::kEnableTool, v["tool_name"].str());
    if (type == "EnableUIItem") return std::make_unique<NamedTA>(s, E::kEnableUIItem, v["ui_item_name"].str());
    if (type == "HighlightInjury") return std::make_unique<InjuryTA>(s, E::kHighlightInjury, v["injury_name"].str());
    if (type == "HighlightTool") return std::make_unique<NamedTA>(s, E::kHighlightTool, v["tool_name"].str());
    if (type == "HighlightUIItem") return std::make_unique<NamedTA>(s, E::kHighlightUIItem, v["ui_item_name"].str());
    if (type == "RefillTagPower") return std::make_unique<PlainTA>(s, E::kRefillTagPower);
    if (type == "RemoveAllTutorialEffects") return std::make_unique<PlainTA>(s, E::kRemoveEffects);
    if (type == "ShowInjuryHitAreas") return std::make_unique<InjuryTA>(s, E::kShowHitAreas, v["injury_name"].str());
    if (type == "ShowPopup") return std::make_unique<ShowPopupTA>(s, v);
    if (type == "PlaySound")
        return std::make_unique<NamedTA>(s, E::kPlaySound, v.has("sound_name") ? v["sound_name"].str() : "normal");
    logError("Unrecognized tutorial action %s", type.c_str());
    return nullptr;
}

void TutorialMgr::post(const TutorialEvent& e) {
    // (0x1c83d4; the original's queue held 15 and wrote past it)
    if (e.type == TutorialEvent::kNone) return;
    if (static_cast<int>(m_events.size()) >= kMaxEvents) {
        logError("TutorialMgr: too many events in a frame");
        return;
    }
    m_events.push_back(e);
}

void TutorialMgr::runAll(std::vector<std::unique_ptr<TutorialAction>>& actions) {
    for (auto& a : actions) post(a->run());
}

void TutorialMgr::update(float dt) {
    // (0x1c25e0)
    if (!m_running) return;
    if (m_current != -1) {
        Step& step = m_steps[m_current];
        if (!step.done) {
            // every condition asked, even after one fails (some keep state)
            bool all = true;
            for (auto& c : step.endConditions) all = c->holds() && all;
            if (all) {
                runAll(step.endActions);
                step.done = true;
                log("tutorial step %d done", m_current + 1);
            } else {
                for (auto& a : step.initialActions) post(a->update(dt));
                return;
            }
        }
    }
    int next = m_current + 1;
    if (next >= static_cast<int>(m_steps.size())) {
        m_running = false;
        log("tutorial %s over", m_name.c_str());
        // (the port's: the unused "tutorial_success" jingle, Sounds/, which
        // no code or data of either build plays, as the last step is done)
        if (m_current != -1) SoundMgr::sharedSoundMgr()->playSound("tutorial_success", false);
        return;
    }
    bool all = true;
    for (auto& c : m_steps[next].initialConditions) all = c->holds() && all;
    if (!all) return;
    m_current = next;
    log("tutorial step %d begins", next + 1);
    runAll(m_steps[next].initialActions);
}

std::vector<TutorialEvent> TutorialMgr::takeEvents() {
    std::vector<TutorialEvent> out;
    out.swap(m_events);
    return out;
}

}  // namespace mt
