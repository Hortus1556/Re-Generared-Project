#include "as3/MediatonicSplashScreen.h"

#include "as3/ResolutionMgr.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

MediatonicSplashScreen* MediatonicSplashScreen::create() {
    MediatonicSplashScreen* s = new MediatonicSplashScreen;
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void MediatonicSplashScreen::load() {
    std::string path = ResolutionMgr::sharedResolutionMgr()->artPath("mediatonic_logo", "png");
    cocos2d::CCSprite* logo = cocos2d::CCSprite::create(path.c_str());
    setPositionOnCanvas(logo, 320, 192);  // the canvas's middle
    addChild(logo);
}

}  // namespace mt
