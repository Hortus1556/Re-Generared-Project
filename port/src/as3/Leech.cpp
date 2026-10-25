#include "as3/Leech.h"

#include <algorithm>
#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/Log.h"
#include "as3/ParticleSystem.h"
#include "engine/cocos/CCDirector.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
float dist2(const CCPoint& a, const CCPoint& b) {  // (0x64584)
    float dx = b.x - a.x, dy = b.y - a.y;
    return dx * dx + dy * dy;
}
}  // namespace

Leech::Leech() {
    // (0x998f8)
    zOrder = 100;
    priority = 3;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    stretchUnit = s * 15.0f;
    pullDistance = static_cast<int>(s * 200.0f);
    pullPerFrame = static_cast<float>(pullDistance / stretchFrames);
    sendsGraphicsEvent = true;
}

Leech::~Leech() {
    if (poison) {
        poison->stopSystem();
        poison->removeFromParentAndCleanup(true);
        poison->release();
    }
}

void Leech::sizeRects() {
    // its rect rises from its mouth (its place); its head is the top
    // quarter of it, from 0.7 of its height
    float w = def->size.width * scale, h = def->size.height * scale;
    rect = CCRectMake(position.x - w * 0.5f, position.y, w, h);
    headRect = CCRectMake(position.x - w * 0.5f, position.y + h * 0.7f, w, h * 0.25f);
}

void Leech::start() {
    // (vf2: not the base's)
    burnt = false;
    bursting = false;
    // (at its full size here: vf4 sizes it the first frame)
    float w = def->size.width, h = def->size.height;
    rect = CCRectMake(position.x - w * 0.5f, position.y, w, h);
    headRect = CCRectMake(position.x - w * 0.5f, position.y + h * 0.7f, w, h * 0.25f);
    rotation0 = static_cast<int>(rotation);
    if (hasStage()) state = stage().state;
}

void Leech::vf3() {
    // (vf3) sucking, at its size
    if (poison) {
        poison->release();
        poison = nullptr;
    }
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0x12);
    state = kSucking;
    if (Animation* a = anim(*this)) {
        a->play("suck", -1, false);
        a->setScale(scale);
    }
}

void Leech::leaveTeeth() {
    // (0x17c28c)
    post(createEvent("LeechTeeth", position.x, position.y, rotation));
}

void Leech::burst() {
    // (0x17c624) poison where it was
    SoundMgr::sharedSoundMgr()->playOnChannel("incision", 3, soundChannel, false, true, false, false);
    if (!poison) {
        poison = ParticleSystem::create("particle_poisonFX");
        if (poison) {
            poison->setPosition(position);
            poison->retain();
            topNode->addChild(poison);
        }
    }
    post(createEvent("PoisonArea", position.x, position.y, static_cast<float>(randInt(0, 359))));
    if (Animation* a = anim(*this)) a->play("explode", 1, false);
    state = kBroken;
    bursting = true;
}

void Leech::vf4(float /*dt*/) {
    // (vf4) it sucks and grows; at its size, it bursts and is done once
    // its bursting is over
    Animation* a = anim(*this);
    if (state == kSucking)
        SoundMgr::sharedSoundMgr()->playOnChannel("leech_suck_loop", 2, soundChannel, true, false, false, false);
    if (bursting) {
        if (state == kBroken && a && !a->isPlaying()) {
            bursting = false;
            if (hasTeeth) leaveTeeth();
        }
        if (bursting) return;
    }
    sizeRects();
    // (for test scripts, what a player sees: its size, burnt or not)
    if (CCDirector::sharedDirector()->getTotalFrames() % 10 == 0)
        log("creature #%d Leech scale %.2f%s", soundChannel, scale, burnt ? " burnt" : "");
    if (dangling || stretching) return;
    if (!hasDependencies) {
        scale += growth;
        if (a) a->setScale(scale);
        if (burstScale <= scale) {
            hasDependencies = true;  // (+0x6700: no tool works on it any more)
            burst();
            post(damageEvent(5));
        }
    } else if (!a || !a->isPlaying()) {
        finished = true;
    }
}

