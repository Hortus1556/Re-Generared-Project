#include "as3/PauseScreen.h"

#include <cstdlib>

#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/EditorPlay.h"
#include "as3/GeneralTipPopupScreen.h"
#include "as3/Log.h"
#include "as3/PartnerInfoUtils.h"
#include "as3/PreOpScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryScreen.h"
#include "as3/WelcomeScreen.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCTexture2D.h"
#include "engine/cocos/CCTouch.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

// the tray's buttons, pressed by the screen's own touches (vf95's order)
const char* const kTrayButtons[4] = {"pausescreen_quit", "pausescreen_sfx", "pausescreen_music", "pausescreen_help"};

}  // namespace

PauseScreen::PauseScreen() : m_sheet("surgery_pause_sheet") {}

PauseScreen* PauseScreen::create(const SurgeryInfo& surgery, const SurgeryHooks& hooks, const SurgeryParams& params,
                                 bool locked) {
    PauseScreen* s = new PauseScreen;
    s->setName("PauseScreen");
    if (s->init()) {
        // (0x11df98) the surgery's sounds paused (the analytics' "game_end_pause": removed)
        s->m_hooks = hooks;
        s->m_surgery = surgery;
        s->m_params = params;
        s->m_locked = locked;
        SoundMgr::sharedSoundMgr()->pauseSounds();
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void PauseScreen::load() {
    SoundMgr::sharedSoundMgr()->playSound("pause_swoosh", false);
    loadSheets(this, {m_sheet});
    // the surgery darkened
    m_shade = CCSprite::create();
    m_shade->addChild(CCLayerColor::create(ccc4(0, 0, 0, 127)));
    addChild(m_shade);
    // the tray, dropped from above
    m_root = CCNode::create();
    addChild(m_root);
    m_root->setPosition(ccp(m_root->getPositionX(), static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height)));
    const SavedGame& slot = SaveData::shared().currentSlot();
    m_def = loadUIDefinition("common/ui_pause_screen.json");
    buildUI(ui, m_root, m_def, this, menu_selector(PauseScreen::onButton));
    // the tag partner's hand: Ophelia's when there is none (the original took
    // the name from the surgery's partner by its index: the same partner)
    const std::string& partner = slot.selectedTagPartner;
    ui.setItemImage(m_def, "pausescreen_taghand", partner.empty() ? "Ophelia" : partnerName(partner));
    m_root->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(0, 0)), 1.0f));
    m_shade->runAction(CCFadeIn::create(0.5f));
    ui.setVisible("pausescreen_button_music_off", !slot.musicOn);
    ui.setVisible("pausescreen_button_music_on", slot.musicOn);
    ui.setVisible("pausescreen_button_sfx_off", !slot.sfxOn);
    ui.setVisible("pausescreen_button_sfx_on", slot.sfxOn);
    // (the layout's blood pack, ui_global_pauseblud: art no sheet has, the
    // original showed nothing there; hidden)
    ui.setVisible("ui_global_totalbludpax", false);
    // (the layout's supertools_* items: art no build has, for consumables no
    // build sold; hidden)
    for (const char* n : {"supertools_panel", "supertools_item01", "supertools_item02", "supertools_button_use01",
                          "supertools_button_use02", "ui_supertools_itemnumber01", "ui_supertools_itemnumber02"})
        ui.setVisible(n, false);
    // the back arrow slides in from the left
    if (CCNode* back = ui.node("ui_button_back")) {
        m_backX = back->getPositionX();
        back->setPositionX(m_backX + back->getContentSize().width * -1.5f);
        back->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(m_backX, back->getPositionY())), 1.0f));
    }
}

void PauseScreen::unload() {
    removeAllChildrenWithCleanup(true);
    unloadSheets({m_sheet});
    // CCDirector::purgeCachedData as cocos2d-x 2.1.1 had it: the fonts and the
    // unused textures. (The engine's also drops the unused sprite frames,
    // which the surgery below still asks for.)
    CCLabelBMFont::purgeCachedData();
    CCTextureCache::sharedTextureCache()->removeUnusedTextures();
}

void PauseScreen::onShown() {
    Scene::onShown();
    setTouchRegistered(true);
}

bool PauseScreen::ccTouchBegan(CCTouch* touch, CCEvent*) {
    // (0x11f964) a tray button pressed: down 5 points while held. (Its back
    // arrow part never runs: the arrow is a menu button, which takes the
    // touch first.)
    if (m_locked) return false;
    CCPoint p = touch->getLocation();
    for (const char* name : kTrayButtons) {
        CCNode* n = ui.node(name);
        if (n && n->boundingBox().containsPoint(p)) {
            n->setPosition(ccpAdd(n->getPosition(), ccp(-5, -5)));
            m_pressed = n;
            return true;
        }
    }
    CCNode* back = ui.node("ui_button_back");
    if (back && back->boundingBox().containsPoint(p)) {
        ui.setItemImage(m_def, "ui_button_back", "selected");
        m_pressed = back;
        return true;
    }
    return false;
}

