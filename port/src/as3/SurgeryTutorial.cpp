// SurgeryScreen's side of the in-game tutorial (TutorialMgr.h): the
// tutorial made and read at load, run each frame and its actions' events
// carried out (0x3c238), the tutorial box at the bottom of the HUD with its
// partner's picture, text and OK button (0x42b34, 0x3ebb8), the pause box at
// the top with the shade while a popup pauses the game (0x33524, 0x33a88),
// the HUD items a tutorial turns off (+0x2e68), the first surgery kept from
// being lost while its tutorial runs (0x35138), and the tutorial marked done
// at the win (0x3cc64 → 0xe7f4c).
#include <algorithm>

#include "as3/Cheats.h"
#include "as3/EffectList.h"
#include "as3/HudEffects.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryScreen.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCMenu.h"

namespace mt {

using namespace cocos2d;

namespace {

// how far the boxes slide in and out, canvas points (0x43078, 0x33738)
constexpr float kSlide = 80;

// a node of the HUD moved from `from` to `to` (0x42b34's and 0x3ebb8's moves)
void slide(CCNode* n, const CCPoint& from, const CCPoint& to, float time, bool in) {
    if (!n) return;
    n->setPosition(from);
    CCActionInterval* move = CCMoveTo::create(time, to);
    n->runAction(in ? static_cast<CCAction*>(CCEaseElasticOut::create(move, 0.6f))
                    : static_cast<CCAction*>(CCEaseElasticIn::create(move, 1.0f)));
}

CCPoint below(const CCPoint& p, float by) { return ccp(p.x, p.y - by); }

}  // namespace

void SurgeryScreen::startTutorial() {
    // (load, 0x41d38) the tool buttons' state (0x3a2b0(1): a tool the
    // surgery offers is on, any other off), the tutorial box's places, then
    // the tutorial, unless this slot has done it
    for (int t = 0; t < kToolCount; ++t) {
        bool offered = std::find(m_def.tools.begin(), m_def.tools.end(), t) != m_def.tools.end();
        std::string n = std::string("ingame_tool_") + kToolNames[t];
        m_enabledItems[n] = offered;
        if (auto* tg = dynamic_cast<CCMenuItemToggle*>(ui.node(n))) {
            tg->setSelectedIndex(offered ? 0 : 2);
            tg->setEnabled(offered);
        }
    }
    m_enabledItems["ingame_tagpartner"] = false;
    auto at = [&](const char* name) { CCNode* n = ui.node(name); return n ? n->getPosition() : CCPointZero; };
    m_tutorialBoxAt = at("ingame_tutorial_box");          // +0x2e80
    m_tutorialPartnerAt = at("ingame_tutorial_partner");  // +0x2e88
    m_tutorialPauseAt = at("ingame_tutorial_pause_box");  // +0x2e90
    m_tutorialOkAt = at("ingame_tutorial_ok");            // +0x2e98
    m_tutorialTextAt = at("tutorial");                    // +0x2ea0

    TutorialSurgery s;
    s.stage = [this] { return m_stage; };
    s.stageCount = [this] { return static_cast<int>(m_runs.size()); };
    s.stageDone = [this](int i) { return i >= 0 && i < static_cast<int>(m_runs.size()) && m_runs[i].done; };
    s.injuries = [this](int i) -> const std::vector<InjuryState*>* {
        return i >= 0 && i < static_cast<int>(m_runs.size()) ? &m_runs[i].injuries : nullptr;
    };
    s.treated = [this](int i) -> const std::vector<InjuryState*>* {
        return i >= 0 && i < static_cast<int>(m_runs.size()) ? &m_runs[i].treated : nullptr;
    };
    s.health = [this] { return m_health; };
    s.stageTime = [this] { return m_stageTime; };
    s.tool = [this]() -> const ToolState* { return m_tool; };
    s.popupShown = [this] { return m_popupShown; };
    s.tagPowerReady = [this] { return m_tagReady; };
    s.setTagPowerRegeneration = [this](bool on) { m_tagCharging = on; };
    m_tutorial.reset(new TutorialMgr(s));
    if (m_def.tutorial.empty()) return;
    // (debug: cheat 12 skips every tutorial, cheat 13 plays the completed
    // ones again: 1.45 loadAssets 0x50468, Cheats.h)
    if (cheat(Cheats::kSkipTutorials)) return;
    const std::vector<std::string>& done = SaveData::shared().currentSlot().completedTutorials;
    if (std::find(done.begin(), done.end(), m_def.tutorial) != done.end() && !cheat(Cheats::kReplayTutorials)) return;
    m_tutorial->start(m_def.tutorial);
}

void SurgeryScreen::runTutorial(float dt) {
    // (0x3c238) a frame of the tutorial, then its actions' events
    if (!m_tutorial) return;
    m_tutorial->update(dt);
    for (const TutorialEvent& e : m_tutorial->takeEvents()) {
        switch (e.type) {
        case TutorialEvent::kClosePopup:
            closeTutorialPopup(false);
            break;
        case TutorialEvent::kDisableTool:
        case TutorialEvent::kEnableTool: {
            // the toggle shown off (2) or on (0), and the tool given or not
            bool on = e.type == TutorialEvent::kEnableTool;
            std::string n = "ingame_tool_" + e.text;
            if (auto* tg = dynamic_cast<CCMenuItemToggle*>(ui.node(n))) {
                tg->setSelectedIndex(on ? 0 : 2);
                tg->setEnabled(on);
            }
            m_enabledItems[n] = on;
            log("tutorial tool %s %s", e.text.c_str(), on ? "enabled" : "disabled");
            break;
        }
        case TutorialEvent::kDisableUIItem:
        case TutorialEvent::kEnableUIItem:
            if (auto* item = dynamic_cast<CCMenuItem*>(ui.node(e.text)))
                item->setEnabled(e.type == TutorialEvent::kEnableUIItem);
            log("tutorial item %s %s", e.text.c_str(),
                e.type == TutorialEvent::kEnableUIItem ? "enabled" : "disabled");  // (for test scripts)
            break;
        case TutorialEvent::kHighlightInjury:
            // (guarded: the original made it for an injury it didn't find)
            if (e.injury) {
                m_effects.add(HighlightEffect::create(e.injury));
                log("tutorial highlight injury #%d", e.injury->soundChannel);
            }
            break;
        case TutorialEvent::kHighlightTool:
        case TutorialEvent::kHighlightUIItem: {
            // an arrow beside the item, by its anchoring
            std::string n = e.type == TutorialEvent::kHighlightTool ? "ingame_tool_" + e.text : e.text;
            CCNode* item = ui.node(n);
            int mode = kUIAbsolute;
            for (const UIItemDef& d : m_hudDef.items)
                if (d.name == n) mode = d.mode;
            // (the port's bar across the top, SurgeryHud.cpp: under a tool, pointing up)
            if (m_topBar && n.compare(0, 12, "ingame_tool_") == 0) mode = kUITop;
            if (item) m_effects.add(TutorialArrowEffect::create(item, mode));
            break;
        }
        case TutorialEvent::kRemoveEffects:
            m_effects.removeTutorialEffects();
            log("tutorial effects removed");
            break;
        case TutorialEvent::kShowHitAreas:
            if (e.injury && e.injury->markersNode) e.injury->buildMarkers(true);
            break;
        case TutorialEvent::kShowPopup:
            showTutorialPopup(e.text, e.character, e.closeButton);
            if (e.pauseGame) {
                // the game stops until OK (0x33d18 with every HUD button
                // but OK turned off, the pause box)
                endStage(false);
                pauseHudForTutorial();
                showTutorialPause();
            }
            break;
        case TutorialEvent::kPlaySound:
            SoundMgr::sharedSoundMgr()->playSound(e.text, false);
            break;
        case TutorialEvent::kRefillTagPower:
            // the tag partner's power full and ready (+0xf94 = +0xfa0, +0xfa4)
            m_tagCharge = m_tagNeeded;
            m_tagReady = true;
            chargeTag();
            break;
        default:
            logError("TutorialActionEvent of type %d not recognized", e.type);
        }
    }
}

void SurgeryScreen::showTutorialPopup(const std::string& text, const std::string& character, bool closeButton) {
    // (0x42b34) the box, the partner and the text spring up from below the
    // screen, unless already up; the OK button if it may be closed
    SoundMgr::sharedSoundMgr()->playSound("tutorial_show_swoosh", false);
    CCNode* box = ui.node("ingame_tutorial_box");
    CCNode* partner = ui.node("ingame_tutorial_partner");
    CCNode* ok = ui.node("ingame_tutorial_ok");
    CCLabelBMFont* label = ui.label("tutorial");
    const float by = static_cast<float>(static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * kSlide));
    if (!m_popupShown) {
        for (CCNode* n : {box, partner, static_cast<CCNode*>(label), ok})
            if (n) n->stopAllActions();
        for (CCNode* n : {box, partner, static_cast<CCNode*>(label)})
            if (n) n->setVisible(true);
        slide(box, below(m_tutorialBoxAt, by), m_tutorialBoxAt, 1.0f, true);
        slide(partner, below(m_tutorialPartnerAt, by), m_tutorialPartnerAt, 1.0f, true);
        slide(label, below(m_tutorialTextAt, by), m_tutorialTextAt, 1.0f, true);
        slide(ok, below(m_tutorialOkAt, by), m_tutorialOkAt, 1.0f, true);
    }
    if (ok) ok->setVisible(closeButton);
    if (label) label->setString(text.c_str());
    // (the original told its analytics the popup was shown)
    ui.setItemImage(m_hudDef, "ingame_tutorial_partner", character);
    m_popupShown = true;
    // (traced for test scripts: the text, and the OK button's centre, cocos points)
    if (ok && closeButton) {
        CCRect bb = ok->boundingBox();
        CCPoint c = ccp(m_tutorialOkAt.x + bb.size.width * 0.5f, m_tutorialOkAt.y + bb.size.height * 0.5f);
        log("tutorial popup ok %.0f,%.0f: %s", c.x, c.y, text.c_str());
    } else {
        log("tutorial popup: %s", text.c_str());
    }
}

