#include "as3/MiniFrog.h"

#include <algorithm>
#include <cmath>

#include "as3/Animation.h"
#include "as3/ClampsController.h"
#include "as3/Creature.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

MiniFrog::MiniFrog() {
    // (0x9889c)
    zOrder = 100;
    priority = 2;
    sendsGraphicsEvent = false;
    timer = hatchTime();
}

float MiniFrog::hatchTime() { return hatchBase + randFloat(-hatchVariance * 0.5f, hatchVariance * 0.5f); }

void MiniFrog::start() {
    // (vf2: not the base's)
    hatched = false;
    red = false;
    if (hasStage()) state = stage().state;
}

void MiniFrog::vf3() {
    // (vf3) its piece for the tongs; the egg wiggles; its first hop's place
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0x3f);
    if (Animation* a = anim(*this)) a->play("spawn_wiggle", -1, false);
    target = hopTarget(position, hop, 0);
    heading();
}

void MiniFrog::heading() {
    // (0x159598) its way to its target, and its turn that way
    angle = headingTo(position, target, dir);
}

void MiniFrog::flash(float dt) {
    // (0x158e7c) green and white (red and white, angry) for a while
    Animation* a = anim(*this);
    if (flashLeft <= 0 || !a) return;
    flashLeft -= dt;
    if (flashLeft <= 0) {
        flashOn = false;
        a->setColor(ccc3(255, 255, 255));
        return;
    }
    flashTick -= dt;
    if (flashTick > 0) return;
    const bool was = flashOn;
    flashOn = !flashOn;
    if (was) a->setColor(ccc3(0x7d, 0xff, 0));
    else a->setColor(red ? ccc3(255, 0, 0) : ccc3(255, 255, 255));
    flashTick = flashEvery;
}

void MiniFrog::hatch() {
    // (0x158f58)
    hatched = true;
    timer = afterHatch;
    if (Animation* a = anim(*this)) {
        a->play("spawn_hatch", 1, false);
        a->queue("idle", 1, false);
    }
    SoundMgr::sharedSoundMgr()->playSound("poison_burst", false);
    goToState(kMiniFrogNormal, true, true);
}

void MiniFrog::finishNow() {
    postCreateAtEnd();
    m_stage = static_cast<int>(def->stages.size()) + 1;
    finished = true;
    onFinished();
}

void MiniFrog::vf4(float dt) {
    // (vf4)
    tracePosition(*this);
    flash(dt);
    const float before = timer;
    timer -= dt;
    Animation* a = anim(*this);
    const int st = hasStage() ? stage().state : -1;
    switch (st) {
    case kMiniFrogEggClosed:
        if (clamps.empty()) {
            if (timer <= 0) hatch();
        } else {
            // a clamp on it: it has a while to be tightened
            timer = clampTime;
            clampDone = false;
            goToState(kMiniFrogEggClamped, true, true);
            if (a) a->showFrame("spawn_clamped", 1);
            lastClamp = 0;
        }
        return;
    case kMiniFrogEggClamped: {
        if (clamps.empty()) {
            timer = hatchTime();
            goToState(kMiniFrogEggClosed, true, true);
            return;
        }
        const Clamp* c = clamps[0];
        if (c->tightness == c->maxTightness) {
            if (!clampDone) {
                clampDone = true;
                // (vf25: event 11, its popup)
                InjuryEvent e;
                e.type = InjuryEvent::kTagPopup;
                e.kind = 0;
                e.useTouch = true;
                post(e);
            }
        } else if (timer <= 0) {
            // not tightened in time: it hatches out of the clamp
            releaseClamps();
            clampDone = false;
            goToState(kMiniFrogNormal, true, true);
        }
        const float ratio = c->tightness / c->maxTightness;
        if (ratio != lastClamp && a) {
            lastClamp = ratio;
            a->showFrame("spawn_clamped",
                         std::max(0, static_cast<int>(ratio * static_cast<float>(a->lengthOf("spawn_clamped") - 1))));
        }
        return;
    }
    case kMiniFrogNormal:
    case kMiniFrogAngry:
        break;
    default:
        return;
    }
    // hopping: the hop starts as its timer runs out, then it moves to its target
    const bool angry = st == kMiniFrogAngry;
    if (angry) flashLeft = flashTime;
    if (timer > 0) return;
    if (before > 0) {
        setRotation(angle);
        SoundMgr::sharedSoundMgr()->playSound("mini_frog_hop", false);
        if (a) {
            a->play(angry ? "jump" : "walk", 1, false);
            a->queue("idle", -1, false);
        }
    }
    const float v = (angry ? angrySpeed : speed) * ResolutionMgr::sharedResolutionMgr()->canvasScale;
    const float nx = position.x + dir.x * v * dt, ny = position.y + dir.y * v * dt;
    const bool overX = (dir.x > 0 && target.x < nx) || (dir.x < 0 && nx < target.x);
    const bool overY = (dir.y > 0 && target.y < ny) || (dir.y < 0 && ny < target.y);
    if (!overX && !overY) {
        moveTo(*this, ccp(nx, ny));
        return;
    }
    // landed
    ++hops;
    if (angry) {
        if (hops < angryHops) {
            timer = angryHopEvery;
        } else {
            if (a) a->setColor(ccc3(255, 255, 255));
            flashLeft = 0;
            red = false;
            goToState(kMiniFrogNormal, true, true);
            timer = hopEvery;
        }
    } else {
        timer = hopEvery;
    }
    moveTo(*this, target);
    target = hopTarget(position, hasStage() && stage().state == kMiniFrogAngry ? angryHop : hop, 0);
    heading();
}

