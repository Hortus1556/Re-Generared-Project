#include "as3/Flea.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCAction.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

Flea::Flea() {
    // (the factory's case 8)
    zOrder = 100;
    priority = 3;
    sendsGraphicsEvent = false;
}

void Flea::start() {
    // (vf2: not the base's) its jump in cocos points
    jumpDistance = static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * static_cast<float>(jumpLength));
    if (hasStage()) state = stage().state;
}

void Flea::vf3() {
    // (vf3) its piece for the tongs; still, on its jump's first frame, for a
    // random while
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0x15);
    if (Animation* a = anim(*this)) {
        a->play("jump", 1, false);
        a->stop();
    }
    startDelay = randFloat(0, startDelayMax);
    jumpsLeft = randInt(0, maxJumps);
}

void Flea::vf4(float dt) {
    // (vf4) jumping: the injury follows its animation, which the jumps move;
    // at each landing one jump fewer before a bite
    tracePosition(*this);
    if (!hasStage() || stage().state != kFleaJumping) return;
    Animation* a = anim(*this);
    if (!a) return;
    moveTo(*this, a->getPosition());
    if (a->numberOfRunningActions() != 0 || startDelay > 0) {
        if (startDelay >= 0) {
            startDelay -= dt;
        } else if (jumpTimer > 0) {
            jumpTimer -= dt;
            if (jumpTimer <= 0) {
                if (jumpsLeft < 1) {
                    post(createEvent("SmallCut", landing.x, landing.y - 20.0f, a->getRotation() + 90.0f));
                    jumpsLeft = randInt(minJumps, maxJumps);
                } else {
                    --jumpsLeft;
                }
            }
        }
    } else {
        jump();
        jumpTimer = jumpEvery;
    }
}

CCPoint Flea::pickLanding() {
    // (0x18b760) a direction (degrees) that keeps it in the play area: on
    // its sides if there is room up and down, up if no room down, down if
    // none up; left or right as there is room, at random if both
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    const CCRect& area = r->rect;
    const float top = s * (area.origin.y + area.size.height - my), bottom = s * (area.origin.y - my);
    const float left = s * (area.origin.x - mx), right = s * (area.origin.x + area.size.width - mx);
    const CCPoint at = anim(*this) ? anim(*this)->getPosition() : position;
    const float d = static_cast<float>(jumpDistance);
    const bool canLeft = left <= at.x - d, canRight = at.x + d <= right;
    int lo, hi;
    if (at.y + d <= top) {
        if (bottom <= at.y - d) {  // sideways
            if (canLeft && canRight) {
                if (randInt(0, 1) == 0) lo = 310, hi = 410;
                else lo = 130, hi = 230;
            } else if (canLeft) {
                lo = 130, hi = 230;
            } else {
                lo = 310, hi = 410;
            }
        } else {  // up
            if (canLeft && canRight) {
                if (randInt(0, 1) == 0) lo = 130, hi = 180;
                else lo = 0, hi = 50;
            } else if (canLeft) {
                lo = 130, hi = 180;
            } else {
                lo = 0, hi = 50;
            }
        }
    } else {  // down
        if (canLeft && canRight) {
            if (randInt(0, 1) == 0) lo = 310, hi = 360;
            else lo = 180, hi = 230;
        } else if (canLeft) {
            lo = 180, hi = 230;
        } else {
            lo = 310, hi = 360;
        }
    }
    const float a = static_cast<float>(randInt(lo, hi)) * 3.14159265358979f / 180.0f;
    return ccp(at.x + d * std::cos(a), at.y + d * std::sin(a));
}

void Flea::jump() {
    // (0x18b07c) up to the middle (95 canvas points over its start), down to
    // its landing, turning a little, facing its way
    landing = pickLanding();
    Animation* a = anim(*this);
    if (!a) return;
    a->setFlipX(position.x <= landing.x);
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCPoint mid = ccp(position.x + (landing.x - position.x) * 0.5f, position.y + 95.0f * s);
    CCFiniteTimeAction* up = CCSpawn::create(CCEaseOut::create(CCMoveTo::create(0.4f, mid), 2.5f),
                                             CCRotateTo::create(0.4f, static_cast<float>(randInt(-turnRange, turnRange))),
                                             nullptr);
    CCFiniteTimeAction* down = CCEaseIn::create(CCMoveTo::create(0.3f, landing), 1.5f);
    a->runAction(CCSequence::create(CCDelayTime::create(0.1f), up, down, CCDelayTime::create(0.5f), nullptr));
    a->play("jump", 1, false);
    SoundMgr::sharedSoundMgr()->playRandomSound("flea_jump", 5, false);
}

HitResult Flea::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) the tazer kills it; dead, the tongs take it
    HitResult r;
    Animation* a = anim(*this);
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    if (r.touched) r.hit = true;
    if (!r.hit) return r;
    onTouch(touch);
    if (stage().state == kFleaJumping) {
        if (a) {
            a->stopAllActions();
            a->play("taser", 3, false);
            a->queue("dead", 1, false);
        }
        SoundMgr::sharedSoundMgr()->playRandomSound("flea_screams", 3, false);
        r.stageDone = true;
        giveCombo(r, *this);
        finishStage();
        // (1.45 Flea::vf8 0x21ada4: a flea zapped, stat 20, StatsMgr.h)
        StatsMgr::sharedInstance()->incrementStat(StatTypes::kZapFlea);
    } else {
        r.piece = grabPiece(*this);
    }
    if (r.hit) r.injury = this;
    return r;
}

void Flea::completeStage() {
    // (vf18) alive: killed; dead: done with outright (0x83624)
    if (stage().state == kFleaJumping) {
        if (Animation* a = anim(*this)) {
            a->stopAllActions();
            a->play("dead", 1, false);
        }
        SoundMgr::sharedSoundMgr()->playRandomSound("flea_screams", 3, false);
        finishStage();
        return;
    }
    postCreateAtEnd();
    m_stage = static_cast<int>(def->stages.size()) + 1;
    finished = true;
    onFinished();
}

int Flea::toolFor(const CCPoint& touch) {
    // (vf27)
    if (!stage().useSizeRect || !touchInRect(*this, touch)) return kNoTool;
    return stage().state == kFleaDead ? kTongs : kTazer;
}

}  // namespace mt
