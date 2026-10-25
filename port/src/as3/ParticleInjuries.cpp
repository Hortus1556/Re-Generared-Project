#include "as3/ParticleInjuries.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as3/Animation.h"
#include "as3/Effect.h"
#include "as3/InjuryKit.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/ToolState.h"
#include "as3/Tools2.h"

namespace mt {

using namespace cocos2d;

namespace {

// a float in lo..hi (0x644cc)
float randomFloat(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / (2147483647.0f / (hi - lo));
}

// a touch (screen points) on the 640x384 canvas the injuries' data use
CCPoint onCanvas(const CCPoint& p) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    return ccp(p.x / s + (640.0f - r->width / s) * 0.5f, p.y / s + (384.0f - r->height / s) * 0.5f);
}

// The direction of a cut drawn from `from` to `to` (canvas points), as a
// turn clockwise from up (vf8 of the poisons: acos of its rise, 180 - that
// going left)
float cutTurn(const CCPoint& from, const CCPoint& to) {
    const float dx = to.x - from.x, dy = to.y - from.y;
    float a = radToDeg(std::acos(dy / std::sqrt(dx * dx + dy * dy)));
    if (to.x < from.x) a = 180.0f - a;
    return a;
}

void playChannel(InjuryState& i, const char* name, bool loop, bool replace) {
    SoundMgr::sharedSoundMgr()->playOnChannel(name, i.soundChannel, loop, replace, false, false);
}

// The drops a dead particle leaves: a PassiveParticle showing the class's
// passive sprite, or a SoakableParticle showing its soakable one, soaking
// in after `delay` seconds and making `makes` (vf4 of the poisons)
InjuryEvent dropEvent(const InjuryState& i, const tCCParticle& p, bool soakable, float delay,
                      const std::vector<std::string>& makes) {
    InjuryEvent e = kit::createEvent(soakable ? "SoakableParticle" : "PassiveParticle", i.position.x + p.pos.x,
                                     i.position.y + p.pos.y, 0);
    e.frame = soakable ? i.def->soakableSpriteRef : i.def->passiveSpriteRef;
    e.delay = delay;
    e.names = makes;
    return e;
}

void postDrops(InjuryState& i, ParticleSystem* p, bool soakable, float delay, const std::vector<std::string>& makes) {
    if (!p) return;
    for (const tCCParticle& d : p->getDeadParticles()) i.post(dropEvent(i, d, soakable, delay, makes));
    p->clearDeadParticles();
}

// the systems still showing something
bool anyRunning(std::initializer_list<ParticleSystem*> list) {
    for (ParticleSystem* p : list)
        if (kit::running(p)) return true;
    return false;
}

void removeAnimation(InjuryState& i, size_t index) {
    if (index >= i.animations.size() || !i.animations[index]) return;
    i.animations[index]->removeFromParentAndCleanup(true);
    i.animations[index] = nullptr;
}

Animation* animationAt(InjuryState& i, size_t index) {
    return index < i.animations.size() ? static_cast<Animation*>(i.animations[index]) : nullptr;
}

// the landings of a chipped injury's chips outside it: a cut every `limit`
// (CementDrop's and SpikeBase's vf4)
void chipLandings(InjuryState& i, ParticleSystem* p, int& count, int limit, bool onlyIfAllowed) {
    if (!p) return;
    std::vector<tCCParticle>& dead = p->getDeadParticles();
    if (!onlyIfAllowed || i.allowNonEssentialCreation) {
        for (const tCCParticle& d : dead) {
            const CCPoint at = ccp(i.position.x + d.pos.x, i.position.y + d.pos.y);
            if (kit::inTurnedRect(at, i.rect, std::floor(i.rotation), i.position)) continue;
            if (++count < limit) continue;
            count = 0;
            i.post(kit::createEvent("SmallCut", at.x, at.y, static_cast<float>(randomInt(0, 359))));
            break;
        }
    }
    p->clearDeadParticles();
}

}  // namespace

// ---- Fire

