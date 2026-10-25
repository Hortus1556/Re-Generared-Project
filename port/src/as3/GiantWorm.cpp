#include "as3/GiantWorm.h"

#include <cmath>
#include <cstdlib>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/InjuryKit.h"
#include "as3/Log.h"
#include "as3/ParticleSystem.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCDirector.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
// a create-injury event as the worm posts them (+8, +9 set; not counted
// toward the cap of 40)
InjuryEvent wormEvent(const char* name, float x, float y, float rotation) {
    InjuryEvent e = createEvent(name, x, y, rotation);
    e.flag8 = true;
    e.flag9 = true;
    e.countsTowardCap = false;
    return e;
}
}  // namespace

GiantWorm::GiantWorm() {
    // (0x908c4, case 38)
    segments = 3;
    unused0 = 3;
    spitAtPatientFrame = 12;
    underTime = 12;
    hitTime = 1;
    maxHealth = 3;
    spitInFrame = 8;
    spitAtScreenFrame = 12;
    exitCutFrame = 11;
    exitCutFrame2 = 23;
    zOrder = 100;
    sendsGraphicsEvent = false;
    priority = 3;
}

GiantWorm::~GiantWorm() {
    kit::dropParticles(spitInFx);
    kit::dropParticles(spitOutFx);
}

void GiantWorm::traceState() {
    static const bool on = platform::debugMode() && std::getenv("AS_TRACE_CREATURES") != nullptr;
    if (!on || state == tracedState) return;
    tracedState = state;
    log("creature #%d GiantWorm state %d health %d loop %d frame %u", soundChannel, state, health, current,
        CCDirector::sharedDirector()->getTotalFrames());
}

void GiantWorm::start() {
    // (vf2: not the base's)
    exitCut = false;
    spatAtPatient = false;
    dead = false;
    underTimer = 0;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    area = CCRectMake(s * (r->rect.origin.x - mx), s * (r->rect.origin.y - my), s * r->rect.size.width,
                      s * r->rect.size.height);
}

void GiantWorm::sizeRects() {
    // (vf31)
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
}

Animation* GiantWorm::loop(int i) {
    if (i < 0 || i >= static_cast<int>(animations.size())) return nullptr;
    return dynamic_cast<Animation*>(animations[static_cast<size_t>(i)]);
}

void GiantWorm::addLoops(int count) {
    // (0x268290)
    for (int i = 0; i < count; ++i) {
        Animation* a = Animation::create("common/anim_creature_toxic_worm_anim.json", 1);
        if (!a) continue;
        animations.push_back(a);
        spritesNode->addChild(a);
        a->setPosition(position);
        a->setRotation(rotation);
    }
}

void GiantWorm::vf3() {
    // (vf3) under the skin, its loops made; its spits ready, unseen
    m_flag58 = true;
    hitTimer = hitTime;
    health = static_cast<int>(maxHealth);
    current = 1;
    state = kGiantWormUnderSkin;
    addLoops(segments);
    // (the original also made its loops the sprites of its first four pieces,
    // which it has none of)
    area = CCRectMake(75, 35, 280, 170);
    sizeRects();
    // (its pictures built again: the spits of before let go of; the
    // original's leaked)
    kit::dropParticles(spitOutFx);
    kit::dropParticles(spitInFx);
    spitOutFx = ParticleSystem::create("particle_wormspitoutFX");
    if (spitOutFx) {
        spitOutFx->retain();
        spitOutFx->stopSystem();
        topNode->addChild(spitOutFx);
        spitOutFx->setVisible(false);
    }
    spitInFx = ParticleSystem::create("particle_wormspitinFX");
    if (spitInFx) {
        spitInFx->retain();
        spitInFx->stopSystem();
        topNode->addChild(spitInFx);
    }
    topZ = topNode->getZOrder();
    if (spitInFx) spitInFx->setVisible(false);
    traceState();
}

void GiantWorm::flipAll() {
    // (0x268ebc)
    flipX = -1;
    for (int i = 0; i < segments + 1; ++i)
        if (Animation* a = loop(i)) a->setScaleX(-1);
}

