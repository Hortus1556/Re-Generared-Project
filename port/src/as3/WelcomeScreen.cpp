#include "as3/WelcomeScreen.h"

#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/AchievementsScreen.h"
#include "as3/CreditsScreen.h"
#include "as3/IntroScreen.h"
#include "as3/Log.h"
#include "as3/SaveData.h"
#include "as3/SaveSlotScreen.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"

namespace mt {

WelcomeScreen* WelcomeScreen::create() {
    WelcomeScreen* s = new WelcomeScreen;
    s->setName("WelcomeScreen");
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void WelcomeScreen::load() {
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->preloadGroup(SoundMgr::kGameCommon);
    SavedGame& slot = SaveData::shared().currentSlot();
    if (!slot.sfxOn) sound->soundEffectsOff();
    if (!slot.musicOn) sound->musicOff();
    sound->stopChannel(SoundMgr::kAmbienceChannel, false);
    sound->playMusic("super-hero-loop", true);
    loadSheets(this, {"welcome_screen_sheet"});
    m_def = loadUIDefinition("common/ui_startscreen.json");
    buildUI(ui, this, m_def, this, menu_selector(WelcomeScreen::onButton));
    ui.setVisible("start_button_play", true);
    ui.setVisible("start_button_credit", true);
    // the Game Center button (1.45 0x318238: Game Center's achievements)
    // opens the port's own list, AchievementsScreen; the other online ones
    // (the news feed, more games): hidden
    ui.setVisible("start_button_gamecenter", true);
    ui.setVisible("start_button_news", false);
    ui.setVisible("start_button_moregames", false);
    // (1.45's layout adds Facebook's and the store's: connect, log in and out,
    // its coin reward, restore purchases: hidden)
    for (const char* n : {"start_button_connect", "start_button_login", "start_button_logout", "start_button_restore",
                          "FB_Connect", "FB_LogIn", "FB_LogOut", "FB_coinreward", "coinreward", "ui_connect_reward"})
        ui.setVisible(n, false);
    // (the original reported the "more games" button's place to its analytics)
    // the current slot's blood packs recharged, saved if one came back (1.45
    // vf87 0x317710; a first launch has no slot to recharge yet)
    if (!SaveData::shared().savedGames.empty()) GameStateUtils::UpdateBloodPacksAndSave();
}

void WelcomeScreen::onButton(cocos2d::CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (name == "start_button_play") {
        SaveData& save = SaveData::shared();
        SavedGame& slot = save.currentSlot();
        // a new player (the first surgery never played, at most one slot): the
        // intro first (debug: not with cheat 11, 1.45 0x318238; Cheats.h)
        if (!cheat(Cheats::kUnlockAll) && slot.starsOf("surgery_bunker_mrgiblets", 0) == 0 &&
            save.savedGames.size() < 2) {
            SoundMgr::sharedSoundMgr()->stopMusic(false);
            SceneMgr::sharedSceneMgr()->replaceScene(IntroScreen::create());
            return;
        }
        SceneMgr::sharedSceneMgr()->replaceScene(SaveSlotScreen::create());
    } else if (name == "start_button_gamecenter") {
        SceneMgr::sharedSceneMgr()->replaceScene(AchievementsScreen::create());
    } else if (name == "start_button_credit") {
        SceneMgr::sharedSceneMgr()->replaceScene(CreditsScreen::create());
    } else {
        log("Clicked unknown button %s", name.c_str());
    }
}

}  // namespace mt
