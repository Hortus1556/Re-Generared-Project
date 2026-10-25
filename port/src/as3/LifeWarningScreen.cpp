#include "as3/LifeWarningScreen.h"

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"
#include "engine/cocos/CCTexture2D.h"

namespace mt {

using namespace cocos2d;

LifeWarningScreen* LifeWarningScreen::create(CCObject* surgery, SEL_CallFunc resume) {
    LifeWarningScreen* s = new LifeWarningScreen;
    s->setName("LifeWarningScreen");
    if (s->init()) {
        // (0x2f1280)
        s->m_surgery = surgery;
        s->m_resume = resume;
        s->setTouchRegistered(true);
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void LifeWarningScreen::load() {
    // (0x2f12b4)
    SoundMgr::sharedSoundMgr()->pauseSounds();
    // the surgery dimmed
    m_dim = CCSprite::create();
    m_dim->addChild(CCLayerColor::create(ccc4(0, 0, 0, 127)));
    addChild(m_dim);
    m_root = CCNode::create();
    addChild(m_root);
    m_def = loadUIDefinition("common/ingame_popup_lowhealth.json");
    buildUI(ui, m_root, m_def, this, menu_selector(LifeWarningScreen::onButton));
    if (CCSprite* heart = dynamic_cast<CCSprite*>(ui.node("ingame_lowhealth_warning")))
        if (CCSpriteFrame* f =
                CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("ingame_lowhealth_warning.png"))
            heart->setDisplayFrame(f);
    // (the texts: the original's own strings, 0x44c4d8 and 0x44c4dc)
    ui.setText("lowhealth_title", "Low Heart Rate Warning!");
    ui.setText("lowhealth_body",
               "Uhoh, looks like your patient's heart rate is running dangerously low. \n\nUse the healing "
               "injection tool to raise their heart rate now!");
    // in from below
    m_root->setPosition(ccp(m_root->getPositionX(), -static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height)));
    m_root->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
    // (traced for test scripts: its OK's centre once risen, cocos points)
    if (CCNode* ok = ui.node("ingame_lowhealth_ok")) {
        CCRect bb = ok->boundingBox();
        CCPoint at = ok->getParent() && ok->getParent() != m_root ? ok->getParent()->getPosition() : CCPointZero;
        log("lowhealth ok %.0f,%.0f", at.x + bb.getMidX() - 1, at.y + bb.getMidY());
    }
}

void LifeWarningScreen::unload() {
    // (0x2f1bd8: CCDirector::purgeCachedData; as TagInfoScreen's, the
    // sprite frames the surgery still uses are kept)
    removeAllChildrenWithCleanup(true);
    CCLabelBMFont::purgeCachedData();
    CCTextureCache::sharedTextureCache()->removeUnusedTextures();
}

void LifeWarningScreen::onButton(CCObject* sender) {
    // (0x2f1978)
    if (shownForLessThanMinimum() || m_closing) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (name != "ingame_lowhealth_ok") return;
    // down, then off
    m_closing = true;
    float h = static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height);
    CCActionInterval* down = CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, h * -1.15f)), 0.25f);
    m_root->runAction(
        CCSequence::create(down, CCCallFuncN::create(this, callfuncN_selector(LifeWarningScreen::closed)), nullptr));
    setTouchRegistered(false);
}

void LifeWarningScreen::closed(CCNode*) {
    // (0x2f1bf4) the sounds on, the surgery on (0x33dec), off
    SoundMgr::sharedSoundMgr()->resumeSounds();
    if (m_surgery && m_resume) (m_surgery->*m_resume)();
    SceneMgr::sharedSceneMgr()->popScene();
}

}  // namespace mt