void SurgeryScreen::closeTutorialPopup(bool instant) {
    // (0x3ebb8) at once: hidden; else, if up, the box drops below the screen
    if (instant)
        for (const char* n : {"ingame_tutorial_box", "ingame_tutorial_partner", "ingame_tutorial_ok", "tutorial"})
            ui.setVisible(n, false);
    if (m_popupShown) {
        SoundMgr::sharedSoundMgr()->playSound("tutorial_show_swoosh", false);
        if (!instant) {
            // Fixed: closed while it still sprang up (a step done within the
            // second, as the restored Useful Brother 1 tutorial's can be), the
            // original's spring (1 s) went on past the drop (0.35 s) and left
            // the box up over the patient: the spring is stopped first
            for (const char* n : {"ingame_tutorial_box", "ingame_tutorial_partner", "ingame_tutorial_ok", "tutorial"})
                if (CCNode* node = ui.node(n)) node->stopAllActions();
            const float by = static_cast<float>(static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * kSlide));
            slide(ui.node("ingame_tutorial_box"), m_tutorialBoxAt, below(m_tutorialBoxAt, by), 0.35f, false);
            slide(ui.node("ingame_tutorial_partner"), m_tutorialPartnerAt, below(m_tutorialPartnerAt, by), 0.35f, false);
            slide(ui.label("tutorial"), m_tutorialTextAt, below(m_tutorialTextAt, by), 0.35f, false);
            slide(ui.node("ingame_tutorial_ok"), m_tutorialOkAt, below(m_tutorialOkAt, by), 0.35f, false);
        }
        log("tutorial popup closed");
    }
    m_popupShown = false;
}

