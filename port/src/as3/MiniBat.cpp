#include "as3/MiniBat.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

MiniBat::MiniBat() {
    // (0x99acc)
    zOrder = 100;
    priority = 3;
}

void MiniBat::newHeading() {
    // (0x184bdc)
    CCPoint to = pointOnPlayAreaSide(lastSide);
    float dx = to.x - canvas.x, dy = to.y - canvas.y, len = std::sqrt(dx * dx + dy * dy);
    dir = len > 0 ? ccp(dx / len, dy / len) : CCPointZero;
}

void MiniBat::vf3() {
    // (vf3) born small, growing to its size
    m_flag58 = true;
    canvas = toCanvas(position);
    newHeading();
    if (Animation* a = anim(*this)) {
        a->setScale(0.15f);
        a->runAction(CCScaleTo::create(growTime, 1.0f));
        a->play("minibat_flying", -1, false);
    }
}

void MiniBat::vf4(float dt) {
    // (vf4) straight on; off a side of the play area, back on it and heading
    // for another
    tracePosition(*this);
    if (!finished)
        SoundMgr::sharedSoundMgr()->playOnChannel("bat_mini_flying_loop", soundChannel, true, false, false, false);
    canvas.x += dir.x * static_cast<float>(speed) * dt;
    canvas.y += dir.y * static_cast<float>(speed) * dt;
    const CCRect& area = ResolutionMgr::sharedResolutionMgr()->rect;
    bool off = false;
    if (canvas.x < area.origin.x) {
        canvas.x = area.origin.x;
        off = true;
    }
    if (canvas.x > area.origin.x + area.size.width) {
        canvas.x = area.origin.x + area.size.width;
        off = true;
    }
    if (canvas.y > area.origin.y + area.size.height) {
        canvas.y = area.origin.y + area.size.height;
        off = true;
    }
    if (canvas.y < area.origin.y) {
        canvas.y = area.origin.y;
        off = true;
    }
    if (off) newHeading();
    CCPoint p = fromCanvas(canvas);
    moveTo(*this, p);
    if (Animation* a = anim(*this)) a->setPosition(p);
}

void MiniBat::onFinished() {
    // (vf20) tazered
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopChannel(soundChannel, false);
    sound->playRandomSound("bat_mini_death", 2, false);
}

}  // namespace mt