CCPoint GiantWorm::placeFor(Animation* node, int k) {
    // (0x268f08) the box cut into segments+1 steps each way: loop k is k
    // steps from the top (flipY 1) or the bottom, and from the right (flipX
    // is never 1); the later loops over the earlier ones when coming up
    // from the top
    const float n1 = static_cast<float>(segments + 1);
    const int stepX = static_cast<int>(area.size.width / n1);
    const int stepY = static_cast<int>(area.size.height / n1);
    const float kf = static_cast<float>(k);
    const float ky = flipY != 1 ? static_cast<float>(segments) - kf : kf;
    const float kx = flipX != 1 ? static_cast<float>(segments) - kf : kf;
    const int y = static_cast<int>(area.origin.y + (area.size.height - static_cast<float>(stepY) * ky));
    const int x = static_cast<int>(area.origin.x + (area.size.width - static_cast<float>(stepX) * kx));
    if (node) node->setZOrder(flipY != 1 ? static_cast<int>(n1 - kf) : static_cast<int>(kf));
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    return ccp(static_cast<float>(x) * s, static_cast<float>(y) * s);
}

void GiantWorm::leaveCut(Animation* node, int kind) {
    // (0x269ac8) a WormCut across where the loop goes in (1) or comes out (2)
    if (!node) return;
    const float sx = node->getScaleX();
    const CCSize sz = node->getSpriteSize();
    double d;
    if (sx == 1.0f) d = kind == 1 ? 3.8 : -3.25;
    else d = kind == 1 ? -3.725 : 2.93;
    const int x = static_cast<int>(node->getPositionX() + sz.width / d);
    const int y = static_cast<int>(node->getPositionY() + sz.height * -0.3825);
    post(wormEvent("WormCut", static_cast<float>(x), static_cast<float>(y), 90));
}

void GiantWorm::spitIn() {
    // (0x269ed0) at its mouth: half its height up, a little to the side
    Animation* head = loop(current - 1);
    if (!head || !spitInFx) return;
    const CCSize sz = head->getSpriteSize();
    CCPoint p = position;
    p.y += sz.height * 0.5f;
    p.x = static_cast<float>(p.x + sz.width * (head->getScaleX() == 1.0f ? 0.29 : -0.3));
    spitInFx->setPosition(p);
    spitInFx->resetSystem();
    spitInFx->setVisible(true);
}

void GiantWorm::spitAtPatient() {
    // (0x269fc0) before its head: a SuperPoisonArea at its last health, else
    // a PoisonArea and a Fire
    Animation* head = loop(current - 1);
    if (!head) return;
    const float sx = head->getScaleX();
    const CCSize sz = head->getSpriteSize();
    const int x = sx == 1.0f ? static_cast<int>(head->getPositionX() + sx * (sz.width / 6.0f))
                             : static_cast<int>(head->getPositionX() + sx * (sz.width / 4.75f));
    const int y = static_cast<int>(head->getPositionY() + sz.height * 0.8);
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    if (health == 1) {
        post(wormEvent("SuperPoisonArea", fx, fy, 0));
    } else {
        post(wormEvent("PoisonArea", fx, fy, 0));
        post(wormEvent("Fire", fx, fy, 0));
    }
    SoundMgr::sharedSoundMgr()->playOnChannel("GiantWorm_Spit", soundChannel, false, true, false, false);
}

void GiantWorm::spitAtScreen() {
    // (0x26ac10) the tools frozen 5 s and poisoned, a green wash
    spatAtScreen = true;
    InjuryEvent freeze;
    freeze.type = InjuryEvent::kEvent17;
    freeze.f58 = 5.0f;
    post(freeze);
    post(kit::effectEvent(10, 0, 0));  // PoisonedTools
    post(kit::effectEvent(11, position.x, position.y));  // GreenScreen
    if (spitOutFx) {
        spitOutFx->setPosition(position);
        spitOutFx->resetSystem();
        spitOutFx->setVisible(true);
    }
    SoundMgr::sharedSoundMgr()->playOnChannel("GiantWorm_Spit", soundChannel, false, true, false, false);
}