void SurgeryScreen::showTutorialPause() {
    // (0x33524) the shade, and the pause box springing down from above the
    // screen, unless already there
    CCNode* box = ui.node("ingame_tutorial_pause_box");
    m_effects.add(TutorialGradientEffect::create());
    if (!box) return;
    box->stopAllActions();
    if (box->getPosition().y == m_tutorialPauseAt.y && box->isVisible()) return;
    const float by = static_cast<float>(static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * kSlide));
    slide(box, ccp(m_tutorialPauseAt.x, m_tutorialPauseAt.y + by), m_tutorialPauseAt, 1.0f, true);
    box->setVisible(true);
}

void SurgeryScreen::hideTutorialPause(bool instant) {
    // (0x33a88) at once: hidden, the shade gone; else the box goes back up
    // and the shade fades
    CCNode* box = ui.node("ingame_tutorial_pause_box");
    if (box) box->stopAllActions();
    if (instant) {
        if (box) box->setVisible(false);
        m_effects.removeType(kEffectTutorialGradient);
        return;
    }
    const float by = static_cast<float>(static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * kSlide));
    CCPoint up = ccp(m_tutorialPauseAt.x, m_tutorialPauseAt.y + by);
    if (!box || box->getPosition().y == up.y) return;
    slide(box, m_tutorialPauseAt, up, 0.35f, false);
    for (Effect* e : m_effects.activeOfType(kEffectTutorialGradient))
        static_cast<TutorialGradientEffect*>(e)->fadeOut();
}

