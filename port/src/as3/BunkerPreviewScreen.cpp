#include "as3/BunkerPreviewScreen.h"

#include <cstdint>

#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryEndScreen.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

BunkerPreviewScreen* BunkerPreviewScreen::create(const SurgeryInfo& surgery, const SurgeryParams& params,
                                                 const SurgeryResult& result) {
    BunkerPreviewScreen* s = new BunkerPreviewScreen;
    s->setName("BunkerPreviewScreen");
    if (s->init()) {
        s->m_surgery = surgery;
        s->m_params = params;
        s->m_result = result;
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void BunkerPreviewScreen::load() {
    loadSheets(this, {"areacommon_bunker"});
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float cs = r->canvasScale;
    m_lastY = 0;
    m_shake = cs + cs;
    // the map as the Bunker's screen shows it at first (AreaScreen: y 118)
    m_map = CCSprite::createWithSpriteFrameName("bunker_map.png");
    if (!m_map) m_map = CCSprite::create();
    setPositionOnCanvas(m_map, 320, 118);
    addChild(m_map);
    schedule(schedule_selector(BunkerPreviewScreen::shake));
    // up to where the second map shows it (y 221), then a pause, then the results
    float marginX = (640.0f - r->width / cs) * 0.5f, marginY = (384.0f - r->height / cs) * 0.5f;
    CCPoint to = ccp(cs * (320.0f - marginX), cs * (221.0f - marginY));
    m_map->runAction(CCSequence::create(CCMoveTo::create(5.2f, to), CCDelayTime::create(1.3f),
                                        CCCallFuncN::create(this, callfuncN_selector(BunkerPreviewScreen::done)),
                                        nullptr));
    m_lastY = m_map->getPosition().y;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    // on a channel of its own (the original's key was this screen)
    sound->playOnChannel("Crumbling_wall", static_cast<int>(reinterpret_cast<intptr_t>(this)), false, false, false,
                         false);
    sound->stopMusic(true);
    const std::string& bgm = m_surgery.area.bgm;
    sound->playAmbience(bgm.substr(0, bgm.find('.')));
}

void BunkerPreviewScreen::shake(float) {
    // a step to the side, the other way each frame, while the map moves
    float y = m_map->getPosition().y;
    m_shake = -m_shake;
    if (y == m_lastY) return;
    m_lastY = y;
    m_map->setPositionX(m_map->getPositionX() + m_shake);
}

void BunkerPreviewScreen::done(CCNode*) {
    unschedule(schedule_selector(BunkerPreviewScreen::shake));
    SceneMgr::sharedSceneMgr()->replaceScene(SurgeryEndScreen::create(m_surgery, m_params, m_result));
}

void BunkerPreviewScreen::unload() { unloadSheets({"areacommon_bunker"}); }

}  // namespace mt
