#include "as3/FireMaggot.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/ClampsController.h"
#include "as3/Creature.h"
#include "as3/ParticleSystem.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
constexpr float kDegToRad = 0.017453292519943295f;

void setRate(ParticleSystem* p, float r) {
    if (p) p->setEmissionRate(r);
}

bool clampTight(const InjuryState& s) {
    return !s.clamps.empty() && s.clamps[0]->tightness == s.clamps[0]->maxTightness;
}
}  // namespace

FireMaggot::FireMaggot() {
    // (0x98e60)
    zOrder = 100;
    priority = 3;
    sendsGraphicsEvent = false;
    breathTimer = randFloat(-1, 1) + 7.0f;
}

FireMaggot::~FireMaggot() {
    for (ParticleSystem* p : {breath, tail, fire})
        if (p) {
            p->stopSystem();
            p->removeFromParentAndCleanup(true);
            p->release();
        }
}

float FireMaggot::breathDelay() { return breathEvery + randFloat(-breathVariance, breathVariance); }

void FireMaggot::start() {
    // (vf2: not the base's) its breath and tail, its fire (off)
    breath = ParticleSystem::create("particle_maggotFX");
    if (breath) {
        breathRate = breath->getEmissionRate();
        breath->setEmissionRate(0);
        breath->setPosition(CCPointZero);
        breath->retain();
    }
    tail = ParticleSystem::create("particle_maggotailFX");
    if (tail) {
        tailRate = tail->getEmissionRate();
        tail->setPosition(CCPointZero);
        tail->retain();
    }
    fire = ParticleSystem::create("particle_maggotFX");
    if (fire) {
        fire->setEmissionRate(0);
        fire->setPosition(CCPointZero);
        fire->retain();
    }
    if (hasStage()) state = stage().state;
}

void FireMaggot::vf3() {
    // (vf3) its piece for the tongs; crawling; its breath at its mouth, its
    // tail's smoke at its tail
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0x33);
    Animation* a = anim(*this);
    if (a) a->play("walk", -1, false);
    SoundMgr::sharedSoundMgr()->playOnChannel("fire_maggot_movement_loop", soundChannel, true, true, false, false);
    newTarget();
    for (ParticleSystem* p : {breath, tail, fire})
        if (p && p->getParent()) p->removeFromParentAndCleanup(false);
    if (a) {
        if (breath) a->attach("cp_maggotBreath", breath);
        if (tail) a->attach("cp_maggotTail", tail);
    }
    if (fire) root->addChild(fire);
}

void FireMaggot::newTarget() {
    // (0x13bb98)
    target = hopTarget(position, 58, 8);
    angle = headingTo(position, target, dir);
}

void FireMaggot::setRotation(float degrees) {
    // (vf21) its tail's smoke stays upright
    InjuryState::setRotation(degrees);
    if (tail) tail->setRotation(-degrees);
}

void FireMaggot::postFire() {
    const float t = rotation * kDegToRad, d = def->size.height;
    post(createEvent("Fire", position.x + d * std::sin(t), position.y + d * std::cos(t), 0));
}

void FireMaggot::resurface() {
    // (0x13d45c) up again where it is
    vacuumed = false;
    breathing = false;
    breathTimer = breathDelay();
    setRate(tail, tailRate);
    if (Animation* a = anim(*this)) {
        a->setVisible(true);
        a->play("out", 1, false);
        a->queue("walk", -1, false);
    }
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->playOnChannel("fire_maggot_resurface", soundChannel, false, true, false, false);
    sound->playOnChannel("fire_maggot_movement_loop", soundChannel, true, false, false, true);
    goToState(kFireMaggotCalmed, true, true);
    post(createEvent("SnakeCut", position.x, position.y, rotation + 90.0f));
}

void FireMaggot::hurtFromUnder() {
    // (0x13da64) the damage effect and the patient's scream
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = 0;
    e.x = std::floor(position.x);
    e.y = std::floor(position.y);
    post(e);
    InjuryEvent s;
    s.type = InjuryEvent::kEvent19;
    post(s);
}

