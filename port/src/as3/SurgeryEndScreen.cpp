#include "as3/SurgeryEndScreen.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/BloodPacks.h"
#include "as3/DialogueScreen.h"
#include "as3/GeneralTipPopupScreen.h"
#include "as3/Log.h"
#include "as3/ParticleSystem.h"
#include "as3/PreOpScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StarPanel.h"
#include "as3/SurgeryCurrencyScreen.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

const char* const kStars[3] = {"endscreen_rating_star_main1", "endscreen_rating_star_main2",
                               "endscreen_rating_star_main3"};

// lo..hi (0x644cc: the original's rand() went to 2^31 - 1)
float randomFloat(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) / (hi - lo));
}

// Whether a row of upgrades opened between these totals of stars: a row's
// stars over `before` and up to `now` (UpgradeInfoUtils 0x1346b0).
bool rowOpenedBetween(const UpgradeInfoUtils* u, int before, int now) {
    for (int stars : u->rowStars)
        if (stars <= now && before < stars) return true;
    return false;
}

// The upgrades not yet bought whose row is open and whose cost the coins
// cover (UpgradeInfoUtils 0x134534; unlike affordable, 0x13459c, the one
// under it in its column need not be bought).
int buyableUpgrades(const UpgradeInfoUtils* u, int stars, int coins) {
    const SavedGame& slot = SaveData::shared().currentSlot();
    int n = 0;
    for (const UpgradeDef& d : u->upgrades) {
        int need = d.row >= 1 && d.row <= 6 ? u->rowStars[d.row - 1] : 0;
        if (need <= stars && d.cost <= coins && !slot.owns(SavedGame::kItemUpgrade, upgradeIndex(d.id))) ++n;
    }
    return n;
}

std::string clock(int seconds) {
    char text[50];
    std::snprintf(text, sizeof(text), "%02d:%02d", seconds / 60, seconds % 60);
    return text;
}

}  // namespace

