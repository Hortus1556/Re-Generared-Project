#include "as3/MiniWorm.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/InjuryKit.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCDirector.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
constexpr double kDegToRad = 0.017453292519943295;

float dist2(const CCPoint& a, const CCPoint& b) {  // (0x64584)
    float dx = b.x - a.x, dy = b.y - a.y;
    return dx * dx + dy * dy;
}

// The touch in a rect turned `degrees` (the original's int) about `about`
// (0x799f0). Fixed as the other creatures' (Creature.h's touchInRect): the
// original turned the touch's offset by -degrees, the way the picture is
// turned rather than back, so for a worm crawling sideways its rects lay
// mirrored across it (its tail band ahead of it, its body across its
// picture on a slant).
bool inTurned(const CCPoint& touch, const CCRect& r, float degrees, const CCPoint& about) {
    const double a = static_cast<int>(degrees) * kDegToRad;
    const double dx = static_cast<int>(touch.x - about.x), dy = static_cast<int>(touch.y - about.y);
    const float x = about.x + static_cast<float>(dx * std::cos(a) - dy * std::sin(a));
    const float y = about.y + static_cast<float>(dx * std::sin(a) + dy * std::cos(a));
    return r.origin.x < x && x < r.origin.x + r.size.width && r.origin.y < y && y < r.origin.y + r.size.height;
}
}  // namespace

MiniWorm::MiniWorm() {
    // (0x9857c)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    step = s * 1.25f;
    boxWidth = s * 400;
    boxHeight = s * 200;
    zOrder = 40;
    sendsGraphicsEvent = false;
    state = kMiniWormMove;
    stretchUnit = s * 15;
    pullDistance = static_cast<int>(s * 100);
    pullPerFrame = static_cast<float>(pullDistance / stretchFrames);
    priority = 2;
}

void MiniWorm::traceState() {
    static const bool on = platform::debugMode() && std::getenv("AS_TRACE_CREATURES") != nullptr;
    if (!on || state == tracedState) return;
    tracedState = state;
    log("creature #%d MiniWorm state %d frame %u", soundChannel, state, CCDirector::sharedDirector()->getTotalFrames());
}

void MiniWorm::newTurn() {
    if (turnDir == 0) turnDir = randInt(0, 1) == 0 ? 1.0f : -1.0f;
    else turnDir = static_cast<float>(randInt(-1, 1));
}

void MiniWorm::start() {
    // (vf2: not the base's) where it crawls: the play area, and the box in
    // the middle of the screen it turns back to
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
    grabbed = false;
    latched = false;
    shocked = false;
}

void MiniWorm::sizeRects() {
    // (vf31) its rect about its place; its tail a band across it from 0.725
    // to 0.375 of its length below its place (turned with its animation)
    const float w = def->size.width, h = def->size.height;
    rect = CCRectMake(position.x - w * 0.5f, position.y - h * 0.5f, w, h);
    tailRect = CCRectMake(position.x - w * 0.5f, static_cast<float>(position.y + h * -0.725), w,
                          static_cast<float>(h * 0.35));
}

void MiniWorm::vf3() {
    // (vf3) its piece for the tongs (the animation itself in the original),
    // crawling, turned any way
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0);
    if (Animation* a = anim(*this)) {
        a->setRotation(static_cast<float>(randInt(0, 359)));
        a->play("mini_worm_move", -1, false);
    }
    latched = false;
    sizeRects();
    rotation0 = static_cast<int>(rotation);
    // (held by the tail: the tongs a little above its middle)
    pieces[0].grabOffset = ccp(0, static_cast<float>(def->size.height * -0.6));
}

void MiniWorm::turnToBox() {
    // (0x25ddf0)
    Animation* a = anim(*this);
    if (!a) return;
    float tx = static_cast<float>(randInt(static_cast<int>(box.origin.x), static_cast<int>(box.origin.x + box.size.width)));
    float ty = static_cast<float>(randInt(static_cast<int>(box.origin.y), static_cast<int>(box.origin.y + box.size.height)));
    double deg = std::atan2(static_cast<double>(ty - a->getPositionY()), static_cast<double>(tx - a->getPositionX())) *
                 -57.29577951308232;
    a->setRotation(static_cast<float>(static_cast<int>(static_cast<float>(deg) + 90.0f)));
}

void MiniWorm::bite() {
    // (0x25e7d8) a SmallCut a third of its length ahead, turned any way, and
    // its damage
    Animation* a = anim(*this);
    const double r = (a ? a->getRotation() : 0) * kDegToRad;
    const double third = (a ? a->getSpriteSize().height : 0) / 3.0;
    const float x = static_cast<float>(static_cast<int>(position.x + std::cos(r) * 0.0 + third * std::sin(r)));
    const float y = static_cast<float>(static_cast<int>(position.y + third * std::cos(r) + std::sin(r) * 0.0));
    post(createEvent("SmallCut", x, y, static_cast<float>(randInt(0, 359))));
    post(damageEvent(static_cast<int>(biteDamage)));
    SoundMgr::sharedSoundMgr()->playSound("MiniWorm_Bite", false);
}

