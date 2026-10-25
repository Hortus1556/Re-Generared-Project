#include "as3/WorldScreen.h"

#include <cstdio>

#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/AreaLockedScreen.h"
#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/DialogueScreen.h"
#include "as3/HorizontalScrollLayer.h"
#include "as3/Log.h"
#include "as3/ParticleSystem.h"
#include "as3/PartnerSelectScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/UpgradeScreen.h"
#include "as3/WelcomeScreen.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

// the tag partners' ids and their pictures' names (0x4322e4, 0x432304)
const char* const kPartnerIds[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};
const char* const kPartnerNames[8] = {"Giblets", "Saturday", "Reverand", "Flow", "Bot", "Nowe", "Crumpet", "Brutality"};

// a surgery's stars won in its three modes
int starsOf(const SavedGame& slot, const std::string& surgery) {
    return slot.starsOf(surgery, 0) + slot.starsOf(surgery, 1) + slot.starsOf(surgery, 2);
}

}  // namespace

std::string partnerButtonPicture(const std::string& partnerId) {
    if (partnerId.empty()) return "";
    std::string name;
    for (int i = 0; i < 8; ++i)
        if (partnerId == kPartnerIds[i]) name = kPartnerNames[i];
    // two have their own art names
    if (name == "Bot") name = "Procrastibot";
    else if (name == "Flow") name = "Flowmaster";
    return name;
}

WorldScreen::WorldScreen() {}

