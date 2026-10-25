#include "as3/SaveSlotScreen.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include "as3/Cheats.h"
#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/HorizontalScrollLayer.h"
#include "as3/IntroScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

SaveSlotScreen::SaveSlotScreen() {}

SaveSlotScreen* SaveSlotScreen::create() {
    SaveSlotScreen* s = new SaveSlotScreen;
    s->setName("SaveSlotScreen");
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

std::string lastPlayedText(const SavedGame& slot) {
    // Fixed: the original (1.45 GetTimeSinceLastSave 0x1389c4) counted from
    // the slot's lastBloodPackTimestamp (+0x88), the time its blood packs
    // last changed, which stands still while they are full; the port counts
    // from the slot's last save.
    long long then = static_cast<long long>(slot.lastSaveTimestamp);
    long long now = static_cast<long long>(std::time(nullptr));
    if (then > now) return "Just now";
    long long d = now - then;
    char b[64];
    auto count = [&](const char* format, double unit) {
        std::snprintf(b, sizeof b, format, static_cast<int>(std::floor(static_cast<double>(d) / unit)));
        return std::string(b);
    };
    if (d >= 63115201) return count("%d Years ago", 31557600);
    if (d > 31557599) return "1 Year ago";
    if (d > 5259479) return count("%d Months ago", 2629740);
    if (d > 2629739) return "1 Month ago";
    if (d > 1209599) return count("%d Weeks ago", 604800);
    if (d > 604799) return "1 Week ago";
    if (d > 172799) return count("%d Days ago", 86400);
    if (d > 86399) return "1 Day ago";
    if (d > 7199) return count("%d Hours ago", 3600);
    if (d > 3599) return "1 Hour ago";
    if (d > 119) return count("%d Minutes ago", 60);
    if (d >= 60) return "1 Minute ago";
    if (d != 0) return count("%d Seconds ago", 1);
    return "1 Seconds ago";
}

void SaveSlotScreen::load() {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float cs = r->canvasScale;
    m_busy = false;
    loadSheets(this, {m_sheet});
    loadSheets(this, {"welcome_screen_sheet"});
    // (the original also made a "start_screen_bg.png" sprite here and placed
    // it, but never added it: the layout has the background)
    m_def = loadUIDefinition("common/ui_saveslots.json");
    buildUI(ui, this, m_def, this, menu_selector(SaveSlotScreen::onButton));
    // the slots' texts are made from these (they are off the canvas)
    const UITextDef* starsText = m_def.text("saveslots_totalstars");
    const UITextDef* coinsText = m_def.text("saveslots_spbalance");
    const UITextDef* playedText = m_def.text("saveslots_lastplayed");

    SaveData& save = SaveData::shared();
    unsigned int current = save.currentSlot().uniqueId;
    int currentIndex = 0;
    AreasDefUtils* areas = AreasDefUtils::shared();
    auto label = [&](CCNode* page, const UITextDef* def, const std::string& text, float x, float y) {
        if (!def) return;
        CCLabelBMFont* l = createTextLabel(*def);
        if (!l) return;
        l->setAlignment(kCCTextAlignmentLeft);
        l->setString(text.c_str());
        l->setPosition(ccp(cs * x, cs * y));
        page->addChild(l);
    };
    for (size_t i = 0; i < save.savedGames.size(); ++i) {
        const SavedGame& slot = save.savedGames[i];
        CCNode* page = CCNode::create();
        page->setTag(static_cast<int>(i));
        // the area of the first surgery not won yet (the Bunker's bonus
        // surgery hubris2 aside), else the Bunker's. 1.45 (loadAssets
        // 0x403ffc) leaves the Hospital and the Flairground out, as areas
        // beside the story (the Flairground has no button picture). Android
        // 1.43, listing the Hospital first, took it for every slot and then
        // showed the Bunker's button for it until the game was finished,
        // whatever the area reached.
        std::string area = "area_bunker";
        bool found = false;
        for (const AreaDef& a : areas->areas) {
            if (a.areaId == "area_hospital" || a.areaId == "area_pocketgods") continue;
            for (const std::string& surgery : a.surgeries)
                if (!found && slot.starsOf(surgery, 0) == 0 && surgery != "surgery_bunker_hubris2") {
                    area = a.areaId;
                    found = true;
                }
        }
        if (slot.uniqueId == current) currentIndex = static_cast<int>(i);
        std::string picture = "savebutton-" + area + ".png";
        CCSprite* button = CCSprite::createWithSpriteFrameName(picture.c_str());
        if (button) page->addChild(button);
        label(page, starsText, std::to_string(slot.totalStars()), -30, -12);
        label(page, coinsText, std::to_string(slot.coins()), 20, -12);
        label(page, playedText, lastPlayedText(slot), -30, -40);
        m_buttons.push_back(button);
        m_pages.push_back(page);
    }
    // the new game, last
    CCNode* page = CCNode::create();
    page->setTag(-1);
    CCSprite* button = CCSprite::createWithSpriteFrameName("savebutton-newgame.png");
    if (button) page->addChild(button);
    m_buttons.push_back(button);
    m_pages.push_back(page);

    m_scroll = HorizontalScrollLayer::create(m_pages, static_cast<int>(cs * 245.0f), static_cast<int>(cs * -45.0f), false);
    m_scroll->setMode(3);  // the page touched, not only the middle one
    m_scroll->setAnchorPoint(ccp(0.5f, 0.5f));
    setPositionOnCanvas(m_scroll, 320, 145);
    m_scroll->setContentSize(CCSizeMake(cs * 425.0f, cs * 285.0f));
    m_scroll->setPressed(this, callfuncN_selector(SaveSlotScreen::onPressed));
    m_scroll->setReleased(this, callfuncN_selector(SaveSlotScreen::onReleased));
    m_scroll->setSelected(this, callfuncN_selector(SaveSlotScreen::onSelected));
    m_scroll->setPageChanged(this, callfuncN_selector(SaveSlotScreen::onPageChanged));
    addChild(m_scroll);
    m_scroll->setPage(currentIndex + 1);
}

void SaveSlotScreen::onSelected(CCNode* page) {
    // (m_busy: set once a slot is picked; the original never set it, and a
    // second tap before the screen went made a second new slot)
    if (shownForLessThanMinimum() || m_busy) return;
    m_busy = true;
    SaveData& save = SaveData::shared();
    if (page->getTag() == -1) {
        // a new slot, made current and saved (0xe35e0); the others are kept
        save.newSlot();
        save.save();
        // (and the analytics' session state: removed)
    } else {
        save.lastLoaded = save.savedGames[page->getTag()].uniqueId;
        save.save();
        SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    }
    SavedGame& slot = save.currentSlot();
    // Fixed: the slot's own sound and music switches. (The original applied
    // them only as the title screen loaded, and only to turn them off: a
    // slot picked after another kept the other's, while its pause screen
    // showed its own.)
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (slot.sfxOn != sound->areSoundEffectsOn()) {
        if (slot.sfxOn) sound->soundEffectsOn();
        else sound->soundEffectsOff();
    }
    if (slot.musicOn != sound->isMusicOn()) {
        if (slot.musicOn) sound->musicOn();
        else sound->musicOff();
    }
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    // (debug: cheat 11 skips both, 1.45 0x404ffc; Cheats.h)
    const bool all = cheat(Cheats::kUnlockAll);
    if (!all && slot.starsOf("surgery_bunker_mrgiblets", 0) == 0) {
        SoundMgr::sharedSoundMgr()->stopMusic(false);
        scenes->replaceScene(IntroScreen::create());
    } else if (!all && slot.starsOf("surgery_bunker_prxy4", 0) == 0) {
        // (the Bunker by its id, as Android 1.43 has it: its Hospital is
        // listed first)
        const AreaDef* bunker = AreasDefUtils::shared()->byId("area_bunker");
        if (!bunker) return;
        slot.lastSelectedArea = bunker->areaId;
        save.saveGame();
        scenes->replaceScene(AreaScreen::create(*bunker, 0));
    } else {
        scenes->replaceScene(WorldScreen::create());
    }
}

}  // namespace mt
