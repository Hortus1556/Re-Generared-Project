#include "as3/CreditsScreen.h"

#include "as3/HorizontalScrollLayer.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/WelcomeScreen.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

// the portraits, in their order (1.45 0x67fec8, 19: 1.02's 0x447f7c had 18,
// without Mark)
const char* const kPortraits[19] = {
    "creditscreen_portrait_Enrique", "creditscreen_portrait_Manuel",    "creditscreen_portrait_Ross",
    "creditscreen_portrait_Pete",    "creditscreen_portrait_Mark",      "creditscreen_portrait_Amy",
    "creditscreen_portrait_Jonm",
    "creditscreen_portrait_Kash",    "creditscreen_portrait_Monica",    "creditscreen_portrait_Mushybees",
    "creditscreen_portrait_Elvira",  "creditscreen_portrait_Jim",       "creditscreen_portrait_Luke",
    "creditscreen_portrait_Joe",     "creditscreen_portrait_Jonp",      "creditscreen_portrait_Vicki",
    "creditscreen_portrait_Jamie",   "creditscreen_portrait_Mary",      "creditscreen_portrait_Paul",
};

}  // namespace

CreditsScreen* CreditsScreen::create() {
    CreditsScreen* s = new CreditsScreen;
    s->setName("CreditsScreen");
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void CreditsScreen::load() {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float cs = r->canvasScale;
    loadSheets(this, {m_sheet});
    CCNode* layer = CCNode::create();
    m_def = loadUIDefinition("common/ui_credits_01.json");
    buildUI(ui, layer, m_def, this, menu_selector(CreditsScreen::onButton));
    addChild(layer);
    // removed (online): the attributions' web page
    ui.setVisible("credits_legalbutton", false);
    // Fixed: 1.02's layout's back button asked for ui_button_back's art,
    // which no sheet loaded here has (the area maps' and the pre-op's do):
    // from the title it had no picture and could not be pressed, leaving the
    // player in the credits. 1.45's layout names the credits' own art
    // (ui_button_creditback): this only matters for 1.02's.
    if (CCMenuItemImage* back = ui.item("ui_button_back")) {
        CCSpriteFrameCache* frames = CCSpriteFrameCache::sharedSpriteFrameCache();
        if (CCSpriteFrame* f = frames->spriteFrameByName("ui_button_creditback-normal.png")) back->setNormalSpriteFrame(f);
        if (CCSpriteFrame* f = frames->spriteFrameByName("ui_button_creditback-selected.png"))
            back->setSelectedSpriteFrame(f);
    }
    for (int i = 0; i < 19; ++i) {
        CCNode* page = CCNode::create();
        page->setTag(i);
        std::string frame = std::string(kPortraits[i]) + ".png";
        CCSprite* portrait = CCSprite::createWithSpriteFrameName(frame.c_str());
        if (portrait) page->addChild(portrait);
        m_portraits.push_back(portrait);
        m_pages.push_back(page);
    }
    m_scroll = HorizontalScrollLayer::create(m_pages, static_cast<int>(cs * 245.0f), static_cast<int>(cs * -110.0f), true);
    m_scroll->setAnchorPoint(ccp(0.5f, 0.5f));
    setPositionOnCanvas(m_scroll, 320, 180);
    m_scroll->setContentSize(CCSizeMake(cs * 425.0f, cs * 285.0f));
    m_scroll->setPressed(this, callfuncN_selector(CreditsScreen::onPage));
    m_scroll->setReleased(this, callfuncN_selector(CreditsScreen::onPage));
    m_scroll->setSelected(this, callfuncN_selector(CreditsScreen::onPage));
    m_scroll->setPageChanged(this, callfuncN_selector(CreditsScreen::onPage));
    addChild(m_scroll);
    m_scroll->moveToPage(1);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopChannel(SoundMgr::kAmbienceChannel, false);
    sound->playMusic("super-hero-loop", true);
}

void CreditsScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (name == "ui_button_back") {
        SceneMgr::sharedSceneMgr()->replaceScene(WelcomeScreen::create());
    } else {
        // (credits_legalbutton opened http://as3.mediatonicgames.com/attributions.html: removed)
        log("%s", name.c_str());
    }
}

}  // namespace mt
