#include "as3/PartnerCooldownInfoScreen.h"

#include <cmath>
#include <cstdio>

#include "as3/Log.h"
#include "as3/PartnerSelectScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SoundMgr.h"

namespace mt {

using namespace cocos2d;

PartnerCooldownInfoScreen* PartnerCooldownInfoScreen::create(const std::string& partner, PartnerSelectScreen* select) {
    PartnerCooldownInfoScreen* s = new PartnerCooldownInfoScreen;
    if (s->init(partner, select)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool PartnerCooldownInfoScreen::init(const std::string& partner, PartnerSelectScreen* select) {
    if (const PartnerInfo* p = PartnerInfoUtils::shared()->partner(partner)) m_partner = *p;
    m_select = select;
    return Scene::init();
}

void PartnerCooldownInfoScreen::load() {
    m_root = CCNode::create();
    addChild(m_root);
    m_def = loadUIDefinition("common/ui_tagpartner_cooldown.json");
    buildUI(ui, m_root, m_def, this, menu_selector(PartnerCooldownInfoScreen::onButton));
    // placed about the portrait, in canvas points
    float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    auto place = [&](const char* name, float x, float y) {
        if (CCNode* n = ui.node(name)) n->setPosition(ccp(x * s, y * s));
    };
    place("selecttag_unlock", -30, 10);
    place("ui_unlock_cost", 30, 28);
    place("selecttag_bar_green", -6, 80);
    place("selecttag_bar_grey", -7, 80);
    place("ui_tag_available_in", -5, 120);
    place("ui_tag_available_time", -5, 105);
}

void PartnerCooldownInfoScreen::setTime(int left, int cost) {
    ui.setText("ui_unlock_cost", std::to_string(cost));
    ui.setText("ui_tag_available_in", "Ready in...");
    int minutes = left / 60;
    char time[50];
    std::snprintf(time, sizeof time, "%02d:%02d:%02d", static_cast<int>(std::ceil(left / 3600)),
                  static_cast<int>(std::ceil(minutes % 60)), static_cast<int>(std::ceil(left - minutes * 60)));
    // (the original also set the time's text definition left-aligned, which
    // only a label made after would have shown)
    ui.setText("ui_tag_available_time", time);
    // the bar fills as the cooldown runs out
    if (CCNode* bar = ui.node("selecttag_bar_green"))
        bar->setScaleX(m_partner.cooldownDuration > 0
                           ? 1.0f - static_cast<float>(left) / static_cast<float>(m_partner.cooldownDuration)
                           : 1.0f);
}

void PartnerCooldownInfoScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    if (ui.nameOf(m_def, sender).empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    // (its one button: end the cooldown)
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->playSound("button_click", false);
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (slot.coins() < m_partner.endCooldownCost) {
        // (the original then opened the shop of coins sold for money: removed)
        sound->playSound("button_click_denied", false);
        return;
    }
    sound->playSound("button_click", false);
    // (the original reported the purchase to its analytics)
    payPartnerCooldown(partnerIndex(m_partner.id), m_partner.endCooldownCost);
    setPartnerCooldown(m_partner.id, 0);
    slot.selectedTagPartner = m_partner.id;
    save.saveGame();
    if (m_select) m_select->refreshPortraits();
}

}  // namespace mt
