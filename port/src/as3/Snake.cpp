#include "as3/Snake.h"

#include <algorithm>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/Log.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

Snake::Snake() {
    // (0x995cc)
    zOrder = 100;
    priority = 2;
    sendsGraphicsEvent = false;
    inLeft = randFloat(1.5f, 2.5f);
}

void Snake::vf3() {
    // (vf3) in its hole: the first frame of coming out; its rect a little
    // higher and taller (by 0.15 of its height)
    m_flag58 = true;
    stunned = false;
    const float d = rect.size.height * 0.15f;
    rect.origin.y += d;
    rect.size.height += d;
    if (Animation* a = anim(*this)) a->showFrame("out", 0);
    inLeft = inTime + randFloat(-inTimeVariance, inTimeVariance);
}

void Snake::goBackIn() {
    if (stunned) log("creature #%d unstunned", soundChannel);  // (for test scripts)
    stunned = false;
    if (Animation* a = anim(*this)) a->play("out", 1, true);
    SoundMgr::sharedSoundMgr()->playRandomSound("snake_hiss", 3, false);
    inLeft = inTime + randFloat(-inTimeVariance, inTimeVariance);
    goToState(kSnakeInHole, false, false);
}

void Snake::vf4(float dt) {
    // (vf4) out of its hole and back in, on its timers (the chainsaw's
    // stops the time out)
    tracePosition(*this);
    const int st = hasStage() ? stage().state : -1;
    // (the port's) one waiting for others (its visual dependencies: the
    // next snake when one is killed, one under stakes) shows its hole from
    // the start (shownWhileWaiting) and stays in it till they are finished.
    // Fixed: the original hid it whole meanwhile (0x77c4c), coming out and
    // going in unseen, so the snakes seemed to appear only once one was
    // killed.
    bool waiting = false;
    for (const InjuryState* d : visualDependencies) waiting = waiting || !d->finished;
    if (st == kSnakeInHole && waiting) {
        inHole = true;
        return;
    }
    if (st == kSnakeInHole) {
        inLeft -= dt;
        if (inLeft <= 0) {
            stunned = false;
            if (Animation* a = anim(*this)) a->play("out", 1, false);
            SoundMgr::sharedSoundMgr()->playRandomSound("snake_hiss", 3, false);
            outLeft = outTime;
            goToState(kSnakeOut, false, false);
        }
    } else if (st == kSnakeOut) {
        if (sawing) {
            sawLeft -= dt;
        } else {
            outLeft -= dt;
            if (outLeft <= 0) goBackIn();
        }
    }
    inHole = hasStage() && stage().state == kSnakeInHole;
}

HitResult Snake::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) out: the tazer stuns it; stunned, the chainsaw held on it cuts it
    HitResult r;
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    if (!r.touched) {
        // (the chainsaw taken off it starts again)
        sawing = false;
        return r;
    }
    r.hit = true;
    onTouch(touch);
    Animation* a = anim(*this);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    const bool out = stage().state == kSnakeOut;
    if (tool->type == kChainsaw) {
        if (stunned && out) {
            if (!sawing) {
                sawing = true;
                sawLeft = sawTime;
                sound->playRandomSound("angry_snake_hiss", 3, false);
                if (a) a->play("chainsaw", -1, false);
            } else if (sawLeft <= 0) {
                sawing = false;
                if (--lives < 1) {
                    sound->playRandomSound("dead_snake_hiss", 2, false);
                    r.stageDone = true;
                    giveCombo(r, *this);
                    finishStage();
                } else {
                    comboGiven = false;
                    r.stageDone = true;
                    giveCombo(r, *this);
                    goBackIn();
                }
            }
        }
    } else if (tool->type == kTazer && out) {
        if (!stunned) {
            if (!comboGiven) {
                r.comboAction = true;
                comboGiven = true;
            } else {
                r.comboBonus = true;
            }
        }
        if (!stunned) log("creature #%d stunned", soundChannel);  // (for test scripts: it shows stunned)
        stunned = true;
        outLeft = std::min(outLeft + stunTime, stunTime + outTime);
        if (a) {
            a->play("shocked", 2, false);
            a->queue("stunned", -1, false);
        }
    }
    r.injury = this;
    return r;
}

void Snake::completeStage() {
    // (vf18) a cut for it
    if (--lives < 1) {
        SoundMgr::sharedSoundMgr()->playRandomSound("dead_snake_hiss", 2, false);
        finishStage();
    } else {
        SoundMgr::sharedSoundMgr()->playRandomSound("snake_hiss", 3, false);
    }
}

void Snake::toolStopped(int tool) {
    // (vf23) the chainsaw lifted off it: it shakes it off
    sawing = false;
    if (tool == kChainsaw && hasStage() && stage().state == kSnakeOut)
        if (Animation* a = anim(*this)) a->play("idle", -1, false);
}

int Snake::toolFor(const CCPoint& touch) {
    // (vf27) the tazer, then stunned the chainsaw
    if (!stage().useSizeRect || !touchInPlainRect(*this, touch)) return kNoTool;
    return stunned ? kChainsaw : kTazer;
}

}  // namespace mt