Fire::Fire() {
    // (0x99210)
    pointHealth = 4;
    priority = 2;
    sendsGraphicsEvent = false;
    directions = {1, 2, 3, 4};
    emberCount = randomInt(0, 22);
    spreadTimer = static_cast<float>(randomInt(0, 22));
    spreadDistance = static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * 75.0f);
    zOrder = 50;
}

Fire::~Fire() {
    kit::dropParticles(flames);
    kit::dropParticles(embers);
}

void Fire::start() {
    InjuryState::start();
    flames = kit::particles("particle_fireFX", position);
    embers = kit::particles("particle_emberFX", position, true);
    if (embers) {
        embers->setDuration(-1);
        embers->setEmissionRate(2.0f);
    }
    spreadTimer = static_cast<float>(randomInt(0, static_cast<int>(spreadEvery * 0.5f)));
    emberCount = randomInt(0, emberLimit / 2);
}

void Fire::vf3() {
    m_flag58 = true;
    playChannel(*this, "fire_loop", true, false);
    kit::reparent(spritesNode, flames);
    kit::reparent(spritesNode, embers);
}

void Fire::vf4(float dt) {
    // smaller as its areas are sucked out (no smaller than minScale)
    std::vector<HitAreaState*> list = areasOf(40, false);  // FireEmitting
    float done = 0;
    for (HitAreaState* a : list)
        if (a->done) done += 1;
    float scale = list.empty() ? 1.0f : 1.0f - done / static_cast<float>(list.size());
    if (scale < minScale) scale = minScale;
    if (flames) flames->setScale(scale);
    spreadTimer += dt;
    if (spreadEvery <= spreadTimer) {
        spread();
        spreadTimer = static_cast<float>(randomInt(0, static_cast<int>(spreadEvery * 0.5f)));
    }
    if (!embers) return;
    // every emberLimit embers landed, a burn where the last fell
    std::vector<tCCParticle>& dead = embers->getDeadParticles();
    for (const tCCParticle& d : dead) {
        if (++emberCount < emberLimit) continue;
        post(kit::createEvent("Burn", position.x + d.pos.x, position.y + d.pos.y, 0));
        emberCount = 0;
        break;
    }
    embers->clearDeadParticles();
}

void Fire::spread() {
    // (0x8c010) to one of the sides it has not spread to yet
    if (directions.empty()) return;
    const int k = randomInt(0, static_cast<int>(directions.size()) - 1);
    const int dir = directions[k];
    directions.erase(directions.begin() + k);
    CCPoint at = CCPointZero;
    const float d = static_cast<float>(spreadDistance);
    switch (dir) {
    case 1: at = ccp(position.x + d, position.y); break;
    case 2: at = ccp(position.x - d, position.y); break;
    case 3: at = ccp(position.x, position.y - d); break;
    case 4: at = ccp(position.x, position.y + d); break;
    default: break;
    }
    post(kit::createEvent("Fire", at.x, at.y, 0));
}

void Fire::onFinished() { SoundMgr::sharedSoundMgr()->stopChannel(soundChannel, true); }

void Fire::pause() {
    kit::pauseParticles(embers);
    kit::pauseParticles(flames);
}

void Fire::resume() {
    kit::resumeParticles(embers);
    kit::resumeParticles(flames);
}

// ---- PassiveParticle, SoakableParticle

PassiveParticle::PassiveParticle() { sendsGraphicsEvent = false; }

void PassiveParticle::start() {
    // (only its reach: no stage events)
    if (hasStage()) state = stage().state;
    const float half = def->size.width * 0.5f;
    reachSq = half * half;
}

void PassiveParticle::vf4(float dt) {
    if (pull.x == 0 && pull.y == 0) return;
    // toward the vacuum, faster the nearer
    const float k = speed / ResolutionMgr::sharedResolutionMgr()->canvasScale * dt * (reachSq - distanceSq);
    position.x += pull.x * k;
    position.y += pull.y * k;
    if (!sprites.empty()) sprites[0]->setPosition(position);
}

