#include "as3/CooldownInfoScreen.h"

#include <cmath>
#include <cstdio>

#include "as3/Log.h"
#include "as3/PartnerSelectScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

namespace {

// 0x44bd10, 0x44bd14
const char* kTitle = "%s is Recharging";
const char* kBody =
    "%s is exhausted from using their powers. Give them %s to rest, or recharge them to unlock immediately.";

}  // namespace

CooldownInfoScreen* CooldownInfoScreen::create(const std::string& partner, PartnerSelectScreen* select) {
    CooldownInfoScreen* s = new CooldownInfoScreen;
    s->setName("CooldownInfoScreen");
    s->m_select = select;
    if (s->init(partner)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool CooldownInfoScreen::init(const std::string& partner) {
    if (const PartnerInfo* p = PartnerInfoUtils::shared()->partner(partner)) m_partner = *p;
    return Scene::init();
}

void CooldownInfoScreen::load() {
    loadSheets(this, {m_sheet});
    // the screen below dimmed
    CCSprite* dim = CCSprite::create();
    dim->addChild(CCLayerColor::create(ccc4(0, 0, 0, 127)));
    addChild(dim);
    m_root = CCNode::create();
    m_def = loadUIDefinition("common/ingame_popup_rechargetag.json");
    buildUI(ui, m_root, m_def, this, menu_selector(CooldownInfoScreen::onButton));
    addChild(m_root);
    // in from below
    m_root->setPosition(ccp(m_root->getPositionX(), -static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height)));
    m_root->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
    ui.setText("rechargetagprice", std::to_string(m_partner.endCooldownCost));
    char title[128];
    std::snprintf(title, sizeof title, kTitle, m_partner.name.c_str());
    ui.setText("rechargetag_title", title);
    schedule(schedule_selector(CooldownInfoScreen::tick), 0);
}

void CooldownInfoScreen::tick(float) {
    int left = partnerCooldownLeft(m_partner.id);
    int minutes = left / 60;
    char time[50];
    std::snprintf(time, sizeof time, "%02d:%02d:%02d", static_cast<int>(std::ceil(left / 3600)),
                  static_cast<int>(std::ceil(minutes % 60)), static_cast<int>(std::ceil(left - minutes * 60)));
    char body[512];
    std::snprintf(body, sizeof body, kBody, m_partner.name.c_str(), time);
    ui.setText("rechargetag_body", body);
    if (left < 1) {
        // rested: the popup goes
        unschedule(schedule_selector(CooldownInfoScreen::tick));
        if (m_closing) return;
        m_closing = true;
        SceneMgr::sharedSceneMgr()->popScene();
    }
}

void CooldownInfoScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum() || m_closing) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (name == "ingame_rechargetag_nobutton") {
        sound->playSound("button_click", false);
        close();
    } else if (name == "ui_button_rechargetag") {
        SaveData& save = SaveData::shared();
        SavedGame& slot = save.currentSlot();
        if (slot.coins() < m_partner.endCooldownCost) {
            // (the original then opened the shop of coins sold for money: removed)
            sound->playSound("button_click_denied", false);
            return;
        }
        sound->playSound("button_click", false);
        payPartnerCooldown(partnerIndex(m_partner.id), m_partner.endCooldownCost);
        setPartnerCooldown(m_partner.id, 0);
        slot.selectedTagPartner = m_partner.id;
        save.saveGame();
        if (m_select) m_select->refreshPortraits();
        close();
    }
}

void CooldownInfoScreen::close() {
    m_closing = true;
    float h = static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height);
    CCActionInterval* down = CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, h * -1.15f)), 0.25f);
    m_root->runAction(CCSequence::create(down, CCCallFuncN::create(this, callfuncN_selector(CooldownInfoScreen::closed)), nullptr));
}

void CooldownInfoScreen::closed(CCNode*) {
    unschedule(schedule_selector(CooldownInfoScreen::tick));
    SceneMgr::sharedSceneMgr()->popScene();
}

}  // namespace mt
