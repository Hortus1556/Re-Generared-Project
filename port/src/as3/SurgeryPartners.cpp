// SurgeryScreen's effects and tag partner (re/as3/notes/surgery.md, "Effects"
// and "Tag partners"): kept apart from SurgeryScreen.cpp, which calls in
// here from its touches, update, events and HUD.
//
// The tag partner: chosen before the surgery (SavedGame::selectedTagPartner),
// its button at the top right fills as the combo actions are done (the
// partner's actions_to_unlock; 4/5 of them with the reduced cooldown
// upgrade; Nowe and Crumpet come full). Pressed when full: the game pauses,
// the first time the partner's TagInfoScreen explains the power, then the
// high five (HighFiveEffect), and the power (0x36204 its effects, 0x36778
// what it does):
//   Mr Giblets   heals 24 (36 with the tag team upgrade), slowly;
//   Saturday     a revive: at death the heart comes back with 30 (42);
//   Reverend     30 s (42): the right tool is picked for what is touched,
//                and no tool hurts the patient;
//   Flow Master  30 s (42): the combo doubled;
//   Bot          12 s (16.8): the clock stands still, heals 15 (21);
//   Nowe         the coins doubled (this surgery);
//   Crumpet      every upgrade for this surgery;
//   Brutality    10 s: the tools put away, a baton smashes an injury done
//                every 0.4 s.
// Once the first two surgeries are done, using the power starts the
// partner's cooldown and unselects it.
#include <algorithm>
#include <cmath>

#include "as3/Cheats.h"
#include "as3/ClampsController.h"
#include "as3/EditorPlay.h"
#include "as3/EventEffects.h"
#include "as3/HudEffects.h"
#include "as3/Injection.h"
#include "as3/Log.h"
#include "as3/PartnerEffects.h"
#include "as3/PartnerInfoUtils.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/SurgeryScreen.h"
#include "as3/TagInfoScreen.h"
#include "as3/ToolEffects.h"
#include "as3/Tools2.h"
#include "as3/UpgradeInfoUtils.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCClippingNode.h"
#include "engine/cocos/CCDrawNode.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

// the partners' stats (StatsMgr 0x440ad4: ids 0x15..0x1e by 0x799c0),
// counters in the slot's statProgress: one not yet counted shows the
// power's TagInfoScreen first
const char* const kTaggedStats[8] = {"TaggedGiblets", "TaggedSaturday", "TaggedReverand", "TaggedFlow",
                                     "TaggedBot",     "TaggedNowe",     "TaggedCrumpet",  "TaggedBrutality"};

bool owns(const char* upgrade) {
    return SaveData::shared().currentSlot().owns(SavedGame::kItemUpgrade, upgradeIndex(upgrade));
}

// a StatsMgr counter (0x2223ec, 1.45 incrementStat 0x2f56ac): the slot's
// statProgress, its achievements checked (StatsMgr.h)
void addStat(const char* name) { StatsMgr::sharedInstance()->incrementStat(name); }

}  // namespace

// ---------------------------------------------------------------- effects

EffectList::ToolContext SurgeryScreen::toolContext() {
    EffectList::ToolContext c;
    c.touch = m_touch;
    c.stageBackground = stageDef().bg;
    c.bg = m_bg;
    c.injuryLayer = m_injuryLayer;
    c.bgExtras = m_bgExtras;
    // the Scanner's +0x52c (0x65318): 0.9 s, 0.5 with its cooldown upgrade
    // (or Crumpet's power: ToolState::applyAllUpgrades)
    auto sc = m_tools.find(kScanner);
    c.scannerInterval = sc != m_tools.end() ? static_cast<Scanner*>(sc->second)->interval
                        : owns("extraction_scanner_cooldown_upgrade") ? 0.5f : 0.9f;
    return c;
}

void SurgeryScreen::showTag(int kind, int points, const CCPoint& at, bool coins) {
    // Nowe's coins sparkle with every popup while her power runs
    if (coins && m_tagUsed && m_partner == 5) {
        CoinTagEffect* c = CoinTagEffect::create();
        c->setPosition(at);
        m_effects.add(c);
    }
    TagEffect* t = TagEffect::create(kind, points, m_combo.doubled != 0);
    t->setPosition(ccp(at.x, at.y + t->getContentSize().height * 0.5f));
    m_effects.add(t);
}

