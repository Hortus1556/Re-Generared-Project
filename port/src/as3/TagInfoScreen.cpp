#include "as3/TagInfoScreen.h"

#include "as3/Log.h"
#include "as3/PartnerInfoUtils.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCTexture2D.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

TagInfoScreen* TagInfoScreen::create(CCObject* surgery, SEL_CallFunc resume) {
    TagInfoScreen* s = new TagInfoScreen;
    s->setName("TagInfoScreen");
    if (s->init()) {
        // (0x29be68)
        s->m_surgery = surgery;
        s->m_resume = resume;
        s->setTouchRegistered(true);
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void TagInfoScreen::load() {
    SoundMgr::sharedSoundMgr()->pauseSounds();
    // the surgery dimmed
    m_dim = CCSprite::create();
    m_dim->addChild(CCLayerColor::create(ccc4(0, 0, 0, 127)));
    addChild(m_dim);
    m_root = CCNode::create();
    addChild(m_root);
    // the partner taken. (The original took the picture's name from the
    // surgery's partner, by its index, and the texts from the slot's
    // selectedTagPartner: the same partner.)
    std::string id = SaveData::shared().currentSlot().selectedTagPartner;
    const PartnerInfo* partner = PartnerInfoUtils::shared()->partner(id);
    m_def = loadUIDefinition("common/in_game_tutorial_tagpartnerpopups.json");
    buildUI(ui, m_root, m_def, this, menu_selector(TagInfoScreen::onButton));
    ui.setItemImage(m_def, "ingame_tutorial_partnerbox_pic", partnerName(id));
    ui.setText("tutorialpartnerbox01", partner ? partner->tutorialTitle : "");
    ui.setText("tutorialpartnerbox02", partner ? partner->tutorialDesc : "");
    // in from below
    m_root->setPosition(ccp(m_root->getPositionX(), -static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height)));
    m_root->runAction(CCEaseElasticOut::create(CCMoveTo::create(0.5f, ccp(-1, 0)), 1.0f));
    // (traced for test scripts: its OK's centre once risen, cocos points)
    if (CCNode* ok = ui.node("ingame_tutorial_ok")) {
        CCRect bb = ok->boundingBox();
        CCPoint at = ok->getParent() && ok->getParent() != m_root ? ok->getParent()->getPosition() : CCPointZero;
        log("tag info ok %.0f,%.0f", at.x + bb.getMidX() - 1, at.y + bb.getMidY());
    }
}

void TagInfoScreen::unload() {
    removeAllChildrenWithCleanup(true);
    // CCDirector::purgeCachedData as cocos2d-x 2.1.1 had it: the fonts and the
    // unused textures. (The engine's also drops the unused sprite frames,
    // which the surgery below still asks for.)
    CCLabelBMFont::purgeCachedData();
    CCTextureCache::sharedTextureCache()->removeUnusedTextures();
}

void TagInfoScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum() || m_closing) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (name != "ingame_tutorial_ok") return;
    // down, then off
    m_closing = true;
    float h = static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height);
    CCActionInterval* down = CCEaseOut::create(CCMoveTo::create(0.3f, ccp(0, h * -1.15f)), 0.25f);
    m_root->runAction(CCSequence::create(down, CCCallFuncN::create(this, callfuncN_selector(TagInfoScreen::closed)), nullptr));
    setTouchRegistered(false);
}

void TagInfoScreen::closed(CCNode*) {
    SoundMgr::sharedSoundMgr()->resumeSounds();
    if (m_surgery && m_resume) (m_surgery->*m_resume)();
    SceneMgr::sharedSceneMgr()->popScene();
}

}  // namespace mt
