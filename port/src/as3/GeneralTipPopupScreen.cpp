#include "as3/GeneralTipPopupScreen.h"

#include <cstdio>

#include "as3/AreasDefUtils.h"
#include "as3/BloodPacks.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/UpgradeScreen.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

namespace {

// the tag partners' ids (0x44ce4c)
const char* const kPartnerIds[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};

// the texts (0x44cc64..0x44ccc4)
const char* const kPartnerTitle = "New Partner Unlocked";
const char* const kPartnerGiblets =
    "You've unlocked a new partner - Mr Giblets\n\nHis healing slobber is perfect for raising your patient's heart rate!";
const char* const kPartnerSaturday =
    "You've unlocked a new Tag Partner - Lady Saturday!\n\nHer power allows you to revive patients from the dead!";
const char* const kPartnerReverend =
    "You've unlocked a new partner - The Reverand!\n\nHis power auto-selects the perfect tool for every injury!";
const char* const kShopTitle = "Need Help?";
const char* const kShopBody = "Struggling a little there? Head to the shop for a little helping hand from Alan!";
const char* const kUpgradesTitle = "Upgrade Your Gear!";
const char* const kUpgradesBody =
    "There are awesome upgrades available NOW! Head to the upgrades machine to improve your abilities!";
const char* const kUnlockedTitle = "New Upgrades Unlocked";
const char* const kUnlockedBody =
    "You've unlocked a new row of upgrades on the upgrades machine! \nHead there now to check them out!";
// (1.45 0x5c61e4, 0x5c61f6 (Android 1.43's 0xf2268c, 0xf22690 said "hospital"),
// 0x5c6258, 0x5c626d)
const char* const kHospitalTitle = "Hospital Unlocked";
const char* const kHospitalBody =
    "You've earned enough stars to unlock a brand new area! Click GO to head straight to the Hospital!";
const char* const kFlairgroundTitle = "Flairground Unlocked";
const char* const kFlairgroundBody = "You've earned enough stars to unlock a brand new area! Click GO to head "
                                     "straight to the Le Flairground Fantastique";
const char* const kSureTitle = "Are you sure?";
// (1.45 0x5c689c, 0x5c68f9) the pack is "refilled" by a win alone
// (SurgeryScreen::finishWin): a quit or a restart leaves it used
const char* const kQuitBody =
    "You will not get a chance to refill the blood pack you used on this patient if you quit now.";
const char* const kRetryBody =
    "You will not get a chance to refill the blood pack you used on this patient if you restart now.";
// (the port's) before the blood packs are in use (the first two surgeries)
const char* const kQuitBodyFree = "Your progress on this patient will be lost if you quit now.";
const char* const kRetryBodyFree = "Your progress on this patient will be lost if you restart now.";
// (1.45 0x5c6642, 0x5c6654, 0x5c66b6, 0x5c66ce)
const char* const kOutTitle = "Out of Bloodpacks";
const char* const kOutBody = "You're out of bloodpacks! You'll have another bloodpack in %s, then you can tackle more "
                             "patients!";
const char* const kFirstTitle = "Introducing Bloodpacks!";
const char* const kFirstBody = "When you fail, quit or retry a surgery, you use up a bloodpack. You need a bloodpack to "
                               "operate on a patient. They recharge every %d minutes and you can carry up to %d.";

}  // namespace

