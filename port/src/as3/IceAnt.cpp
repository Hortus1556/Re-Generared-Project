#include "as3/IceAnt.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

IceAnt::IceAnt() {
    // (0x98ca4)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    step = 3 * s;
    boxWidth = 400 * s;
    boxHeight = 200 * s;
    zOrder = 100;
    priority = 3;
    sendsGraphicsEvent = false;
    state = kIceAntWalking;
}

void IceAnt::newTurn() {
    if (turnDir == 0) turnDir = randInt(0, 1) == 0 ? 1.0f : -1.0f;
    else turnDir = static_cast<float>(randInt(-1, 1));
}

void IceAnt::start() {
    // (vf2: not the base's) where it walks: the play area, and the box in
    // the middle of the screen it turns back to
    turnTimer = randFloat(0, turnEvery * 0.5f);
    turnDir = static_cast<float>(randInt(-1, 1));
    clock0 = unused2;
    clock1 = unused3;
    dead = false;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    bounds = CCRectMake(s * (r->rect.origin.x - mx), s * (r->rect.origin.y - my), s * r->rect.size.width,
                        s * r->rect.size.height);
    box = CCRectMake(static_cast<float>(r->width / 2) - boxWidth * 0.5f,
                     static_cast<float>(r->height / 2) - boxHeight * 0.5f, boxWidth, boxHeight);
    biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
    unusedTimer = static_cast<float>(randInt(0, static_cast<int>(unused0 * 0.5f)));
    // (its scurrying loop: in vf3 below. The original started it here, and
    // every injury's vf2 runs as the surgery loads, 0x75ee0: the ants of a
    // later stage were heard from the first one on)
}

void IceAnt::vf3() {
    // (vf3) its piece for the tongs; walking, turned any way
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0);
    if (Animation* a = anim(*this)) {
        a->setRotation(static_cast<float>(randInt(0, 359)));
        a->play("walk", -1, false);
    }
    // (the original's vf2 0x144178: as its stage shows, here)
    SoundMgr::sharedSoundMgr()->playOnChannel("ice_ants_scurrying_loop", soundChannel, true, false, false, false);
}

void IceAnt::turnToMiddle() {
    // (0x1444d0)
    Animation* a = anim(*this);
    if (!a) return;
    float tx = static_cast<float>(randInt(static_cast<int>(box.origin.x), static_cast<int>(box.origin.x + box.size.width)));
    float ty = static_cast<float>(randInt(static_cast<int>(box.origin.y), static_cast<int>(box.origin.y + box.size.height)));
    double deg = std::atan2(static_cast<double>(ty - a->getPositionY()), static_cast<double>(tx - a->getPositionX())) *
                 -57.29577951308232;
    a->setRotation(static_cast<float>(static_cast<int>(deg + 90.0)));
}

void IceAnt::bite() {
    // (0x1445dc) frost where it bites, and its damage
    Animation* a = anim(*this);
    post(createEvent("FrostBite", position.x, position.y, a ? a->getRotation() : 0));
    post(damageEvent(static_cast<int>(biteDamage)));
}

void IceAnt::vf4(float dt) {
    // (vf4) the injury where its animation walks
    tracePosition(*this);
    Animation* a = anim(*this);
    clock0 += dt;
    clock1 += dt;
    if (a) position = animPlace(*this, a);  // (the port's grabPiece: see animPlace)
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    if (dead || !a) return;
    if (state == kIceAntBite) {
        if (!a->isPlaying()) {
            biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
            newTurn();
            SoundMgr::sharedSoundMgr()->playOnChannel("ice_ants_scurrying_loop", soundChannel, true, false, false,
                                                      false);
            state = kIceAntWalking;
            a->play("walk", -1, false);
        }
        return;
    }
    if (state == kIceAntWalking) {
        if (turnTimer < turnEvery) {
            turnTimer += dt;
        } else {
            newTurn();
            turnTimer = randFloat(0, turnEvery * 0.5f);
        }
        const float rot = a->getRotation() + turnSpeed * turnDir;
        a->setRotation(rot);
        const double ang = static_cast<double>(90.0f - rot) * 0.017453292519943295;
        float x = static_cast<float>(a->getPositionX() + step * std::cos(ang));
        float y = static_cast<float>(a->getPositionY() + step * std::sin(ang));
        const bool outX = x <= bounds.origin.x || bounds.origin.x + bounds.size.width <= x;
        const bool outY = y <= bounds.origin.y || bounds.origin.y + bounds.size.height <= y;
        if (outX || outY) {
            // at a side: it stays and turns to the middle
            x = a->getPositionX();
            y = a->getPositionY();
            turnToMiddle();
            turnDir = 0;
            turnTimer = randFloat(0, turnEvery * 0.5f);
        }
        a->setPosition(ccp(x, y));
        biteTimer += dt;
        if (biteEvery <= biteTimer) {
            biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
            state = kIceAntBite;
            a->queue("bite", 1, false);
            bite();
        }
        return;
    }
    if (state == kIceAntTazer && !a->isPlaying()) {
        state = kIceAntDead;
        finishStage();
        a->play("dead", -1, false);
        dead = true;
    }
}

HitResult IceAnt::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) the tazer kills it; dead, the tongs take it
    HitResult r;
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    if (!r.touched) return r;
    r.hit = true;
    if (dead) {
        r.injury = this;
        r.piece = grabPiece(*this);
        return r;
    }
    if (state != kIceAntDead && state != kIceAntTazer && tool->type == kTazer) {
        Animation* a = anim(*this);
        if (a) {
            a->stopAllActions();
            a->play("chainsaw", 2, false);
        }
        SoundMgr::sharedSoundMgr()->playOnChannel("ice_ants_death", soundChannel, false, true, false, false);
        r.stageDone = true;
        giveCombo(r, *this);
        state = kIceAntTazer;
    }
    r.injury = this;
    return r;
}

void IceAnt::completeStage() {
    // (vf18)
    if (state == kIceAntDead) {
        postCreateAtEnd();
        m_stage = static_cast<int>(def->stages.size()) + 1;
        finished = true;
        onFinished();
        return;
    }
    Animation* a = anim(*this);
    if (a) a->stopAllActions();
    state = kIceAntDead;
    finishStage();
    if (a) a->play("dead", -1, false);
    dead = true;
}

int IceAnt::toolFor(const CCPoint& touch) {
    // (vf27)
    if (!stage().useSizeRect || !touchInRect(*this, touch)) return kNoTool;
    return stage().state == kIceAntDead ? kTongs : kTazer;
}

}  // namespace mt