void MiniWorm::tearOff() {
    // (vf32) pulled off: a WormCut where its head bit (a quarter
    // of its length that way), across it, and the damage
    SoundMgr::sharedSoundMgr()->playOnChannel("leech_burned_tear", soundChannel, false, true, false, false);
    Animation* a = anim(*this);
    const float deg = a ? a->getRotation() : 0;
    const double r = deg * kDegToRad;
    const double part = (a ? a->getSpriteSize().height : 0) / 4.25;
    const float x = static_cast<float>(static_cast<int>(position.x + std::cos(r) * 0.0 + part * std::sin(r)));
    const float y = static_cast<float>(static_cast<int>(position.y + part * std::cos(r) + std::sin(r) * 0.0));
    post(createEvent("WormCut", x, y, static_cast<float>(static_cast<int>(deg + 90.0f))));
    post(damageEvent(static_cast<int>(biteDamage)));
}

void MiniWorm::lunge() {
    // (0x25f368)
    lunging = true;
    if (Animation* a = anim(*this)) a->play("mini_worm_lunge", 1, false);
    SoundMgr::sharedSoundMgr()->playSound("MiniWorm_Bite", false);
}

void MiniWorm::release() {
    // (0x25f180; vf23 has its own copy) crawling on as it is
    biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
    turnDir = static_cast<float>(randInt(-1, 1));
    turnTimer = randFloat(0, turnEvery * 0.5f);
    state = kMiniWormMove;
    grabbed = false;
    latched = false;
    lunging = false;
    if (Animation* a = anim(*this)) a->play("mini_worm_move", -1, false);
    SoundMgr::sharedSoundMgr()->playOnChannel("MiniWorm_StretchRelease", soundChannel, false, true, false, false);
}

void MiniWorm::unwrap() {
    // (the port's: the original's piece sprite was the animation node
    // itself, put back where the tongs left it) the animation out of the
    // sprite grabPiece made, at its place and turn, one of the injury's
    // animations again
    CCSprite* holder = pieces.empty() ? nullptr : pieces[0].sprite;
    if (!holder || holder->getChildrenCount() == 0) return;
    Animation* a = dynamic_cast<Animation*>(static_cast<CCNode*>(holder->getChildren()->objectAtIndex(0)));
    if (!a) return;
    a->retain();
    a->removeFromParentAndCleanup(false);
    a->setPosition(holder->getPosition());
    a->setRotation(holder->getRotation());
    spritesNode->addChild(a);
    a->release();
    animations.insert(animations.begin(), a);
    if (holder->getParent()) holder->removeFromParentAndCleanup(true);
    pieces[0].sprite = nullptr;
}

void MiniWorm::vf4(float dt) {
    // (vf4) the injury where its animation crawls (or where the tongs carry it)
    tracePosition(*this);
    traceState();
    if (!grabbed) unwrap();
    sizeRects();
    placeHitPoints();
    Animation* a = anim(*this);
    CCSprite* holder = pieces.empty() ? nullptr : pieces[0].sprite;
    if (holder) position = holder->getPosition();
    else if (a) position = a->getPosition();
    if (dead || grabbed || state == kMiniWormDangling || !a) return;
    if (state == kMiniWormShocked) {
        // the shock wears off: it crawls on
        shockTimer += dt;
        if (shockTime <= shockTimer) {
            shockTimer = 0;
            shocked = false;
            a->play("mini_worm_move", -1, false);
            state = kMiniWormMove;
            biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
            turnDir = static_cast<float>(randInt(-1, 1));
            turnTimer = randFloat(0, turnEvery * 0.5f);
        }
    } else if (lunging && !a->isPlaying()) {
        // the lunge over: bitten onto the tongs, or let go of meanwhile
        if (!latched) {
            release();
        } else {
            a->play("mini_worm_streched_end", -1, false);
            state = kMiniWormLatched;
        }
    }
    if (latched) return;
    if (state == kMiniWormBite) {
        if (!a->isPlaying()) {
            bite();
            a->play("mini_worm_move", -1, false);
            state = kMiniWormMove;
            biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
            newTurn();
        }
        return;
    }
    if (state != kMiniWormMove) return;
    if (turnTimer < turnEvery) {
        turnTimer += dt;
    } else {
        newTurn();
        turnTimer = randFloat(0, turnEvery * 0.5f);
    }
    const float rot = a->getRotation() + turnSpeed * turnDir;
    a->setRotation(rot);
    const double ang = static_cast<double>(90.0f - rot) * kDegToRad;
    float x = static_cast<float>(a->getPositionX() + step * std::cos(ang));
    float y = static_cast<float>(a->getPositionY() + step * std::sin(ang));
    const bool outX = x <= bounds.origin.x || bounds.origin.x + bounds.size.width <= x;
    const bool outY = y <= bounds.origin.y || bounds.origin.y + bounds.size.height <= y;
    if (outX || outY) {
        // at a side: it stays and turns to the middle
        x = a->getPositionX();
        y = a->getPositionY();
        turnToBox();
        turnDir = 0;
        turnTimer = turnEvery * 0.75f;
    }
    a->setPosition(ccp(x, y));
    biteTimer += dt;
    if (biteEvery <= biteTimer) {
        biteTimer = static_cast<float>(randInt(0, static_cast<int>(biteEvery * 0.5f)));
        state = kMiniWormBite;
        turnToBox();
        a->play("mini_worm_bite", 1, false);
    }
}