GeneralTipPopupScreen* GeneralTipPopupScreen::create(const std::string& type) {
    GeneralTipPopupScreen* s = new GeneralTipPopupScreen;
    s->m_type = type;
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

GeneralTipPopupScreen* GeneralTipPopupScreen::create(const std::string& type, CCObject* pause, SEL_CallFunc quit,
                                                     SEL_CallFunc retry) {
    GeneralTipPopupScreen* s = new GeneralTipPopupScreen;
    s->setName("GeneralTips_" + type);
    s->m_type = type;
    s->m_pause = pause;
    s->m_quit = quit;
    s->m_retry = retry;
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void GeneralTipPopupScreen::build(const char* layout) {
    m_def = loadUIDefinition(layout);
    buildUI(ui, m_popup, m_def, this, menu_selector(GeneralTipPopupScreen::onButton));
    addChild(m_popup);
}

void GeneralTipPopupScreen::load() {
    // (the pause screen has the popups' sheet already)
    if (!m_pause) loadSheets(this, {m_sheet});
    m_shade = CCSprite::create();
    m_shade->addChild(CCLayerColor::create(ccc4(0, 0, 0, 127)));
    addChild(m_shade);
    m_popup = CCNode::create();
    if (m_type == "hospital_unlocked" || m_type == "pocketgods_unlocked") {
        // (1.45 loadAssets 0x426368, cases 0 and 1; Android 1.43's 0x4ced30
        // had the Hospital's alone) one layout, with each area's picture
        const bool flair = m_type == "pocketgods_unlocked";
        build("common/in_game_tutorial_gotohospital.json");
        ui.setText("upgradesbox01", flair ? kFlairgroundTitle : kHospitalTitle);
        ui.setText("upgradesbox02", flair ? kFlairgroundBody : kHospitalBody);
        ui.setVisible("ingame_pocketgods_pic", flair);
        ui.setVisible("ingame_hospital_pic", !flair);
    } else if (m_type == "upgrades_unlocked") {
        build("common/in_game_tutorial_upgradespopups.json");
        ui.setText("upgradesbox01", kUnlockedTitle);
        ui.setText("upgradesbox02", kUnlockedBody);
    } else if (m_type == "upgrades") {
        build("common/in_game_tutorial_upgradespopups.json");
        ui.setText("upgradesbox01", kUpgradesTitle);
        ui.setText("upgradesbox02", kUpgradesBody);
    } else if (m_type == "shop") {
        build("common/in_game_tutorial_shoppopups.json");
        ui.setText("shopbox01", kShopTitle);
        ui.setText("shopbox02", kShopBody);
    } else if (m_type == "partner") {
        build("common/in_game_tutorial_partnerpopups.json");
        ui.setText("partnerbox01", kPartnerTitle);
        int index = SaveData::shared().currentSlot().selectedPartnerIndex;
        if (index == 1) {
            ui.setText("partnerbox02", kPartnerSaturday);
            ui.setItemImage(m_def, "ingame_tutorial_partnerbox_pic", "Saturday");
            m_partner = 1;
        } else if (index == 2) {
            ui.setText("partnerbox02", kPartnerReverend);
            ui.setItemImage(m_def, "ingame_tutorial_partnerbox_pic", "Reverand");
            m_partner = 2;
        } else {
            ui.setText("partnerbox02", kPartnerGiblets);
            ui.setItemImage(m_def, "ingame_tutorial_partnerbox_pic", "Giblets");
            ui.setVisible("ingame_popup_nobutton", false);
        }
    } else if (m_type == "quit_warning") {
        build("common/ingame_popup_quitretry.json");
        ui.setVisible("ingame_popup_retry", false);
        // (1.45 case 9) the blood pack it cost, pictured. (The pause screen
        // asks only after the first two surgeries, when one was taken; the
        // port's free texts otherwise.)
        const bool packs = GameStateUtils::BloodPacksInUse(SaveData::shared().currentSlot());
        ui.setVisible("ingame_bludpax_exit", packs);
        ui.setText("quitretry_title", kSureTitle);
        ui.setText("quitretry_body", packs ? kQuitBody : kQuitBodyFree);
    } else if (m_type == "retry_warning") {
        build("common/ingame_popup_quitretry.json");
        ui.setVisible("ingame_popup_quit", false);
        const bool packs = GameStateUtils::BloodPacksInUse(SaveData::shared().currentSlot());  // (1.45 case 10)
        ui.setVisible("ingame_bludpax_exit", packs);
        ui.setText("quitretry_title", kSureTitle);
        ui.setText("quitretry_body", packs ? kRetryBody : kRetryBodyFree);
    } else if (m_type == "bloodpacks_out" || m_type == "bloodpacks_first") {
        // (1.45 cases 7 and 8) The packs are never sold on the PC: the buy
        // button (the App Store's, or 1.45's bundles screen) and the count
        // it sold are hidden, and both popups close with the centred OK
        // (out of packs, the original's OK sat left of the buy button).
        log("blood packs: popup %s", m_type.c_str());
        build("common/ingame_popup_bludpax.json");
        ui.setVisible("ui_button_buybludpax", false);
        ui.setText("ui_purchase_packnumber_01", "");
        ui.setVisible("ingame_popup_nobutton", false);
        ui.setVisible("ingame_bludpax_ok", true);
        if (m_type == "bloodpacks_out") {
            // the time to the next, each second
            ui.setText("bludpaxbox01", kOutTitle);
            updateBloodPacks(0);
            schedule(schedule_selector(GeneralTipPopupScreen::updateBloodPacks), 1.0f);
        } else {
            char body[256];
            std::snprintf(body, sizeof body, kFirstBody, GameStateUtils::RechargeSecs() / 60,
                          GameStateUtils::MaxBloodPacks());
            ui.setText("bludpaxbox01", kFirstTitle);
            ui.setText("bludpaxbox02", body);
        }
    } else {
        log("GeneralTipPopupScreen: no popup %s on the PC", m_type.c_str());
        scheduleOnce(schedule_selector(GeneralTipPopupScreen::closeNow), 0);
        return;
    }
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    m_popup->setPosition(ccp(m_popup->getPositionX(), -static_cast<float>(r->height)));
    m_popup->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
}

void GeneralTipPopupScreen::unload() {
    removeAllChildrenWithCleanup(true);
    CCDirector::sharedDirector()->purgeCachedData();
}

void GeneralTipPopupScreen::updateBloodPacks(float dt) {
    // (1.45 0x42acf0) the time to the next pack (which recharges the slot)
    const std::string next = GameStateUtils::GetTimeUntilNextBloodpack();
    // Not in the original: one back, the popup goes by itself, the start
    // free again (the original's went on to the time to the one after,
    // saying it was out of packs; it closed itself only after a purchase,
    // vf91 0x42b198)
    // (dt 0: the first, from load, while it is being pushed: not yet)
    if (dt > 0 && SaveData::shared().currentSlot().numBloodPacks > 0) {
        log("blood packs: one back, out-of-packs popup closed");
        close();
        return;
    }
    char body[256];
    std::snprintf(body, sizeof body, kOutBody, next.c_str());
    ui.setText("bludpaxbox02", body);
}

void GeneralTipPopupScreen::close() {
    unschedule(schedule_selector(GeneralTipPopupScreen::updateBloodPacks));
    unload();
    unschedule(schedule_selector(GeneralTipPopupScreen::closeNow));
    SceneMgr::sharedSceneMgr()->popScene();  // (this screen may be gone after it)
}

void GeneralTipPopupScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->playSound("button_click", false);
    if (name == "ingame_popup_nobutton") {
        // (1.45 0x42a5a8: out of blood packs too, back to the screen under it)
        close();
    } else if (name == "ingame_popup_upgradesbutton") {
        sound->playSound("button_click", false);  // (twice, as the original)
        SceneMgr::sharedSceneMgr()->replaceScene(UpgradeScreen::create("area"));
    } else if (name == "ingame_popup_partnerbutton") {
        sound->playSound("button_click", false);
        if (m_partner != 0) {
            SaveData& save = SaveData::shared();
            save.currentSlot().selectedTagPartner = kPartnerIds[m_partner];
            save.saveGame();
        }
        // it slides out downwards, then goes
        const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
        CCFiniteTimeAction* out =
            CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, static_cast<float>(r->height) * -1.15f)), 0.25f);
        m_popup->runAction(CCSequence::create(
            out, CCCallFuncN::create(this, callfuncN_selector(GeneralTipPopupScreen::closeNode)), nullptr));
    } else if (name == "ingame_bludpax_ok" || name == "ingame_popup_goback") {
        close();
    } else if (name == "ingame_popup_gotohospital") {
        // (1.45 clickedButton 0x42a5a8, Android 1.43's 0x4d2d80) the world map
        // on the page of the area announced
        const char* id = m_type == "pocketgods_unlocked" ? "area_pocketgods" : "area_hospital";
        const std::vector<AreaDef>& areas = AreasDefUtils::shared()->areas;
        for (size_t i = 0; i < areas.size(); ++i)
            if (areas[i].areaId == id)
                SceneMgr::sharedSceneMgr()->replaceScene(WorldScreen::create(static_cast<int>(i) + 1));
    } else if (name == "ingame_popup_quit") {
        CCObject* pause = m_pause;
        SEL_CallFunc quit = m_quit;
        close();
        if (pause && quit) (pause->*quit)();
    } else if (name == "ingame_popup_retry") {
        CCObject* pause = m_pause;
        SEL_CallFunc retry = m_retry;
        close();
        if (pause && retry) (pause->*retry)();
    } else {
        // (ui_button_buybludpax bought a blood pack from the App Store: removed)
        log("%s", name.c_str());
    }
}

}  // namespace mt
