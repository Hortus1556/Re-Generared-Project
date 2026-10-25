#include "as3/Ghost.h"

#include <cmath>
#include <cstdlib>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCDirector.h"
#include "platform/Platform.h"

// (Android 1.43: the Hospital's ghost. Its fields are at +0x10000.. of the
// Android injury, 0x10000 bytes there; the base's +0xffd8 is the port's
// zOrder, +0xffef sendsGraphicsEvent, +0xfff8 priority, +0xfffc createdLater.)

namespace mt {

using namespace cocos2d;
using namespace creature;

Ghost::Ghost() {
    // (Android 0x55e86c) the lengths are canvas points, in cocos points
    // (worldToScreenMagnitude)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    step = 5 * s;
    boxWidth = 400 * s;
    boxHeight = 200 * s;
    zOrder = 250;
    sendsGraphicsEvent = false;
    priority = 3;
    state = kWalking;
}

void Ghost::start() {
    // (vf2, Android onCreated 0x4e15dc: not the base's) where it drifts: the
    // play area, and the box in the middle of the screen it heads for
    dead = false;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    bounds = CCRectMake(s * (r->rect.origin.x - mx), s * (r->rect.origin.y - my), s * r->rect.size.width,
                        s * r->rect.size.height);
    box = CCRectMake(static_cast<float>(r->width / 2) - boxWidth * 0.5f,
                     static_cast<float>(r->height / 2) - boxHeight * 0.5f, boxWidth, boxHeight);
    // (the original drew the hide timer from 0 to half itself: 0)
    hideTimer = static_cast<float>(randInt(0, static_cast<int>(hideTimer * 0.5f)));
    lookTimer = static_cast<float>(randInt(0, static_cast<int>(lookEvery * 0.5f)));
    heading = 0;
    lives = 3;
}

void Ghost::vf3() {
    // (vf3, Android onAddedToScene 0x4e17dc) drifting any way. (The original
    // also gave its animation as the first piece's sprite, for the tongs a
    // ghost never takes: left out.)
    m_flag58 = true;
    heading = static_cast<float>(randInt(0, 359));
    if (Animation* a = anim(*this)) a->play("walk", -1, false);
    traceState();
}

void Ghost::traceState() {
    // (the port's, AS_TRACE_CREATURES: its state and lives as they change,
    // for test scripts; its place is traced with the others')
    static const bool on = platform::debugMode() && std::getenv("AS_TRACE_CREATURES") != nullptr;
    if (!on || (state == tracedState && lives == tracedLives)) return;
    tracedState = state;
    tracedLives = lives;
    log("creature #%d Ghost state %d lives %d frame %u", soundChannel, state, lives,
        CCDirector::sharedDirector()->getTotalFrames());
}

void Ghost::lookAtMiddle() {
    // (Android 0x4e18b0) the way to a random place of the middle box
    Animation* a = anim(*this);
    if (!a) return;
    const float tx = static_cast<float>(
        randInt(static_cast<int>(box.origin.x), static_cast<int>(box.origin.x + box.size.width)));
    const float ty = static_cast<float>(
        randInt(static_cast<int>(box.origin.y), static_cast<int>(box.origin.y + box.size.height)));
    heading = static_cast<float>(std::atan2(static_cast<double>(ty - a->getPositionY()),
                                            static_cast<double>(tx - a->getPositionX())) *
                                 57.29577951308232);
}

void Ghost::setLivesAndHide(float n) {
    // (Android 0x4e1a80)
    strength = 0;
    lives = static_cast<int>(n);
    if (Animation* a = anim(*this)) a->setOpacity(0);
    state = kInvisible;
    hideTimer = 0;
    traceState();
}

void Ghost::die() {
    // (Android runLogic 0x4e2100.., autoCompleteStage 0x4e2428) a frostbite
    // where it was (event 2, Android's child class 0x5a), and it is healed
    // (Android InjuryState::heal 0x56227c: its end's injury, past its last
    // stage, finished, vf20)
    dead = true;
    Animation* a = anim(*this);
    const CCPoint at = a ? a->getPosition() : position;
    InjuryEvent e = createEvent("FrostBite", static_cast<float>(static_cast<int>(at.x)),
                                static_cast<float>(static_cast<int>(at.y)), 0);
    post(e);
    postCreateAtEnd();
    m_stage = static_cast<int>(def->stages.size()) + 1;
    finished = true;
    log("injury #%d finished", soundChannel);
    onFinished();
}

void Ghost::vf4(float dt) {
    // (vf4, Android runLogic 0x4e1b20) the injury where its animation is
    tracePosition(*this);
    Animation* a = anim(*this);
    if (a) position = animPlace(*this, a);
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    if (dead || !a) return;
    if (state == kWalking || state == kInvisible) {
        // straight on; at a side of the play area, or every 1.5 s, it stays
        // a frame and turns to the middle
        const double h = static_cast<double>(heading) * 0.017453292519943295;
        float x = static_cast<float>(a->getPositionX() + step * std::cos(h));
        float y = static_cast<float>(a->getPositionY() + step * std::sin(h));
        const bool inside = bounds.origin.x <= x && x <= bounds.origin.x + bounds.size.width &&
                            bounds.origin.y <= y && y <= bounds.origin.y + bounds.size.height;
        if (!inside || lookEvery <= lookTimer) {
            lookTimer = static_cast<float>(randInt(0, static_cast<int>(lookEvery * 0.5f)));
            x = a->getPositionX();
            y = a->getPositionY();
            lookAtMiddle();
        }
        hideTimer += dt;
        a->setPosition(ccp(x, y));
        lookTimer += dt;
        if (state != kInvisible || hideTimer < hideTime) return;
        // back: whole strength, smaller with each life lost, seen again
        strength = maxStrength;
        a->setScale(static_cast<float>(lives) * 0.2f + 0.4f);
        state = kWalking;
        hideTimer = 0;
        a->setOpacity(255);
        traceState();
        return;
    }
    if (state == kDying) {
        // swelling to 5 times its size as it fades out
        dyingTimer += dt;
        const float k = dyingTimer / dyingTime;
        a->setScale(k * 5.0f);
        a->setOpacity((1.0f - k) * 255.0f);
        if (dyingTimer < dyingTime) return;
        die();
        return;
    }
    if (state == kStruggle && !a->isPlaying()) {
        // its struggle played out: free again
        state = kWalking;
        a->play("walk", -1, false);
        traceState();
    }
}

HitResult Ghost::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8, Android runInteraction 0x4e25b8) the vacuum catches it walking;
    // held on it as it struggles, it drains its strength
    HitResult r;
    if (stage().useSizeRect && rect.containsPoint(touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    if (r.touched) {
        if (dead) {
            // (the original also gave its first piece: none here)
            r.hit = true;
            r.injury = this;
            return r;
        }
        r.hit = true;
    }
    if (r.hit) {
        Animation* a = anim(*this);
        if (state == kWalking && tool->type == kVacuum) {
            state = kStruggle;
            if (a) a->play("struggle", 1, false);
            strength -= tool->power;
            traceState();
        } else if (state == kStruggle && !(strength > 0)) {
            // its strength out: a life lost
            --lives;
            if (lives == 0) {
                state = kDying;
                if (a) a->stopAllActions();
                dyingTimer = 0;
            } else {
                // it splits: hidden, and another ghost with its lives
                if (a) a->play("walk", -1, false);
                giveCombo(r, *this);
                setLivesAndHide(static_cast<float>(lives));
                const CCPoint at = a ? a->getPosition() : position;
                InjuryEvent e = createEvent("Ghost", static_cast<float>(static_cast<int>(at.x)),
                                            static_cast<float>(static_cast<int>(at.y)), 0);
                e.value = static_cast<float>(lives);
                post(e);
            }
            traceState();
        } else if (state == kStruggle) {
            // fading with its strength (30 to 255)
            strength -= tool->power;
            if (a) a->setOpacity(strength / maxStrength * 225.0f + 30.0f);
        }
        r.injury = this;
    } else if (r.touched) {
        r.injury = this;
    }
    return r;
}

void Ghost::completeStage() {
    // (vf18, Android autoCompleteStage 0x4e2428) dying: dead at once;
    // otherwise it starts to die
    if (state == kDying) {
        die();
        return;
    }
    state = kDying;
    if (Animation* a = anim(*this)) a->stopAllActions();
    dyingTimer = 0;
    traceState();
}

int Ghost::toolFor(const CCPoint& touch) {
    // (vf27, Android getCurrentLegalTool 0x4e22d8) the vacuum, walking or
    // struggling. (Fixed: the original read the state of its stage's
    // definition, "GhostWalk", no state the game knows, so it never named
    // the vacuum: the ghost's own state here.)
    if (!stage().useSizeRect || !touchInRect(*this, touch)) return kNoTool;
    return state == kWalking || state == kStruggle ? kVacuum : kNoTool;
}

}  // namespace mt
