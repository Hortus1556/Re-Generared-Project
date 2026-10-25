#include "as3/PartnerSelectScreen.h"

#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/CooldownInfoScreen.h"
#include "as3/GeneralTipPopupScreen.h"
#include "as3/HorizontalScrollLayer.h"
#include "as3/Log.h"
#include "as3/PartnerCooldownInfoScreen.h"
#include "as3/PartnerInfoScreen.h"
#include "as3/PartnerInfoUtils.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryScreen.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/UpgradeScreen.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

// bought or given, or (1.45: onSelectedPage 0x2c6518) today's free partner
// (debug: or every one with cheat 17, 1.45 0x2c4530/0x2c6518/0x2c7aa0; Cheats.h)
bool owns(const std::string& id) {
    if (!id.empty() && id == PartnerInfoUtils::shared()->freeToday) return true;
    if (cheat(Cheats::kAllPartners)) return true;
    return SaveData::shared().currentSlot().owns(SavedGame::kItemPartner, partnerIndex(id));
}

void setFrame(CCSprite* sprite, const std::string& frame) {
    if (CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frame.c_str()))
        sprite->setDisplayFrame(f);
}

// the partner a page shows (its tag: its place in PartnerInfoUtils' list)
const PartnerInfo* partnerOf(CCNode* page) {
    const std::vector<PartnerInfo>& all = PartnerInfoUtils::shared()->partners;
    int i = page ? page->getTag() : -1;
    return i >= 0 && i < static_cast<int>(all.size()) ? &all[i] : nullptr;
}

}  // namespace

PartnerSelectScreen* PartnerSelectScreen::create(const std::string& from, const SurgeryInfo& surgery,
                                                 const SurgeryParams& params) {
    PartnerSelectScreen* s = new PartnerSelectScreen;
    s->setName("PartnerSelectScreen");
    if (s->init(surgery, params)) {
        s->autorelease();
        s->m_from = from;
        return s;
    }
    delete s;
    return nullptr;
}

bool PartnerSelectScreen::init(const SurgeryInfo& surgery, const SurgeryParams& params) {
    if (!Scene::init()) return false;
    m_surgery = surgery;
    m_params = params;
    m_tick = nullptr;
    return true;
}

PartnerSelectScreen::~PartnerSelectScreen() {
    CC_SAFE_RELEASE(m_tick);
    CC_SAFE_RELEASE(m_shadeBottom);
    CC_SAFE_RELEASE(m_shadeTop);
}