void SurgeryScreen::comboPopup() {
    // (0x3dbbc) an action that scores: "NAILED IT" after a run of wrong
    // tries, else praise by the points
    int kind = m_combo.repeatedWrong() ? TagEffect::kCombo : TagEffect::kGood;
    int points = m_combo.action();
    if (m_def.tagAllowed) chargeTag();
    showTag(kind, points, m_touch, true);
}

void SurgeryScreen::effectFinished(Effect* e) {
    // (0x35cf0) what an effect's end starts
    switch (e->getType()) {
    case kEffectHighFive:
        // the partner is in: the game goes on, with its power
        resumeGame();
        startPartnerEffects();
        startPartnerPower();
        break;
    case kEffectWin:
        win();
        break;
    case kEffectSaturdayHeart:
        // the heart reached the meter: sparkles fly from it
        if (CCNode* heart = ui.node("ingame_comboheart")) {
            SaturdayParticleEffect* p = SaturdayParticleEffect::create();
            p->setPosition(heart->getPosition());
            p->moveTo(ccp(static_cast<float>(heart->getPositionX() * 0.45),
                          static_cast<float>(heart->getPositionY() * 0.94)));
            m_effects.add(p);
        }
        break;
    case kEffectBatonIn: {
        // the baton strikes: its injury's stage done, with a popup
        BatonInEffect* b = static_cast<BatonInEffect*>(e);
        InjuryState* inj = b->injury();
        auto it = std::find(m_batoned.begin(), m_batoned.end(), inj);
        if (it == m_batoned.end() || !inj) break;  // (gone with its stage)
        m_batoned.erase(it);
        int kind = inj->createdLater ? TagEffect::kPartner : TagEffect::kGood;
        int points = m_combo.action();
        if (m_def.tagAllowed) chargeTag();
        BatonOutEffect* out = BatonOutEffect::create(b->angle());
        out->setPosition(b->baton()->getPosition());
        m_effects.add(out);
        BrutalityParticlesEffect* p = BrutalityParticlesEffect::create();
        p->setPosition(inj->position);
        m_effects.add(p);
        showTag(kind, points, inj->position, false);
        // the clamps on it let go (0x69150, its vf24)
        if (ClampsController* c = clamps()) {
            std::vector<Clamp*> on = inj->clamps;
            for (Clamp* k : on) {
                inj->removeClamp(k);
                c->releaseClamp(k);
            }
        }
        if (!inj->finished) inj->completeStage();
        // a particle struck: every particle of the stage goes with it (0x37288)
        if (inj->def->base == kPassiveParticle || inj->def->base == kSoakableParticle) {
            for (InjuryState* i : run().injuries)
                if (i->def->base == kPassiveParticle || i->def->base == kSoakableParticle) i->finished = true;
            removeTreated();
        }
        break;
    }
    default:
        break;
    }
}

void SurgeryScreen::effectsFrame(float dt) {
    // (0x35c4c) the effects' frame, the ended ones gone (effectFinished);
    // the chainsaw's trail at the tool, shown while it is held (0x34c40,
    // 0x39624: hidden again for a frame it cuts what spurts)
    m_effects.update(dt, this);
    if (!m_trail) return;
    bool on = m_tool && m_tool->active && m_tool->type == kChainsaw && m_showTrail;
    if (on) m_trail->setPosition(m_tool->position);
    m_trail->setVisible(on);  // (its emitters on or off; the node stays shown)
    if (!(m_tool && m_tool->active)) m_showTrail = false;
}

// ---------------------------------------------------------------- pausing

void SurgeryScreen::resumeGame() {
    // (0x33e04) the injuries' animations go on (0x43438), and their
    // particle systems (vf29) if the game was frozen
    const bool wasFrozen = m_ending;
    m_ending = false;
    pauseInjuryAnimations(false);
    if (wasFrozen)
        for (InjuryState* i : run().injuries) i->resume();
}