int PassiveParticle::toolFor(const CCPoint& touch) {
    // (vf27: the vacuum near it; nothing if the tool asking is the cutter)
    distanceSq = ccpDistanceSQ(position, touch);
    return distanceSq < ResolutionMgr::sharedResolutionMgr()->canvasScale * 4000.0f ? kVacuum : kNoTool;
}

HitResult PassiveParticle::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    if (!takesTool(tool->type)) return r;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    distanceSq = ccpDistanceSQ(position, touch);
    if (distanceSq <= radius * s) {
        // sucked up
        r.hit = true;
        r.stageDone = true;
        kit::comboFor(*this, r);
        finishStage();
    } else {
        // the vacuum's suction (+0x520) reaches it: drawn toward it
        const float suction = tool->type == kVacuum ? static_cast<Vacuum*>(tool)->suction : 0;
        if (distanceSq < suction * s) {
            r.hit = true;
            const float dx = touch.x - position.x, dy = touch.y - position.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            pull = len > 0 ? ccp(dx / len, dy / len) : CCPointZero;
        } else {
            pull = CCPointZero;
        }
    }
    if (r.hit) r.injury = this;
    return r;
}

SoakableParticle::SoakableParticle() { sendsGraphicsEvent = false; }

void SoakableParticle::setSoakTime(float t) { soakTime = randomFloat(t, t + t / 2.5f); }

void SoakableParticle::vf4(float dt) {
    if (pull.x != 0 || pull.y != 0) {
        PassiveParticle::vf4(dt);
        return;
    }
    soakTime -= dt;
    if (soakTime > 0) return;
    // soaked in: what it makes, where it is
    for (const std::string& n : makes) post(kit::createEvent(n, position.x, position.y, 0));
    finishStage();
}

// ---- PoisonArea

PoisonArea::PoisonArea() {
    priority = 2;
    sendsGraphicsEvent = false;
}

PoisonArea::~PoisonArea() {
    kit::dropParticles(bubbles);
    kit::dropParticles(passive);
    kit::dropParticles(soakable);
}

void PoisonArea::start() {
    // (no stage events: its state only)
    if (hasStage()) state = stage().state;
    bubbles = kit::particles(def->bubbleFxRef, position);
    soakable = kit::particles(def->soakableFxRef, position, true);
    passive = kit::particles(def->passiveFxRef, position, true);
    kit::stop(passive);
    kit::stop(soakable);
}

void PoisonArea::vf3() {
    for (ParticleSystem* p : {bubbles, soakable, passive}) kit::reparent(spritesNode, p);
    kit::stop(soakable);
    kit::stop(passive);
    if (bubbles) bubbles->setRotation(rotation);
    m_flag58 = true;
    if (Animation* a = animationAt(*this, 0)) a->play("poison", -1, false);
    if (bubbles) {
        bubblePosVar = bubbles->getPosVar();
        bubbleTotal = static_cast<float>(bubbles->getTotalParticles());
        bubbleLifeVar = bubbles->getLifeVar();
        bubbleStartSize = bubbles->getStartSize();
        bubbleEndSize = bubbles->getEndSize();
    }
}

void PoisonArea::vf4(float) {
    if (!finished) playChannel(*this, "poison_loop", true, false);
    if (!hasStage() || stage().state != 15) return;  // PoisonAreaEmitting
    landDrops();
    if (!anyRunning({passive, soakable})) finishStage();
}

void PoisonArea::landDrops() {
    // its drops fallen: poison where they land (the soakable ones soak in
    // after 20 s, making this poison again)
    postDrops(*this, passive, false, 0, {});
    postDrops(*this, soakable, true, 20.0f, {def->name});
}

int PoisonArea::toolFor(const CCPoint& touch) {
    // (vf27: the syringe on it; nothing if the tool asking is the cutter)
    if (hasStage() && stage().useSizeRect && kit::inInjury(*this, touch)) return kInjection;
    return kNoTool;
}

void PoisonArea::burst(float cutRotation) {
    // its drops spill (as many as its size), a cut across it
    kit::reset(soakable);
    kit::reset(passive);
    if (soakable) soakable->setTotalParticles(static_cast<unsigned>(soakableDrops * scale));
    if (passive) passive->setTotalParticles(static_cast<unsigned>(passiveDrops * scale));
    post(kit::createEvent("SmallCut", position.x, position.y, cutRotation));
}

