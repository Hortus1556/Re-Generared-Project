#include "as3/PartnerInfoScreen.h"

#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/Log.h"
#include "as3/PartnerInfoUtils.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCTexture2D.h"

namespace mt {

using namespace cocos2d;

namespace {

// the file's headings (0x43f228, 0x43f22c, 0x43f230)
const char* kSkillsTitle = "Tag-Power";
const char* kBioTitle = "Bio";
const char* kStatsTitle = "Info";

}  // namespace

PartnerInfoScreen* PartnerInfoScreen::create(const std::string& partner, PartnerSelectScreen* select) {
    PartnerInfoScreen* s = new PartnerInfoScreen;
    s->m_partner = partner;
    s->m_select = select;
    s->setName("PartnerInfoScreen_" + partner);
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void PartnerInfoScreen::load() {
    loadSheets(this, {m_sheet});
    m_root = CCNode::create();
    m_def = loadUIDefinition("common/ui_tagpartner_popup.json");
    buildUI(ui, m_root, m_def, this, menu_selector(PartnerInfoScreen::onButton));
    addChild(m_root);
    m_hud.build(this, this, false, menu_selector(PartnerInfoScreen::onButton));
    // in from below
    m_root->setPosition(ccp(m_root->getPositionX(), -static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height)));
    tick(0);
    SavedGame& slot = SaveData::shared().currentSlot();
    for (const PartnerInfo& p : PartnerInfoUtils::shared()->partners) {
        if (p.id != m_partner) continue;
        ui.setText("tagscreen_popup_tagname", p.name);
        ui.setText("tagscreen_popup_skills_title", kSkillsTitle);
        ui.setText("tagscreen_popup_skills_body", p.skills);
        ui.setText("tagscreen_popup_bio_title", kBioTitle);
        ui.setText("tagscreen_popup_bio_body", p.bio);
        ui.setText("tagscreen_popup_stats_title", kStatsTitle);
        ui.setText("tagscreen_popup_stats_body", p.stats);
        ui.setItemImage(m_def, "tagscreen_popup_photo", partnerName(m_partner));
        // (debug: every one owned with cheat 17, 1.45 0x2d45bc; Cheats.h)
        if (slot.owns(SavedGame::kItemPartner, partnerIndex(m_partner)) || cheat(Cheats::kAllPartners)) {
            // owned: to take it
            ui.setVisible("tagscreen_button_buy", false);
            ui.setVisible("tagscreen_button_tag", true);
            ui.setText("tagscreen_popup_cost", "");
        } else if (p.purchasable) {
            // on sale: its price
            ui.setVisible("tagscreen_button_buy", true);
            ui.setVisible("tagscreen_button_tag", false);
            ui.setText("tagscreen_popup_cost", std::to_string(p.cost));
        }
    }
    m_root->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
    m_hud.refresh();
    schedule(schedule_selector(PartnerInfoScreen::tick), 1.0f);
}

void PartnerInfoScreen::unload() {
    removeAllChildrenWithCleanup(true);
    // CCDirector::purgeCachedData as cocos2d-x 2.1.1 had it: the fonts and the
    // unused textures. (The engine's also drops the unused sprite frames,
    // which the partner select screen below still asks for when it shows again.)
    CCLabelBMFont::purgeCachedData();
    CCTextureCache::sharedTextureCache()->removeUnusedTextures();
}

void PartnerInfoScreen::tick(float) {
    m_hud.refresh();
    GameStateUtils::UpdateBloodPacksAndSave();  // (1.45 updateAvailable 0x2d6760)
}

void PartnerInfoScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum() || m_closing) return;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (m_hud.owns(sender)) {
        // (the top bar's button to the shop of coins sold for money: removed, GlobalHud.h)
        sound->playSound("button_click", false);
        return;
    }
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (name == "ui_button_back") {
        sound->playSound("button_click", false);
        m_closing = true;
        float h = static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height);
        CCActionInterval* down = CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, h * -1.15f)), 0.25f);
        m_root->runAction(
            CCSequence::create(down, CCCallFuncN::create(this, callfuncN_selector(PartnerInfoScreen::close)), nullptr));
    } else if (name == "tagscreen_button_buy") {
        const PartnerInfo* p = PartnerInfoUtils::shared()->partner(m_partner);
        if (!p) return;
        if (slot.coins() < p->cost) {
            // (the original then opened the shop of coins sold for money: removed)
            sound->playSound("button_click_denied", false);
            return;
        }
        sound->playSound("button_click", false);
        // (the original reported the purchase to its analytics)
        slot.choosePartner(partnerIndex(m_partner), p->cost);
        save.saveGame();
        ui.setVisible("tagscreen_button_buy", false);
        ui.setVisible("tagscreen_button_tag", true);
        ui.setText("tagscreen_popup_cost", "");
        tick(0);
    } else if (name == "tagscreen_button_tag") {
        sound->playSound("button_click", false);
        slot.selectedTagPartner = m_partner;
        save.saveGame();
        m_closing = true;
        close(nullptr);
    }
}

void PartnerInfoScreen::close(CCNode*) {
    unload();
    unschedule(schedule_selector(PartnerInfoScreen::tick));
    SceneMgr::sharedSceneMgr()->popScene();
}

}  // namespace mt