// ---------------------------------------------------------------- the tag partner

void SurgeryScreen::setupTagPartner() {
    // (0x41d38) the partner chosen, the actions its power takes
    SavedGame& slot = SaveData::shared().currentSlot();
    m_partnerId = slot.selectedTagPartner;
    if (editor::isEditedPlay(m_params)) m_partnerId.clear();  // (a surgery editor's level: no partner, EditorPlay.h)
    m_tagCharge = 0;
    m_tagReady = false;
    m_tagUsed = false;
    m_tagCharging = true;
    m_tagNeeded = 0;
    m_partner = -1;
    if (!m_partnerId.empty()) {
        m_partner = partnerIndex(m_partnerId);
        if (const PartnerInfo* p = PartnerInfoUtils::shared()->partner(m_partnerId))
            m_tagNeeded = static_cast<float>(p->actionsToUnlock);
    }
    if (owns("score_reduced_tag_team_cooldown") || slot.allUpgrades)
        m_tagNeeded = std::floor(m_tagNeeded * 0.8f);
    // Nowe's and Crumpet's powers are ready at once
    if (m_partner == 5 || m_partner == 6) {
        m_tagCharge = m_tagNeeded;
        m_tagReady = true;
    }
}

void SurgeryScreen::buildTagButton() {
    // (0x3f298) the partner's picture in the button, greyed above its charge
    m_tagButton = dynamic_cast<CCMenuItemToggle*>(ui.node("ingame_tagpartner"));
    if (m_partnerId.empty()) {
        if (m_tagButton) m_tagButton->setVisible(false);
        ui.setVisible("ingame_tag_image", false);
        return;
    }
    if (m_tagButton) m_tagButton->setVisible(true);
    std::string name = partnerName(m_partnerId);
    ui.setVisible("ingame_tag_image", true);
    ui.setItemImage(m_hudDef, "ingame_tag_image", name);
    CCNode* image = ui.node("ingame_tag_image");
    CCSprite* grey = CCSprite::createWithSpriteFrameName(("ingame_tag_partner-" + name + ".png").c_str());
    if (!image || !grey) return;
    // (0x2eae4) clipped by a rectangle the picture's size, which the
    // charge moves up (refreshTagButton)
    CCSize s = grey->getContentSize();
    grey->setAnchorPoint(CCPointZero);
    grey->setColor(ccc3(0x7f, 0x7f, 0x7f));
    CCDrawNode* rect = CCDrawNode::create();
    CCPoint corners[4] = {ccp(0, 0), ccp(s.width, 0), ccp(s.width, s.height), ccp(0, s.height)};
    ccColor4F white = {1, 1, 1, 1};
    rect->drawPolygon(corners, 4, white, 0, white);
    CCClippingNode* clip = CCClippingNode::create(rect);
    clip->addChild(grey);
    clip->setPosition(image->getPosition());
    m_hud->addChild(clip);
    m_tagGrey = clip;
    m_tagGreyMask = rect;
}

void SurgeryScreen::refreshTagButton() {
    // (0x37850) the charge shown; full: the button lit, sparkling
    if (m_partnerId.empty() || !m_tagButton) return;
    if (m_tagGreyMask && m_tagNeeded > 0) {
        CCNode* image = ui.node("ingame_tag_image");
        float h = image ? image->getContentSize().height : 0;
        m_tagGreyMask->setPositionY(std::ceil(std::min(1.0f, m_tagCharge / m_tagNeeded) * h));
    } else if (m_tagGreyMask) {
        m_tagGreyMask->setPositionY(10000);  // nothing to charge: all lit
    }
    m_enabledItems["ingame_tagpartner"] = m_tagReady;
    if (!m_tagReady) {
        m_tagButton->setSelectedIndex(2);
        if (m_effects.sparkle && m_effects.sparkle->isAlive()) m_effects.sparkle->stop();
        return;
    }
    if (!m_effects.sparkle) {
        SparkleEffect* s = SparkleEffect::create();
        m_effects.sparkle = s;
        m_effects.add(s);
        placeTagSparkle();
    }
    if (!m_effects.sparkle->isAlive()) m_effects.sparkle->restart();
    if (m_tagButton->getSelectedIndex() != 1) m_tagButton->setSelectedIndex(0);
}