HitResult PoisonArea::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    if (!hasStage() || stage().state != 14) return r;  // PoisonAreaIdle
    r.touched = stage().useSizeRect && kit::inInjury(*this, touch);
    if (!takesTool(tool->type)) {
        r.injury = this;
        return r;
    }
    if (!r.touched) {
        swiping = false;
        return r;
    }
    r.hit = true;
    const CCPoint at = onCanvas(touch);
    if (!swiping) {
        swipeStart = at;
        swiping = true;
    } else if (static_cast<float>(swipeSq) <= ccpDistanceSQ(swipeStart, at)) {
        // cut through: it bursts
        const float turn = cutTurn(swipeStart, at);
        r.stageDone = true;
        kit::comboFor(*this, r);
        burst(turn);
        SoundMgr::sharedSoundMgr()->playOnChannel("poison_burst", soundChannel, false, true, false, false);
        removeAnimation(*this, 0);
        finishStage();
    }
    if (r.hit) r.injury = this;
    return r;
}

void PoisonArea::completeStage() {
    burst(0);
    removeAnimation(*this, 0);
    finishStage();
}

void PoisonArea::toolStopped(int) {
    swiping = false;
    lastFill = -1;
}

HitResult PoisonArea::injectionHitTest(const CCPoint& touch, int mode, float fill) {
    // (vf9) drawn out by the syringe: smaller, its bubbles fewer
    HitResult r;
    if (!takesTool(kInjection) || !kit::inInjury(*this, touch)) return r;
    if (lastFill == -1.0f) lastFill = fill;
    scale -= std::max(0.0f, (lastFill - fill) / injectionDivisor);
    if (Animation* a = animationAt(*this, 0)) a->setScale(scale);
    if (bubbles) {
        bubbles->setPosVar(ccp(bubblePosVar.x * scale, bubblePosVar.y * scale));
        bubbles->setTotalParticles(static_cast<unsigned>(bubbleTotal * scale));
        bubbles->setLifeVar(bubbleLifeVar * scale);
        bubbles->setStartSize(bubbleStartSize * scale);
        bubbles->setEndSize(bubbleEndSize * scale);
    }
    if (mode == 0) {
        r.hit = true;
        r.injury = this;
        if (scale <= minScale) {
            // all drawn out
            SoundMgr::sharedSoundMgr()->stopChannel(soundChannel, false);
            kit::finishNow(*this);
            kit::comboFor(*this, r);
            InjuryEvent e;
            e.type = InjuryEvent::kEvent8;
            post(e);
        }
    }
    lastFill = fill;
    return r;
}

// ---- SuperPoisonArea

SuperPoisonArea::SuperPoisonArea() {
    // (0x987bc)
    priority = 3;
    sendsGraphicsEvent = false;
}

SuperPoisonArea::~SuperPoisonArea() {
    for (ParticleSystem** p : {&soak1, &passive1, &soak2, &passive2, &soak3, &passive3, &bubbles})
        kit::dropParticles(*p);
}

void SuperPoisonArea::start() {
    if (hasStage()) state = stage().state;
    bubbles = kit::particles(def->bubbleFxRef, position);
    soak1 = kit::particles(def->soakableFxRef, position, true);
    passive1 = kit::particles(def->passiveFxRef, position, true);
    soak2 = kit::particles(def->soakableFxRef, position, true);
    passive2 = kit::particles(def->passiveFxRef, position, true);
    if (soak2) soak2->setDuration(-1);
    if (passive2) passive2->setDuration(-1);
}