WorldScreen* WorldScreen::create(int area) {
    WorldScreen* s = new WorldScreen;
    s->setName("WorldScreen");
    if (s->init()) {
        s->m_area = area;
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void WorldScreen::load() {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float cs = r->canvasScale;
    loadSheets(this, {"world_map_sheet1"});
    loadSheets(this, {"world_map_sheet2"});
    loadSheets(this, {"world_map_sheet3"});  // (1.45 0x174ddc: the Flairground's button)
    loadSheets(this, {"area_unlock_popup_sheet"});
    loadSheets(this, {"global_ui_sheet"});
    // the layout and the top bar in one node, over the pages
    CCNode* layer = CCNode::create();
    m_def = loadUIDefinition("common/ui_worldmap.json");
    buildUI(ui, layer, m_def, this, menu_selector(WorldScreen::onButton));
    // the top bar's main layout (the original: its own target was this
    // node; none of its buttons is shown), its buy button hidden (0x2ba23c)
    m_hud.build(layer, this, false, menu_selector(WorldScreen::onButton));
    addChild(layer, 200);
    // the map's background goes under the pages
    if (CCNode* bg = ui.node("worldmap_bottomscreen-other")) {
        bg->retain();
        bg->removeFromParent();
        addChild(bg, 0);
        bg->release();
    }
    // the templates of the pages' labels and star
    for (const char* name : {"worldmap_starslocation", "worldmap_starlock", "worldmap_locationstar"})
        if (CCNode* n = ui.node(name)) n->removeFromParent();
    const UITextDef* starsText = m_def.text("worldmap_starslocation");
    const UITextDef* costText = m_def.text("worldmap_starlock");

    AreasDefUtils* areas = AreasDefUtils::shared();
    const SavedGame& slot = SaveData::shared().currentSlot();
    int affordable = UpgradeInfoUtils::shared()->affordable(slot.totalStars(), slot.coins());
    if (affordable < 1) {
        ui.setVisible("ui_notification", false);
        ui.setText("ui_popup_upgradenumber", "");
    } else {
        ui.setText("ui_popup_upgradenumber", std::to_string(affordable));
        ui.setVisible("ui_notification", true);
    }

    bool finished = slot.starsOf("surgery_exileclinic_alanprobe", 0) != 0;
    std::vector<CCNode*> pages;
    int remaining = 0;  // patients of the area before not treated yet
    // (1.45 0x174ddc, as Android 1.43's 0x887fb8) an area's cost is weighed
    // against every star won. 1.02 counted those of the areas before it: the
    // same for its areas, which open in turn, but not for the Flairground and
    // the Hospital, listed first and opened by stars alone.
    int stars = 0;
    for (const AreaDef& a : areas->areas)
        for (const std::string& s : a.surgeries) stars += starsOf(slot, s);
    if (cheat(Cheats::kStars250)) stars = 250;  // (debug: GetStarsBalance 0x138878; Cheats.h)
    int current = 0;
    for (int i = 0; i < static_cast<int>(areas->areas.size()); ++i) {
        const AreaDef& a = areas->areas[i];
        // (debug: cheat 11 opens every area, 1.45 0x174ddc; Cheats.h) The Flairground,
        // the Hospital and the Bunker need no area before them done (1.45; the
        // money-bought unlock, isAreaUnlockedByPurchase, is left out).
        bool first = a.areaId == "area_pocketgods" || a.areaId == "area_hospital" || a.areaId == "area_bunker";
        bool locked = ((!first && remaining > 0) || a.unlockCost > stars) && !cheat(Cheats::kUnlockAll);
        m_locked[i] = locked;
        CCNode* page = CCNode::create();
        page->setTag(i);
        if (slot.lastSelectedArea == a.areaId) current = i;
        CCSprite* icon = nullptr;
        char text[64];
        if (!locked) {
            int won = 0, most = 0;
            for (const std::string& s : a.surgeries) {
                // the Bunker's bonus surgery counts once the game is finished
                if (a.areaId == "area_bunker" && !finished && s == "surgery_bunker_hubris2") continue;
                won += starsOf(slot, s);
                most += 5;
            }
            std::snprintf(text, sizeof text, "%d/%d", won, most);
            if (won == 0) {
                // a new area sparkles
                if (ParticleSystem* fx = ParticleSystem::create("particle_newareaFX")) {
                    fx->setScale(2.0f);
                    fx->setAnchorPoint(ccp(0.5f, 0.5f));
                    fx->setPosition(ccp(cs * 0, cs * 0));
                    page->addChild(fx);
                }
            }
            std::string frame = a.worldMapIcon + "-normal.png";
            icon = CCSprite::createWithSpriteFrameName(frame.c_str());
            if (icon) page->addChild(icon);
            if (starsText) {
                if (CCLabelBMFont* l = createTextLabel(*starsText)) {
                    l->setAnchorPoint(ccp(1, 0));
                    l->setAlignment(kCCTextAlignmentRight);
                    l->setString(text);
                    l->setPosition(ccp(cs * 30, cs * -104));
                    page->addChild(l);
                }
            }
            if (CCSprite* star = CCSprite::createWithSpriteFrameName("worldmap_locationstar_yellow-normal.png")) {
                star->setAnchorPoint(CCPointZero);
                star->setPosition(ccp(cs * 29, cs * -103));
                page->addChild(star);
            }
        } else {
            std::string frame = a.worldMapIcon + "-locked.png";
            icon = CCSprite::createWithSpriteFrameName(frame.c_str());
            if (icon) page->addChild(icon);
            std::snprintf(text, sizeof text, "%d", a.unlockCost);
            if (costText) {
                if (CCLabelBMFont* l = createTextLabel(*costText)) {
                    l->setAnchorPoint(ccp(1, 0));
                    l->setAlignment(kCCTextAlignmentRight);
                    l->setString(text);
                    l->setPosition(ccp(cs * 15, cs * -30));
                    page->addChild(l);
                }
            }
        }
        m_icons.push_back(icon);
        pages.push_back(page);
        // this area's patients not treated yet, for the next one
        remaining = 0;
        for (const std::string& s : a.surgeries)
            if (slot.starsOf(s, 0) == 0 && s != "surgery_bunker_hubris2") ++remaining;
    }

    // the tag partner's picture on its button
    std::string partner = partnerButtonPicture(slot.selectedTagPartner);
    if (!partner.empty()) ui.setItemImage(m_def, "ui_button_tagpartner", partner);

    m_scroll = HorizontalScrollLayer::create(pages, static_cast<int>(cs * 245.0f), static_cast<int>(cs * 10.0f), true);
    m_scroll->setAnchorPoint(ccp(0.5f, 0.5f));
    setPositionOnCanvas(m_scroll, 320, 180);
    m_scroll->setContentSize(CCSizeMake(cs * 425.0f, cs * 285.0f));
    m_scroll->setPressed(this, callfuncN_selector(WorldScreen::onPressed));
    m_scroll->setReleased(this, callfuncN_selector(WorldScreen::onReleased));
    m_scroll->setSelected(this, callfuncN_selector(WorldScreen::onSelected));
    m_scroll->setPageChanged(this, callfuncN_selector(WorldScreen::onPageChanged));
    m_scroll->setPage(current + 1);
    addChild(m_scroll, 100);
    if (m_area >= 0) m_scroll->moveToPage(m_area);
    // (the port's) a dot for each page: the layout has six; 1.45 lit only
    // those six for its eight areas. (1.45's "coming soon" pages before and
    // after the areas, as Android's, are left out: nothing is coming.)
    if (CCNode* last = ui.node("worldmap_scrollcircle_06")) {
        CCNode* menu = last->getParent();
        for (int i = 7; i <= static_cast<int>(pages.size()) && menu && menu->getParent(); ++i) {
            CCSprite* dot = CCSprite::createWithSpriteFrameName("worldmap_scrollcircle-normal.png");
            if (!dot) break;
            dot->setAnchorPoint(last->getAnchorPoint());
            dot->setPosition(ccp(menu->getPositionX() + last->getPositionX() + cs * 16 * (i - 6),
                                 menu->getPositionY() + last->getPositionY()));
            menu->getParent()->addChild(dot, menu->getZOrder());
            m_extraDots.push_back(dot);
        }
        // the whole row in the middle of the dark tray the background has for
        // it: the layout's six were 2 canvas points left of its middle and 4
        // under it; the added dots go right, so the row moves left by half
        // of them (fixed: the eight sat off the tray, the last past its end)
        const float dx = cs * (2 - 16 * 0.5f * static_cast<float>(m_extraDots.size())), dy = cs * 4;
        for (int i = 1; i < 7; ++i) {
            char name[64];
            std::snprintf(name, sizeof name, "worldmap_scrollcircle_0%d", i);
            if (CCNode* d = ui.node(name)) d->setPosition(ccpAdd(d->getPosition(), ccp(dx, dy)));
        }
        for (CCSprite* d : m_extraDots) d->setPosition(ccpAdd(d->getPosition(), ccp(dx, dy)));
    }
    updateDots();

    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopChannel(SoundMgr::kAmbienceChannel, false);
    sound->playMusic("super-hero-loop", true);
    m_hud.refresh();
    schedule(schedule_selector(WorldScreen::tick), 1.0f);
}

void WorldScreen::tick(float) {
    m_hud.refresh();
    GameStateUtils::UpdateBloodPacksAndSave();  // (1.45 updateStatus 0x1786a4)
}

void WorldScreen::onShown() {
    Scene::onShown();
    setTouchRegistered(true);
    m_scroll->setTouchOn(true);
    m_busy = false;
    // (and the ads: removed)
}

void WorldScreen::onRemoved() {
    unschedule(schedule_selector(WorldScreen::tick));
    // (and the ads: removed)
}

void WorldScreen::updateDots() {
    for (int i = 1; i < 7; ++i) {
        char name[64];
        std::snprintf(name, sizeof name, "worldmap_scrollcircle_0%d", i);
        ui.setItemImage(m_def, name, i == m_scroll->page() ? "selected" : "normal");
    }
    for (size_t k = 0; k < m_extraDots.size(); ++k) {
        bool on = static_cast<int>(k) + 7 == m_scroll->page();
        m_extraDots[k]->setDisplayFrame(CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(
            on ? "worldmap_scrollcircle-selected.png" : "worldmap_scrollcircle-normal.png"));
    }
}

void WorldScreen::onPageChanged(CCNode*) { updateDots(); }

void WorldScreen::onPressed(CCNode* page) {
    if (shownForLessThanMinimum()) return;
    int i = page->getTag();
    if (m_locked[i] || !m_icons[i]) return;
    std::string frame = AreasDefUtils::shared()->areas[i].worldMapIcon + "-selected.png";
    m_icons[i]->setDisplayFrame(CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frame.c_str()));
}

void WorldScreen::onReleased(CCNode* page) {
    int i = page->getTag();
    if (m_locked[i] || !m_icons[i]) return;
    std::string frame = AreasDefUtils::shared()->areas[i].worldMapIcon + "-normal.png";
    m_icons[i]->setDisplayFrame(CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frame.c_str()));
}