void FireMaggot::crawl(float dt) {
    // turned towards its target first, then on to it
    if (angle == rotation) {
        const float v = 40.0f * ResolutionMgr::sharedResolutionMgr()->canvasScale;
        const float nx = position.x + dir.x * v * dt, ny = position.y + dir.y * v * dt;
        const bool overX = (dir.x > 0 && target.x < nx) || (dir.x < 0 && nx < target.x);
        const bool overY = (dir.y > 0 && target.y < ny) || (dir.y < 0 && ny < target.y);
        if (!overX && !overY) {
            moveTo(*this, ccp(nx, ny));
            return;
        }
        moveTo(*this, target);
        if (hasStage() && stage().state == kHidden) hurtFromUnder();
        newTarget();
        return;
    }
    const float step = 90.0f * dt, rot = rotation;
    float next;
    if (angle <= rot) {
        if (rot - angle <= 180.0f) {
            next = rot - step;
            if (next < angle) next = angle;
        } else {
            next = step + rot;
            if (360.0f <= next) next = angle;
        }
    } else if (angle - rot <= 180.0f) {
        next = step + rot;
        if (angle < next) next = angle;
    } else {
        next = rot - step;
        if (next < 0) {
            next += 360.0f;
            if (next < angle) next = angle;
        }
    }
    setRotation(next);
}

void FireMaggot::vf4(float dt) {
    // (vf4)
    tracePosition(*this);
    Animation* a = anim(*this);
    if (!a || !hasStage()) return;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    switch (stage().state) {
    case kHidden: {
        // under the skin once its burrowing is over, going about
        bool moving = false;
        if (!a->isPlaying()) {
            if (a->isVisible()) a->setVisible(false);
            moving = true;
        }
        timer -= dt;
        if (timer <= 0) resurface();
        if (moving) crawl(dt);
        return;
    }
    case kClamped:
        if (!clamps.empty()) {
            clampTimer -= dt;
            if (clampTimer > 0) {
                if (!clampDone && clampTight(*this)) {
                    // (vf25: its popup)
                    InjuryEvent e;
                    e.type = InjuryEvent::kTagPopup;
                    e.useTouch = true;
                    post(e);
                    clampTimer = clampHold;
                    clampDone = true;
                }
                return;
            }
        }
        // shrugged off
        releaseClamps();
        clampDone = false;
        a->play("clamped_shrugoff", 1, false);
        a->queue("walk", -1, false);
        sound->playOnChannel("fire_maggot_movement_loop", soundChannel, true, true, false, false);
        goToState(kFireMaggotCalmed, true, true);
        return;
    case kFireMaggotAngry:
        timer -= dt;
        if (timer <= 0) {
            setRate(breath, 0);
            if (!vacuumed) postFire();
            if (clamps.empty()) {
                // it burrows
                timer = underTime;
                a->play("burrow", 1, false);
                sound->playOnChannel("fire_maggot_burrow", soundChannel, false, true, false, false);
                goToState(kHidden, true, true);
                post(createEvent("SnakeCut", position.x, position.y, rotation + 90.0f));
            } else {
                releaseClamps();
                clampDone = false;
                vacuumed = false;
                breathing = false;
                breathTimer = breathDelay();
                setRate(tail, tailRate);
                a->play("clamped_shrugoff", 1, false);
                a->queue("walk", -1, false);
                sound->playOnChannel("fire_maggot_movement_loop", soundChannel, true, true, false, false);
                goToState(kFireMaggotCalmed, true, true);
            }
        } else if (!a->isPlaying()) {
            a->play("angry", 1, false);
            if (!vacuumed) sound->playOnChannel("fire_maggot_fire_breathe", soundChannel, false, true, false, false);
            else sound->stopChannel(soundChannel, false);
        }
        return;
    case kFireMaggotCalmed:
        break;
    default:
        return;
    }
    // calm: crawling, breathing fire now and then
    const bool moving = a->isPlaying() && playing(a) == "walk" && clamps.empty() && !breathing;
    if (!moving && !clamps.empty()) {
        // a clamp put on it holds it (1.45 FireMaggot::vf4 0x1ad330: stat 1, StatsMgr.h)
        StatsMgr::sharedInstance()->incrementStat(StatTypes::kClampMaggot);
        setRate(fire, 0);
        clampTimer = clampTime;
        a->play("clamped", -1, false);
        sound->stopChannel(soundChannel, false);
        goToState(kClamped, true, true);
        return;
    }
    if (vacuumed) {
        vacuumTimer -= dt;
        if (vacuumTimer > 0) {
            if (moving) crawl(dt);
            return;
        }
        vacuumed = false;
        breathing = false;
        breathTimer = breathDelay();
        setRate(tail, tailRate);
        if (moving) crawl(dt);
        return;
    }
    breathTimer -= dt;
    if (breathing) {
        if (breathTimer > 0) return;
        a->resume();
        breathing = false;
        postFire();
        setRate(fire, 0);
        breathTimer = breathDelay();
        return;
    }
    if (breathTimer <= 0) {
        // it stops and breathes fire
        a->pause();
        sound->playOnChannel("fire_maggot_fire_breathe", soundChannel, false, true, false, false);
        breathing = true;
        if (fire) {
            const float t = rotation * kDegToRad, d = def->size.height * 0.5f;
            fire->setPosition(ccp(position.x + d * std::sin(t), position.y + d * std::cos(t)));
            fire->setRotation(rotation);
            fire->setEmissionRate(breathRate);
        }
        breathTimer = breathTime;
        // (0x13bc98 goes on to the crawl of this frame: one step more)
        if (moving) crawl(dt);
        return;
    }
    if (moving) crawl(dt);
}