void MiniWorm::takeInTongs(HitResult& r) {
    // (vf8) hanging from the tongs by its tail, crawling still
    lunging = false;
    r.injury = this;
    r.piece = grabPiece(*this);
    if (r.piece) r.piece->grabOffset = ccp(0, static_cast<float>(def->size.height * -0.6));
    rotation = 180;
    giveCombo(r, *this);
    setAnimRotation(*this, 180);
    if (Animation* a = anim(*this)) a->play("mini_worm_move", -1, false);
    grabbed = true;
    state = kMiniWormDangling;
    latched = false;
    shocked = false;
}

HitResult MiniWorm::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8)
    HitResult r;
    if (grabbed) {
        // carried in the tongs
        r.hit = true;
        r.injury = this;
        r.piece = pieces.empty() ? nullptr : &pieces[0];
        return r;
    }
    Animation* a = anim(*this);
    const float animRot = a ? a->getRotation() : 0;
    const int t = tool->type;
    if (latched) {
        if (state == kMiniWormLatched && a) {
            // bitten on: stretched towards the tongs, and torn off at last
            const float d2 = dist2(touch, position);
            const float tail2 = dist2(tailRect.origin, position);
            int frames;
            if (d2 < tail2) {
                frames = 1;
                const std::string now = playing(a);
                if (now != "mini_worm_streched_loop" && now != "mini_worm_lunge")
                    a->play("mini_worm_streched_loop", -1, false);
            } else {
                frames = static_cast<int>(std::ceil((d2 / stretchUnit) / (pullPerFrame * pullPerFrame)));
            }
            const float angle = static_cast<float>(
                std::atan2(static_cast<double>(touch.y - position.y), static_cast<double>(touch.x - position.x)) *
                57.29577951308232);
            rotation = 90.0f - angle;
            a->setRotation(90.0f - angle + 180.0f);
            frames = std::min(frames, stretchFrames);
            if (frames > 1) {
                if (playing(a) != "mini_worm_streched_end") {
                    SoundMgr::sharedSoundMgr()->playOnChannel("MiniWorm_Stretch", soundChannel, false, true, false,
                                                              false);
                    a->play("mini_worm_streched_end", 1, false);
                }
                a->showFrame("mini_worm_streched_end", frames);
            }
            r.hit = true;
            r.injury = this;
            if (static_cast<float>(pullDistance * pullDistance) <= d2) {
                tearOff();
                takeInTongs(r);
                return r;
            }
        }
        if (t == kTongs) {
            // (lunging: the tongs held, the injury not given, as the original)
            r.hit = true;
            return r;
        }
    }
    if (t == kTongs) {
        // shocked, its body taken; its tail taken, it bites deeper
        if (inTurned(touch, rect, animRot, position) && shocked) {
            takeInTongs(r);
            r.hit = true;
            return r;
        }
        if (inTurned(touch, tailRect, animRot, position)) {
            latched = true;
            lunge();
            r.hit = true;
            return r;
        }
    }
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) {
        r.hit = true;
        if (dead) {
            r.injury = this;
            r.piece = pieces.empty() ? nullptr : &pieces[0];
            return r;
        }
        // (the original also left alone the ice ant's dead and tazered states,
        // which it never has)
        if ((state | 2) != kIceAntTazer && t == kTazer && state != kMiniWormShocked && a) {
            a->stopAllActions();
            a->play("mini_worm_taser", -1, false);
            SoundMgr::sharedSoundMgr()->playSound("ice_ants_death", false);
            giveCombo(r, *this);
            shockTimer = 0;
            shocked = true;
            state = kMiniWormShocked;
        }
    }
    if (r.hit) r.injury = this;
    return r;
}

void MiniWorm::completeStage() {
    // (vf18: the tutorial's and the partners') done with at once
    if (Animation* a = anim(*this)) a->stopAllActions();
    SoundMgr::sharedSoundMgr()->playSound("ice_ants_death", false);
    kit::finishNow(*this);
}

void MiniWorm::toolStopped(int /*tool*/) {
    // (vf23) let go of, carried or bitten on: it crawls on
    if (grabbed || latched) release();
}

int MiniWorm::toolFor(const CCPoint& touch) {
    // (vf27) the tongs on its tail, or carried or held; the tazer on
    // its body, then the tongs while it is shocked
    if (grabbed || latched) return kTongs;
    Animation* a = anim(*this);
    if (inTurned(touch, tailRect, a ? a->getRotation() : 0, position)) return kTongs;
    if (!stage().useSizeRect) return kNoTool;
    if (!inTurned(touch, rect, rotation, position)) return kNoTool;
    return shocked ? kTongs : kTazer;
}

}  // namespace mt
