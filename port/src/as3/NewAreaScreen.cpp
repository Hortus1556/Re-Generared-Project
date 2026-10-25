#include "as3/NewAreaScreen.h"

#include "as3/AreasDefUtils.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

NewAreaScreen* NewAreaScreen::create(const std::string& area, Scene* map) {
    NewAreaScreen* s = new NewAreaScreen;
    s->setName("NewAreaPopup");
    s->m_map = map;
    s->m_area = area;
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void NewAreaScreen::load() {
    loadSheets(this, {m_sheet});
    // the map darkened under the letter
    m_shade = CCSprite::create();
    m_shade->addChild(CCLayerColor::create(ccc4(0, 0, 0, 127)));
    addChild(m_shade);
    m_popup = CCNode::create();
    m_def = loadUIDefinition("common/ui_newarea_screen.json");
    buildUI(ui, m_popup, m_def, this, menu_selector(NewAreaScreen::onButton));
    addChild(m_popup);
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    m_popup->setPosition(ccp(m_popup->getPositionX(), -static_cast<float>(r->height)));
    // the area by its id, or the first (0xf418c)
    AreasDefUtils* areas = AreasDefUtils::shared();
    const AreaDef* area = areas->areas.empty() ? nullptr : &areas->areas[0];
    for (const AreaDef& a : areas->areas)
        if (a.areaId == m_area) {
            area = &a;
            break;
        }
    if (area) {
        ui.setText("ui_newarea_title", area->newTitle);
        ui.setText("ui_newarea_bodytext", area->newBody);
    }
    m_popup->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
}

void NewAreaScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    if (name != "ui_button_ok") {
        log("%s", name.c_str());
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    // it slides out downwards, then goes
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    CCFiniteTimeAction* out =
        CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, static_cast<float>(r->height) * -1.15f)), 0.25f);
    m_popup->runAction(
        CCSequence::create(out, CCCallFuncN::create(this, callfuncN_selector(NewAreaScreen::close)), nullptr));
}

void NewAreaScreen::close(CCNode*) { SceneMgr::sharedSceneMgr()->popScene(); }

}  // namespace mt
