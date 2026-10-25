// mt::TutorialMgr (0x10bc bytes; made by SurgeryScreen::load, 0x1c119c, at
// the screen's +0x2e64): a surgery's in-game tutorial, from
// Downloaded/Tutorials/<name>.json (0x1c122c). Its steps (15 at most) each
// have initial conditions and actions, end conditions and actions
// (16 of each at most). A step begins when the step before it is done and
// all its initial conditions hold: its initial actions run. It is done when
// all its end conditions hold (at once if it has none): its end actions run.
// While a step waits for its end, its initial actions are updated each frame
// (ShowPopupTA shows its popup again every time_to_relaunch seconds while it
// is closed). After the last step the tutorial is over.
//
// The actions don't act on the screen themselves: each gives an event
// (TutorialActionEvent, 0x18 bytes; a queue of 15 at +0xf4c, count +0x10b4)
// that the screen carries out after the update (0x3c238). The conditions
// read the surgery (the original passed them the screen's +0x8dc).
//
// The conditions (vf2: holds?) and actions (vf2: run, vf3: update) are
// mt::TutorialCondition / mt::TutorialAction subclasses, by their JSON
// "type"; an unknown one is logged and left out (0x1c1c2c, 0x1c201c).
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/Json.h"

namespace mt {

class InjuryState;
class ToolState;

// What the conditions and actions read of the surgery (the screen's +0x8dc
// block in the original; filled by the screen).
struct TutorialSurgery {
    std::function<int()> stage;           // +0x8f4: the stage shown (1-based)
    std::function<int()> stageCount;      // +0xf78 (the block's +0x69c)
    std::function<bool(int)> stageDone;   // the stage run's done flag (+0x84), by index (0-based)
    // the injuries still to treat of a stage (+0x88), by index (0-based)
    std::function<const std::vector<InjuryState*>*(int)> injuries;
    // the treated ones (+0x94): the port's fix for FinishedInjuryStateTC (below)
    std::function<const std::vector<InjuryState*>*(int)> treated;
    std::function<float()> health;        // +0x8e0
    std::function<float()> stageTime;     // 1.45 +0x70c: seconds on the stage shown
    std::function<const ToolState*()> tool;  // +0x8fc: the selected tool
    std::function<bool()> popupShown;     // +0x1008: the tutorial box is up
    // the tag partner's power is ready (+0xfa4) and its regeneration on
    // (+0xf91): with the tag partners (unset: never ready, ignored)
    std::function<bool()> tagPowerReady;
    std::function<void(bool)> setTagPowerRegeneration;

    // an injury by its name in a stage's list still to treat, or in any
    // stage (stage -1), the first found (0x77884)
    InjuryState* findInjury(const std::string& name, int stage = -1) const;
};

// What an action asks of the screen (TutorialActionEvent).
struct TutorialEvent {
    enum Type {
        kNone = 0,
        kClosePopup = 1,         // ClosePopupTA
        kDisableTool = 2,        // DisableToolTA: text = the tool's name
        kDisableUIItem = 3,      // DisableUIItemTA: text = the item's name
        kEnableTool = 4,         // EnableToolTA
        kEnableUIItem = 5,       // EnableUIItemTA
        kHighlightInjury = 6,    // HighlightInjuryTA: injury
        kHighlightTool = 7,      // HighlightToolTA: text = the tool's name
        kHighlightUIItem = 8,    // HighlightUIItemTA: text = the item's name
        kRemoveEffects = 9,      // RemoveAllTutorialEffectsTA
        kShowHitAreas = 10,      // ShowInjuryHitAreasTA: injury
        kShowPopup = 11,         // ShowPopupTA: text, character, closeButton, pauseGame
        kPlaySound = 12,         // PlaySoundTA: text = the sound
        kRefillTagPower = 13,    // RefillTagPowerTA
    };
    int type = kNone;                // +0
    std::string text;                // +4
    std::string character;           // +8
    int value = 0;                   // +0xc
    bool closeButton = false;        // +0x10
    bool pauseGame = false;          // +0x11
    InjuryState* injury = nullptr;   // +0x14
};

class TutorialCondition {
public:
    explicit TutorialCondition(const TutorialSurgery* surgery) : m_surgery(surgery) {}
    virtual ~TutorialCondition() = default;
    virtual bool holds() = 0;  // vf2

protected:
    const TutorialSurgery* m_surgery;  // +4
};

class TutorialAction {
public:
    explicit TutorialAction(const TutorialSurgery* surgery) : m_surgery(surgery) {}
    virtual ~TutorialAction() = default;
    virtual TutorialEvent run() = 0;                     // vf2
    virtual TutorialEvent update(float) { return {}; }   // vf3 (0x1c8420: none)

protected:
    const TutorialSurgery* m_surgery;  // +4
};

class TutorialMgr {
public:
    static constexpr int kMaxSteps = 15, kMaxEntries = 16, kMaxEvents = 15;

    explicit TutorialMgr(const TutorialSurgery& surgery);  // 0x1c11ac
    ~TutorialMgr();                                        // 0x1c1060
    TutorialMgr(const TutorialMgr&) = delete;
    TutorialMgr& operator=(const TutorialMgr&) = delete;

    // Reads the tutorial (0x1c122c) and starts it.
    void start(const std::string& name);
    // A frame (0x1c25e0): the current step's end, the next step's start.
    void update(float dt);
    // The events of this frame's actions, taken (the screen's 0x3c238).
    std::vector<TutorialEvent> takeEvents();

    // a tutorial was started for this surgery (+8): the screen keeps the
    // first surgery from being lost while it runs, and the win marks it done
    bool started() const { return m_started; }
    // its steps are still running (+9)
    bool running() const { return m_running; }
    int currentStep() const { return m_current; }
    const std::string& name() const { return m_name; }

private:
    struct Step {  // 0x104 bytes
        std::vector<std::unique_ptr<TutorialCondition>> initialConditions;  // +0, count +0x3c
        std::vector<std::unique_ptr<TutorialAction>> initialActions;        // +0x40, count +0x7c
        std::vector<std::unique_ptr<TutorialCondition>> endConditions;      // +0x80, count +0xbc
        std::vector<std::unique_ptr<TutorialAction>> endActions;            // +0xc0, count +0xfc
        bool done = false;                                                  // +0x100
    };
    std::unique_ptr<TutorialCondition> makeCondition(const json::Value& v);  // 0x1c1c2c
    std::unique_ptr<TutorialAction> makeAction(const json::Value& v);        // 0x1c201c
    void post(const TutorialEvent& e);                                       // 0x1c83d4
    void runAll(std::vector<std::unique_ptr<TutorialAction>>& actions);

    TutorialSurgery m_surgery;   // +0x10b8 (the original kept a pointer)
    int m_current = -1;          // +4
    bool m_started = false;      // +8
    bool m_running = false;      // +9
    std::vector<Step> m_steps;   // +0xc (count +0xf48)
    std::vector<TutorialEvent> m_events;  // +0xf4c (count +0x10b4)
    std::string m_name;
};

}  // namespace mt