void GiantWorm::leaveRemains() {
    // (0x2686e0) a GiantWormIncision at its head, a GiantWormLoopIncision at
    // each loop
    Animation* head = loop(current - 1);
    if (head) {
        const float sx = head->getScaleX();
        const CCSize sz = head->getSpriteSize();
        const double d = sx == 1.0f ? 0.155 : 0.154;
        InjuryEvent e = createEvent("GiantWormIncision", 0, 0, 0);
        e.x = static_cast<float>(static_cast<int>(head->getPositionX() + sz.width * d * sx));
        e.y = static_cast<float>(static_cast<int>(head->getPositionY() + sz.height * -0.35));
        e.allowedOverLock = true;
        e.countsTowardCap = false;
        post(e);
    }
    for (int i = 0; i < segments; ++i) {
        Animation* a = loop(i);
        if (!a) continue;
        const float sx = a->getScaleX();
        const CCSize sz = a->getSpriteSize();
        const double d = sx == 1.0f ? -0.00775 : -0.05;
        InjuryEvent e = createEvent("GiantWormLoopIncision", 0, 0, 0);
        e.x = static_cast<float>(static_cast<int>(a->getPositionX() + sz.width * d * sx));
        e.y = static_cast<float>(static_cast<int>(a->getPositionY() + sz.height * 0.00675));
        e.allowedOverLock = true;
        e.countsTowardCap = false;
        post(e);
    }
}

void GiantWorm::die(Animation* head) {
    // (vf8, vf18) its head turned back (kept in place) and dying
    SoundMgr::sharedSoundMgr()->playOnChannel("GiantWorm_death", soundChannel, false, true, false, false);
    if (head) {
        // Turned back, its tail stays in the hole the head came out of
        // (0x269ac8's WormCut, kind 1: w/-3.725 from the turned head): the
        // death's last frame, and the GiantWormFront and WormStuck 0x2686e0
        // leaves for it, go into the patient 0.29·w right of the unturned
        // head. Fixed: the original moved it by w·-0.215 (0x26bbe0), which
        // put the dead head and its remains about 45 points beside its
        // wound (the loops are always turned: 0x268ebc).
        if (head->getScaleX() == -1.0f) {
            const double w = head->getSpriteSize().width;
            const int hole = static_cast<int>(head->getPositionX() + w / -3.725);
            head->setPositionX(static_cast<float>(hole - w * 0.29));
        }
        head->setScaleX(1);
        head->queue("death", 1, false);
    }
    dead = true;
    state = kGiantWormDead;
}

void GiantWorm::vf4(float dt) {
    // (vf4) the injury at the loop it is at, then its state's course: one
    // step a frame
    tracePosition(*this);
    Animation* cur = loop(current - 1);
    sizeRects();
    placeHitPoints();
    if (cur) position = cur->getPosition();
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    [&] {
        if (!cur) return;
        if (state == kGiantWormMoving) {
            if (!cur->isPlaying()) {
                // this loop is through: the next comes up, or its head
                if (current - 1 != segments) cur->play("burrow_loop", -1, false);
                ++current;
                sound->playOnChannel("GiantWorm_Loop", soundChannel, false, true, false, false);
                Animation* next = loop(current - 1);
                if (!next) return;
                if (current - 1 == segments) {
                    for (int i = 0; i < segments; ++i)
                        if (Animation* a = loop(i)) {
                            a->showFrame("burrow_loop", 21);
                            a->pause();
                        }
                    next->setVisible(true);
                    next->play("look_around_start", 1, false);
                    next->queue("look_around_loop", 1, false);
                    reacted = false;
                    state = kGiantWormLookingAround;
                } else {
                    next->setVisible(true);
                    next->play("burrow_start", 1, false);
                }
                next->setPosition(placeFor(next, current - 1));
                leaveCut(next, 1);
                exitCut = false;
            } else if (exitCutFrame <= cur->frame() && !exitCut) {
                leaveCut(cur, 2);
                exitCut = true;
            }
            return;
        }
        if (state == kGiantWormSpittingAtPatient && cur->isPlaying() && spitInFrame <= cur->frame() && !spatIn) {
            spatIn = true;
            spitIn();
            return;
        }
        if (state == kGiantWormUnderSkin) {
            underTimer += dt;
            if (underTime <= underTimer) {
                // up again, from the top or the bottom
                flipY = 1;
                if (randInt(0, 1) == 0) flipY = -flipY;
                randInt(0, 1);
                flipAll();
                current = 1;
                state = kGiantWormMoving;
                Animation* tail = loop(0);
                if (!tail) return;
                tail->play("burrow_start", 1, false);
                tail->setPosition(placeFor(tail, current - 1));
                tail->setVisible(true);
                sound->playOnChannel("GiantWorm_Loop", soundChannel, false, true, false, false);
                leaveCut(tail, 1);
            }
            return;
        }
        if (state == kGiantWormLookingAround && !cur->isPlaying()) {
            topNode->setZOrder(-10);
            cur->play("spit_at_patient", 1, false);
            state = kGiantWormSpittingAtPatient;
            spatIn = false;
            return;
        }
        if (state == kGiantWormSpittingAtPatient) {
            if (spitAtPatientFrame <= cur->frame() && !spatAtPatient) {
                spitAtPatient();
                spatAtPatient = true;
                return;
            }
            if (!cur->isPlaying()) {
                // back under, the tail first
                exitCut = false;
                spatAtPatient = false;
                for (int i = 0; i < segments; ++i)
                    if (Animation* a = loop(i)) a->resume();
                topNode->setZOrder(topZ);
                if (spitInFx) spitInFx->setVisible(false);
                cur->play("burrow_loop", -1, false);
                if (Animation* tail = loop(segments - current + 1)) tail->play("burrow_end", 1, false);
                state = kGiantWormGoingUnder;
                return;
            }
            if (exitCutFrame2 <= cur->frame() && !exitCut) {
                leaveCut(cur, 2);
                exitCut = true;
                return;
            }
        }
        if (state == kGiantWormGoingUnder) {
            Animation* sinking = loop(segments - current + 1);
            if (!sinking || !sinking->isPlaying()) {
                sound->playSound("GiantWorm_Loop", false);
                if (sinking) sinking->setVisible(false);
                if (current == 1) {
                    underTimer = 0;
                    state = kGiantWormUnderSkin;
                    return;
                }
                const int was = current--;
                if (Animation* next = loop(segments - was + 2)) next->play("burrow_end", 1, false);
                exitCut = false;
                return;
            }
        }
        if (state == kGiantWormSpittingAtScreen) {
            if (spitAtScreenFrame <= cur->frame() && !spatAtScreen) {
                spitAtScreen();
                return;
            }
            if (!cur->isPlaying()) {
                if (spitOutFx) spitOutFx->setVisible(false);
                cur->play("spit_at_patient", 1, false);
                state = kGiantWormSpittingAtPatient;
                return;
            }
        }
        if (state == kGiantWormDead && !cur->isPlaying()) {
            leaveRemains();
            finishStage();
        }
    }();
    // (+5) only its head up is to be pointed at
    hiddenFromPick = !(state == kGiantWormLookingAround || state == kGiantWormSpittingAtScreen ||
                       state == kGiantWormSpittingAtPatient);
    traceState();
}