void SurgeryScreen::pauseHudForTutorial() {
    // (0x33d18's HUD part) every button and toggle off but the tutorial's
    // OK, as they were kept to be put back
    m_tutorialPaused = true;
    m_pausedItems.clear();
    for (size_t i = 0; i < m_hudDef.items.size(); ++i) {
        const UIItemDef& d = m_hudDef.items[i];
        if (d.type != kUIButton && d.type != kUIToggle) continue;
        auto* item = dynamic_cast<CCMenuItem*>(ui.node(d.name));
        if (!item) continue;
        m_pausedItems.emplace_back(item, item->isEnabled());
        if (d.name != "ingame_tutorial_ok") item->setEnabled(false);
    }
}

void SurgeryScreen::tutorialOk() {
    // (0x30d9c, "ingame_tutorial_ok"; not once the patient is dead) the
    // game goes on (0x33e04), the pause box goes, the box drops
    if (m_health <= 0) return;
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    // Fixed: the original's resume undid any pause (a partner coming in); only
    // the tutorial's own is undone here
    if (m_tutorialPaused) {
        m_tutorialPaused = false;
        resumeGame();
        for (auto& p : m_pausedItems) p.first->setEnabled(p.second);
        m_pausedItems.clear();
    }
    hideTutorialPause(false);
    closeTutorialPopup(false);
}

void SurgeryScreen::clampForTutorial() {
    // (0x35138) the first surgery can't be lost while its tutorial is on:
    // at least a second left and 5 health
    if (m_surgery.id != "surgery_bunker_mrgiblets" || !m_tutorial || !m_tutorial->started()) return;
    if (m_time <= 1.0f) m_time = 1.0f;
    if (m_health <= 5.0f) m_health = m_healthShown = 5.0f;
}

void SurgeryScreen::completeTutorial() {
    // (0x3cc64 → 0xe7f4c) a won surgery's tutorial is done for this slot
    if (!m_tutorial || !m_tutorial->started()) return;
    std::vector<std::string>& done = SaveData::shared().currentSlot().completedTutorials;
    if (std::find(done.begin(), done.end(), m_def.tutorial) != done.end())
        log("Tutorial %s was already completed", m_def.tutorial.c_str());
    else
        done.push_back(m_def.tutorial);
}

}  // namespace mt