void SuperPoisonArea::vf3() {
    m_flag58 = true;
    if (Animation* a = animationAt(*this, 1)) a->play("superpoison", -1, false);
    if (Animation* a = animationAt(*this, 0)) a->showFrame("veins", static_cast<int>(veinFrames));
    for (ParticleSystem* p : {soak2, passive2}) kit::reparent(spritesNode, p);
    kit::reset(soak2);
    kit::reset(passive2);
    if (soak2) soak2->setEmissionRate(soakRate);
    if (passive2) passive2->setEmissionRate(passiveRate);
    puff(puffAmount, position);
    for (ParticleSystem* p : {bubbles, soak1, passive1}) kit::reparent(spritesNode, p);
    if (bubbles) {
        bubblePosVar = bubbles->getPosVar();
        bubbleTotal = static_cast<float>(bubbles->getTotalParticles());
        bubbleLifeVar = bubbles->getLifeVar();
        bubbleStartSize = bubbles->getStartSize();
        bubbleEndSize = bubbles->getEndSize();
    }
}

void SuperPoisonArea::puff(float amount, const CCPoint& at) {
    // (0x19b1c4) a few drops at once
    kit::reset(soak1);
    kit::reset(passive1);
    if (passive1) passive1->setPosition(at);
    if (soak1) {
        soak1->setPosition(at);
        soak1->setTotalParticles(static_cast<unsigned>(soakDrops * (scale * amount)));
    }
    if (passive1) passive1->setTotalParticles(static_cast<unsigned>(passiveDrops * (scale * amount)));
}

void SuperPoisonArea::burst(float amount, const CCPoint& at) {
    // (0x19b2b0) its burst's drops, and the sound
    soak3 = kit::particles(def->soakableFxRef, position, true);
    passive3 = kit::particles(def->passiveFxRef, position, true);
    kit::reset(passive3);
    kit::reset(soak3);
    if (passive3) passive3->setPosition(at);
    if (soak3) {
        soak3->setPosition(at);
        soak3->setTotalParticles(static_cast<unsigned>(soakDrops * (scale * amount)));
    }
    if (passive3) passive3->setTotalParticles(static_cast<unsigned>(passiveDrops * (scale * amount)));
    for (ParticleSystem* p : {soak3, passive3}) kit::reparent(spritesNode, p);
    SoundMgr::sharedSoundMgr()->playOnChannel("poison_burst", soundChannel, false, true, false, false);
}

void SuperPoisonArea::landDrops(ParticleSystem* p, bool soakable, float delay) {
    // the soakable drops soak in as fire and poison
    postDrops(*this, p, soakable, delay, soakable ? std::vector<std::string>{"Fire", "PoisonArea"}
                                                  : std::vector<std::string>{});
}

void SuperPoisonArea::vf4(float) {
    if (!finished) playChannel(*this, "super_poison_loop", true, true);
    landDrops(passive2, false, 20.0f);
    landDrops(soak2, true, 1.0f);
    landDrops(passive1, false, 20.0f);
    landDrops(soak1, true, 1.0f);
    if (!hasStage() || stage().state != 15) return;  // PoisonAreaEmitting
    if (soak3) {
        landDrops(soak3, true, 1.0f);
        if (passive3) landDrops(passive3, false, 20.0f);
    }
    if (!anyRunning({passive1, soak1, passive2, soak2, soak3, passive3})) finishStage();
}

int SuperPoisonArea::toolFor(const CCPoint& touch) {
    if (hasStage() && stage().useSizeRect && kit::inInjury(*this, touch)) return kInjection;
    return kNoTool;
}

void SuperPoisonArea::cutOpen(float cutRotation) {
    post(kit::createEvent("SmallCut", position.x, position.y, cutRotation));
    removeAnimation(*this, 0);
    removeAnimation(*this, 1);
    kit::stop(soak2);
    kit::stop(passive2);
    burst(burstAmount, position);
    puff(puffAmount, position);
    finishStage();
}

HitResult SuperPoisonArea::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    if (!hasStage() || stage().state != 14) return r;  // PoisonAreaIdle
    r.touched = stage().useSizeRect && kit::inInjury(*this, touch);
    if (!takesTool(tool->type)) {
        r.injury = this;
        return r;
    }
    if (!r.touched) {
        swiping = false;
        return r;
    }
    r.hit = true;
    const CCPoint at = onCanvas(touch);
    if (!swiping) {
        swipeStart = at;
        swiping = true;
    } else if (static_cast<float>(swipeSq) <= ccpDistanceSQ(swipeStart, at)) {
        const float turn = cutTurn(swipeStart, at);
        r.stageDone = true;
        kit::comboFor(*this, r);
        cutOpen(turn);
    }
    if (r.hit) r.injury = this;
    return r;
}