void PauseScreen::ccTouchMoved(CCTouch* touch, CCEvent*) {
    // (0x120000) off the button: let go of it
    if (!m_pressed) return;
    CCMenuItem* item = dynamic_cast<CCMenuItem*>(m_pressed);
    CCRect r = item ? item->rect() : m_pressed->boundingBox();
    if (r.containsPoint(touch->getLocation())) return;
    if (m_pressed != ui.node("ui_button_back")) m_pressed->setPosition(ccpAdd(m_pressed->getPosition(), ccp(5, 5)));
    m_pressed = nullptr;
}

void PauseScreen::ccTouchEnded(CCTouch*, CCEvent*) {
    // (0x12006c) released on the button: it acts. (Put back up first: the
    // original did it after, when quitting may have taken the screen down.
    // The back arrow was never pushed down: the original moved it up 5
    // points anyway.)
    if (!m_pressed) return;
    CCNode* n = m_pressed;
    m_pressed = nullptr;
    if (n != ui.node("ui_button_back")) n->setPosition(ccpAdd(n->getPosition(), ccp(5, 5)));
    onButton(n);
}

void PauseScreen::onButton(CCObject* sender) {
    // (0x11ef70) not while the tray moves
    if (shownForLessThanMinimum()) return;
    if (m_root->numberOfRunningActions() != 0) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (name == "ui_button_back") {
        closeTray();
    } else if (name == "pausescreen_quit") {
        setTouchRegistered(false);
        if (earlySurgeries()) {
            quit();
        } else {
            SceneMgr::sharedSceneMgr()->pushScene(
                GeneralTipPopupScreen::create("quit_warning", this, callfunc_selector(PauseScreen::quit),
                                              callfunc_selector(PauseScreen::retry)),
                false);
        }
    } else if (name == "pausescreen_help") {
        // (the help button restarts the surgery)
        setTouchRegistered(false);
        if (earlySurgeries()) {
            retry();
        } else {
            SceneMgr::sharedSceneMgr()->pushScene(
                GeneralTipPopupScreen::create("retry_warning", this, callfunc_selector(PauseScreen::quit),
                                              callfunc_selector(PauseScreen::retry)),
                false);
        }
    } else if (name == "pausescreen_music") {
        toggleMusic();
    } else if (name == "pausescreen_sfx") {
        toggleSfx();
    }
}

bool PauseScreen::earlySurgeries() const {
    // the first two surgeries not both won: quit and restart ask nothing
    const SavedGame& slot = SaveData::shared().currentSlot();
    return slot.starsOf("surgery_bunker_mrgiblets", 0) == 0 || slot.starsOf("surgery_bunker_prxy4", 0) == 0;
}

void PauseScreen::closeTray() {
    // (0x11ef70's back) the tray up and away, then the surgery again
    SoundMgr::sharedSoundMgr()->playSound("pause_swoosh", false);
    setTouchRegistered(false);
    float h = static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height);
    CCActionInterval* up = CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, h * 1.15f)), 0.25f);
    m_root->runAction(CCSequence::create(up, CCCallFuncN::create(this, callfuncN_selector(PauseScreen::resumed)), nullptr));
    m_shade->runAction(CCFadeOut::create(0.1f));
    if (CCNode* back = ui.node("ui_button_back")) {
        CCPoint to = ccp(m_backX + back->getContentSize().width * -1.5f, back->getPositionY());
        back->runAction(CCEaseElasticOut::create(CCMoveTo::create(1.0f, to), 0.5f));
    }
}

void PauseScreen::resumed(CCNode*) {
    // (0x1200b0) the surgery's sounds on again, and the surgery shown
    retain();
    autorelease();  // (popScene lets go of this screen)
    setTouchRegistered(false);
    SoundMgr::sharedSoundMgr()->resumeSounds();
    SceneMgr::sharedSceneMgr()->popScene();
}

void PauseScreen::noteFailure() {
    // the surgery left unfinished: noted in the slot as failed, with its mode
    // and stage. (The original also took off the revives used in it: the
    // revives are removed.)
    if (editor::isEditedPlay(m_params)) return;  // (a surgery editor's level: nothing kept, EditorPlay.h)
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    slot.lastFailedSurgery = m_surgery.id;
    slot.lastFailedSurgeryMode = m_params.mode;
    slot.lastFailedSurgeryStage = m_hooks.stage;
    save.saveGame();
}