void FireMaggot::tazered(bool tight) {
    // (0x13e570)
    setRate(fire, 0);
    Animation* a = anim(*this);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (--lives < 1) {
        if (a) a->play(tight ? "clamped_taser" : "taser", 3, false);
        releaseClamps();
        clampDone = false;
        if (a) a->queue("dead", 1, false);
        sound->playOnChannel("fire_maggot_death", soundChannel, false, true, false, false);
        goToState(kFireMaggotDead, true, true);
        return;
    }
    if (tight) {
        if (a) {
            a->play("clamped_taser", 3, false);
            a->queue("clamped_angry", -1, false);
        }
    } else {
        releaseClamps();
        clampDone = false;
        if (a) a->play("taser", 3, false);
    }
    timer = angryTime;
    goToState(kFireMaggotAngry, true, true);
    if (vacuumed) return;
    setRate(breath, breathRate);
    sound->playOnChannel("fire_maggot_fire_breathe", soundChannel, true, true, false, false);
}

HitResult FireMaggot::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) the vacuum sucks its flame; the tazer hurts it; the cutter
    // brings it up; dead, the tongs take it
    HitResult r;
    if (stage().useSizeRect && touchInRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    if (!r.touched) return r;
    r.hit = true;
    r.injury = this;
    onTouch(touch);
    switch (stage().state) {
    case kHidden:
        if (anim(*this) && !anim(*this)->isVisible()) resurface();
        return r;
    case kClamped:
        if (tool->type == kVacuum) {
            vacuumed = true;
            setRate(tail, 0);
            return r;
        }
        if (tool->type != kTazer) return r;
        tazered(clampTight(*this));
        break;
    case kFireMaggotDead:
        r.piece = grabPiece(*this);
        return r;
    case kFireMaggotCalmed:
        if (tool->type == kVacuum) {
            vacuumed = true;
            vacuumTimer = vacuumTime;
            setRate(tail, 0);
            if (breathing) {
                setRate(fire, 0);
                breathing = false;
                if (Animation* a = anim(*this)) a->resume();
            }
            return r;
        }
        if (tool->type != kTazer) return r;
        tazered(false);
        break;
    default:
        return r;
    }
    giveCombo(r, *this);
    return r;
}

void FireMaggot::die() {
    setRate(breath, 0);
    setRate(tail, 0);
    setRate(fire, 0);
    if (Animation* a = anim(*this)) a->play("dead", 1, false);
    SoundMgr::sharedSoundMgr()->playOnChannel("fire_maggot_death", soundChannel, false, true, false, false);
    goToState(kFireMaggotDead, true, true);
}

void FireMaggot::completeStage() {
    // (vf18)
    if (stage().state == kFireMaggotDead) {
        postCreateAtEnd();
        m_stage = static_cast<int>(def->stages.size()) + 1;
        finished = true;
        onFinished();
        return;
    }
    if (--lives < 1) die();
}

int FireMaggot::toolFor(const CCPoint& touch) {
    // (vf27) the vacuum, a clamp, the tazer; the cutter (the scanner to
    // find it) under the skin; the tongs dead
    if (!stage().useSizeRect) return kNoTool;
    const int st = stage().state;
    if (!touchInRect(*this, touch)) return st == kHidden ? kScanner : kNoTool;
    if (st == kFireMaggotDead) return kTongs;
    if (st == kHidden) return kCutter;
    if (!vacuumed) return kVacuum;
    return st == kClamped && clampTight(*this) ? kTazer : kClamp;
}

}  // namespace mt
