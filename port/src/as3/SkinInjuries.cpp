#include "as3/SkinInjuries.h"

#include <algorithm>

#include "as3/Animation.h"
#include "as3/Effect.h"
#include "as3/InjuryKit.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;

namespace {

Animation* firstAnimation(InjuryState& i) {
    return i.animations.empty() ? nullptr : static_cast<Animation*>(i.animations[0]);
}

}  // namespace

// ---- FrostBite

FrostBite::FrostBite() {
    zOrder = 250;
    pointHealth = 1;
    sendsGraphicsEvent = false;
}

void FrostBite::vf3() {
    m_flag58 = true;
    SoundMgr::sharedSoundMgr()->playOnChannel("frostbite_spread", soundChannel, false, true, false, false);
    if (Animation* a = firstAnimation(*this)) a->play("frostbite", 1, false);
}

HitResult FrostBite::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    r.injury = this;
    r.touched = kit::inInjury(*this, touch);
    if (!takesTool(tool->type)) {
        if (r.touched) {
            // the wrong tool on it: the tools freeze for 5 s
            InjuryEvent freeze;
            freeze.type = InjuryEvent::kEvent17;
            freeze.f58 = 5.0f;
            post(freeze);
            post(kit::effectEvent(7, 0, 0));  // FrozenTools
            post(kit::effectEvent(8, position.x, position.y));  // WhiteScreen
        }
        return r;
    }
    bool hit = kit::wearAll(*this, touch, tool);
    if (stage().useSizeRect && r.touched) hit = true;
    r.hit = hit;
    int done = 0, total = 0;
    kit::countPoints(*this, done, total);
    if (hit) {
        // fading as it thaws
        float o = total ? 1.0f - static_cast<float>(done) / static_cast<float>(total) : 1.0f;
        if (o < minOpacity) o = minOpacity;
        if (Animation* a = firstAnimation(*this)) a->setOpacity(o * 255.0f);
    }
    if (hit && done == total) {
        r.stageDone = true;
        kit::comboFor(*this, r);
        finishStage();
    }
    return r;
}

// ---- Boil

Boil::Boil() {
    priority = 2;
    sendsGraphicsEvent = false;
    m_fd94 = false;
}

void Boil::start() {
    // (its one piece, its rect about its place: not used by the port's
    // screen; no stage events)
    if (hasStage()) state = stage().state;
}

void Boil::vf3() {
    m_flag58 = true;
    if (Animation* a = firstAnimation(*this)) {
        a->play("grow", -1, false);
        a->setScale(size);
    }
}

void Boil::vf4(float) {
    if (poppedAt < size) SoundMgr::sharedSoundMgr()->playOnChannel("boil_loop", soundChannel, true, false, false, false);
    Animation* a = firstAnimation(*this);
    if (!bursting) {
        size += growth;
        if (a) a->setScale(size);
        if (burstAt <= size) {
            // grown too big: it bursts by itself, hurting
            bursting = true;
            hasDependencies = true;
            burst();
            post(kit::damageEvent(5));
        }
    } else if (!a || !a->isPlaying()) {
        // its burst over
        finished = true;
    }
}

void Boil::burst() {
    // (0x89f8c) a cut, and poison around it (more the bigger it was)
    post(kit::createEvent("LargeCut", position.x, position.y, static_cast<float>(randomInt(0, 359))));
    post(kit::createEvent("PoisonArea", position.x + 25, position.y + 25, static_cast<float>(randomInt(0, 359))));
    if (poison2At <= size)
        post(kit::createEvent("PoisonArea", position.x - 25, position.y - 25, static_cast<float>(randomInt(0, 359))));
    if (poison3At <= size)
        post(kit::createEvent("PoisonArea", position.x + 25, position.y - 25, static_cast<float>(randomInt(0, 359))));
    if (poison4At <= size)
        post(kit::createEvent("PoisonArea", position.x - 25, position.y + 25, static_cast<float>(randomInt(0, 359))));
    if (Animation* a = firstAnimation(*this)) a->play("burst", 1, false);
    SoundMgr::sharedSoundMgr()->playOnChannel("leech_burned_pop", soundChannel, false, true, false, false);
}

int Boil::toolFor(const CCPoint& touch) {
    // (vf27: the syringe on it; nothing if the tool asking is the cutter)
    if (hasStage() && stage().useSizeRect && kit::inInjury(*this, touch)) return kInjection;
    return kNoTool;
}

void Boil::completeStage() {
    burst();
    bursting = true;
    hasDependencies = true;
    size = 0;
}

HitResult Boil::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) cut open: it bursts
    HitResult r;
    if (bursting || !takesTool(tool->type)) return r;
    r.hit = kit::wearAll(*this, touch, tool);
    if (kit::allAreasDone(*this) && r.hit) {
        r.stageDone = true;
        kit::comboFor(*this, r);
        bursting = true;
        hasDependencies = true;
        burst();
    }
    if (r.hit) r.injury = this;
    return r;
}

HitResult Boil::injectionHitTest(const CCPoint& touch, int mode, float fill) {
    // (vf9) drawn out by the syringe: smaller, and done once small enough
    HitResult r;
    if (bursting || !takesTool(kInjection) || !kit::inInjury(*this, touch)) return r;
    if (lastFill == -1.0f) lastFill = fill;
    size -= std::max(0.0f, (lastFill - fill) / injectionDivisor);
    if (mode == 0) {
        r.hit = true;
        r.injury = this;
        if (size <= poppedAt) {
            SoundMgr::sharedSoundMgr()->stopChannel(soundChannel, false);
            finishStage();
            kit::comboFor(*this, r);
            InjuryEvent e;
            e.type = InjuryEvent::kEvent8;
            post(e);
        }
    }
    lastFill = fill;
    return r;
}

}  // namespace mt
