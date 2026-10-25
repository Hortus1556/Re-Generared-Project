#include "as3/CutInjuries.h"

#include <cmath>

#include "as3/Effect.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

Incision::Incision() {
    zOrder = 101;
    sendsGraphicsEvent = false;
}

void Incision::start() {
    if (!hasStage()) return;
    state = stage().state;
    if (stage().createAtStart.empty()) return;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    InjuryEvent e;
    e.type = InjuryEvent::kCreateInjury;
    e.name = stage().createAtStart;
    e.x = std::floor(position.x + stage().createAtStartOffset.x * s);
    e.y = std::floor(position.y + stage().createAtStartOffset.y * s);
    e.rotation = std::floor(rotation);
    e.allowedOverLock = true;
    e.countsTowardCap = false;
    // (0x86be0: +0x70 = this) its wound waits for the incision to be cut
    e.interactionDependency = this;
    post(e);
}

void Incision::onFinished() { SoundMgr::sharedSoundMgr()->playRandomSound("incision", 3, false); }

void placeStaple(InjuryState& cut, const CCPoint& touch, ToolState* tool) {
    if (!cut.hasStage() || cut.stage().state != kOpen) return;
    CCSprite* staple = CCSprite::create();
    if (CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("tool_staple.png"))
        staple->setDisplayFrame(f);
    // the two points of the cut's line at the press's distance from its
    // middle (its picture's turn, clockwise from up), the nearer one
    const float d = ccpDistance(cut.position, touch);
    const float turn = cut.sprites.empty() ? cut.rotation : cut.sprites[0]->getRotation();
    const double a1 = turn * 0.017453292519943295, a2 = (turn + 180.0f) * 0.017453292519943295;
    CCPoint p1 = ccp(static_cast<float>(cut.position.x + d * std::sin(a1)),
                     static_cast<float>(cut.position.y + d * std::cos(a1)));
    CCPoint p2 = ccp(static_cast<float>(cut.position.x + d * std::sin(a2)),
                     static_cast<float>(cut.position.y + d * std::cos(a2)));
    staple->setPosition(ccpDistance(p1, touch) < ccpDistance(p2, touch) ? p1 : p2);
    staple->setRotation(static_cast<float>(randomInt(-10, 10)) + cut.rotation);
    cut.topNode->addChild(staple);
    if (tool) tool->countUse();
}

}  // namespace mt
