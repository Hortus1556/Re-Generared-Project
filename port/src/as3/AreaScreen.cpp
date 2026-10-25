#include "as3/AreaScreen.h"

#include <cstdio>

#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/EditorLevelsScreen.h"
#include "as3/InfoScreen.h"
#include "as3/Log.h"
#include "as3/NewAreaScreen.h"
#include "as3/ParticleSystem.h"
#include "as3/PartnerSelectScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/UpgradeScreen.h"
#include "as3/WelcomeScreen.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

AreaScreen::AreaScreen() {}

AreaScreen* AreaScreen::create(const AreaDef& area, int from) {
    AreaScreen* s = new AreaScreen;
    s->setName("AreaScreen");
    s->m_fromWorld = from != 0;
    // 0x1f378
    if (s->init()) {
        s->m_area = area;
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void AreaScreen::hideButtons() {
    for (const char* name : {"ui_button_updates", "ui_button_partner", "ui_button_tagpartner", "ui_notification"})
        ui.setVisible(name, false);
    ui.setText("ui_popup_upgradenumber", "");
}

void AreaScreen::load() {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float cs = r->canvasScale;
    SavedGame& slot = SaveData::shared().currentSlot();
    loadSheets(this, {m_area.bgSpritesheet});
    loadSheets(this, {m_area.mapSpritesheet});
    CCSprite* bg = CCSprite::createWithSpriteFrameName(m_area.bgSprite.c_str());
    if (bg) setPositionOnCanvas(bg, 320, 192);
    bool finished = slot.starsOf("surgery_exileclinic_alanprobe", 0) != 0;
    if (m_area.areaId == "area_bunker") {
        // the Bunker shows its lower part, and its second map once the game is finished
        float y = 118;
        if (finished) {
            m_area.mapUiDefinitionFile = "bunker_map_2.json";
            y = 221;
        }
        if (bg) setPositionOnCanvas(bg, 320, y);
    }
    if (bg) addChild(bg, m_bgZ);
    CCNode* layer = CCNode::create();
    std::string file = "common/" + m_area.mapUiDefinitionFile;
    m_def = loadUIDefinition(file.c_str());
    buildUI(ui, layer, m_def, this, menu_selector(AreaScreen::onButton));
    m_hud.build(layer, this, true, menu_selector(AreaScreen::onButton));
    addEditorEntry(this, m_area);  // (the port's: the surgery editor's button, EditorLevelsScreen.h)
    // (1.45 loadAssets 0x28380) the map of the Flairground or the Hospital
    // seen: its "unlocked" popup no longer needed (SurgeryEndScreen)
    if (m_area.areaId == "area_pocketgods" && !slot.seenPocketgodsUnlockedPopup) {
        slot.seenPocketgodsUnlockedPopup = true;
        SaveData::shared().saveGame();
    } else if (m_area.areaId == "area_hospital" && !slot.seenHospitalUnlockedPopup) {
        slot.seenHospitalUnlockedPopup = true;
        SaveData::shared().saveGame();
    }
    addChild(layer, m_layoutZ);
    // (Android 1.43's maps, the Hospital's) the Facebook friends' pictures
    // ("profile_<patient><n>"): online, removed
    for (auto& item : ui.byName)
        if (item.first.compare(0, 8, "profile_") == 0 && item.second) item.second->setVisible(false);

    // each patient's file, by their stars
    int total = 0, most = 0, won = 0, boss = -1;
    for (size_t j = 0; j < m_area.surgeries.size(); ++j) {
        const std::string& surgery = m_area.surgeries[j];
        if (m_area.areaId == "area_bunker" && !finished && surgery == "surgery_bunker_hubris2") continue;
        int main = slot.starsOf(surgery, 0), blue = slot.starsOf(surgery, 1), red = slot.starsOf(surgery, 2);
        total += main + blue + red;
        most += 5;
        if (surgery != m_area.bossSurgery && main != 0) ++won;
        else if (surgery == m_area.bossSurgery) boss = static_cast<int>(j);
        const std::string& s = j < m_area.surgeryShortNames.size() ? m_area.surgeryShortNames[j] : surgery;
        ui.setVisible("file_" + s, main != 0);
        ui.setVisible("file_unknown_" + s, main == 0);
        ui.setVisible("file_locked_" + s, false);
        std::string star = "file_star_" + s;
        ui.setItemImage(m_def, star + "_1", main > 0 ? "yellow" : "darkyellow");
        ui.setItemImage(m_def, star + "_2", main > 1 ? "yellow" : "darkyellow");
        ui.setItemImage(m_def, star + "_3", main > 2 ? "yellow" : "darkyellow");
        ui.setItemImage(m_def, star + "_4", blue != 0 ? "blue" : "darkblue");
        ui.setItemImage(m_def, star + "_5", red != 0 ? "red" : "darkred");
    }
    // the boss waits for the area's three other patients
    // (debug: cheat 11 opens it, 1.45 0x28380; Cheats.h)
    if (boss >= 0 && won != 3 && boss < static_cast<int>(m_area.surgeryShortNames.size()) && !cheat(Cheats::kUnlockAll)) {
        const std::string& s = m_area.surgeryShortNames[boss];
        ui.setVisible("file_locked_" + s, true);
        ui.setVisible("file_" + s, false);
        ui.setVisible("file_unknown_" + s, false);
        for (const char* n : {"_1", "_2", "_3", "_4", "_5"}) ui.setVisible("file_star_" + s + n, false);
    }

    if (m_area.areaId == "area_bunker" && slot.starsOf("surgery_bunker_prxy4", 0) == 0 &&
        slot.starsOf("surgery_bunker_mrgiblets", 0) != 0) {
        // the second patient sparkles; nothing else to do yet
        m_highlight = ParticleSystem::create("particle_surgeryHighlightFX");
        if (m_highlight) {
            m_highlight->setScale(1.0f);
            m_highlight->setPosition(ccp(static_cast<float>(r->width / 2) + cs * 108.0f,
                                         static_cast<float>(r->height / 2) + cs * -16.0f));
            addChild(m_highlight);
        }
        hideButtons();
    } else {
        m_highlight = nullptr;
        int n = UpgradeInfoUtils::shared()->affordable(slot.totalStars(), slot.coins());
        if (n < 1) {
            ui.setVisible("ui_notification", false);
            ui.setText("ui_popup_upgradenumber", "");
        } else {
            ui.setText("ui_popup_upgradenumber", std::to_string(n));
            ui.setVisible("ui_notification", true);
        }
    }

    // the tag partner's picture on its button
    std::string partner = partnerButtonPicture(slot.selectedTagPartner);
    if (!partner.empty()) ui.setItemImage(m_def, "ui_button_tagpartner", partner);

    // a new area from the world map: its introduction first, without the buttons
    if (total == 0 && m_fromWorld) {
        ui.setVisible("ui_button_back", false);
        hideButtons();
    }
    char stars[32];
    std::snprintf(stars, sizeof stars, "%d/%d", total, most);
    m_hud.setMapInfo(m_area.title, stars);
    addSnow();

    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    std::string ambience = m_area.bgm.substr(0, m_area.bgm.find('.'));
    sound->playAmbience(ambience);
    sound->stopMusic(true);
    m_hud.refresh();
    schedule(schedule_selector(AreaScreen::tick), 1.0f);
}

void AreaScreen::addSnow() {
    if (m_area.areaId != "area_exileclinic") return;
    if (ParticleSystem* snow = ParticleSystem::create("particle_exilesnowFX")) {
        const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
        snow->setPosition(ccp(static_cast<float>(r->width), static_cast<float>(r->height)));
        addChild(snow);
    }
}

void AreaScreen::tick(float) {
    m_hud.refresh();
    GameStateUtils::UpdateBloodPacksAndSave();  // (1.45 updateStatus 0x2e018)
}

void AreaScreen::onShown() {
    Scene::onShown();
    // (and the ads: removed)
    const SavedGame& slot = SaveData::shared().currentSlot();
    if (!m_introduced) {
        int total = 0;
        for (const std::string& s : m_area.surgeries)
            total += slot.starsOf(s, 0) + slot.starsOf(s, 1) + slot.starsOf(s, 2);
        if (total != 0 && !m_area.surgeries.empty()) return;
        if (m_fromWorld) {
            SceneMgr::sharedSceneMgr()->pushScene(NewAreaScreen::create(m_area.areaId, this), false);
            m_introduced = true;
        }
        return;
    }
    // back from the introduction
    for (const char* name : {"ui_button_back", "ui_button_updates", "ui_button_partner", "ui_button_tagpartner"})
        ui.setVisible(name, true);
    int n = UpgradeInfoUtils::shared()->affordable(slot.totalStars(), slot.coins());
    if (n > 0) {
        ui.setText("ui_popup_upgradenumber", std::to_string(n));
        ui.setVisible("ui_notification", true);
    }
}

void AreaScreen::onRemoved() {
    if (m_highlight) m_highlight->stopSystem();
    unschedule(schedule_selector(AreaScreen::tick));
    // (and the ads: removed)
}

void AreaScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    // "file_unknown_giblets" -> "giblets", "ui_button_back" -> "back"
    bool locked = name.find("locked") != std::string::npos;
    std::string what = name.substr(name.rfind('_') + 1);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    SavedGame& slot = SaveData::shared().currentSlot();
    if (what == "back") {
        sound->playSound("button_click", false);
        // before the Bunker's second patient there is no world map yet
        if (m_area.areaId == "area_bunker" && slot.starsOf("surgery_bunker_prxy4", 0) == 0)
            scenes->replaceScene(WelcomeScreen::create());
        else
            scenes->replaceScene(WorldScreen::create());
    } else if (what == "updates") {
        sound->playSound("button_click", false);
        scenes->replaceScene(UpgradeScreen::create("area"));
    } else if (what == "partner") {
        sound->playSound("button_click", false);
        scenes->replaceScene(PartnerSelectScreen::create("area", SurgeryInfo(), SurgeryParams()));
    } else if (!locked) {
        sound->playSound("button_click", false);
        for (size_t j = 0; j < m_area.surgeryShortNames.size() && j < m_area.surgeries.size(); ++j) {
            if (what != m_area.surgeryShortNames[j]) continue;
            // the patient's case
            slot.selectedSurgery = m_area.surgeries[j];
            scenes->replaceScene(InfoScreen::create(m_area.surgeries[j], m_area));
            return;
        }
        log("Clicked unknown button %s", name.c_str());
    } else {
        sound->playSound("button_click_denied", false);
    }
}

}  // namespace mt