void SuperPoisonArea::completeStage() { cutOpen(0); }

// (1.45 0x2303d8) a super poison patch cured: stat 7 (StatsMgr.h)
void SuperPoisonArea::onFinished() { StatsMgr::sharedInstance()->incrementStat(StatTypes::kCureSuperPoisonPatch); }

void SuperPoisonArea::toolStopped(int) {
    swiping = false;
    lastFill = -1;
}

HitResult SuperPoisonArea::injectionHitTest(const CCPoint& touch, int mode, float fill) {
    // (vf9) drawn out: its veins go a frame at a time, its bubbles fewer
    HitResult r;
    if (!takesTool(kInjection) || !kit::inInjury(*this, touch)) return r;
    if (lastFill == -1.0f) lastFill = fill;
    scale = std::max(0.0f, scale - std::max(0.0f, (lastFill - fill) / injectionDivisor));
    const float frame = std::floor(scale / fullScale * veinFrames);
    if (Animation* a = animationAt(*this, 0)) a->showFrame("veins", static_cast<int>(frame));
    if (frame < veinLimit) {
        if (Animation* a = animationAt(*this, 1)) a->setScale(frame / veinLimit <= 1.0f ? shrunkScale : 1.0f);
        if (bubbles) {
            bubbles->setPosVar(ccp(bubblePosVar.x * scale, bubblePosVar.y * scale));
            bubbles->setTotalParticles(static_cast<unsigned>(bubbleTotal * scale));
            bubbles->setLifeVar(bubbleLifeVar * scale);
            bubbles->setStartSize(bubbleStartSize * scale);
            bubbles->setEndSize(bubbleEndSize * scale);
        }
    }
    if (mode == 0) {
        r.hit = true;
        r.injury = this;
        if (scale <= minScale) {
            removeAnimation(*this, 0);
            removeAnimation(*this, 1);
            kit::stop(soak2);
            kit::stop(passive2);
            kit::comboFor(*this, r);
            r.stageDone = true;
            InjuryEvent e;
            e.type = InjuryEvent::kEvent8;
            post(e);
            SoundMgr::sharedSoundMgr()->stopChannel(soundChannel, false);
            kit::finishNow(*this);
        }
    }
    lastFill = fill;
    return r;
}

void SuperPoisonArea::pause() {
    for (ParticleSystem* p : {soak1, passive1, soak2, passive2}) kit::pauseParticles(p);
}

void SuperPoisonArea::resume() {
    for (ParticleSystem* p : {soak1, passive1, soak2, passive2}) kit::resumeParticles(p);
}

// ---- the chipped ones

namespace {

// Their hit test (SuperPoisonScab vf8 0x19a454, CementDrop vf8 0x1bab14,
// SpikeBase vf8 0x1e0584): every area worn by the tool, the touch on its
// rect also counts; touched, it fades with its points (on its 4th stage,
// with a floor of `minOpacity`; minOpacity < 0: never) or chips (vf17); all
// its points done, the stage is (unless `endsStage` is false: a brimstone
// before its last stage, 1.45 0x25b198).
HitResult chipHitTest(InjuryState& i, const CCPoint& touch, ToolState* tool, float minOpacity, bool frameChange,
                      bool endsStage = true) {
    HitResult r;
    if (!i.takesTool(tool->type)) return r;
    bool touched = kit::wearAll(i, touch, tool);
    if (i.stage().useSizeRect && kit::inInjury(i, touch)) touched = true;
    if (touched) r.hit = true;
    int done = 0, total = 0;
    kit::countPoints(i, done, total);
    if (touched) {
        if (i.m_stage == 4 && minOpacity >= 0) {
            float o = total ? 1.0f - static_cast<float>(done) / static_cast<float>(total) : 1.0f;
            if (o < minOpacity) o = minOpacity;
            if (!i.sprites.empty()) i.sprites[0]->setOpacity(static_cast<GLubyte>(o * 255.0f));
        } else {
            r.useful = true;
            r.b1d = true;
            i.onTouch(touch);
        }
    }
    if (r.hit && done == total && endsStage) {
        r.stageDone = true;
        kit::comboFor(i, r);
        i.finishStage();
        if (frameChange && !i.finished && i.stage().needsGraphic && i.stageFound) {
            InjuryEvent e;
            e.type = InjuryEvent::kSetFrame;
            e.frame = i.frameName();
            e.sprite = i.sprites.empty() ? nullptr : i.sprites[0];
            i.post(e);
        }
    }
    if (r.hit) r.injury = &i;
    return r;
}

}  // namespace