void PartnerSelectScreen::load() {
    float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    m_leaving = false;
    if (m_from == "preop") {
        loadSheets(this, {m_preopSheet});
        // shades over the pre-op screen, above and below, faded in once it is in
        CCSize win = CCDirector::sharedDirector()->getWinSize();
        m_shadeBottom = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 0, 150), ccp(0, -1));
        m_shadeBottom->setContentSize(CCSizeMake(win.width, win.height * 0.25f));
        m_shadeTop = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 0, 150), ccp(0, 1));
        m_shadeTop->setContentSize(CCSizeMake(win.width, win.height * 0.25f));
        m_shadeTop->setPosition(ccp(0, win.height * 0.75f));
        m_shadeBottom->retain();
        m_shadeTop->retain();
        addChild(m_shadeBottom);
        addChild(m_shadeTop);
        m_shadeBottom->setOpacity(0);
        m_shadeTop->setOpacity(0);
    } else {
        loadSheets(this, {m_sheet});
    }
    m_def = loadUIDefinition("common/ui_tagpartner_01.json");
    buildUI(ui, this, m_def, this, menu_selector(PartnerSelectScreen::onButton));
    // (1.45's layout: the revives sold for money, and the badges' templates)
    for (const char* n : {"ui_button_revives", "ui_popup_revivenumber", "tagscreen_employee", "tagscreen_freetag"})
        ui.setVisible(n, false);
    // the big portrait of the chosen partner: pressed, it drops it
    if (CCMenuItem* big = ui.item("ui_tagscreen")) {
        big->setTarget(this, menu_selector(PartnerSelectScreen::onButton));
        big->setEnabled(true);
    }
    m_hud.build(this, this, false, menu_selector(PartnerSelectScreen::onButton));
    CC_SAFE_RELEASE_NULL(m_tick);
    m_tick = CCSprite::createWithSpriteFrameName("tagscreen_tickbox-normal.png");
    if (m_tick) {
        m_tick->retain();
        m_tick->setAnchorPoint(CCPointZero);
        m_tick->setPosition(ccp(10 * s, -50 * s));
    }

    // a page for each partner: its portrait, and its cooldown under it
    const SavedGame& slot = SaveData::shared().currentSlot();
    const std::vector<PartnerInfo>& partners = PartnerInfoUtils::shared()->partners;
    int chosen = 0;
    for (size_t i = 0; i < partners.size(); ++i) {
        const PartnerInfo& p = partners[i];
        CCNode* page = CCNode::create();
        page->setTag(static_cast<int>(i));
        if (slot.selectedTagPartner == p.id) chosen = static_cast<int>(i);
        CCSprite* portrait =
            CCSprite::createWithSpriteFrameName(("tagscreen_portrait-" + partnerName(p.id) + "-normal.png").c_str());
        if (!portrait) portrait = CCSprite::create();
        page->addChild(portrait);
        PartnerCooldownInfoScreen* cooldown = PartnerCooldownInfoScreen::create(p.id, this);
        cooldown->load();
        page->addChild(cooldown);
        cooldown->setPosition(ccp(-25 * s, -90 * s));
        // (1.45 loadAssets 0x2c24c8) today's free partner: its "employee of
        // the day" badge, and "free" while neither bought nor resting
        if (p.freeToday) {
            if (CCSprite* badge = CCSprite::createWithSpriteFrameName("tagscreen_employee-normal.png")) {
                badge->setAnchorPoint(CCPointZero);
                badge->setPosition(ccp(-54 * s, -135 * s));
                page->addChild(badge);
            }
            const bool bought = SaveData::shared().currentSlot().owns(SavedGame::kItemPartner, partnerIndex(p.id));
            if (!bought && partnerCooldownLeft(p.id) == 0)
                if (CCSprite* badge = CCSprite::createWithSpriteFrameName("tagscreen_freetag-normal.png")) {
                    badge->setAnchorPoint(CCPointZero);
                    badge->setPosition(ccp(-40 * s, -62 * s));
                    page->addChild(badge);
                }
        }
        m_portraits.push_back(portrait);
        m_pages.push_back(page);
        m_cooldowns.push_back(cooldown);
    }
    int startPage = partners.empty() ? 1 : chosen + 1;
    refreshPortraits();
    refreshPriceTags();
    refreshUpgrades();

    // the row of pages, 135 canvas points apart, any of them pressed
    m_scroll = HorizontalScrollLayer::create(m_pages, static_cast<int>(245 * s), static_cast<int>(-110 * s), false);
    m_scroll->setMode(3);
    m_scroll->setAnchorPoint(ccp(0.5f, 0.5f));
    setPositionOnCanvas(m_scroll, 320, 205);
    m_scroll->setContentSize(CCSizeMake(425 * s, 285 * s));
    m_scroll->setPressed(this, callfuncN_selector(PartnerSelectScreen::pagePressed));
    m_scroll->setReleased(this, callfuncN_selector(PartnerSelectScreen::pageReleased));
    m_scroll->setSelected(this, callfuncN_selector(PartnerSelectScreen::pageSelected));
    m_scroll->setPageChanged(this, callfuncN_selector(PartnerSelectScreen::pageChanged));
    addChild(m_scroll);
    // to the chosen one, the second at least
    m_scroll->moveToPage(startPage < 2 ? 2 : startPage);

    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopMusic(true);
    sound->playAmbience("tech_tree_ambience");
    m_hud.refresh();
    schedule(schedule_selector(PartnerSelectScreen::tick), 1.0f);

    if (m_from == "preop") {
        // in from the right over the pre-op screen, then the shades
        hideForPreop();
        float w = 425 * s;
        setPosition(ccp(w + w, 0));
        CCActionInterval* in = CCEaseIn::create(CCMoveTo::create(0.4f, ccp(0, 0)), 0.35f);
        runAction(CCSequence::create(in, CCCallFunc::create(this, callfunc_selector(PartnerSelectScreen::fadeInShades)),
                                     nullptr));
    } else {
        hideBrowseOnly();
    }
}