HitResult GiantWorm::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) the chainsaw on its head while it looks around
    HitResult r;
    r.injury = this;
    if (dead || !takesTool(tool->type)) return r;
    if (touchInPlainRect(*this, touch)) r.touched = true;
    if ((state != kGiantWormLookingAround && state != kGiantWormReact) || !r.touched) return r;
    SoundMgr::sharedSoundMgr()->playOnChannel("GiantWorm_Chainsaw_React", soundChannel, false, true, false, false);
    reacted = true;
    r.hit = true;
    state = kGiantWormReact;
    Animation* head = loop(current - 1);
    if (head && (!head->isPlaying() || playing(head) != "react")) head->play("react", 1, false);
    hitTimer = hitTime;
    --health;
    giveCombo(r, *this);
    if (health == 0) {
        die(head);
    } else {
        // it spits at the screen, then at the patient
        if (head) head->queue("look_around_spit", 1, false);
        spatAtScreen = false;
        state = kGiantWormSpittingAtScreen;
    }
    traceState();
    return r;
}

void GiantWorm::completeStage() {
    // (vf18: the tutorial's and the partners') a cut: dead at the last,
    // else straight back under
    hitTimer = hitTime;
    --health;
    Animation* cur = loop(current - 1);
    if (health == 0) {
        die(cur);
        traceState();
        return;
    }
    SoundMgr::sharedSoundMgr()->playOnChannel("GiantWorm_Chainsaw_React", soundChannel, false, true, false, false);
    exitCut = false;
    spatAtPatient = false;
    for (int i = 0; i < segments; ++i)
        if (Animation* a = loop(i)) a->resume();
    topNode->setZOrder(topZ);
    if (spitInFx) spitInFx->setVisible(false);
    if (cur) cur->play("burrow_loop", -1, false);
    if (Animation* tail = loop(segments + 1 - current)) tail->play("burrow_end", 1, false);
    state = kGiantWormGoingUnder;
    traceState();
}

}  // namespace mt
