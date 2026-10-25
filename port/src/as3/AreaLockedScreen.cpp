#include "as3/AreaLockedScreen.h"

#include <cstdio>

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

namespace {

// the texts (0x448528, 0x44852c, 0x448530)
const char* const kTitle = "Unlocking %s";
const char* const kStarsOnly =
    "You need to collect %d stars in order to unlock %s. \n\nComplete bonus challenge surgeries for extra stars.";
const char* const kAreaAndStars = "You need to treat all patients in %s and collect %d stars to unlock %s";

}  // namespace

AreaLockedScreen* AreaLockedScreen::create(const std::string& area, const std::string& previousArea, int stars) {
    AreaLockedScreen* s = new AreaLockedScreen;
    s->setName("AreaLockedPopup1");
    // 0x2b19dc
    if (s->init()) {
        s->m_area = area;
        s->m_previousArea = previousArea;
        s->m_stars = stars;
        s->setTouchRegistered(true);
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

AreaLockedScreen* AreaLockedScreen::create(const std::string& area, int stars) {
    AreaLockedScreen* s = new AreaLockedScreen;
    s->setName("AreaLockedPopup2");
    // 0x2b1ce0
    if (s->init()) {
        s->m_area = area;
        s->m_previousArea = "";
        s->m_stars = stars;
        s->setTouchRegistered(true);
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void AreaLockedScreen::load() {
    // the screen darkened under the popup
    m_shade = CCSprite::create();
    m_shade->addChild(CCLayerColor::create(ccc4(0, 0, 0, 127)));
    addChild(m_shade);
    m_popup = CCNode::create();
    addChild(m_popup);
    m_def = loadUIDefinition("common/worldmap_locked_popup.json");
    buildUI(ui, m_popup, m_def, this, menu_selector(AreaLockedScreen::onButton));
    char text[512];
    std::snprintf(text, sizeof text, kTitle, m_area.c_str());
    ui.setText("worldmaplockedpopup01", text);
    if (m_previousArea.empty())
        std::snprintf(text, sizeof text, kStarsOnly, m_stars, m_area.c_str());
    else
        std::snprintf(text, sizeof text, kAreaAndStars, m_previousArea.c_str(), m_stars, m_area.c_str());
    ui.setText("worldmaplockedpopup02", text);
    // (1.45 loadAssets 0x3bcf10, its layout: OK beside a button unlocking
    // the area with money, which 1.45 hid when the store gave no price, OK
    // then moved 75 points right, to the middle: so always here, purchases
    // being removed)
    ui.setText("worldmaplockedpopup_button", "");
    ui.setVisible("ingame_tutorial_unlockbutton", false);
    if (CCNode* ok = ui.node("ingame_tutorial_okbutton"))
        ok->setPositionX(ok->getPositionX() + 75.0f * ResolutionMgr::sharedResolutionMgr()->canvasScale);
    SoundMgr::sharedSoundMgr()->playSound("screen_swoosh", false);  // (1.45)
    // it drops in from below the screen, with a bounce (to x -1, as the original)
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    m_popup->setPosition(ccp(m_popup->getPositionX(), -static_cast<float>(r->height)));
    m_popup->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
}

void AreaLockedScreen::unload() {
    removeAllChildrenWithCleanup(true);
    CCDirector::sharedDirector()->purgeCachedData();
}

void AreaLockedScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    // (the original made its "button_click" but never played it)
    if (name == "ingame_tutorial_ok" || name == "ingame_tutorial_okbutton") {  // (1.02's name, 1.45's)
        // it slides out downwards, then goes
        const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
        CCFiniteTimeAction* out =
            CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, static_cast<float>(r->height) * -1.15f)), 0.25f);
        m_popup->runAction(
            CCSequence::create(out, CCCallFuncN::create(this, callfuncN_selector(AreaLockedScreen::close)), nullptr));
        setTouchRegistered(false);
    }
}

void AreaLockedScreen::close(CCNode*) { SceneMgr::sharedSceneMgr()->popScene(); }

}  // namespace mt
