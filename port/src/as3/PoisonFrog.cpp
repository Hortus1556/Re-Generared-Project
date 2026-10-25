#include "as3/PoisonFrog.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
constexpr double kDegToRad = 0.017453292519943295;
}

PoisonFrog::PoisonFrog() {
    // (0x98fd4)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    walkStep = 2 * s;
    runStep = 7 * s;
    boxWidth = 300 * s;
    boxHeight = 175 * s;
    dartDistance = 75 * s;
    zOrder = 100;
    priority = 3;
    sendsGraphicsEvent = false;
    state = kPoisonFrogWalking;
}

void PoisonFrog::start() {
    // (vf2: not the base's) the screen wobbles a moment (event 18); where it walks
    hopSounded = false;
    hopTimer = 0;
    InjuryEvent w;
    w.type = InjuryEvent::kEvent18;
    post(w);
    puffTimer = puffEvery;
    flashTimer = 0;
    blueTimer = blueEvery;
    dead = false;
    hurtFlash = redOn = greenOn = stunned = false;
    health = static_cast<int>(burnHealth);
    lives = lives0;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    bounds = CCRectMake(s * (r->rect.origin.x - mx), s * (r->rect.origin.y - my), s * r->rect.size.width,
                        s * r->rect.size.height);
    box = CCRectMake(static_cast<float>(r->width / 2) - boxWidth * 0.5f,
                     static_cast<float>(r->height / 2) - boxHeight * 0.5f, boxWidth, boxHeight);
    lookTimer = static_cast<float>(randInt(0, static_cast<int>(lookEvery * 0.5f)));
    turnTimer = static_cast<float>(randInt(0, static_cast<int>(turnEvery * 0.5f)));
}

void PoisonFrog::vf3() {
    // (vf3) its piece for the tongs; walking, turned any way
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0);
    if (Animation* a = anim(*this)) {
        a->setRotation(static_cast<float>(randInt(0, 359)));
        a->play("walk", -1, false);
    }
    hopSounded = true;
    hopTimer = 0;
    SoundMgr::sharedSoundMgr()->playOnChannel("poison_frog_hop_slow", soundChannel, false, false, false, false);
}

void PoisonFrog::turnToMiddle() {
    // (0x1919f8)
    Animation* a = anim(*this);
    if (!a) return;
    float tx = static_cast<float>(randInt(static_cast<int>(box.origin.x), static_cast<int>(box.origin.x + box.size.width)));
    float ty = static_cast<float>(randInt(static_cast<int>(box.origin.y), static_cast<int>(box.origin.y + box.size.height)));
    double deg = std::atan2(static_cast<double>(ty - a->getPositionY()), static_cast<double>(tx - a->getPositionX())) *
                 -57.29577951308232;
    a->setRotation(static_cast<float>(static_cast<int>(deg + 90.0)));
}

void PoisonFrog::spit() {
    // (0x191b04)
    dartFired = false;
    if (Animation* a = anim(*this)) a->play("dart", 1, false);
    SoundMgr::sharedSoundMgr()->playOnChannel("poison_frog_dart_spit", 2, soundChannel, false, true, false, false);
}

void PoisonFrog::puff() {
    // (0x193f28) at most every second: the blue cloud (effect 4) and the
    // screen's liquid wobble for 8 s (event 15)
    if (puffTimer < puffEvery) return;
    SoundMgr::sharedSoundMgr()->playSound("poison_frog_gas_cloud", false);
    puffTimer = 0;
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = 4;
    e.x = std::floor(position.x);
    e.y = std::floor(position.y);
    post(e);
    InjuryEvent w;
    w.type = InjuryEvent::kLiquid;
    w.f58 = 8.0f;
    post(w);
}

void PoisonFrog::blueScreen() {
    // (0x194714) at most every 8 s (effect 5)
    if (blueTimer < blueEvery) return;
    blueTimer = 0;
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = 5;
    e.x = std::floor(position.x);
    e.y = std::floor(position.y);
    post(e);
}

void PoisonFrog::walk(float step) {
    Animation* a = anim(*this);
    const double ang = static_cast<double>(90.0f - a->getRotation()) * kDegToRad;
    float x = static_cast<float>(a->getPositionX() + step * std::cos(ang));
    float y = static_cast<float>(a->getPositionY() + step * std::sin(ang));
    const bool outX = x <= bounds.origin.x || bounds.origin.x + bounds.size.width <= x;
    const bool outY = y <= bounds.origin.y || bounds.origin.y + bounds.size.height <= y;
    if (outX || outY) {
        x = a->getPositionX();
        y = a->getPositionY();
        turnToMiddle();
        turnTimer = 0;
    }
    a->setPosition(ccp(x, y));
}

