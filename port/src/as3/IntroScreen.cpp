#include "as3/IntroScreen.h"

#include "as3/AreasDefUtils.h"
#include "as3/Cheats.h"
#include "as3/DialogueScreen.h"
#include "as3/Log.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/VideoPlayer.h"

namespace mt {

IntroScreen* IntroScreen::create() {
    IntroScreen* s = new IntroScreen;
    s->setName("IntroScreen");
    s->autorelease();  // (0x2a32d4 does not init the scene)
    return s;
}

void IntroScreen::onShown() {
    Scene::onShown();
    m_sinceTap = m_skipWindow;
    setTouchRegistered(true);
    schedule(schedule_selector(IntroScreen::tick));
    // (debug: not with cheat 15, 1.45 0x3a86ac: tick() then goes on at once; Cheats.h)
    if (!cheat(Cheats::kSkipIntroMovie)) VideoPlayer::sharedVideoPlayer()->playVideo("Videos/intro", "m4v");
}

bool IntroScreen::ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) {
    if (shownForLessThanMinimum()) return false;
    // a second tap within the window skips
    if (m_sinceTap < m_skipWindow) finish();
    m_sinceTap = 0;
    return false;
}

void IntroScreen::tick(float dt) {
    if (VideoPlayer::sharedVideoPlayer()->isPlaying()) m_sinceTap += dt;
    else finish();
}

void IntroScreen::finish() {
    unschedule(schedule_selector(IntroScreen::tick));
    // the first area and its first surgery chosen and saved, then that
    // surgery's intro
    SavedGame& slot = SaveData::shared().currentSlot();
    // (the Bunker by its id, as Android 1.43 has it: its Hospital is listed
    // first; iOS took the first area)
    const AreaDef* bunker = AreasDefUtils::shared()->byId("area_bunker");
    if (!bunker || bunker->surgeries.empty()) {
        logError("IntroScreen: no first area");
        return;
    }
    const AreaDef& area = *bunker;
    slot.lastSelectedArea = area.areaId;
    slot.selectedSurgery = area.surgeries[0];
    SaveData::shared().saveGame();
    SurgeryInfo info = loadSurgeryInfo(area.surgeries[0], area);
    SurgeryParams params;  // the main surgery
    VideoPlayer::sharedVideoPlayer()->stop();
    SceneMgr::sharedSceneMgr()->replaceScene(DialogueScreen::create(info, params));
}

}  // namespace mt