void PauseScreen::quit() {
    // (0x1200cc; the analytics' "game_end_quit": removed) this screen off,
    // the surgery stopped, then the area's map, or the title screen before
    // the first surgery is won
    retain();
    autorelease();  // (popScene lets go of this screen)
    noteFailure();
    setTouchRegistered(false);
    SceneMgr::sharedSceneMgr()->popScene();
    m_hooks.call(m_hooks.cleanUp);  // (SurgeryScreen 0x3d574)
    Scene* next;
    if (editor::isEditedPlay(m_params))
        next = editor::leavePlay();  // (back to the editor or its list)
    else if (SaveData::shared().currentSlot().starsOf("surgery_bunker_mrgiblets", 0) == 0)
        next = WelcomeScreen::create();
    else
        next = AreaScreen::create(m_surgery.area, 0);
    SceneMgr::sharedSceneMgr()->replaceScene(next);
}

void PauseScreen::retry() {
    // (0x120350) the surgery's pre-op screen again. (Its paused sounds are
    // stopped: the original left them paused for good.)
    noteFailure();
    setTouchRegistered(false);
    SoundMgr::sharedSoundMgr()->stopAllSounds();
    if (editor::isEditedPlay(m_params)) {  // (a surgery editor's level: again at once)
        SceneMgr::sharedSceneMgr()->replaceScene(editor::replay());
        return;
    }
    SceneMgr::sharedSceneMgr()->replaceScene(PreOpScreen::create(m_surgery, m_params));
}

void PauseScreen::toggleMusic() {
    // (0x1203c0)
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (!slot.musicOn) {
        slot.musicOn = true;
        SoundMgr::sharedSoundMgr()->musicOn();
    } else {
        slot.musicOn = false;
        SoundMgr::sharedSoundMgr()->musicOff();
    }
    ui.setVisible("pausescreen_button_music_off", !slot.musicOn);
    ui.setVisible("pausescreen_button_music_on", slot.musicOn);
    save.saveGame();
}

void PauseScreen::toggleSfx() {
    // (0x1205f0)
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (!slot.sfxOn) {
        slot.sfxOn = true;
        SoundMgr::sharedSoundMgr()->soundEffectsOn();
    } else {
        slot.sfxOn = false;
        SoundMgr::sharedSoundMgr()->soundEffectsOff();
    }
    ui.setVisible("pausescreen_button_sfx_off", !slot.sfxOn);
    ui.setVisible("pausescreen_button_sfx_on", slot.sfxOn);
    save.saveGame();
}

// ---- the debug hook

namespace {

// pushes the pause screen over the surgery screen on top, as its pause button is to
class PauseKick : public CCObject {
public:
    PauseKick(const SurgeryInfo& info, const SurgeryParams& params) : m_info(info), m_params(params) {}
    void push() {
        SceneMgr* scenes = SceneMgr::sharedSceneMgr();
        SurgeryHooks hooks;
        hooks.screen = scenes->topScene();
        platform::debugTrace("pause viewer: pushed over %s", hooks.screen ? scenes->topScene()->getName().c_str() : "-");
        scenes->pushScene(PauseScreen::create(m_info, hooks, m_params), false);
    }

private:
    SurgeryInfo m_info;
    SurgeryParams m_params;
};

}  // namespace

Scene* pauseViewer() {
    if (!platform::debugMode()) return nullptr;
    const char* spec = std::getenv("AS_PAUSE");
    if (!spec || !*spec) return nullptr;
    std::string id = spec;
    float seconds = 5;
    std::string::size_type comma = id.find(',');
    if (comma != std::string::npos) {
        seconds = static_cast<float>(std::atof(id.c_str() + comma + 1));
        id = id.substr(0, comma);
    }
    const AreaDef* area = nullptr;
    for (const AreaDef& a : AreasDefUtils::shared()->areas)
        for (const std::string& s : a.surgeries)
            if (s == id) area = &a;
    if (!area) {
        logError("AS_PAUSE: no area has %s", id.c_str());
        return nullptr;
    }
    SurgeryInfo info = loadSurgeryInfo(id, *area);
    SurgeryParams params;
    PauseKick* kick = new PauseKick(info, params);
    kick->autorelease();
    SceneMgr::sharedSceneMgr()->runAction(CCSequence::create(
        CCDelayTime::create(seconds), CCCallFunc::create(kick, callfunc_selector(PauseKick::push)), nullptr));
    return SurgeryScreen::create(info, params);
}

}  // namespace mt