void SurgeryScreen::placeTagSparkle() {
    // the sparkle on the button, a third across and halfway up from its
    // place (0x37850), wherever the layout put it (from the button's
    // parent's space to the screen's, where the effects are)
    if (!m_effects.sparkle || !m_tagButton || !m_tagButton->getParent()) return;
    const CCSize size = m_tagButton->getContentSize();
    const CCPoint at = ccp(m_tagButton->getPositionX() + size.width / 3.0f, m_tagButton->getPositionY() + size.height * 0.5f);
    m_effects.sparkle->setPosition(convertToNodeSpace(m_tagButton->getParent()->convertToWorldSpace(at)));
}

void SurgeryScreen::chargeTag() {
    // (0x37054) an action done: the partner's button fills
    if (m_partnerId.empty()) return;
    if (m_tagUsed && (m_partner == 5 || m_partner == 6)) return;
    if (!m_tagCharging || m_powerRunning) return;
    m_tagCharge += 1;
    log("tag charge %.0f/%.0f", m_tagCharge, m_tagNeeded);  // (traced for test scripts)
    if (m_tagCharge < m_tagNeeded) return;
    if (m_tagCharge == m_tagNeeded) SoundMgr::sharedSoundMgr()->playSound("Tag_Power_Ready", false);
    m_tagCharge = m_tagNeeded;
    m_tagReady = true;
}

void SurgeryScreen::tagPressed() {
    // (0x3eb38) the game pauses; the first time, the power explained first
    endStage(false);
    if (m_partner < 0 || m_partner > 7) return;
    SavedGame& slot = SaveData::shared().currentSlot();
    unsigned int seen = slot.statProgress[kTaggedStats[m_partner]];
    addStat(kTaggedStats[m_partner]);
    if (seen != 0) {
        startHighFive();
        return;
    }
    SceneMgr::sharedSceneMgr()->pushScene(
        TagInfoScreen::create(this, callfunc_selector(SurgeryScreen::tagInfoClosed)), false);
    // the pause box rises, the screen's top and bottom shaded (0x33524)
    showTutorialPause();
}

void SurgeryScreen::tagInfoClosed() {
    // (0x33740) the high five; the pause box and the shade go (0x33a88)
    startHighFive();
    hideTutorialPause(false);
}

void SurgeryScreen::startHighFive() {
    // (0x33758) the partner comes in (the game stays paused till it is done)
    addStat("UseTagPower");
    m_effects.highFiveIndex = m_effects.add(HighFiveEffect::create(m_partnerId));
    if ((m_partner == 5 || m_partner == 6) && m_tagButton) m_tagButton->setEnabled(false);
    m_tagCharge = 0;
    m_tagReady = false;
    // (its use told to the analytics: removed)
    if (m_tagUsed) return;
    // past the first two surgeries the partner cools down, unselected
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (slot.starsOf("surgery_bunker_mrgiblets", 0) != 0 && slot.starsOf("surgery_bunker_prxy4", 0) != 0) {
        startPartnerCooldown(m_partnerId);
        m_tagUsed = true;
        slot.selectedTagPartner.clear();
        save.saveGame();
    }
}

void SurgeryScreen::startPartnerEffects() {
    // (0x36204) the power's effects
    switch (m_partner) {
    case 0:  // Mr Giblets licks across from the canvas's top right (it places itself)
        m_effects.add(GibletsEffect::create());
        break;
    case 2: {  // the Reverend: the tools blessed, his light under the finger
        ReverandToolsEffect* t = ReverandToolsEffect::create();
        t->addToolParticles(ui);
        m_effects.add(t);
        ReverandCursorEffect* c = ReverandCursorEffect::create();
        c->setPosition(m_touch);
        m_effects.partnerIndex = m_effects.add(c);
        break;
    }
    case 3:  // Flow Master's glow on the score box
        if (CCNode* box = ui.node("ingame_combometer_scorebox")) {
            FlowMasterEffect* f = FlowMasterEffect::create();
            f->setPosition(box->getPosition());
            m_effects.add(f);
        }
        break;
    case 4:  // Bot's glow at the heart
        if (CCNode* heart = ui.node("ingame_comboheart")) {
            BotEffect* b = BotEffect::create();
            b->setPosition(heart->getPosition());
            m_effects.add(b);
        }
        break;
    case 6: {  // Crumpet: the tools shine
        ToolsSheenEffect* s = ToolsSheenEffect::create();
        s->addToolSheens(ui);
        m_effects.add(s);
        SoundMgr::sharedSoundMgr()->playSound("tool_boost_charge", false);
        break;
    }
    case 7:  // Brutality: the tools put away
        slideToolButtons(true);
        break;
    default:  // Saturday, Nowe: none
        break;
    }
}

