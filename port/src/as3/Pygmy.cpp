#include "as3/Pygmy.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {

void playSound(Pygmy& p, const char* name) {
    // (SoundUtils::queueSfx(name, this, false, true, false, false))
    SoundMgr::sharedSoundMgr()->playOnChannel(name, p.soundChannel, false, true, false, false);
}

// its own picture's half made beside it: 1.45's child 0x2a
bool isHalfClass(const InjuryDef* def) {
    static const int half = injuryClassIndex("PygmyHalf");
    return def && def->classIndex == half;
}

}  // namespace

Pygmy::Pygmy() {
    // (1.45 0xc37a8; Layout::worldToScreenMagnitude is the canvas scale)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    boxWidth = 400 * s;
    boxHeight = 200 * s;
    zOrder = 100;                // +0x10230
    sendsGraphicsEvent = false;  // +0x10247
    m_fd94 = false;              // +0x10244
    m_fd95 = true;               // +0x10245
}

void Pygmy::setState(int s) {
    state = s;
    // (for test scripts: the state it is in, which its stage's never shows)
    log("creature #%d Pygmy state %d", soundChannel, s);
}

void Pygmy::vf3() {
    // (1.45 onAddedToScene 0x4cae3c) where it walks: the play area, and the
    // box in the middle of the screen it heads back to (as the spider's);
    // its piece for the tongs is its animation (+0x6bbc); a PygmyHalf is a
    // right half from the start; it stands turned a little either way
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    bounds = CCRectMake(s * (r->rect.origin.x - mx), s * (r->rect.origin.y - my), s * r->rect.size.width,
                        s * r->rect.size.height);
    box = CCRectMake(static_cast<float>(r->width / 2) - boxWidth * 0.5f,
                     static_cast<float>(r->height / 2) - boxHeight * 0.5f, boxWidth, boxHeight);
    turnTimer = static_cast<float>(randInt(0, static_cast<int>(turnEvery * 0.5f)));
    heading = static_cast<float>(randInt(0, 359));
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0);
    Animation* a = anim(*this);
    if (isHalfClass(def)) {
        setState(kRightHalf);
        if (a) a->play("right", -1, false);
        cut = true;
        rightHalf = true;
    } else {
        setState(kAlive);
        if (a) a->play("idle", -1, false);
    }
    if (a) a->setRotation(randFloat(-tilt, tilt));
}

CCNode* Pygmy::mover(Animation* a) {
    if (!pieces.empty() && pieces[0].sprite && a && a->getParent() == pieces[0].sprite) return pieces[0].sprite;
    return a;
}

bool Pygmy::carried() const {
    return !pieces.empty() && pieces[0].sprite && pieces[0].sprite->getParent() != spritesNode;
}

void Pygmy::lookAtMiddle() {
    // (1.45 0x4cb8c8) its heading towards a random place of the middle box
    Animation* a = anim(*this);
    CCNode* m = mover(a);
    if (!m) return;
    const float tx = static_cast<float>(
        randInt(static_cast<int>(box.origin.x), static_cast<int>(box.origin.x + box.size.width)));
    const float ty = static_cast<float>(
        randInt(static_cast<int>(box.origin.y), static_cast<int>(box.origin.y + box.size.height)));
    heading = static_cast<float>(std::atan2(static_cast<double>(ty - m->getPositionY()),
                                            static_cast<double>(tx - m->getPositionX())) *
                                 57.29577951308232);
}

void Pygmy::split(Animation* a) {
    // (1.45 runLogic, autoCompleteStage) it steps 30 canvas points left and
    // its right half is made 30 to the right of where it stood, turned as it
    CCNode* m = mover(a);
    if (!m) return;
    const float d = ResolutionMgr::sharedResolutionMgr()->canvasScale * 30.0f;
    const float x = m->getPositionX();
    m->setPositionX(x - d);
    post(createEvent("PygmyHalf", static_cast<float>(static_cast<int>(x + d)),
                     static_cast<float>(static_cast<int>(m->getPositionY())),
                     static_cast<float>(static_cast<int>(a->getRotation() + (m != a ? m->getRotation() : 0)))));
    splitPending = false;
}