SurgeryEndScreen* SurgeryEndScreen::create(const SurgeryInfo& surgery, const SurgeryParams& params,
                                           const SurgeryResult& result) {
    SurgeryEndScreen* s = new SurgeryEndScreen;
    s->setName("SurgeryEndScreen");
    if (s->init(surgery, params, result)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

SurgeryEndScreen* SurgeryEndScreen::createLost(const SurgeryInfo& surgery, const SurgeryParams& params,
                                               const SurgeryResult& result, const SurgeryHooks& hooks) {
    // (the original gave this one no name)
    SurgeryEndScreen* s = new SurgeryEndScreen;
    s->m_hooks = hooks;
    if (s->init(surgery, params, result)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool SurgeryEndScreen::init(const SurgeryInfo& surgery, const SurgeryParams& params, const SurgeryResult& result) {
    // (0xb8298; the analytics' "surgery" event: removed)
    if (!Scene::init()) return false;
    m_coinsShown = !result.won;  // (no coins when lost)
    m_won = result.won;
    for (int i = 0; i < 3; ++i) m_stars[i] = result.stars[i];
    m_result = result;
    m_surgery = surgery;
    m_params = params;
    return true;
}

void SurgeryEndScreen::load() {
    // (0xb8670) the letter's sheet
    loadSheets(this, {m_won ? "surgeryend_lunacyfalls" : "surgeryend_fail"});
    m_shade = CCLayerColor::create(ccc4(0, 0, 0, 127));
    addChild(m_shade);
    if (m_won) {
        // the area's background; the Bunker's map after the last surgery's
        // first win. (Added over the shade, which then darkens nothing under
        // it: as the original.)
        loadSheets(this, {m_surgery.area.bgSpritesheet});
        CCSprite* bg;
        if (m_result.firstTime && m_surgery.id == "surgery_exileclinic_alanprobe") {
            loadSheets(this, {"areacommon_bunker"});
            bg = CCSprite::createWithSpriteFrameName("bunker_map.png");
            if (bg) setPositionOnCanvas(bg, 320, 221);
        } else {
            bg = CCSprite::createWithSpriteFrameName(m_surgery.area.bgSprite.c_str());
            if (bg) setPositionOnCanvas(bg, 320, 192);
        }
        if (bg) addChild(bg);
        m_shade->setOpacity(0);
    }
    m_def = loadUIDefinition("common/ui_end_screen.json");
    buildUI(ui, this, m_def, this, menu_selector(SurgeryEndScreen::onButton));
    m_hud.build(this, this, false, menu_selector(SurgeryEndScreen::onButton));

    for (const char* name : {"ui_endscreen_patientname", "ui_endscreen_surgerytime", "ui_endscreen_conditions",
                             "ui_endscreen_score"})
        ui.setVisible(name, m_won);
    // The stars, as this play earned them: the main surgery's, yellow, as
    // many as the slot has; a challenge's one star alone, in the middle, in
    // its colour (blue, red). Each one shown waits, invisible and large, to
    // drop in; those just gained sparkle. (Fixed: the original took the
    // middle star's colour from any challenge the slot had ever won, so the
    // main surgery played again showed it blue or red, and a challenge won
    // showed the main surgery's three stars dropping in.)
    const bool challenge = m_params.mode > 0;
    const int main = challenge ? 0 : m_stars[0];
    const int gained = m_result.starsGained;
    for (const char* name : {"endscreen_rating_basestar1", "endscreen_rating_basestar2", "endscreen_rating_basestar3"})
        ui.setItemImage(m_def, name, "normal");
    ui.setVisible("endscreen_rating_basestar1", main != 0);
    ui.setVisible("endscreen_rating_basestar2", true);
    ui.setVisible("endscreen_rating_basestar3", main != 0);
    for (int i = 0; i < 3; ++i) {
        m_starShown[i] = challenge ? i == 1 : main > i;
        ui.setVisible(kStars[i], m_starShown[i]);
        ui.setItemImage(m_def, kStars[i], !challenge ? "yellow" : m_params.mode == 1 ? "blue" : "red");
        if (!m_starShown[i]) continue;
        if (CCMenuItemImage* star = ui.item(kStars[i])) {
            star->setOpacity(0);
            star->setScale(2.5f);
        }
        if (challenge ? gained > 0 : i >= main - gained) sparkle(i + 1);
    }
    for (const char* name : {"endscreen_letter_over", "endscreen_letter_page_bonus", "endscreen_letter_under"})
        ui.setVisible(name, m_result.won);
    for (const char* name : {"ui_endscreen_patientnamefail", "ui_endscreen_timefail", "ui_endscreen_scorefail"})
        ui.setVisible(name, !m_result.won);
    for (const char* name : {"ui_button_quit", "ui_button_retry", "ui_button_next", "ui_button_revive",
                             "ui_button_use_revive", "ui_button_get_revive"})
        ui.setVisible(name, false);
    ui.setVisible("end_screen_fail", !m_result.won);
    // (1.45 hideSideBarAndFBButton: its layout's Facebook buttons and the
    // pre-op's star panel, never shown here)
    ui.setVisible("ui_button_facepost", false);
    ui.setVisible("ui_button_faceconnect", false);
    hideStarPanel(ui);
    ui.setText("ui_endscreen_revive", "");

    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (!m_result.won) {
        sound->playMusic("lose_loop", true);
        ui.setText("ui_endscreen_patientnamefail", m_surgery.patientTitle);
        ui.setText("ui_endscreen_timefail", clock(m_result.timeLeft));
        ui.setText("ui_endscreen_scorefail", std::to_string(m_result.score) + " POINTS");
        // (the original's progress was a float, rounded up: the port's is that already)
        char done[128];
        std::snprintf(done, sizeof(done), "%d%s", m_result.progress, "% Complete");
        ui.setText("ui_endscreen_percentagecomplete", done);
    } else {
        sound->playMusic("super-hero-loop", true);
        ui.setText("ui_endscreen_patientname", m_surgery.patientTitle);
        ui.setText("ui_endscreen_conditions", m_surgery.conditions);
        ui.setText("ui_endscreen_surgerytime", clock(m_result.timeLeft));
        ui.setText("ui_endscreen_score", std::to_string(m_result.score) + " POINTS");
        ui.setText("ui_endscreen_percentagecomplete", "");
    }

    if (!m_won) {
        showButtons();
    } else {
        // the stars drop in 0.4 s apart, each with its sound, then the coins.
        // (The original gave the three stars one scale and one fade action
        // between them, which a node takes from the one before as it starts:
        // each star has its own here.)
        int order = 0;
        for (int i = 0; i < 3; ++i) {
            CCNode* star = ui.node(kStars[i]);
            if (!m_starShown[i] || !star) continue;
            const float delay = 0.4f * static_cast<float>(order++);
            CCFiniteTimeAction* drop = CCEaseElasticOut::create(CCScaleTo::create(1.0f, 1.0f));
            CCFiniteTimeAction* fade = CCFadeIn::create(0.4f);
            if (delay == 0) {
                star->runAction(drop);
                star->runAction(fade);
            } else {
                star->runAction(CCSequence::create(CCDelayTime::create(delay), drop, nullptr));
                star->runAction(CCSequence::create(CCDelayTime::create(delay), fade, nullptr));
            }
            runAction(CCSequence::create(CCDelayTime::create(delay),
                                         CCCallFuncN::create(this, callfuncN_selector(SurgeryEndScreen::starImpact)),
                                         nullptr));
        }
        runAction(CCSequence::create(CCDelayTime::create(m_coinDelay),
                                     CCCallFuncN::create(this, callfuncN_selector(SurgeryEndScreen::showCoins)),
                                     CCDelayTime::create(0.5f),
                                     CCCallFuncN::create(this, callfuncN_selector(SurgeryEndScreen::coinParticles)),
                                     nullptr));
    }

    // The first win of an area's boss (or of the Bunker's prxy4) completes
    // the area: its ending comes next, then the area after it.
    m_areaComplete = false;
    m_nextArea = 0;
    m_completedArea = 0;
    const std::vector<AreaDef>& areas = AreasDefUtils::shared()->areas;
    if (m_result.firstTime) {
        for (size_t i = 0; i < areas.size(); ++i) {
            if (areas[i].areaId != m_surgery.area.areaId) continue;
            if (areas[i].bossSurgery == m_surgery.id) {
                m_completedArea = static_cast<int>(i);
                // (after the last, the second: 1.45 loadAssets 0xeebf4, as
                // Android 1.43. The last area, the Exile Clinic, goes to the
                // Bunker's map anyway: next())
                m_nextArea = i + 1 < areas.size() ? static_cast<int>(i) + 1 : 1;
                m_areaComplete = true;
            } else if (m_surgery.id == "surgery_bunker_prxy4") {
                // (its own area's ending, Android 1.43 loadAssets 0x7778e4: no longer the
                // first area since the Hospital is listed before the Bunker)
                m_completedArea = static_cast<int>(i);
                m_nextArea = static_cast<int>(i) + 1;
                m_areaComplete = true;
            }
        }
    }

    m_hud.refresh();
    schedule(schedule_selector(SurgeryEndScreen::tick), 1.0f);
    // The win or loss tune alone: no ambience under it. (The original then
    // started the area's ambience and stopped the music, fading out the tune
    // it had just started at nothing: it was never heard. The port plays
    // the tune and nothing else.)
    sound->stopChannel(SoundMgr::kAmbienceChannel, false);
}

void SurgeryEndScreen::tick(float) {
    // (0xbddc0; 1.45 updateStatus 0xf69f4) the top bar, the blood packs
    // recharged, saved when one came back
    m_hud.refresh();
    GameStateUtils::UpdateBloodPacksAndSave();
}

void SurgeryEndScreen::onRemoved() { unschedule(schedule_selector(SurgeryEndScreen::tick)); }

void SurgeryEndScreen::onShown() {
    // (0xbe4a8) after the coins' cheque, the tips, once. (Removed: the ads
    // before it, and the "rate us" prompt after a win.)
    Scene::onShown();
    if (!m_coinsShown) return;
    if (!m_tipsShown) showTips();
}

void SurgeryEndScreen::sparkle(int star) {
    // (0xbd058) sparkles at the star's middle
    if (star < 1 || star > 3) return;
    CCNode* node = ui.node(kStars[star - 1]);
    ParticleSystem* p = ParticleSystem::create("particle_starhighlightFX");
    if (!node || !p) return;
    p->setPosition(ccp(node->getContentSize().width * 0.5f, node->getContentSize().height * 0.5f));
    node->addChild(p);
}

void SurgeryEndScreen::starImpact(CCNode*) { SoundMgr::sharedSoundMgr()->playSound("Star_Impact", false); }

void SurgeryEndScreen::showCoins(CCNode*) {
    // (0xbd46c) the coins' cheque over this screen
    m_shade->runAction(CCFadeTo::create(1.0f, 100));
    SceneMgr::sharedSceneMgr()->pushScene(
        SurgeryCurrencyScreen::create(this, m_result.coinsFromScore, m_result.coinsBonus), false);
    m_coinsShown = true;
}

void SurgeryEndScreen::coinParticles(CCNode*) {
    // (0xbd4b0) the points particles in the middle, and the coins' sound
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    if (ParticleSystem* p = ParticleSystem::create("particle_surgeonpointsFX")) {
        m_coinParticles = p;
        p->setPosition(ccp(static_cast<float>(r->width / 2), static_cast<float>(r->height / 2) + r->canvasScale * 40.0f));
        addChild(p);
    }
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->playOnChannel("Money_Reward_Intro", coinChannel(), false, false, false, false);
    sound->playOnChannel("Money_Reward_Loop", coinChannel(), true, false, false, false);
}

void SurgeryEndScreen::stopCoinSound() { SoundMgr::sharedSoundMgr()->stopChannel(coinChannel(), true); }

void SurgeryEndScreen::hideShade() { m_shade->setOpacity(0); }

void SurgeryEndScreen::showButtons() {
    // (0xbd714) the particles stopped; the main surgery's first win only goes on
    if (m_result.won && m_coinParticles) m_coinParticles->stopSystem();
    if (!m_result.firstTime) {
        // (1.45 showButtons 0xf45e0 showed the revive, "use revive" with the
        // count owned or "get revive" sold for money: removed, left hidden)
        ui.setVisible("ui_button_quit", !m_won);
        ui.setVisible("ui_button_next", m_won);
        ui.setVisible("ui_button_retry", true);
        // (traced for test scripts: the buttons' centres, cocos points)
        for (const char* name : {"ui_button_quit", "ui_button_retry", "ui_button_next"})
            if (CCNode* b = ui.node(name))
                if (b->isVisible()) {
                    CCRect bb = b->boundingBox();
                    platform::debugTrace("end button %s %.0f,%.0f", name, bb.getMidX(), bb.getMidY());
                }
    } else {
        // next alone, moved over to the middle
        if (CCNode* next = ui.node("ui_button_next")) {
            next->setPositionX(next->getPositionX() + ResolutionMgr::sharedResolutionMgr()->canvasScale * -60.0f);
            next->setVisible(true);
        }
    }
}

void SurgeryEndScreen::showTips() {
    // (0xbdde8) the Hospital opened (Android 1.43); a tag partner given; or,
    // unless an area was completed, a new row of upgrades, or now and then
    // the upgrades to be bought
    if (m_tipsShown) return;
    const SavedGame& slot = SaveData::shared().currentSlot();
    const int coins = slot.coins();
    const int stars = slot.totalStars();
    const UpgradeInfoUtils* upgrades = UpgradeInfoUtils::shared();
    const bool rowOpened = rowOpenedBetween(upgrades, stars - m_result.starsGained, stars);
    const int buyable = buyableUpgrades(upgrades, stars, coins);
    const float chance = randomFloat(0, 1);
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    // (1.45 showTipsLogic 0xf6b8c; Android 1.43's 0x77c13c had the
    // Hospital's alone) first of all, once a slot each: the stars have
    // reached Le Flairground Fantastique's cost, else the Hospital's, after
    // any surgery won but an area's boss (whose win shows the area's end
    // instead) and outside that area. The Flairground's popup is the only tip
    // of that win; after the Hospital's the others may follow, each over the
    // one before. (Tips come after a win only, as on Android: onBecomeActive
    // 0x77ee80.)
    SavedGame& state = SaveData::shared().currentSlot();
    const bool notBoss = m_surgery.id != m_surgery.area.bossSurgery;
    const AreaDef* flairground = AreasDefUtils::shared()->byId("area_pocketgods");
    if (m_result.won && flairground && stars >= flairground->unlockCost && !state.seenPocketgodsUnlockedPopup &&
        notBoss && m_surgery.area.areaId != "area_pocketgods") {
        state.seenPocketgodsUnlockedPopup = true;
        SaveData::shared().saveGame();
        scenes->pushScene(GeneralTipPopupScreen::create("pocketgods_unlocked"), false);
        m_tipsShown = true;
        return;
    }
    const AreaDef* hospital = AreasDefUtils::shared()->byId("area_hospital");
    if (m_result.won && hospital && stars >= hospital->unlockCost && !state.seenHospitalUnlockedPopup && notBoss &&
        m_surgery.area.areaId != "area_hospital") {
        state.seenHospitalUnlockedPopup = true;
        SaveData::shared().saveGame();
        scenes->pushScene(GeneralTipPopupScreen::create("hospital_unlocked"), false);
        m_tipsShown = true;
    }
    // (1.45: the partner given and the upgrades' tip each in its own right;
    // 1.02's 0xbdde8 showed the upgrades' only without a partner given)
    if (m_result.b2c) {
        scenes->pushScene(GeneralTipPopupScreen::create("partner"), false);
        m_tipsShown = true;
    }
    if (!m_areaComplete) {
        if (rowOpened)
            scenes->pushScene(GeneralTipPopupScreen::create("upgrades_unlocked"), false);
        else if (chance <= 0.4f && buyable > 0)
            scenes->pushScene(GeneralTipPopupScreen::create("upgrades"), false);
        m_tipsShown = true;
    }
}

void SurgeryEndScreen::onButton(CCObject* sender) {
    // (0xbcd98)
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    SaveData& save = SaveData::shared();
    if (name == "ui_button_retry") {
        SceneMgr::sharedSceneMgr()->replaceScene(PreOpScreen::create(m_surgery, m_params));
    } else if (name == "ui_button_next") {
        next();
    } else if (name == "ui_button_quit") {
        save.saveGame();
        next();
    }
}

void SurgeryEndScreen::next() {
    // (0xbe250) the area's map; an area completed: its ending story, or after
    // the last surgery the Bunker's map
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    const std::vector<AreaDef>& areas = AreasDefUtils::shared()->areas;
    if (!m_areaComplete || areas.empty()) {
        scenes->replaceScene(AreaScreen::create(m_surgery.area, 0));
        return;
    }
    if (m_surgery.id == "surgery_hospital_chastitybeltt" || m_surgery.id == "surgery_pocketgods_whale") {
        // (1.45 goToNextScreen 0xf7000, as Android 1.43's 0x77eb58) the
        // Hospital's and the Flairground's bosses: back to the world map, no
        // ending story (the Flairground's dialogue_area_pocketgods_complete
        // is in no build)
        scenes->replaceScene(WorldScreen::create());
    } else if (m_surgery.id == "surgery_exileclinic_alanprobe") {
        size_t bunker = 0;
        for (size_t i = 0; i < areas.size(); ++i)
            if (areas[i].areaId == "area_bunker") bunker = i;
        SaveData& save = SaveData::shared();
        save.currentSlot().lastSelectedArea = areas[bunker].areaId;
        save.saveGame();
        scenes->replaceScene(AreaScreen::create(areas[bunker], 0));
    } else {
        size_t done = static_cast<size_t>(m_completedArea) < areas.size() ? m_completedArea : 0;
        scenes->replaceScene(DialogueScreen::createAreaComplete(areas[done], m_nextArea + 1));
    }
}

// ---- the debug hook

namespace {

}  // namespace

// AS_ENDSCREEN=<surgery>[,<mode>,<won>,<stars>,<gained>,<first>,<partner given>]
// (debug mode only): this screen on a made-up result of the surgery, e.g.
// AS_ENDSCREEN=surgery_bunker_prxy4,0,1,3,3,1 (its first win, three new
// stars: the Bunker completed), ...,1,1,1,1,0 (a challenge won: a blue
// star), ...,0,0 (lost: shown on its own, no surgery under it). The result's
// coins are 120 for the score and 50 a new star; nothing is saved.
Scene* endScreenViewer() {
    if (!platform::debugMode()) return nullptr;
    const char* spec = std::getenv("AS_ENDSCREEN");
    if (!spec || !*spec) return nullptr;
    std::vector<std::string> f;
    std::string cur;
    for (const char* c = spec;; ++c) {
        if (*c == ',' || !*c) {
            f.push_back(cur);
            cur.clear();
            if (!*c) break;
        } else {
            cur += *c;
        }
    }
    auto num = [&](size_t i, int def) { return i < f.size() && !f[i].empty() ? std::atoi(f[i].c_str()) : def; };
    const AreaDef* area = nullptr;
    for (const AreaDef& a : AreasDefUtils::shared()->areas)
        for (const std::string& s : a.surgeries)
            if (s == f[0] || a.bossSurgery == f[0]) area = &a;
    if (!area) {
        logError("AS_ENDSCREEN: no area has %s", f[0].c_str());
        return nullptr;
    }
    SurgeryInfo info = loadSurgeryInfo(f[0], *area);
    SurgeryParams params;
    params.mode = num(1, 0);
    SurgeryResult r;
    r.surgery = f[0];
    r.area = area->areaId;
    r.won = num(2, 1) != 0;
    int stars = num(3, 3);
    r.stars[0] = static_cast<unsigned char>(params.mode == 0 ? stars : 3);
    if (params.mode > 0) r.stars[params.mode] = 1;
    r.starsGained = num(4, 0);
    r.firstTime = num(5, 0) != 0;
    r.b2c = num(6, 0) != 0;
    r.timeLeft = 83;
    r.score = 12345;
    r.coinsFromScore = r.won ? 120 : 0;
    r.coinsBonus = r.won ? 50 * r.starsGained : 0;
    r.progress = 64;
    r.partner = SaveData::shared().currentSlot().selectedTagPartner;
    platform::debugTrace("end screen viewer: %s mode %d won %d stars %d gained %d first %d", f[0].c_str(), params.mode,
                         r.won, stars, r.starsGained, r.firstTime);
    if (r.won) return SurgeryEndScreen::create(info, params, r);
    return SurgeryEndScreen::createLost(info, params, r);
}

}  // namespace mt
