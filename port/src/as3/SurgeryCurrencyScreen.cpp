#include "as3/SurgeryCurrencyScreen.h"

#include <string>

#include "as3/Log.h"
#include "as3/PartnerInfoUtils.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryEndScreen.h"
#include "engine/cocos/CCAction.h"

namespace mt {

using namespace cocos2d;

SurgeryCurrencyScreen* SurgeryCurrencyScreen::create(SurgeryEndScreen* results, int coinsFromScore, int coinsBonus) {
    SurgeryCurrencyScreen* s = new SurgeryCurrencyScreen;
    s->setName("SurgeryCurrencyScreen");
    s->m_results = results;
    s->m_coinsFromScore = coinsFromScore;
    s->m_coinsBonus = coinsBonus;
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void SurgeryCurrencyScreen::load() {
    loadSheets(this, {"surgery_currency_sheet"});
    // the cheque, dropped from above
    m_root = CCNode::create();
    addChild(m_root);
    m_root->setPosition(ccp(m_root->getPositionX(), static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height)));
    m_def = loadUIDefinition("common/ui_currency_screen.json");
    buildUI(ui, m_root, m_def, this, menu_selector(SurgeryCurrencyScreen::onButton));
    // (1.45 updateDisplay 0x2ffc98) the coin doubler's buy button and its
    // icon (it was sold for money: removed), hidden
    ui.setVisible("currency_coindoubler_button", false);
    ui.setVisible("currency_doubler_icon", false);
    // Filthy Nowe's power used (partners.json's index 5): his cheque, the
    // shares shown halved (as before his doubling)
    float share = 1;
    const SurgeryResult& r = m_results->result();
    if (r.partnerFlag && partnerIndex(r.partner) == 5) {
        ui.setItemImage(m_def, "currency_cheque", "filthy");
        share = 0.5f;
    }
    // (1.45 showed the coin doubler owned, its icon pulsing and the shares
    // halved again: removed)
    ui.setText("ui_currency_sp", std::to_string(m_coinsBonus + m_coinsFromScore));
    ui.setText("ui_currency_scorebonus", std::to_string(static_cast<int>(static_cast<float>(m_coinsFromScore) * share)));
    ui.setText("ui_currency_starbonus", std::to_string(static_cast<int>(static_cast<float>(m_coinsBonus) * share)));
    m_root->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
}

void SurgeryCurrencyScreen::onButton(CCObject* sender) {
    // (0x229ff8) OK: down and away
    if (shownForLessThanMinimum() || m_closing) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (name != "currency_next_button") return;
    m_closing = true;
    float h = static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height);
    CCActionInterval* down = CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, h * -1.15f)), 0.25f);
    m_root->runAction(
        CCSequence::create(down, CCCallFuncN::create(this, callfuncN_selector(SurgeryCurrencyScreen::closed)), nullptr));
}

void SurgeryCurrencyScreen::closed(CCNode*) {
    // (0x22a250) the results go on: the coins' sound off, their buttons, no shade
    retain();
    autorelease();  // (popScene lets go of this screen)
    m_results->stopCoinSound();
    m_results->showButtons();
    m_results->hideShade();
    SceneMgr::sharedSceneMgr()->popScene();
}

}  // namespace mt
