#include "as3/Spider.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

Spider::Spider() {
    // (0x560564; Layout::worldToScreenMagnitude is the canvas scale)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    step = 8 * s;
    fleeStep = 12 * s;
    boxWidth = 200 * s;
    boxHeight = 100 * s;
    farFromMiddle = 150 * s;
    fireNear = 120 * s;
    zOrder = 100;                 // +0xffd8
    sendsGraphicsEvent = false;   // +0xffef
    priority = 3;                 // +0xfff8
    state = kSpiderWalking;
}

void Spider::start() {
    // (onCreated 0x71057c, as the ice ant's) where it walks: the play area,
    // and the box in the middle of the screen it turns back to
    turnTimer = randFloat(0, turnEvery * 0.5f);
    turnDir = static_cast<float>(randInt(-1, 1));
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
}

void Spider::vf3() {
    // (onAddedToScene 0x7107bc) its piece for the tongs (its animation);
    // walking, turned any way
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0);
    if (Animation* a = anim(*this)) {
        a->setRotation(static_cast<float>(randInt(0, 359)));
        a->play("walk", -1, false);
    }
}

void Spider::lookAtMiddle() {
    // (0x7108b0, the ice ant's 0x1444d0)
    Animation* a = anim(*this);
    if (!a) return;
    float tx = static_cast<float>(randInt(static_cast<int>(box.origin.x), static_cast<int>(box.origin.x + box.size.width)));
    float ty = static_cast<float>(randInt(static_cast<int>(box.origin.y), static_cast<int>(box.origin.y + box.size.height)));
    double deg = std::atan2(static_cast<double>(ty - a->getPositionY()), static_cast<double>(tx - a->getPositionX())) *
                 57.29577951308232;
    a->setRotation(static_cast<float>(static_cast<int>(90.0 - deg)));
}

void Spider::fleeFromFire() {
    // (0x710ac8) away from the flame, give or take 20 degrees; but far from
    // the middle with the flame close, to a random place of the middle box
    // instead, running faster for a moment
    Animation* a = anim(*this);
    if (!a) return;
    float tx = static_cast<float>(randInt(static_cast<int>(box.origin.x), static_cast<int>(box.origin.x + box.size.width)));
    float ty = static_cast<float>(randInt(static_cast<int>(box.origin.y), static_cast<int>(box.origin.y + box.size.height)));
    const double mdx = tx - a->getPositionX(), mdy = ty - a->getPositionY();
    const double fdx = fire.x - a->getPositionX(), fdy = fire.y - a->getPositionY();
    const double toMiddle = std::sqrt(mdx * mdx + mdy * mdy), toFire = std::sqrt(fdx * fdx + fdy * fdy);
    float deg;
    if (farFromMiddle < toMiddle && fireNear > toFire) {
        deg = static_cast<float>(std::atan2(mdy, mdx) * 57.29577951308232);
        deg += static_cast<float>(randInt(-20, 20));
        fleeTimer = fleeTime;
    } else {
        deg = static_cast<float>(std::atan2(fdy, fdx) * 57.29577951308232) - 180.0f;
        deg += static_cast<float>(randInt(-20, 20));
    }
    a->setRotation(static_cast<float>(static_cast<int>(90.0f - deg)));
}

void Spider::reactToPlayer(const CCPoint& touch) {
    // (0x711df8) it runs from the flame
    fleeTimer = fleeTime;
    fire = touch;
    fleeFromFire();
    turnDir = 0;
    turnTimer = randFloat(0, turnEvery * 0.5f);
}

void Spider::bite() {
    // (0x710ed8) a dark poison where it bites, a cut turned its way, and its
    // damage (the ice ant's makes a frostbite)
    Animation* a = anim(*this);
    if (a) a->play("bite", 1, false);
    state = kSpiderBite;
    post(createEvent("DarkPoisonArea", position.x, position.y, 0));
    post(createEvent("SmallCut", position.x, position.y, a ? a->getRotation() : 0));
    post(damageEvent(static_cast<int>(biteDamage)));
}