void WorldScreen::onSelected(CCNode* page) {
    if (shownForLessThanMinimum() || m_busy) return;
    AreasDefUtils* areas = AreasDefUtils::shared();
    int at = m_scroll->page();  // (the page in the middle, not the one tapped)
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (at < 1 || at > static_cast<int>(areas->areas.size())) return;
    const AreaDef& area = areas->areas[at - 1];
    // (1.45) the Flairground, the Hospital and the Bunker have no area before them
    const bool first =
        area.areaId == "area_pocketgods" || area.areaId == "area_hospital" || area.areaId == "area_bunker";
    if (!m_locked[page->getTag()]) {
        SoundMgr::sharedSoundMgr()->playSound("button_click", false);
        slot.lastSelectedArea = area.areaId;
        save.saveGame();
        // (1.45 onSelectedPage 0x177b08, Android's 0x88bef8) an area with an
        // intro story (the Hospital, the Flairground) shows it while none of
        // its stars is won, then its map
        int won = 0;
        for (const std::string& s : area.surgeries) won += starsOf(slot, s);
        if (won == 0 && !area.introDialogueFile.empty()) {
            if (DialogueScreen* intro = DialogueScreen::createAreaIntro(area)) {
                SceneMgr::sharedSceneMgr()->replaceScene(intro);
                return;
            }
        }
        SceneMgr::sharedSceneMgr()->replaceScene(AreaScreen::create(area, 1));
        return;
    }
    // shut: why, over this screen
    m_busy = true;
    const AreaDef* before = first || at < 2 ? nullptr : &areas->areas[at - 2];
    int remaining = 0;
    if (before)
        for (const std::string& s : before->surgeries)
            if (slot.starsOf(s, 0) == 0 && s != "surgery_bunker_hubris2") ++remaining;
    setTouchRegistered(false);
    m_scroll->setTouchOn(false);
    if (remaining < 1)
        SceneMgr::sharedSceneMgr()->pushScene(AreaLockedScreen::create(area.title, area.unlockCost), false);
    else
        SceneMgr::sharedSceneMgr()->pushScene(AreaLockedScreen::create(area.title, before->title, area.unlockCost),
                                              false);
}

void WorldScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    if (name == "ui_button_updates") {
        scenes->replaceScene(UpgradeScreen::create("world"));
    } else if (name == "ui_button_partner") {
        scenes->replaceScene(PartnerSelectScreen::create("world", SurgeryInfo(), SurgeryParams()));
    } else if (name == "ui_button_back") {
        scenes->replaceScene(WelcomeScreen::create());
    }
}

}  // namespace mt
