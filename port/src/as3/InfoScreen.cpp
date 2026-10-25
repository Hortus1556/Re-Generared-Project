#include "as3/InfoScreen.h"

#include "as3/AreaScreen.h"
#include "as3/DialogueScreen.h"
#include "as3/Log.h"
#include "as3/PreOpScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

InfoScreen* InfoScreen::create(const std::string& surgery, const AreaDef& area) {
    InfoScreen* s = new InfoScreen;
    s->setName("SurgeryInfo_" + surgery);
    // 0xaaa60
    if (s->init()) {
        s->m_surgery = loadSurgeryInfo(surgery, area);
        s->m_area = area;
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void InfoScreen::load() {
    loadSheets(this, {m_surgery.infoSpritesheet});
    loadSheets(this, {m_area.bgSpritesheet});
    if (CCSprite* bg = CCSprite::createWithSpriteFrameName(m_surgery.area.bgSprite.c_str())) {
        setPositionOnCanvas(bg, 320, 192);
        addChild(bg);
    }
    m_def = loadUIDefinition("common/ui_surgeryinfo_screen.json");
    buildUI(ui, this, m_def, this, menu_selector(InfoScreen::onButton));

    const SavedGame& slot = SaveData::shared().currentSlot();
    int main = slot.starsOf(m_surgery.id, 0), blue = slot.starsOf(m_surgery.id, 1),
        red = slot.starsOf(m_surgery.id, 2);
    const char* star1 = main > 0 ? "yellow" : "normal";
    const char* star2 = main > 1 ? "yellow" : "normal";
    const char* star3 = main > 2 ? "yellow" : "normal";
    ui.setItemImage(m_def, "surgeryinfo_evaluationstar_1", star1);
    ui.setItemImage(m_def, "surgeryinfo_evaluationstar_2", star2);
    ui.setItemImage(m_def, "surgeryinfo_evaluationstar_3", star3);
    ui.setItemImage(m_def, "surgeryinfo_evaluationstar_blue", blue != 0 ? "blue" : "normal");
    ui.setItemImage(m_def, "surgeryinfo_evaluationstar_red", red != 0 ? "red" : "normal");
    ui.setItemImage(m_def, "surgeryinfo_main_postit_star1", star1);
    ui.setItemImage(m_def, "surgeryinfo_main_postit_star2", star2);
    ui.setItemImage(m_def, "surgeryinfo_main_postit_star3", star3);
    ui.setItemImage(m_def, "surgeryinfo_bonus1_star", blue != 0 ? "yellow" : "normal");
    ui.setItemImage(m_def, "surgeryinfo_bonus2_star", red != 0 ? "yellow" : "normal");
    ui.setVisible("ui_biutton_start_bonusI", false);
    ui.setVisible("ui_biutton_start_bonusII", false);
    ui.setText("ui_surgeryinfo_locationtitle", m_surgery.locationTitle);
    ui.setText("surgery_basic_desc", m_surgery.initialDescription);
    ui.setText("surgery_main_obs_1", m_surgery.observationsOphelia);
    ui.setText("surgery_main_obs_2", m_surgery.observationsAlan);
    ui.setText("surgery_main_sym_1", m_surgery.symptomsOphelia);
    ui.setText("surgery_main_sym_2", m_surgery.symptomsAlan);
    ui.setText("surgery_main_act_1", m_surgery.actionsOphelia);
    ui.setText("surgery_main_act_2", m_surgery.actionsAlan);
    ui.setText("surgery_bonus01_desc", m_surgery.bonus1Description);
    ui.setText("surgery_bonus02_desc", m_surgery.bonus2Description);
    ui.setText("surgery_bonus01_title", m_surgery.bonus1Title);
    ui.setText("surgery_bonus02_title", m_surgery.bonus2Title);
    ui.setItemImage(m_def, "surgeryinfo_postit_mainBack", "back");
    ui.setItemImage(m_def, "surgeryinfo_postit_mainFront", "normal");
    ui.setItemImage(m_def, "surgeryinfo_postit_bonus1Back", "back");
    ui.setItemImage(m_def, "surgeryinfo_postit_bonus1Front", "normal");
    ui.setItemImage(m_def, "surgeryinfo_postit_bonus2Back", "back");
    ui.setItemImage(m_def, "surgeryinfo_postit_bonus2Front", "normal");
    if (main == 0) {
        showPage(0);
    } else {
        // Fixed: Useful Brother 2's case file names the photo "brother2",
        // which no sheet has (the original showed none); "brother" is the
        // unused one meant for it (docs/as3-unused.md). A missing photo
        // falls back to its name without the trailing number.
        std::string photo = m_surgery.photoSuffix;
        auto have = [](const std::string& suffix) {
            return CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(
                       ("surgeryinfo_photo-" + suffix + ".png").c_str()) != nullptr;
        };
        if (!have(photo)) {
            std::string base = photo;
            while (!base.empty() && base.back() >= '0' && base.back() <= '9') base.pop_back();
            if (!base.empty() && have(base)) photo = base;
        }
        ui.setItemImage(m_def, "patient_photo", photo);
        showPage(1);
        // a challenge not won yet pulses
        auto pulse = [&](const char* name) {
            CCMenuItemImage* it = ui.item(name);
            if (!it) return;
            it->setAnchorPoint(ccp(0.5f, 0.5f));
            const CCPoint& p = it->getPosition();
            const CCSize& size = it->getContentSize();
            it->setPosition(ccp(p.x + size.width * 0.5f, p.y + size.height * 0.5f));
            it->runAction(CCRepeatForever::create(static_cast<CCActionInterval*>(
                CCSequence::create(CCScaleTo::create(0.5f, 1.05f), CCScaleTo::create(0.5f, 1.0f), nullptr))));
        };
        if (blue == 0) pulse("surgeryinfo_postit_bonus1Back");
        if (red == 0) pulse("surgeryinfo_postit_bonus2Back");
    }
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopMusic(true);
    sound->playAmbience(m_area.bgm.substr(0, m_area.bgm.find('.')));
}

void InfoScreen::showPage(int page) {
    m_page = page;
    ui.setVisible("surgery_basic_desc", page == 0);
    ui.setVisible("surgeryinfo_postit_mainBack", page > 1);
    for (const char* name : {"surgeryinfo_postit_mainFront", "surgeryinfo_main_postit_star1",
                             "surgeryinfo_main_postit_star2", "surgeryinfo_main_postit_star3", "surgery_main_obs_1",
                             "surgery_main_obs_2", "surgery_main_sym_1", "surgery_main_sym_2", "surgery_main_act_1",
                             "surgery_main_act_2"})
        ui.setVisible(name, page == 1);
    ui.setVisible("surgeryinfo_postit_bonus1Back", page != 2 && page != 0);
    for (const char* name : {"surgeryinfo_postit_bonus1Front", "surgeryinfo_bonus1_star", "surgery_bonus01_desc",
                             "surgery_bonus01_title"})
        ui.setVisible(name, page == 2);
    ui.setVisible("surgeryinfo_postit_bonus2Back", page != 3 && page != 0);
    for (const char* name : {"surgeryinfo_postit_bonus2Front", "surgeryinfo_bonus2_star", "surgery_bonus02_desc",
                             "surgery_bonus02_title"})
        ui.setVisible(name, page == 3);
    ui.setVisible("surgeryinfo_page_basic", page == 0);
    ui.setVisible("surgeryinfo_page_main", page == 1);
    ui.setVisible("surgeryinfo_page_bonus1", page == 2);
    ui.setVisible("surgeryinfo_page_bonus2", page == 3);
}

void InfoScreen::turnTo(int page) {
    SoundMgr::sharedSoundMgr()->playSound("page_shuffle", false);
    ui.setVisible("ui_button_start", page == 1);
    ui.setVisible("ui_biutton_start_bonusI", page == 2);
    ui.setVisible("ui_biutton_start_bonusII", page == 3);
    showPage(page);
}

void InfoScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    SaveData& save = SaveData::shared();
    if (name == "ui_button_start" || name == "ui_biutton_start_bonusI" || name == "ui_biutton_start_bonusII") {
        // the page's surgery: the slot's selectedMode is 1 + its mode
        SurgeryParams params;
        if (m_page == 2) {
            params.mode = 1;
            save.currentSlot().selectedMode = 2;
            save.saveGame();
            scenes->replaceScene(PreOpScreen::create(m_surgery, params));
        } else if (m_page == 3) {
            params.mode = 2;
            save.currentSlot().selectedMode = 3;
            save.saveGame();
            scenes->replaceScene(PreOpScreen::create(m_surgery, params));
        } else {
            // the main surgery, through its story
            params.mode = 0;
            save.currentSlot().selectedMode = 1;
            save.saveGame();
            // (and an ad: removed)
            scenes->replaceScene(DialogueScreen::create(m_surgery, params));
        }
    } else if (name == "ui_button_back") {
        scenes->replaceScene(AreaScreen::create(m_surgery.area, 0));
    } else if (name == "surgeryinfo_postit_mainBack") {
        turnTo(1);
    } else if (name == "surgeryinfo_postit_bonus1Back") {
        turnTo(2);
    } else if (name == "surgeryinfo_postit_bonus2Back") {
        turnTo(3);
    } else if (name == "surgeryinfo_bonuslink") {
        // the first challenge not won
        if (m_page == 0) return;
        turnTo(save.currentSlot().starsOf(m_surgery.id, 1) == 0 ? 2 : 3);
    }
}

}  // namespace mt