void SurgeryScreen::startPartnerPower() {
    // (0x36778) the power itself
    const bool upgraded = owns("score_tag_team_upgrade");
    log("tag power %s starts", m_partnerId.c_str());  // (traced for test scripts)
    switch (m_partner) {
    case 0:  // Mr Giblets heals; the heart rises slowly to it
        m_health += upgraded ? 36.0f : 24.0f;
        break;
    case 1:  // Lady Saturday: a revive, the meter's heart bouncing in
        if (CCNode* heart = ui.node("ingame_comboheart")) {
            if (CCRGBAProtocol* rgba = dynamic_cast<CCRGBAProtocol*>(heart)) rgba->setOpacity(0);
            heart->setScale(2);
            CCFiniteTimeAction* bounce =
                CCSequence::create(CCEaseBounceInOut::create(CCScaleTo::create(0.25f, 1.25f)),
                                   CCEaseBounceInOut::create(CCScaleTo::create(0.25f, 1)), nullptr);
            heart->runAction(CCFadeIn::create(0.5f));
            heart->runAction(CCRepeat::create(bounce, 4));
        }
        ++m_hearts;
        break;
    case 2:  // the Reverend
        m_powerRunning = true;
        m_reverendTimer = upgraded ? 42.0f : 30.0f;
        break;
    case 3:  // Flow Master
        m_powerRunning = true;
        m_combo.setDoubled(true);
        m_combo.setFlag1c(true);
        m_flowTimer = m_flowTimer2 = upgraded ? 42.0f : 30.0f;
        break;
    case 4:  // Bot: the clock stopped, and a heal
        m_powerRunning = true;
        m_botTimer = upgraded ? 16.8f : 12.0f;
        m_health += upgraded ? 21.0f : 15.0f;
        break;
    case 6: {  // Crumpet: every upgrade, each tool tuned at once (0x667fc)
        for (ToolState* t : m_toolList) t->applyAllUpgrades();
        SaveData& save = SaveData::shared();
        save.currentSlot().allUpgrades = true;
        save.saveGame();
        break;
    }
    case 7:  // Brutality: the batons, the tool put down
        m_powerRunning = true;
        m_brutalityTimer = 10;
        m_batonTimer = 0.4f;
        hideTray();
        if (m_trail) m_trail->setVisible(false);
        if (m_tool && m_tool->active) touchEnded();
        break;
    default:  // Nowe: her coins at the end
        break;
    }
    // (0x35034: the heart's colour, with the HUD)
}