HitResult MiniFrog::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8)
    HitResult r;
    if (stage().useSizeRect && touchInRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        // (any other tool hatches the egg)
        if (!hatched) hatch();
        return r;
    }
    if (!r.touched) return r;
    r.hit = true;
    r.injury = this;
    Animation* a = anim(*this);
    switch (stage().state) {
    case kMiniFrogEggClosed:
        if (tool->type != kClamp) hatch();
        break;
    case kMiniFrogEggClamped:
        if (clampDone) {
            health -= tool->power;
            if (health <= 0) {
                r.stageDone = true;
                releaseClamps();
                finishNow();
            }
        }
        break;
    case kMiniFrogNormal:
        flashLeft = flashTime;
        hops = 0;
        goToState(kMiniFrogAngry, true, true);
        if (a) a->play("tazer", 3, false);
        target = hopTarget(position, angryHop, 0);
        heading();
        giveCombo(r, *this);
        break;
    case kMiniFrogAngry:
        red = true;
        flashLeft = flashTime;
        health -= tool->power;
        if (health <= 0) {
            flashLeft = 0;
            if (a) {
                a->play("chainsaw", 3, false);
                a->queue("dead", 1, false);
                a->setColor(ccc3(255, 255, 255));
            }
            SoundMgr::sharedSoundMgr()->playSound("mini_frog_death", false);
            r.stageDone = true;
            giveCombo(r, *this);
            finishStage();
        }
        break;
    case kMiniFrogDead:
        r.piece = grabPiece(*this);
        break;
    default:
        break;
    }
    return r;
}

void MiniFrog::completeStage() {
    // (vf18)
    const int st = stage().state;
    if (st == kMiniFrogDead || st == kMiniFrogEggClosed) {
        finishNow();
        return;
    }
    if (Animation* a = anim(*this)) a->play("dead", 1, false);
    goToState(kMiniFrogDead, true, true);
}

int MiniFrog::toolFor(const CCPoint& touch) {
    // (vf27)
    if (!touchInRect(*this, touch)) return kNoTool;
    const int st = stage().state;
    if (st == kMiniFrogDead) return kTongs;
    if (st == kMiniFrogEggClamped && !clamps.empty() && clamps[0]->tightness == clamps[0]->maxTightness)
        return kChainsaw;
    if (!hatched) return kClamp;
    if (st == kMiniFrogAngry) return kLighter;
    if (st == kMiniFrogNormal) return kTazer;
    return kNoTool;
}

}  // namespace mt