SuperPoisonScab::SuperPoisonScab() {
    sendsGraphicsEvent = false;
    m_fd94 = false;
    m_fd95 = true;
    pointHealth = 3;
}

SuperPoisonScab::~SuperPoisonScab() { kit::dropParticles(chips); }

void SuperPoisonScab::start() {
    InjuryState::start();
    chips = kit::particles("particle_skabFX", position);
    kit::stop(chips);
}

void SuperPoisonScab::vf4(float) {
    if (!chipping) kit::stop(chips);
    chipping = false;
}

HitResult SuperPoisonScab::hitTest(const CCPoint& touch, ToolState* tool) {
    return chipHitTest(*this, touch, tool, minOpacity, false);
}

void SuperPoisonScab::onTouch(const CCPoint& touch) {
    playChannel(*this, "cement_chipping_loop", false, false);
    chipping = true;
    kit::addOnce(topNode, chips);
    if (chips && !chips->isActive()) chips->resetSystem();
    if (chips) chips->setPosition(touch);
}

CementDrop::CementDrop() {
    // (0x908c4 case 6; 1.45's 0xb8838 sends no graphics event any more, its
    // +0x10247 0: 1.02's +0xfd97 was 1, an area no injury is made over)
    sendsGraphicsEvent = false;
    m_fd94 = false;
    m_fd95 = true;
    pointHealth = 2;
}

CementDrop::~CementDrop() {
    kit::dropParticles(cement);
    kit::dropParticles(dust);
}

bool CementDrop::isBrimstone() const {
    static const int k = injuryClassIndex("Brimstone");  // (1.45's child 0x19)
    return def && def->classIndex == k;
}

bool CementDrop::isBarnacle() const {
    static const int k = injuryClassIndex("Barnacle");  // (1.45's child 0x18)
    return def && def->classIndex == k;
}

void CementDrop::start() {
    // (1.45 0x25a24c: the brimstone's and the barnacle's own chips; the
    // brimstone's first fire 6 s on)
    InjuryState::start();
    dust = kit::particles("particle_dustFX", position);
    const char* chips = "particle_cementFX";
    if (isBrimstone()) {
        chips = "particle_brimstoneFX";
        fireEvery = fireInterval;
        fireTimer = fireInterval - firstFireEarly;
    } else if (isBarnacle()) {
        chips = "particle_barnacleFX";
    }
    cement = kit::particles(chips, position, true);
    kit::stop(cement);
    kit::stop(dust);
    if (dust) dust->setEmissionRate(150.0f);
}

void CementDrop::vf3() {
    // (it does not set +0x58)
    kit::reparent(topNode, dust);
    kit::reparent(topNode, cement);
}

void CementDrop::vf4(float dt) {
    if (!chipping) {
        if (m_stage != 4) kit::stop(dust);
        kit::stop(cement);
    }
    chipping = false;
    if (!isBrimstone()) {
        chipLandings(*this, cement, landings, landingsForCut, true);
        return;
    }
    // (1.45 0x25a3a8) a brimstone: a Fire where it is every 12 s till its
    // last stage; its chips' landings counted a frame at a time, every
    // third leaving ash where the first landed, past 30 its next stage
    fireTimer += dt;
    if (fireEvery <= fireTimer && m_stage != 4) {
        fireTimer -= fireEvery;
        post(kit::createEvent("Fire", static_cast<float>(static_cast<int>(position.x)),
                              static_cast<float>(static_cast<int>(position.y)), 0));
    }
    if (!cement) return;
    std::vector<tCCParticle>& dead = cement->getDeadParticles();
    if (!dead.empty()) {
        ++landingFrames;
        if (landingFrames % ashEvery == 0) post(dropEvent(*this, dead[0], false, 0, std::vector<std::string>()));
        if (framesForStage < landingFrames && m_stage != 4) finishStage();
    }
    cement->clearDeadParticles();
}