void PartnerSelectScreen::fadeInShades() {
    if (m_shadeBottom) m_shadeBottom->runAction(CCFadeIn::create(0.25f));
    if (m_shadeTop) m_shadeTop->runAction(CCFadeIn::create(0.25f));
}

void PartnerSelectScreen::unload() {
    removeAllChildrenWithCleanup(true);
    CCSpriteFrameCache::sharedSpriteFrameCache()->removeUnusedSpriteFrames();
    CCDirector::sharedDirector()->purgeCachedData();
    // (the pages and what is on them went with the children)
    m_scroll = nullptr;
    m_portraits.clear();
    m_pages.clear();
    m_priceTags.clear();
    m_prices.clear();
    m_cooldowns.clear();
}

void PartnerSelectScreen::onShown() {
    Scene::onShown();
    m_leaving = false;
    tick(0);
    refreshPortraits();
    refreshPriceTags();
    if (m_scroll) m_scroll->setTouchOn(true);
    if (!isTouchRegistered()) setTouchRegistered(true);
    // (the original turned the top bar's coins button back on here, and showed its ads)
    ui.setVisible("ui_button_back", true);
    if (m_from != "preop") ui.setVisible("ui_button_updates", true);
}

void PartnerSelectScreen::onRemoved() {
    CC_SAFE_RELEASE_NULL(m_tick);
    if (m_scroll) m_scroll->setTouchOn(false);
    if (isTouchRegistered()) setTouchRegistered(false);
    unload();
    if (m_from == "preop") {
        CC_SAFE_RELEASE_NULL(m_shadeBottom);
        CC_SAFE_RELEASE_NULL(m_shadeTop);
    }
    unschedule(schedule_selector(PartnerSelectScreen::tick));
    // (and its ads)
}

void PartnerSelectScreen::tick(float) {
    m_hud.refresh();
    GameStateUtils::UpdateBloodPacksAndSave();  // (1.45 updateStatus 0x2c6c60)
    // Not in the original (0x201a74): the cooldowns shown count down, and a
    // partner rested shows ready at once. (Its times stood still until the
    // screen showed again; the port's cooldowns are short enough to watch.)
    if (SceneMgr::sharedSceneMgr()->topScene() != this) return;
    const std::vector<PartnerInfo>& partners = PartnerInfoUtils::shared()->partners;
    for (size_t i = 0; i < partners.size() && i < m_cooldowns.size(); ++i) {
        if (!m_cooldowns[i]->isVisible()) continue;
        int left = partnerCooldownLeft(partners[i].id);
        if (left < 1) {
            refreshPortraits();
            return;
        }
        m_cooldowns[i]->setTime(left, partners[i].endCooldownCost);
    }
}

void PartnerSelectScreen::refreshPortraits() {
    SavedGame& slot = SaveData::shared().currentSlot();
    const std::vector<PartnerInfo>& partners = PartnerInfoUtils::shared()->partners;
    bool preop = m_from == "preop";
    if (slot.selectedTagPartner.empty()) {
        // none chosen: skip, not start
        if (preop) {
            ui.setVisible("ui_button_start", false);
            ui.setVisible("ui_button_skip", true);
        }
        ui.setItemImage(m_def, "ui_tagscreen", "select");
    } else {
        if (preop) {
            ui.setVisible("ui_button_start", true);
            ui.setVisible("ui_button_skip", false);
        }
        ui.setItemImage(m_def, "ui_tagscreen", partnerName(slot.selectedTagPartner));
    }
    if (m_tick) m_tick->removeFromParent();
    for (size_t i = 0; i < partners.size() && i < m_pages.size(); ++i) {
        const PartnerInfo& p = partners[i];
        std::string name = partnerName(p.id);
        m_cooldowns[i]->setVisible(false);
        if (!owns(p.id) && !p.purchasable) {
            // the story gives it later
            setFrame(m_portraits[i], "tagscreen_portrait-" + name + "-locked.png");
        } else {
            int left = partnerCooldownLeft(p.id);
            if (static_cast<float>(left) <= 0.0f) {
                setFrame(m_portraits[i], "tagscreen_portrait-" + name + "-normal.png");
            } else {
                // resting: greyed, the time left under it
                setFrame(m_portraits[i], "tagscreen_portrait-" + name + "-grey.png");
                m_cooldowns[i]->setVisible(true);
                m_cooldowns[i]->setTime(left, p.endCooldownCost);
            }
        }
        if (slot.selectedTagPartner == p.id && m_tick) m_pages[i]->addChild(m_tick);
    }
}