HitResult Leech::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8)
    HitResult r;
    r.injury = this;
    r.touched = touchInRect(*this, touch);
    if (!takesTool(tool->type)) return r;
    const int t = tool->type;
    Animation* a = anim(*this);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    const bool cutting = t == kStapler || t == kCutter || t == kTazer || t == kChainsaw;
    if (r.touched && cutting) {
        // a cut: it bursts
        hasDependencies = true;
        burst();
        post(damageEvent(5));
        r.hit = true;
        return r;
    }
    const bool sizeRect = stage().useSizeRect;
    if (t == kTongs) {
        if (!stretching) {
            if (!sizeRect || dangling || !touchInRect(*this, touch, headRect)) return r;
            sound->playOnChannel("leech_stretch", soundChannel, false, true, false, false);
            stretching = true;
            r.hit = true;
            return r;
        }
        // pulled: stretched towards the tongs
        const float d2 = dist2(position, touch);
        const float head2 = dist2(headRect.origin, position);
        int frames = static_cast<int>(std::ceil((d2 / stretchUnit) / (pullPerFrame * pullPerFrame)));
        if (d2 < head2) frames = 1;
        if (a && a->frame() < frames + 1)
            sound->playOnChannel("leech_stretch", soundChannel, false, true, false, false);
        const float angle = std::atan2(touch.y - position.y, touch.x - position.x) * 57.29577951308232f;
        rotation = 90.0f - angle;
        setAnimRotation(*this, rotation);
        if (a) a->showFrame("stretch", std::min(frames, stretchFrames));
        r.hit = true;
        const float pull = static_cast<float>(pullDistance) * scale;
        if (d2 < pull * pull) return r;
        // off: its teeth stay in unless burnt first
        if (!burnt && hasTeeth) {
            sound->playOnChannel("leech_burned_tear", soundChannel, false, true, false, false);
            hasTeeth = false;
            leaveTeeth();
        } else {
            sound->playOnChannel("leech_burned_pop", soundChannel, false, true, false, false);
            hasTeeth = false;
        }
        r.piece = grabPiece(*this);
        if (a) a->play("dangle", -1, false);
        rotation = 0;
        setAnimRotation(*this, 0);
        dangling = true;
        stretching = false;
        return r;
    }
    if (!r.touched || !sizeRect) return r;
    r.hit = true;
    if (stage().state != kSucking || t != kLighter) return r;
    if (playing(a) == "burnt") return r;
    burnt = true;
    sound->playOnChannel("leech_burn", 2, soundChannel, false, true, false, false);
    if (a) {
        a->play("burnt", 1, false);
        a->queue("suck", -1, false);
    }
    return r;
}

HitResult Leech::injectionHitTest(const CCPoint& touch, int mode, float fill) {
    // (vf9) what the syringe gives it shrinks it
    HitResult r;
    if (!takesTool(kInjection) || !touchInRect(*this, touch)) return r;
    if (lastFill == -1.0f) lastFill = fill;
    const float shrink = std::max(0.0f, (lastFill - fill) / injectionDivisor);
    if (minScale <= scale - shrink) scale -= shrink;
    if (mode == 0) {
        r.hit = true;
        r.injury = this;
    }
    lastFill = fill;
    return r;
}

void Leech::completeStage() {
    // (vf18)
    burst();
    hasDependencies = true;
}

void Leech::onFinished() {
    // (vf20, 1.45 Leech::onHealed 0x207f44) pulled off whole: stat 11
    // (StatsMgr.h). Fixed: the original counted every leech pulled off
    // (+0x102a0, cleared by either pull), also one that tore and left its
    // teeth in; the port counts the clean pull alone, burnt first.
    if (!hasTeeth && burnt) StatsMgr::sharedInstance()->incrementStat(StatTypes::kRemoveLeechNotLeavingTeeth);
    SoundMgr::sharedSoundMgr()->stopChannel(soundChannel, false);
}

void Leech::backToSucking() {
    // (0x17d87c) let go of: it goes back to sucking where it was
    if (dangling && playing(anim(*this)) == "dangle" && hasTeeth) burnt = false;
    lastFill = -1;
    dangling = false;
    stretching = false;
    state = kSucking;
    SoundMgr::sharedSoundMgr()->playOnChannel("leech_suck_loop", 2, soundChannel, true, true, false, false);
    if (Animation* a = anim(*this)) a->play("suck", -1, false);
    rotation = static_cast<float>(rotation0);
    setAnimRotation(*this, rotation);
}

void Leech::toolStopped(int /*tool*/) {
    // (vf23)
    if (dangling || stretching) backToSucking();
}

int Leech::toolFor(const CCPoint& touch) {
    // (vf27) the lighter on it until burnt, then the tongs on its head
    if (touchInRect(*this, touch) && !burnt) return kLighter;
    if (touchInRect(*this, touch, headRect) || dangling || stretching) return kTongs;
    return kNoTool;
}

}  // namespace mt