HitResult CementDrop::hitTest(const CCPoint& touch, ToolState* tool) {
    // (1.45 0x25b198: its new picture is finishStage's now; a brimstone's
    // stages before the last end by its landings, not by its points)
    return chipHitTest(*this, touch, tool, minOpacity, false, m_stage == 4 || !isBrimstone());
}

void CementDrop::onTouch(const CCPoint& touch) {
    // (1.45 0x25b460: its systems restarted only once stopped; 1.02's
    // 0x1bb3e8 restarted them every frame first, so the dust's 300 a second
    // was never set)
    chipping = true;
    if (cement && !cement->isActive()) {
        kit::reset(cement);
        kit::reset(dust);
        if (dust) dust->setEmissionRate(300.0f);
    }
    if (cement) cement->setPosition(touch);
    if (dust) dust->setPosition(touch);
    playChannel(*this, "cement_chipping_loop", false, false);
}

void CementDrop::finishStage() {
    // (1.45 0x25b620: a brimstone's landings counted again, its fire's
    // interval set)
    landingFrames = 0;
    fireEvery = fireInterval;
    InjuryState::finishStage();
    if (m_stage == 4) {
        // the cement off: its last stage's points whole again, a cloud of dust
        SoundMgr::sharedSoundMgr()->playOnChannel("cement_removed", soundChannel, false, true, false, false);
        for (HitAreaState& a : areas)
            for (HitPointState& p : a.points) p.health = 1.0f;
        kit::reset(dust);
        if (dust) {
            dust->setEmissionRate(1200.0f);
            dust->setDuration(0.5f);
        }
    }
    // its new stage's picture (1.45: here, so a brimstone's own advance
    // shows it too; 1.02 posted it from vf8)
    if (!finished && hasStage() && stage().needsGraphic && stageFound) {
        InjuryEvent e;
        e.type = InjuryEvent::kSetFrame;
        e.frame = frameName();
        e.sprite = sprites.empty() ? nullptr : sprites[0];
        post(e);
    }
}

int CementDrop::toolFor(const CCPoint& touch) {
    // (1.45 0x25be40) on it: the chainsaw, the cream on its last stage
    if (!hasStage() || !stage().useSizeRect || !kit::inInjury(*this, touch)) return kNoTool;
    if (m_stage == 4) return kCream;
    return m_stage < 4 ? kChainsaw : kNoTool;
}

SpikeBase::SpikeBase() {
    // (0x908c4 case 0x24)
    zOrder = 2;
    sendsGraphicsEvent = true;
    m_fd94 = false;
    m_fd95 = true;
    pointHealth = 3;
}

SpikeBase::~SpikeBase() { kit::dropParticles(chips); }

void SpikeBase::start() {
    InjuryState::start();
    chips = kit::particles("particle_cementFX", position, true);
    kit::stop(chips);
}

void SpikeBase::vf3() {
    kit::reparent(topNode, chips);
}

void SpikeBase::vf4(float) {
    if (!chipping) kit::stop(chips);
    chipping = false;
    chipLandings(*this, chips, landings, landingsForCut, false);
}

HitResult SpikeBase::hitTest(const CCPoint& touch, ToolState* tool) {
    return chipHitTest(*this, touch, tool, -1.0f, true);
}

void SpikeBase::onTouch(const CCPoint& touch) {
    chipping = true;
    kit::reset(chips);
    if (chips && !chips->isActive()) {
        kit::reset(chips);
        chips->setEmissionRate(300.0f);
    }
    if (chips) chips->setPosition(touch);
}

}  // namespace mt