void Spider::vf4(float dt) {
    // (runLogic 0x711128) its rect and hit points where it was, then the
    // injury where its animation walks
    tracePosition(*this);
    Animation* a = anim(*this);
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    if (a) position = animPlace(*this, a);  // (the port's grabPiece: see animPlace)
    if (dead || !a) return;
    if (state == kSpiderWalking) {
        if (turnTimer < turnEvery) {
            turnTimer += dt;
        } else {
            if (turnDir == 0) turnDir = randInt(0, 1) == 0 ? 1.0f : -1.0f;
            else turnDir = static_cast<float>(randInt(-1, 1));
            turnTimer = randFloat(0, turnEvery * 0.5f);
        }
        const float rot = a->getRotation() + turnSpeed * turnDir;
        a->setRotation(rot);
        const double ang = static_cast<double>(90.0f - rot) * 0.017453292519943295;
        // (faster while it runs from the flame)
        const float d = fleeTimer > 0 ? fleeStep : step;
        if (fleeTimer > 0) fleeTimer -= dt;
        float x = static_cast<float>(a->getPositionX() + d * std::cos(ang));
        float y = static_cast<float>(a->getPositionY() + d * std::sin(ang));
        const bool outX = x <= bounds.origin.x || bounds.origin.x + bounds.size.width <= x;
        const bool outY = y <= bounds.origin.y || bounds.origin.y + bounds.size.height <= y;
        if (outX || outY) {
            // at a side: it stays and turns away (still running from the
            // flame) or to the middle
            x = a->getPositionX();
            y = a->getPositionY();
            if (fleeTimer > 0) fleeFromFire();
            else lookAtMiddle();
            turnDir = 0;
            turnTimer = randFloat(0, turnEvery * 0.5f);
        }
        a->setPosition(ccp(x, y));
        biteTimer += dt;
        if (biteEvery <= biteTimer) {
            biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
            bite();
        }
        return;
    }
    if (state == kSpiderBite && !a->isPlaying()) {
        state = kSpiderWalking;
        a->play("walk", -1, false);
    }
}

HitResult Spider::hitTest(const CCPoint& touch, ToolState* tool) {
    // (runInteraction 0x7119e4) the lighter burns it, scaring it if it is
    // used elsewhere; dead, the tongs take it
    HitResult r;
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    // (Fixed: not dead. The original's 1.45 0x4a0988 turned a dead spider
    // too, away from the tongs held anywhere else, a new random turn every
    // frame: the dead spiders twitched about while others were picked up.)
    if (!r.touched && !dead) reactToPlayer(touch);
    if (r.touched) {
        r.hit = true;
        if (dead) {
            r.injury = this;
            r.piece = grabPiece(*this);
            return r;
        }
    }
    if (r.hit && state != kSpiderDead && tool->type == kLighter) {
        lives -= tool->power;
        if (lives <= 0) {
            if (Animation* a = anim(*this)) a->play("death", 1, false);
            SoundMgr::sharedSoundMgr()->playOnChannel("ice_ants_death", soundChannel, false, true, false, false);
            r.stageDone = true;
            giveCombo(r, *this);
            state = kSpiderDead;
            dead = true;
            finishStage();
        }
    }
    if (r.hit) r.injury = this;
    return r;
}

void Spider::completeStage() {
    // (autoCompleteStage 0x71192c) dead: healed (taken out); else it dies
    if (state == kSpiderDead) {
        // (InjuryState::heal 0x56227c)
        postCreateAtEnd();
        m_stage = static_cast<int>(def->stages.size()) + 1;
        finished = true;
        onFinished();
        return;
    }
    if (Animation* a = anim(*this)) a->play("death", 1, false);
    state = kSpiderDead;
    finishStage();
    dead = true;
}

int Spider::toolFor(const CCPoint& touch) {
    // (getCurrentLegalTool 0x711810)
    if (!stage().useSizeRect || !touchInRect(*this, touch)) return kNoTool;
    return stage().state == kSpiderDead ? kTongs : kLighter;
}

}  // namespace mt
