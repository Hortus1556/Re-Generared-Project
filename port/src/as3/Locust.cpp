#include "as3/Locust.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCAction.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
// Its sounds. 1.45 plays "locust_jump" (5 variants) and "locust_screams" (3),
// which no build has: the original's locust was silent. Its file lists the
// flea's sounds (flea_jump_1..5, flea_screams_1..3), so the port plays those
// (fixed).
const char* const kJumpSound = "flea_jump";
const char* const kScreamSound = "flea_screams";
}  // namespace

Locust::Locust() {
    // (1.45 0xb8838 case 0xc: the flea's tuning; zOrder +0x10230, its
    // priority +0x10250, no graphics event +0x10247)
    zOrder = 100;
    priority = 3;
    sendsGraphicsEvent = false;
}

void Locust::start() {
    // (vf2 0x4b7cdc) its piece for the tongs (1.45 writes the def's first
    // piece slot over, kind 0x1a; vf3 here), its jump in cocos points
    jumpDistance = static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * static_cast<float>(jumpLength));
    if (hasStage()) state = stage().state;
}

void Locust::vf3() {
    // (vf3 0x4b7d78) its animation is its piece's node; still, on its jump's
    // first frame, for a random while
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0x1a);
    if (Animation* a = anim(*this)) {
        a->play("jump", 1, false);
        a->stop();
    }
    startDelay = randFloat(0, startDelayMax);
    jumpsLeft = randInt(0, maxJumps);
}

void Locust::vf4(float dt) {
    // (vf4 0x4b7de8) jumping: the injury follows its animation, which the
    // jumps move; at each landing one jump fewer before a bite
    tracePosition(*this);
    if (!hasStage() || stage().state != kJumping) return;
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

CCPoint Locust::pickLanding() {
    // (getNewPositionToGo 0x4b8a00, the flea's 1.45 0x21b1c0 with the same
    // angles) a direction (degrees) that keeps it in the play area: on its
    // sides if there is room up and down, up if no room down, down if none
    // up; left or right as there is room, at random if both
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

void Locust::jump() {
    // (setupJumpAnimation 0x4b8324) up to the middle (95 canvas points over
    // its start), down to its landing, turning a little, facing its way
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
    SoundMgr::sharedSoundMgr()->playRandomSound(kJumpSound, 5, false);
}

HitResult Locust::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8 0x4b85fc) the tazer kills it; dead, the tongs take it. (1.45
    // keeps no stat for it, the flea's StatsMgr 0x14 left out)
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
    if (stage().state == kJumping) {
        if (a) {
            a->stopAllActions();
            a->play("taser", 3, false);
            a->queue("dead", 1, false);
        }
        SoundMgr::sharedSoundMgr()->playRandomSound(kScreamSound, 3, false);
        r.stageDone = true;
        giveCombo(r, *this);
        finishStage();
    } else {
        r.piece = grabPiece(*this);
    }
    if (r.hit) r.injury = this;
    return r;
}

void Locust::completeStage() {
    // (vf18 0x4b887c) alive: killed; dead: healed, done with outright
    // (InjuryState::heal, 1.02's 0x83624)
    if (stage().state == kJumping) {
        if (Animation* a = anim(*this)) {
            a->stopAllActions();
            a->play("dead", 1, false);
        }
        SoundMgr::sharedSoundMgr()->playRandomSound(kScreamSound, 3, false);
        finishStage();
        return;
    }
    postCreateAtEnd();
    m_stage = static_cast<int>(def->stages.size()) + 1;
    finished = true;
    onFinished();
}

int Locust::toolFor(const CCPoint& touch) {
    // (vf27 0x4b859c)
    if (!stage().useSizeRect || !touchInRect(*this, touch)) return kNoTool;
    return stage().state == kDead ? kTongs : kTazer;
}

}  // namespace mt