void PoisonFrog::startWalking() {
    state = kPoisonFrogWalking;
    if (Animation* a = anim(*this)) a->play("walk", -1, false);
    hopSounded = true;
    hopTimer = 0;
    SoundMgr::sharedSoundMgr()->playOnChannel("poison_frog_hop_slow", soundChannel, false, false, false, false);
}

void PoisonFrog::startRunning() {
    state = kPoisonFrogAngry;
    if (Animation* a = anim(*this)) a->play("run", -1, false);
    hopTimer = 0;
    hopSounded = true;
    SoundMgr::sharedSoundMgr()->playOnChannel("poison_frog_hop_fast", soundChannel, false, false, false, false);
}

void PoisonFrog::vf4(float dt) {
    // (vf4) the injury where its animation walks
    tracePosition(*this);
    Animation* a = anim(*this);
    puffTimer += dt;
    blueTimer += dt;
    placeHitPoints();
    if (a) position = animPlace(*this, a);  // (the port's grabPiece: see animPlace)
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    if (dead || !a) return;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    // stunned: it flashes green (red as the lighter hurts it)
    if (stunned) {
        flashTimer += dt;
        if (flashEvery <= flashTimer) {
            flashTimer = 0;
            if (!hurtFlash) {
                a->setColor(greenOn ? ccc3(255, 255, 255) : ccc3(0x7d, 0xff, 0));
                greenOn = !greenOn;
            } else {
                hurtFlash = false;
                greenOn = false;
                a->setColor(redOn ? ccc3(255, 255, 255) : ccc3(255, 0, 0));
                redOn = !redOn;
            }
        }
    }
    // its hops' sound, once a hop
    if (state == kPoisonFrogWalking || state == kPoisonFrogAngry) {
        hopTimer += dt;
        const std::string cur = playing(a);
        if (!hopSounded && a->frame() > 0 &&
            ((cur == "walk" && hopTimer <= slowHop) || (cur == "run" && hopTimer <= fastHop))) {
            hopSounded = true;
            sound->playOnChannel(state == kPoisonFrogAngry ? "poison_frog_hop_fast" : "poison_frog_hop_slow",
                                 soundChannel, false, false, false, false);
        }
        if ((state == kPoisonFrogAngry && fastHop < hopTimer) || (state == kPoisonFrogWalking && slowHop < hopTimer)) {
            hopTimer = 0;
            hopSounded = false;
        }
    } else {
        hopSounded = false;
        hopTimer = 0;
    }
    switch (state) {
    case kPoisonFrogWalking:
        walk(walkStep);
        turnTimer += dt;
        if (turnEvery <= turnTimer) {
            turnTimer = static_cast<float>(randInt(0, static_cast<int>(turnEvery * 0.5f)));
            a->setRotation(static_cast<float>(randInt(0, 359)));
        }
        lookTimer += dt;
        if (lookEvery <= lookTimer) {
            lookTimer = static_cast<float>(randInt(0, static_cast<int>(lookEvery * 0.5f)));
            state = kPoisonFrogLooking;
            // (the port's: it sits, the unused "idle" of anim_creature_frog_anim,
            // instead of hopping on the spot to the end of its hop; then it looks)
            a->play("idle", 1, false);
            a->queue("lookleft", 1, false);
            log("creature #%d PoisonFrog sits", soundChannel);  // (for test scripts)
            a->queue("lookright", 1, false);
            sound->stopChannel(soundChannel, false);
        }
        break;
    case kPoisonFrogLooking:
        if (!a->isPlaying()) {
            state = kPoisonFrogFiring;
            turnToMiddle();
            spit();
        }
        break;
    case kPoisonFrogAngry:
        walk(runStep);
        turnTimer += runTurnRate * dt;
        if (turnEvery <= turnTimer) {
            turnTimer = static_cast<float>(randInt(0, static_cast<int>(turnEvery * 0.5f)));
            a->setRotation(static_cast<float>(randInt(0, 359)));
        }
        runTimer += dt;
        if (runTime <= runTimer) {
            a->setColor(ccc3(255, 255, 255));
            greenOn = hurtFlash = stunned = false;
            log("creature #%d unstunned", soundChannel);  // (for test scripts)
            startWalking();
        }
        break;
    case kPoisonFrogFiring:
        if (dartFrame <= a->frame() && !dartFired) {
            // its dart where its tongue reaches, turned back towards the
            // frog (0x191c68: its turn + 180)
            dartFired = true;
            const double ang = static_cast<double>(90.0f - a->getRotation()) * kDegToRad;
            post(createEvent("PoisonDart", static_cast<float>(a->getPositionX() + dartDistance * std::cos(ang)),
                             static_cast<float>(a->getPositionY() + dartDistance * std::sin(ang)),
                             a->getRotation() + 180.0f));
        } else if (!a->isPlaying()) {
            lookTimer = static_cast<float>(randInt(0, static_cast<int>(lookEvery * 0.5f)));
            turnTimer = static_cast<float>(randInt(0, static_cast<int>(turnEvery * 0.5f)));
            a->setRotation(static_cast<float>(randInt(0, 359)));
            if (!stunned) startWalking();
            else startRunning();
        }
        break;
    case kPoisonFrogShocked:
        if (!a->isPlaying()) {
            lookTimer = static_cast<float>(randInt(0, static_cast<int>(lookEvery * 0.5f)));
            turnTimer = static_cast<float>(randInt(0, static_cast<int>(turnEvery * 0.5f)));
            a->setRotation(static_cast<float>(randInt(0, 359)));
            startRunning();
            // (the port's: it leaps away, the unused "jump" of its animation, then runs)
            a->play("jump", 1, false);
            a->queue("run", -1, false);
            log("creature #%d PoisonFrog jumps", soundChannel);  // (for test scripts)
            stunned = true;
            log("creature #%d stunned", soundChannel);  // (for test scripts: it flashes green)
            runTimer = 0;
            greenOn = false;
        }
        break;
    case kPoisonFrogHurt:
        if (!a->isPlaying()) {
            stunned = false;
            log("creature #%d unstunned", soundChannel);  // (for test scripts)
            startWalking();
        }
        break;
    default:
        break;
    }
}