void PartnerSelectScreen::refreshPriceTags() {
    for (CCSprite* t : m_priceTags) t->removeFromParent();
    m_priceTags.clear();
    for (CCNode* l : m_prices) l->removeFromParent();
    m_prices.clear();
    float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    const std::vector<PartnerInfo>& partners = PartnerInfoUtils::shared()->partners;
    const UITextDef* priceDef = m_def.text("ui_tag_partner_price");
    for (size_t i = 0; i < partners.size() && i < m_pages.size(); ++i) {
        const PartnerInfo& p = partners[i];
        if (owns(p.id) || !p.purchasable) continue;
        // on sale: its price on a tag
        if (CCSprite* tag = CCSprite::createWithSpriteFrameName("tagscreen_price_tag.png")) {
            tag->setAnchorPoint(CCPointZero);
            tag->setPosition(ccp(-30 * s, -50 * s));
            m_pages[i]->addChild(tag);
            m_priceTags.push_back(tag);
        }
        if (!priceDef) continue;
        if (CCLabelBMFont* price = createTextLabel(*priceDef)) {
            price->setAnchorPoint(ccp(1, 0));
            price->setAlignment(kCCTextAlignmentRight);
            price->setString(std::to_string(p.cost).c_str());
            price->setPosition(ccp(30 * s, -46 * s));
            m_pages[i]->addChild(price);
            m_prices.push_back(price);
        }
    }
}

void PartnerSelectScreen::refreshUpgrades() {
    const SavedGame& slot = SaveData::shared().currentSlot();
    int n = UpgradeInfoUtils::shared()->affordable(slot.totalStars(), slot.coins());
    if (n < 1 || m_from == "preop") {
        ui.setVisible("ui_notification", false);
        ui.setText("ui_popup_upgradenumber", "");
    } else {
        ui.setText("ui_popup_upgradenumber", std::to_string(n));
        ui.setVisible("ui_notification", true);
    }
}

void PartnerSelectScreen::hideBrowseOnly() {
    for (const char* n : {"ui_button_start", "ui_button_skip", "selecttag_background", "ui_tagscreen",
                          "tagscreen_selectedbox", "tagscreen_selecttitle"})
        ui.setVisible(n, false);
}

void PartnerSelectScreen::hideForPreop() {
    ui.setVisible("tagscreen_background", false);
    ui.setVisible("ui_button_updates", false);
}

void PartnerSelectScreen::beforeCovered() {
    ui.setVisible("ui_button_start", false);
    ui.setVisible("ui_button_skip", false);
    if (m_scroll) m_scroll->setTouchOn(false);
    // (and the top bar's coins button off: hidden on the PC)
    setTouchRegistered(false);
}

void PartnerSelectScreen::pagePressed(CCNode* page) {
    if (shownForLessThanMinimum()) return;
    // (the original looked the partner's cooldown and ownership up, and did
    // nothing with them)
}

void PartnerSelectScreen::pageReleased(CCNode* page) {
    const PartnerInfo* p = partnerOf(page);
    if (!p || page->getTag() >= static_cast<int>(m_portraits.size())) return;
    int left = partnerCooldownLeft(p->id);
    if (!owns(p->id) && !p->purchasable) return;  // locked: it stays so
    if (static_cast<float>(left) == 0.0f)
        setFrame(m_portraits[page->getTag()], "tagscreen_portrait-" + partnerName(p->id) + "-normal.png");
}

