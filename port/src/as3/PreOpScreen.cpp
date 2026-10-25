#include "as3/PreOpScreen.h"

#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/AreaScreen.h"
#include "as3/Log.h"
#include "as3/PartnerSelectScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StarPanel.h"
#include "as3/SurgeryScreen.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/UpgradeScreen.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

PreOpScreen* PreOpScreen::create(const SurgeryInfo& surgery, const SurgeryParams& params) {
    PreOpScreen* s = new PreOpScreen;
    s->setName("PreOpScreen");
    if (s->init()) {
        s->m_surgery = surgery;
        s->m_params = params;
        s->m_surgeryId = surgery.id;
        s->m_name = surgery.patientTitle;
        s->m_picture = surgery.preopBgSprite;
        s->m_sheets = {"preop_common", surgery.preopSpritesheet};
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void PreOpScreen::load() {
    loadSheets(this, m_sheets);
    if (CCSprite* picture = CCSprite::createWithSpriteFrameName(m_picture.c_str())) {
        setPositionOnCanvas(picture, 320, 192);
        addChild(picture);
    }
    m_def = loadUIDefinition("common/ui_preoperation_screen.json");
    buildUI(ui, this, m_def, this, menu_selector(PreOpScreen::onButton));
    m_hud.build(this, this, false, menu_selector(PreOpScreen::onButton));

    const SavedGame& slot = SaveData::shared().currentSlot();
    auto star = [&](const char* name, bool on) { ui.setItemImage(m_def, name, on ? "on" : "off"); };
    int main = slot.starsOf(m_surgeryId, 0);
    star("preop_star1", main > 0);
    star("preop_star2", main > 1);
    star("preop_star3", main > 2);
    star("preop_star4", slot.starsOf(m_surgeryId, 1) != 0);
    star("preop_star5", slot.starsOf(m_surgeryId, 2) != 0);
    ui.setText("charactername", m_name);

    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopMusic(true);
    const std::string& sfx = m_surgery.preSurgerySfx;
    sound->playAmbience(sfx.substr(0, sfx.find('.')));
    // before the first surgery is won, no upgrades (debug: cheat 11 shows
    // them, 1.45 0x11887c, and the port's updateUpgrades follows; Cheats.h)
    if (slot.starsOf("surgery_bunker_mrgiblets", 0) == 0 && !cheat(Cheats::kUnlockAll)) {
        ui.setVisible("ui_button_updates", false);
        ui.setVisible("ui_notification", false);
        ui.setText("ui_popup_upgradenumber", "");
    } else {
        updateUpgrades();
    }
    schedule(schedule_selector(PreOpScreen::tick), 1.0f);
}

void PreOpScreen::tick(float) {
    m_hud.refresh();
    GameStateUtils::UpdateBloodPacksAndSave();  // (1.45 updateStatus 0x11b6f4)
}

void PreOpScreen::onShown() {
    Scene::onShown();
    updateUpgrades();
    for (const char* name : {"preop_star1", "preop_star2", "preop_star3", "preop_star4", "preop_star5", "panel",
                             "ui_button_start", "ui_button_back"})
        ui.setVisible(name, true);
    ui.setText("charactername", m_name);
    showStarPanel(ui, m_surgery, m_params.mode);  // (1.45 onBecomeActive 0x11fb3c: showSideScoreBar)
}

void PreOpScreen::onRemoved() { unschedule(schedule_selector(PreOpScreen::tick)); }

void PreOpScreen::updateUpgrades() {
    ui.setVisible("ui_button_updates", true);
    const SavedGame& slot = SaveData::shared().currentSlot();
    int n = UpgradeInfoUtils::shared()->affordable(slot.totalStars(), slot.coins());
    if (n < 1) {
        ui.setVisible("ui_notification", false);
        ui.setText("ui_popup_upgradenumber", "");
    } else {
        ui.setText("ui_popup_upgradenumber", std::to_string(n));
        ui.setVisible("ui_notification", true);
    }
    if (slot.starsOf("surgery_bunker_mrgiblets", 0) == 0 && !cheat(Cheats::kUnlockAll)) {
        ui.setText("ui_popup_upgradenumber", "");
        ui.setVisible("ui_notification", false);
        ui.setVisible("ui_button_updates", false);
    }
}

void PreOpScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->stopChannel(SoundMgr::kAmbienceChannel, false);
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    if (name == "ui_button_start") {
        SaveData& save = SaveData::shared();
        SavedGame& slot = save.currentSlot();
        bool early = slot.starsOf("surgery_bunker_mrgiblets", 0) == 0 || slot.starsOf("surgery_bunker_prxy4", 0) == 0;
        if (early) {
            // the second surgery comes with Mr Giblets
            if (m_surgery.id == "surgery_bunker_prxy4") {
                slot.choosePartner(0, 0);
                slot.selectedTagPartner = "giblets";
                save.saveGame();
            }
            scenes->replaceScene(SurgeryScreen::create(m_surgery, m_params));
        } else {
            // the tag partner first, over this screen (shown again when it comes back)
            ui.setText("ui_popup_upgradenumber", "");
            for (const char* n : {"ui_notification", "ui_button_updates", "ui_button_start", "preop_star1",
                                  "preop_star2", "preop_star3", "preop_star4", "preop_star5", "ui_button_back",
                                  "panel"})
                ui.setVisible(n, false);
            ui.setText("charactername", "");
            hideStarPanel(ui);  // (1.45 clickedButton 0x119aa4: hideSideScorebar)
            scenes->pushScene(PartnerSelectScreen::create("preop", m_surgery, m_params), false);
        }
    } else if (name == "ui_button_back") {
        scenes->replaceScene(AreaScreen::create(m_surgery.area, 0));
    } else if (name == "ui_button_updates") {
        scenes->replaceScene(UpgradeScreen::create("preop"));
    } else if (name == "ui_button_tagpartner") {
        scenes->replaceScene(PartnerSelectScreen::create("preop", m_surgery, m_params));
    }
}

}  // namespace mt
