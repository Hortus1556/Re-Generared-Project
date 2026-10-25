// The surgery's part of the F1 debug menu (debug mode only; DebugMenu.h).
// Not in the original: what a tester asks of a running surgery, done through
// the screen's own parts so the game goes on as it would (a stage taken away
// is ended by the stage's own logic, a win goes through the results and the
// save).
#include <algorithm>

#include "as3/ClampsController.h"
#include "as3/Log.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryScreen.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCMenu.h"

namespace mt {

using namespace cocos2d;

bool SurgeryScreen::s_debugGodMode = false;

SurgeryScreen* SurgeryScreen::running() {
    // the screens are the scene manager's children (a pause or a popup over
    // the surgery too)
    CCArray* kids = SceneMgr::sharedSceneMgr()->getChildren();
    if (!kids) return nullptr;
    for (CCObject* o : kids->data)
        if (SurgeryScreen* s = dynamic_cast<SurgeryScreen*>(o)) return s;
    return nullptr;
}

bool SurgeryScreen::debugPlayable() {
    return SceneMgr::sharedSceneMgr()->topScene() == this && !m_over && !m_runs.empty() && m_hud;
}

void SurgeryScreen::debugWin() {
    if (m_over || m_runs.empty()) return;
    // every stage counted done (the results' progress), then the game's own win
    m_completedStages = static_cast<int>(m_def.stages.size());
    m_weightDone = 0;
    for (const SurgeryStageDef& st : m_def.stages) m_weightDone += st.weight;
    log("debug: win now");
    win();
}

void SurgeryScreen::debugLose() {
    if (m_over || m_runs.empty()) return;
    log("debug: lose now");
    m_health = m_healthShown = 0;
    lose();
}

void SurgeryScreen::debugNextStage() {
    if (m_over || m_runs.empty()) return;
    // The tool let go of (the tongs may hold a piece of one), what the tray
    // shows taken away, the clamps let go of (as Brutality's baton does).
    deselectTool();
    dropHeld();
    if (m_trayShown) hideTray();
    m_trayPiece = nullptr;
    m_trayInjury = nullptr;
    log("debug: stage %d taken away", m_stage);
    std::vector<InjuryState*> list = run().injuries;
    for (InjuryState* i : list) {
        if (i->finished) continue;
        if (ClampsController* c = clamps()) {
            std::vector<Clamp*> on = i->clamps;
            for (Clamp* k : on) {
                i->removeClamp(k);
                c->releaseClamp(k);
            }
        }
        if (i->toSurgeryStage != 0 && i->m_stage - 1 < i->stageEndToSwap) {
            // An injury that sends the surgery to another stage when it gets
            // past one of its stages (an incision cut open): its stages done
            // by its own logic (as the baton's strikes) up to that one, so the
            // screen makes the jump as in play (drainInjuryEvents) and the
            // injury is as play leaves it when the surgery comes back.
            for (int n = 0; n < 64 && !i->finished && i->m_stage - 1 < i->stageEndToSwap; ++n) i->completeStage();
            continue;
        }
        // Any other counts as finished and hides; the next frame the screen
        // removes them (their areas with them) and, the stage empty, goes on
        // to its next stage or the win.
        i->finished = true;
        if (i->root) i->root->setVisible(false);
    }
}

void SurgeryScreen::debugHeal() {
    if (m_health < 100) m_health = m_healthShown = 100;
}

void SurgeryScreen::debugAddTime(float seconds) { m_time += seconds; }

bool SurgeryScreen::debugHasPartner() { return !m_partnerId.empty() && m_tagButton && m_tagButton->isVisible(); }

void SurgeryScreen::debugFillTag() {
    if (!debugHasPartner() || m_powerRunning) return;
    // as the last action charging it would (chargeTag), the button on again
    if (!m_tagReady) SoundMgr::sharedSoundMgr()->playSound("Tag_Power_Ready", false);
    m_tagCharging = true;
    m_tagCharge = m_tagNeeded;
    m_tagReady = true;
    m_tagButton->setEnabled(true);
    log("debug: tag charge full");
    refreshTagButton();
}

void SurgeryScreen::debugFreeTools() {
    // every tool's button shown and usable, whatever the surgery offers or
    // a tutorial or a frost turned off
    m_toolsFrozen = false;
    m_frozenTimer = 0;
    for (int t = 0; t < kToolCount; ++t) {
        const std::string n = std::string("ingame_tool_") + kToolNames[t];
        ui.setVisible(n, true);
        m_enabledItems[n] = true;
        if (CCMenuItemToggle* tg = dynamic_cast<CCMenuItemToggle*>(ui.node(n))) {
            tg->setEnabled(true);
            tg->setSelectedIndex(m_tool && m_tool->type == t ? 1 : 0);
            // (under a tutorial's pause: on again when it ends too)
            for (auto& p : m_pausedItems)
                if (p.first == tg) p.second = true;
        }
    }
    m_enabledItems["ingame_tool_Injection_out"] = true;
    log("debug: every tool free");
}

void SurgeryScreen::debugLeave() { cleanUp(); }

void SurgeryScreen::debugGodMode() {
    // (checkEnd, each frame) the heart and the clock kept up: nothing can
    // stop the heart (a sudden death's mistake neither) nor run the time out
    if (!s_debugGodMode) return;
    if (m_health < 100) m_health = m_healthShown = 100;
    if (m_time < 60) m_time = 60;
}

}  // namespace mt