void Pygmy::vf4(float dt) {
    // (1.45 runLogic 0x4cb088) its sway, up and down by turns; the injury
    // where its animation is; walking while an animation plays (a loop: it
    // walks on; a reaction: it stands); a reaction played out, it walks on
    // as what it now is
    if (swayingUp) {
        swayClock += dt;
        if (swayClock >= swayTime) swayingUp = false;
    } else {
        swayClock -= dt;
        if (swayClock <= -swayTime) swayingUp = true;
    }
    touchedNow = false;
    tracePosition(*this);
    Animation* a = anim(*this);
    if (!a) return;
    CCNode* m = mover(a);
    // (InjuryState::setScreenPosition: its rect and its hit points)
    position = animPlace(*this, a);
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    const float bob = sway * (swayClock / swayTime);
    if (a->isPlaying() || finished) {
        // (+0x10248, which no one sets for it: the port's carried instead)
        if (carried() || state < kAlive || state > kBurnt) return;
        const double h = static_cast<double>(heading) * 0.017453292519943295;
        const float d = step * s;
        float x = static_cast<float>(m->getPositionX() + std::cos(h) * d);
        float y = static_cast<float>(m->getPositionY() + std::sin(h) * d) + s * bob;
        const bool outX = x <= bounds.origin.x || bounds.origin.x + bounds.size.width <= x;
        const bool outY = y <= bounds.origin.y || bounds.origin.y + bounds.size.height <= y;
        if (outX || outY || turnEvery <= turnTimer) {
            // (at a side, or its time up: it stays and heads for the middle)
            turnTimer = static_cast<float>(randInt(0, static_cast<int>(turnEvery * 0.5f)));
            x = m->getPositionX();
            y = m->getPositionY();
            lookAtMiddle();
        }
        m->setPosition(ccp(x, y));
        turnTimer += dt;
        if (state == kLeftHalf || state == kRightHalf) a->setRotation(a->getRotation() + spinSpeed);
        return;
    }
    if (burnt) {
        setState(kBurnt);
        a->stopAllActions();
        a->play("burnt", -1, false);
        return;
    }
    if (!cut) {
        setState(kAlive);
        a->stopAllActions();
        a->play("idle", -1, false);
        return;
    }
    if (splitPending) split(a);
    setState(rightHalf ? kRightHalf : kLeftHalf);
    a->stopAllActions();
    a->play(rightHalf ? "right" : "left", -1, false);
    spinSpeed = randFloat(-spin, spin);
}

void Pygmy::finishStage() {
    // (1.45 advanceStage 0x4cc560: taken out on the tray) healed, the
    // injury finished whatever stage it is on (InjuryState::heal)
    postCreateAtEnd();
    m_stage = static_cast<int>(def->stages.size()) + 1;
    finished = true;
    onFinished();
}

HitResult Pygmy::hitTest(const CCPoint& touch, ToolState* tool) {
    // (1.45 runInteraction 0x4cb9c0) the tongs take it, whole or halved;
    // whole (or burnt), any other of its tools makes it react once: the
    // lighter sets it on fire and the tazer shocks it (both leave it burnt),
    // the stapler leaves a staple in it, the cutter and the chainsaw slice
    // it in two; the cream and the vacuum make it twitch. A half takes no
    // cut (not even as a wrong try)
    HitResult r;
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) r.touched = true;
    const bool half = state == kLeftHalf || state == kRightHalf;
    const int t = tool->type;
    if (!takesTool(t)) {
        if (half && (t == kCutter || t == kChainsaw)) r.touched = false;
        else if (r.touched) r.injury = this;
        return r;
    }
    if (!r.touched) return r;
    Animation* a = anim(*this);
    if (half && (t == kCutter || t == kChainsaw)) {
        r.touched = false;
        return r;
    }
    r.hit = true;
    r.injury = this;
    if (t == kTongs && state >= kAlive && state <= kBurnt) {
        // (its animation is its piece: the port puts it in a sprite of its
        // own, turned as it was: grabPiece)
        r.piece = grabPiece(*this);
        if (a && (state == kAlive || state == kBurnt)) {
            a->stopAllActions();
            a->play("twitch", 1, false);
        }
        return r;
    }
    if (!a || (state != kAlive && state != kBurnt)) return r;
    switch (t) {
    case kLighter:
        burnt = true;
        setState(kInjured);
        a->stopAllActions();
        a->play("fire", 3, false);
        playSound(*this, "pygmy_onfire");
        break;
    case kStapler: {
        // a staple in it, turned as it is, give or take 20 degrees
        CCSprite* staple = CCSprite::create();
        if (CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("tool_staple.png"))
            staple->setDisplayFrame(f);
        staple->setPosition(ccp(2.8f, 0));
        staple->setZOrder(100);
        staple->setRotation(static_cast<float>(randInt(-20, 20)) + rotation);
        setState(kInjured);
        a->stopAllActions();
        a->play("twitch", 1, false);
        a->addChild(staple);
        playSound(*this, "pygmy_react");
        break;
    }
    case kCutter:
    case kChainsaw:
        setState(kInjured);
        burnt = false;
        cut = true;
        splitPending = true;
        a->stopAllActions();
        a->play("cut", 1, false);
        playSound(*this, "pygmy_sliced");
        break;
    case kTazer:
        burnt = true;
        setState(kInjured);
        a->stopAllActions();
        a->play("tazer", 5, false);
        playSound(*this, "pygmy_shocked");
        break;
    default:
        setState(kInjured);
        a->stopAllActions();
        a->play("twitch", 1, false);
        playSound(*this, "pygmy_react");
        break;
    }
    return r;
}

void Pygmy::onTouch(const CCPoint&) {
    // (1.45 onHit 0x4cc564)
    touchedNow = true;
}

void Pygmy::completeStage() {
    // (1.45 autoCompleteStage 0x4cc0e4: Brutality's baton) a half is taken
    // out; a whole one is sliced in two at once
    if (state == kLeftHalf || state == kRightHalf) {
        finishStage();
        return;
    }
    Animation* a = anim(*this);
    if (!a) return;
    split(a);
    setState(kLeftHalf);
    a->stopAllActions();
    a->play("left", -1, false);
    spinSpeed = randFloat(-spin, spin);
}

int Pygmy::toolFor(const CCPoint& touch) {
    // (1.45 getCurrentLegalTool 0x4cc574) the tongs on it, turned with it
    return touchInRect(*this, touch) ? kTongs : kNoTool;
}

}  // namespace mt
