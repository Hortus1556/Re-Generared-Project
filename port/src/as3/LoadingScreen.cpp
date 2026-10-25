#include "as3/LoadingScreen.h"

#include <cstdlib>

#include "as3/Content.h"
#include "as3/SaveData.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

namespace {

const char* const kSheet = "loading_screen_sheet";

// a whole number from `low` to `high` (0x644b0)
int randomBetween(int low, int high) { return std::rand() % (high - low + 1) + low; }

}  // namespace

LoadingScreen* LoadingScreen::create() {
    LoadingScreen* s = new LoadingScreen;  // (0x23ad78)
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void LoadingScreen::load() {
    // the tip: the one after the last shown, or a random one the first time
    json::Value root;
    readJson("Data/loading_tips.json", root);
    const json::Value& tips = root["loading_tips"];
    const int count = static_cast<int>(tips.size());
    SavedGame& slot = SaveData::shared().currentSlot();
    std::string text;
    if (count > 0) {  // (with none the original divided by zero)
        int index;
        if (slot.loadingTip == -1) {
            index = randomBetween(0, count - 1);
        } else {
            index = slot.loadingTip + 1;
            if (index == count) index = 0;
        }
        slot.loadingTip = index;
        const json::Value& t = tips[static_cast<size_t>(index)]["text"];
        if (t.type == json::Value::kString) text = t.string;
    }

    loadSheets(this, {kSheet});
    CCSize win = CCDirector::sharedDirector()->getWinSize();
    // a gradient (23, 23, 23) to (80, 80, 80) along (0, 0): with no
    // direction, cocos draws it in its start colour only
    CCLayerGradient* back = CCLayerGradient::create(ccc4(23, 23, 23, 255), ccc4(80, 80, 80, 255), ccp(0, 0));
    back->setContentSize(win);
    addChild(back);

    CCSprite* picture = CCSprite::createWithSpriteFrameName("anim_loading_1.png");
    if (picture) {
        picture->setAnchorPoint(ccp(0.5f, 0.5f));
        picture->setPosition(ccp(win.width * 0.5f, win.height * 0.5f));
        addChild(picture);
    }

    // (0xa9e7c with a width in cocos points, as the original passed it)
    CCLabelBMFont* tip = createLabel(text.c_str(), "font-TutorialHeader01", win.width / 1.25f, kCCTextAlignmentLeft);
    if (tip) {
        addChild(tip);
        tip->setAnchorPoint(ccp(0.5f, 1));
        tip->setColor(ccc3(255, 255, 255));
        tip->setPosition(ccp(win.width * 0.5f, win.height / 4.5f));
    }
}

void LoadingScreen::unload() { unloadSheets({kSheet}); }

}  // namespace mt