HitResult PoisonFrog::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) the tazer stuns it; stunned, the lighter hurts it; anything else
    // on it unstunned, it puffs poison and the tool is put down (event 8)
    HitResult r;
    r.injury = this;
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) r.touched = true;
    auto poisoned = [&]() {
        puff();
        blueScreen();
        InjuryEvent e;
        e.type = InjuryEvent::kEvent8;
        post(e);
    };
    if (!takesTool(tool->type)) {
        if (r.touched && !stunned && !dead) poisoned();
        return r;
    }
    if (!r.touched) return r;
    r.hit = true;
    if (dead) {
        r.piece = grabPiece(*this);
        return r;
    }
    Animation* a = anim(*this);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (!stunned && tool->type == kTazer && state != kPoisonFrogShocked) {
        if (a) {
            a->stopAllActions();
            a->play("tazer", 2, false);
        }
        sound->playOnChannel("poison_frog_hit_croak_1", soundChannel, false, true, false, false);
        if (!comboGiven) {
            comboGiven = true;
            r.comboAction = true;
        } else {
            r.comboBonus = true;
        }
        state = kPoisonFrogShocked;
        return r;
    }
    if (!stunned) {
        poisoned();
        return r;
    }
    if (tool->type != kLighter) return r;
    health = static_cast<int>(static_cast<float>(health) - tool->power);
    hurtFlash = true;
    if (health >= 1) return r;
    // a life burnt away
    flashTimer = 0;
    health = static_cast<int>(burnHealth);
    --lives;
    comboGiven = false;
    r.stageDone = true;
    giveCombo(r, *this);
    hurtFlash = redOn = greenOn = stunned = false;
    if (a) a->setColor(ccc3(255, 255, 255));
    if (lives < 1) {
        dead = true;
        if (a) a->play("dead", 1, false);
        sound->playOnChannel("poison_frog_death", 2, soundChannel, false, true, false, false);
        // (its dead stage, taking the tongs, without its events)
        ++m_stage;
        stageFound = true;
        state = kPoisonFrogDead;
    } else {
        state = kPoisonFrogHurt;
        if (a) a->play("chainsaw", 1, false);
        sound->playOnChannel("poison_frog_hit_croak_2", soundChannel, false, true, false, false);
        runTimer = 0;
        flashTimer = 0;
    }
    return r;
}

void PoisonFrog::completeStage() {
    // (vf18) a life gone (the tutorial's, a partner's)
    if (stage().state == kPoisonFrogDead) {
        postCreateAtEnd();
        m_stage = static_cast<int>(def->stages.size()) + 1;
        finished = true;
        onFinished();
        return;
    }
    --lives;
    Animation* a = anim(*this);
    if (a) a->setColor(ccc3(255, 255, 255));
    if (lives < 1) {
        dead = true;
        if (a) a->play("dead", 1, false);
        SoundMgr::sharedSoundMgr()->playOnChannel("poison_frog_death", 2, soundChannel, false, true, false, false);
        ++m_stage;
        stageFound = true;
        state = kPoisonFrogDead;
    } else {
        SoundMgr::sharedSoundMgr()->playOnChannel("poison_frog_hit_croak_2", soundChannel, false, true, false, false);
    }
}

int PoisonFrog::toolFor(const CCPoint& touch) {
    // (vf27)
    if (!touchInRect(*this, touch)) return kNoTool;
    if (stunned) return kLighter;
    if (stage().state == kPoisonFrogDead) return kTongs;
    const int k = state - kPoisonFrogWalking;
    if (k >= 0 && k < 4) return k != 1 ? kTazer : kNoTool;
    return kNoTool;
}

}  // namespace mt