void SurgeryScreen::updatePartnerPowers(float dt) {
    // (0x3b2ec) the timed powers running out
    if (m_botTimer != 0) {
        m_botTimer -= dt;
        if (m_botTimer < 0) {
            for (Effect* e : m_effects.activeOfType(kEffectBot)) static_cast<BotEffect*>(e)->end();
            m_botTimer = 0;
            m_powerRunning = false;
            log("tag power %s ends", m_partnerId.c_str());
        }
    }
    if (m_flowTimer > 0) {
        m_flowTimer -= dt;
        if (m_flowTimer <= 0) {
            m_effects.removeType(kEffectFlowMaster);
            m_combo.setDoubled(false);
            m_powerRunning = false;
            log("tag power %s ends", m_partnerId.c_str());
        }
    }
    if (m_flowTimer2 > 0) {
        m_flowTimer2 -= dt;
        if (m_flowTimer2 <= 0) {
            m_combo.setFlag1c(false);
            m_powerRunning = false;
            log("tag power %s ends", m_partnerId.c_str());
        }
    }
    // (the Reverend's also stood still while a tutorial step held it, and
    // for ever with cheat 10: 1.45 runLogic 0x49348, Cheats.h)
    if (m_reverendTimer > 0 && !cheat(Cheats::kReverendForever)) {
        m_reverendTimer -= dt;
        if (m_reverendTimer <= 0) {
            m_effects.removeType(kEffectReverandTools);
            m_effects.removeType(kEffectReverandCursor);
            m_effects.partnerIndex = -1;
            m_effects.add(ReverandEndEffect::create());
            m_powerRunning = false;
            log("tag power %s ends", m_partnerId.c_str());
        }
    }
    if (m_brutalityTimer > 0) {
        m_brutalityTimer -= dt;
        if (m_brutalityTimer <= 0) {
            m_powerRunning = false;
            log("tag power %s ends", m_partnerId.c_str());
            endBrutality();
        }
        if (m_batonTimer > 0) {
            m_batonTimer -= dt;
            if (m_batonTimer <= 0) {
                InjuryState* target = batonTarget();
                if (target) {
                    m_batoned.push_back(target);
                    BatonInEffect* b = BatonInEffect::create();
                    b->setInjury(target);
                    b->setPosition(target->position);
                    m_effects.add(b);
                }
                m_batonTimer = target ? 0.4f : 0.2f;
            }
        }
    }
}

void SurgeryScreen::healthCaughtUp() {
    // (0x3b2ec) the healing partners' sparkles stop with the heart
    if (m_partner == 0)
        for (Effect* e : m_effects.activeOfType(kEffectGiblets)) static_cast<GibletsEffect*>(e)->stopGiblets();
    else if (m_partner == 1)
        for (Effect* e : m_effects.activeOfType(kEffectSaturdayParticle))
            static_cast<SaturdayParticleEffect*>(e)->stop();
}

bool SurgeryScreen::reviveWithSaturday() {
    // (0x35138) dead with Lady Saturday's revive: her heart flies back
    if (m_hearts <= 0 || m_partner != 1) return false;
    --m_hearts;
    if (CCNode* heart = ui.node("ingame_comboheart")) {
        SaturdayHeartEffect* h = SaturdayHeartEffect::create();
        h->setPosition(heart->getPosition());
        h->moveTo(ccp(static_cast<float>(heart->getPositionX() * 0.535),
                      static_cast<float>(heart->getPositionY() * 0.9825)));
        m_effects.add(h);
    }
    m_health = owns("score_tag_team_upgrade") ? 42.0f : 30.0f;
    addStat("RevivePatient");
    log("tag power saturday revives");
    return true;
}

void SurgeryScreen::slideToolButtons(bool out) {
    // (0x37318 out, 0x3bf58 back) the tool buttons slide off their edge by
    // 3/10 of the screen, and back; put away they can't be used (0x34108)
    const float w = static_cast<float>(ResolutionMgr::sharedResolutionMgr()->width);
    if (out) {
        ui.setVisible("ingame_injection_bar_grey", false);
        ui.setVisible("ingame_injection_bar_green", false);
    }
    // (every toggle but the tag partner's: the injection's out button too,
    // last, standing in the injection's place once its out mode is chosen)
    for (int k = 0; k <= kToolCount; ++k) {
        const int t = k < kToolCount ? k : static_cast<int>(kInjection);
        std::string n = k < kToolCount ? std::string("ingame_tool_") + kToolNames[t] : "ingame_tool_Injection_out";
        CCMenuItemToggle* b = dynamic_cast<CCMenuItemToggle*>(ui.node(n));
        if (!b) continue;
        bool left = b->getPositionX() < w * 0.5f;
        float dx = (left == out ? -w : w) * 0.3f;
        // (the port's bar across the top, SurgeryHud.cpp: up off the
        // screen's top by as much, and back down)
        CCPoint by = m_topBar ? ccp(0, (out ? w : -w) * 0.3f) : ccp(dx, 0);
        b->runAction(CCEaseIn::create(CCMoveBy::create(0.5f, by), 0.5f));
        // Fixed: the clamps' count goes with their button (the original left
        // it alone on the screen's edge while the tools were away)
        if (t == kClamp && k < kToolCount)
            if (CCNode* count = ui.node("clampnumber"))
                count->runAction(CCEaseIn::create(CCMoveBy::create(0.5f, by), 0.5f));
        // back: the tools the surgery offers on again (0x3a2b0(0))
        bool on = !out && std::find(m_def.tools.begin(), m_def.tools.end(), t) != m_def.tools.end();
        m_enabledItems[n] = on;
        b->setEnabled(on);
        // (the injection's lit by its mode: in 1, out 0)
        bool lit = m_tool && m_tool->type == t &&
                   (t != kInjection || (static_cast<Injection*>(m_tool)->mode == 0) == (k == kToolCount));
        b->setSelectedIndex(!on ? 2 : lit ? 1 : 0);
    }
}