void PartnerSelectScreen::pageSelected(CCNode* page) {
    if (shownForLessThanMinimum() || m_leaving) return;
    const PartnerInfo* p = partnerOf(page);
    if (!p) return;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    float left = static_cast<float>(partnerCooldownLeft(p->id));
    bool owned = owns(p->id);
    // available: owned or on sale, and rested
    if ((owned || p->purchasable) && left == 0.0f) {
        if (m_from == "preop" && owned) {
            // taken at once
            sound->playSound("button_click", false);
            SaveData& save = SaveData::shared();
            save.currentSlot().selectedTagPartner = p->id;
            save.saveGame();
            refreshPortraits();
        } else {
            // its file: buy it, or take it
            m_leaving = true;
            if (m_scroll) m_scroll->setTouchOn(false);
            sound->playSound("button_click", false);
            beforeCovered();
            SceneMgr::sharedSceneMgr()->pushScene(PartnerInfoScreen::create(p->id, this), false);
        }
        return;
    }
    sound->playSound("button_click_denied", false);
    if (left > 0.0f) {
        // resting: the offer to end it
        beforeCovered();
        SceneMgr::sharedSceneMgr()->pushScene(CooldownInfoScreen::create(p->id, this), false);
    }
}

void PartnerSelectScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (m_hud.owns(sender)) {
        // (the top bar's button to the shop of coins sold for money: removed, GlobalHud.h)
        return;
    }
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    if (name == "ui_button_back") {
        if (m_from == "world") {
            scenes->replaceScene(WorldScreen::create());
        } else if (m_from == "preop") {
            // out to the right again, back to the pre-op screen
            if (m_shadeBottom) m_shadeBottom->runAction(CCFadeOut::create(0.15f));
            if (m_shadeTop) m_shadeTop->runAction(CCFadeOut::create(0.15f));
            float w = 425 * ResolutionMgr::sharedResolutionMgr()->canvasScale;
            CCActionInterval* out = CCEaseIn::create(CCMoveTo::create(0.4f, ccp(w + w, 0)), 0.35f);
            runAction(CCSequence::create(out, CCCallFunc::create(this, callfunc_selector(PartnerSelectScreen::slidOut)),
                                         nullptr));
            if (m_scroll) m_scroll->moveToPage(1);
        } else if (m_from == "area") {
            for (const AreaDef& area : AreasDefUtils::shared()->areas)
                if (area.areaId == slot.lastSelectedArea) scenes->replaceScene(AreaScreen::create(area, 0));
        }
    } else if (name == "ui_button_start" || name == "ui_button_skip") {
        // (1.45 clickedButton 0x2c3740) after the first two surgeries a blood
        // pack is taken (a win gives it back, SurgeryScreen::finishWin);
        // none left: the popup with the time to the next, the start refused
        if (GameStateUtils::BloodPacksInUse(slot)) {
            GameStateUtils::UpdateBloodPacksAndTime(slot);
            if (slot.numBloodPacks < 1) {
                log("blood packs: none left, %s refused", m_surgery.id.c_str());
                beforeCovered();
                scenes->pushScene(GeneralTipPopupScreen::create("bloodpacks_out"), false);
                return;
            }
            GameStateUtils::UseBloodPack(slot);
            log("blood packs: one taken for %s, %d left", m_surgery.id.c_str(), slot.numBloodPacks);
            save.saveGame();
        }
        scenes->replaceScene(SurgeryScreen::create(m_surgery, m_params));
    } else if (name == "ui_tagscreen") {
        // the partner dropped
        slot.selectedTagPartner = "";
        ui.setItemImage(m_def, "ui_tagscreen", "select");
        save.saveGame();
        refreshPortraits();
    } else if (name == "ui_button_updates") {
        scenes->replaceScene(UpgradeScreen::create(m_from));
    }
}

void PartnerSelectScreen::slidOut() {
    // (the original also rebuilt the selected surgery's info here, unused)
    SceneMgr::sharedSceneMgr()->popScene();
}

}  // namespace mt