void SurgeryScreen::endBrutality() {
    // (0x3bf58) the tools come back
    slideToolButtons(false);
}

InjuryState* SurgeryScreen::batonTarget() {
    // (0x3c198) an injury the baton can finish: shown, not done, no other
    // depending on it, not one being pulled out (state 0x13, but an
    // UnderSurfaceExtraction's). (The original preferred the one with the
    // highest +0xfda0; the port takes the first.)
    for (InjuryState* i : run().injuries) {
        if (i->finished || i->hasDependencies) continue;
        if (std::find(m_batoned.begin(), m_batoned.end(), i) != m_batoned.end()) continue;
        if (!i->root || !i->root->isVisible()) continue;
        if (i->state == 0x13 && i->def->base != kUnderSurfaceExtraction) continue;
        return i;
    }
    return nullptr;
}

bool SurgeryScreen::reverendPicksTool() {
    // (0x38d8c, 0x38bf0) the Reverend's power: the tool for what the finger
    // is on is picked, and starts at once. (The original weighed the
    // injuries under the finger by its +0xfda0 and skipped the clamps' and
    // pulls in progress; the port takes the topmost one whose stage takes a
    // tool the surgery has.)
    if (m_tool && m_tool->type == kTongs && static_cast<Tongs*>(m_tool)->holdingPiece) return false;
    InjuryState* best = nullptr;
    int tool = kNoTool;
    for (InjuryState* i : run().injuries) {
        if (i->finished) continue;
        int t = i->toolFor(m_touch);
        if (t == kNoTool || !m_tools.count(t)) continue;
        if (!best || best->zOrder <= i->zOrder) {
            best = i;
            tool = t;
        }
    }
    if (!best || (m_tool && m_tool->type == tool)) return false;
    if (m_tool && m_tool->active) touchEnded();
    log("tag power reverend picks %s", kToolNames[tool]);  // (traced for test scripts)
    selectTool(tool);
    // (0x38d8c) the injection picked to draw out: its out mode
    if (m_tool && m_tool->type == kInjection) static_cast<Injection*>(m_tool)->mode = 0;
    // every toggle off but the ones shown off (0x39090: the injection's out
    // button too), the tool's on; the injection's menu shut
    for (int i = 0; i <= kToolCount; ++i) {
        std::string n = i < kToolCount ? std::string("ingame_tool_") + kToolNames[i] : "ingame_tool_Injection_out";
        if (CCMenuItemToggle* tg = dynamic_cast<CCMenuItemToggle*>(ui.node(n))) {
            if (i == tool) tg->setSelectedIndex(1);
            else if (tg->getSelectedIndex() != 2) tg->setSelectedIndex(0);
        }
    }
    if (m_injectionMenu) hideInjectionMenu();
    // (the touch begins again with the new tool)
    if (m_tool) {
        m_tool->begin(m_touch);
        m_tool->setPosition(m_touch);
        m_toolGrace = m_tool->turnOnGrace;
        if (m_tool->active) m_effects.toolTouchBegan(m_touch);
    }
    return true;
}

}  // namespace mt
