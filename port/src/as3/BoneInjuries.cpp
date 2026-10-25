#include "as3/BoneInjuries.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "as3/Animation.h"
#include "as3/ClampsController.h"
#include "as3/Effect.h"
#include "as3/InjuryKit.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr int kBroken = 33, kClamped = 37;

CCSprite* half(const InjuryState& i, size_t piece, const CCPoint& at, float turn) {
    CCSprite* sp = CCSprite::create();
    if (piece < i.def->pieces.size())
        if (CCSpriteFrame* f =
                CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(i.def->pieces[piece].sprite.c_str()))
            sp->setDisplayFrame(f);
    sp->setPosition(at);
    sp->setRotation(turn);
    return sp;
}

// (traced for test scripts: what a player sees of each half: its middle, its
// turn and the turn it is in line at (clockwise from up, on the screen), its
// length and width; cocos points)
void traceHalves(const InjuryState& i, CCNode* halves, const std::vector<CCSprite*>& sprites, float t1, float t2) {
    if (!halves || sprites.size() < 2) return;
    for (int k = 0; k < 2; ++k) {
        CCSprite* sp = sprites[static_cast<size_t>(k)];
        const CCPoint c = halves->convertToWorldSpace(sp->getPosition());
        const float turn = halves->getRotation();
        log("injury #%d half %d at %.0f,%.0f turn %.1f to %.1f size %.0f,%.0f", i.soundChannel, k, c.x, c.y,
            sp->getRotation() + turn, (k == 0 ? t1 : t2) + turn, sp->getContentSize().width,
            sp->getContentSize().height);
    }
}

// a touch (in the halves' node) on a half, its picture's rect turned with it
bool onHalf(CCSprite* sp, const CCPoint& local) {
    const CCPoint c = sp->getPosition();
    const CCSize sz = sp->getContentSize();
    CCRect r = CCRectMake(c.x - sz.width * 0.5f, c.y - sz.height * 0.5f, sz.width, sz.height);
    return kit::inTurnedRect(local, r, std::floor(sp->getRotation()), c);
}

// the turn that points a half from its middle at the touch (clockwise from up)
float turnTo(CCSprite* sp, const CCPoint& local) {
    const CCPoint c = sp->getPosition();
    return static_cast<float>(std::atan2(local.y - c.y, local.x - c.x) * -57.29577951308232 + 90.0);
}

Animation* firstAnimation(InjuryState& i) {
    return i.animations.empty() ? nullptr : static_cast<Animation*>(i.animations[0]);
}

void playIfNot(Animation* a, const char* name) {
    if (!a) return;
    const char* cur = a->currentName();
    if (!cur || std::strcmp(cur, name) != 0) a->play(name, -1, false);
}

// a spurt as open as `open` (1 open, 0 clamped shut)
void spurtAt(ParticleSystem* p, float open, float emission, float total, float speed, float minSpeed) {
    if (!p) return;
    p->setEmissionRate(open * emission);
    p->setTotalParticles(static_cast<unsigned>(open * total));
    p->setSpeed(std::max(open * speed, minSpeed));
}

float openness(const Clamp* c) { return c ? 1.0f - c->tightness / c->maxTightness : 1.0f; }

}  // namespace

// ---- BrokenBone

BrokenBone::BrokenBone() {
    zOrder = 50;
    m_fd94 = false;
    sendsGraphicsEvent = true;
}

void BrokenBone::start() {
    if (hasStage()) state = stage().state;
}

void BrokenBone::vf3() {
    // its two halves (its pieces' pictures), a little off line
    m_flag58 = true;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    halves = CCNode::create();
    halves->setAnchorPoint(ccp(0.5f, 0.5f));
    halves->setPosition(position);
    const int sc = static_cast<int>(scatter);
    CCSprite* a = half(*this, 0, ccp(1.5f * s, -341.0f * s), static_cast<float>(randomInt(-sc, sc)) + target1);
    halves->addChild(a);
    sprites.push_back(a);
    CCSprite* b = half(*this, 1, ccp(0, 291.0f * s), static_cast<float>(randomInt(-sc, sc)) + target2);
    halves->addChild(b);
    sprites.push_back(b);
    halves->setRotation(rotation);
    spritesNode->addChild(halves);
    traceHalves(*this, halves, sprites, target1, target2);
}

void BrokenBone::placeNodes() {
    if (halves) halves->setPosition(position);
    for (HitAreaState& a : areas)
        if (!a.done)
            for (HitPointState& p : a.points)
                if (p.marker) p.marker->setPosition(p.position);
}

HitResult BrokenBone::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    if (!takesTool(tool->type)) return r;
    if (stage().state == kBroken) {
        if (tool->type != kTongs) return r;  // (the original wore its (absent) areas then)
        if (tool->target && tool->target != this) return r;
        if (!halves || sprites.size() < 2) return r;
        const CCPoint local = halves->convertToNodeSpace(touch);
        // a half in the tongs turns to point at the touch, within its range
        CCSprite* turned = nullptr;
        float target = 0;
        if (onHalf(sprites[0], local)) {
            r.injury = this;
            turned = sprites[0];
            target = target1;
        } else if (onHalf(sprites[1], local)) {
            turned = sprites[1];
            target = target2;
        }
        // (fixed: off both halves the tongs are kept, only the in-line check
        // below is made, as 0x1a59c0; the port put the tongs down there, so
        // a pull slipping off its half dropped them)
        if (turned) {
            const float a = turnTo(turned, local);
            if (!(target - scatter < a && a < target + scatter)) {
                r.deselectTool = true;
                return r;
            }
            turned->setRotation(a);
            traceHalves(*this, halves, sprites, target1, target2);
            r.hit = true;
            r.injury = this;
        }
        // both in line: set
        const float a0 = sprites[0]->getRotation(), a1 = sprites[1]->getRotation();
        if (target1 - tolerance < a0 && a0 < target1 + tolerance && target2 - tolerance < a1 &&
            a1 < target2 + tolerance) {
            r.deselectTool = true;
            sprites[0]->setRotation(target1);
            sprites[1]->setRotation(target2);
            r.stageDone = true;
            kit::comboFor(*this, r);
            finishStage();
            SoundMgr::sharedSoundMgr()->playRandomSound("limb_replacement", 2, false);
        }
        return r;
    }
    if (tool->type == kTongs) return r;
    // the lighter, the cream: its areas
    r.hit = kit::wearAll(*this, touch, tool);
    if (kit::allAreasDone(*this) && r.hit) {
        r.stageDone = true;
        kit::comboFor(*this, r);
        finishStage();
    }
    if (r.hit) r.injury = this;
    return r;
}

void BrokenBone::completeStage() {
    if (sprites.size() >= 2) {
        sprites[0]->setRotation(target1);
        sprites[1]->setRotation(target2);
    }
    kit::markAreasDone(*this);
    finishStage();
}

// ---- BrokenVein

BrokenVein::BrokenVein() {
    priority = 2;
    m_fd94 = true;
    sendsGraphicsEvent = false;
    zOrder = 51;
}

BrokenVein::~BrokenVein() {
    kit::dropParticles(spurt1);
    kit::dropParticles(spurt2);
}

void BrokenVein::start() {
    if (hasStage()) state = stage().state;
    for (ParticleSystem** p : {&spurt1, &spurt2}) {
        *p = kit::particles(def->passiveFxRef, CCPointZero);
        if (!*p) continue;
        (*p)->setDuration(-1);
        (*p)->setEmissionRate(emission);
        (*p)->setTotalParticles(static_cast<unsigned>(total));
        (*p)->setSpeed(speed);
    }
}

namespace {

// a half's end (its middle plus a part of its height along its turn, in
// the halves' node): `div` 2.2 for the spurt, 2.5/3.5 for the clamp point
CCPoint endOf(CCSprite* sp, double div) {
    const double a = sp->getRotation() * 0.017453292519943295;
    const double h = sp->getContentSize().height / div;
    const CCPoint c = sp->getPosition();
    return ccp(static_cast<float>(c.x + std::cos(a) * 0.0 + h * std::sin(a)),
               static_cast<float>(c.y + std::sin(a) * 0.0 + std::cos(a) * h));
}

}  // namespace

void BrokenVein::vf3() {
    m_flag58 = true;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    halves = CCNode::create();
    halves->setAnchorPoint(ccp(0.5f, 0.5f));
    halves->setPosition(position);
    const int sc = static_cast<int>(range);
    CCSprite* a = half(*this, 0, ccp(0, -282.0f * s), static_cast<float>(randomInt(-sc, sc)) + target1);
    sprites.push_back(a);
    CCSprite* b = half(*this, 1, ccp(0, 190.0f * s), static_cast<float>(randomInt(-sc, sc)) + target2);
    halves->addChild(a);
    halves->addChild(b);
    sprites.push_back(b);
    halves->setRotation(rotation);
    spritesNode->addChild(halves);
    // the spurts at the halves' ends, turned with them
    if (spurt1) {
        spurt1->setPosition(endOf(a, 2.2));
        spurt1->setRotation(a->getRotation());
        kit::reparent(halves, spurt1);
    }
    if (spurt2) {
        spurt2->setPosition(endOf(b, 2.2));
        spurt2->setRotation(b->getRotation());
        kit::reparent(halves, spurt2);
    }
}

void BrokenVein::placeNodes() {
    if (halves) halves->setPosition(position);
}

void BrokenVein::update(float dt) {
    // (vf5) its damage, less the tighter its clamps (while broken)
    damageNow = 0;
    if (!hasStage()) return;
    damageTimer += dt;
    if (damageTimer < stage().damageTimer) return;
    damageTimer = 0;
    float f = 2.0f;
    if (!clamps.empty()) {
        f -= clamps[0]->tightness / clamps[0]->maxTightness;
        if (clamps.size() >= 2) f -= clamps[1]->tightness / clamps[1]->maxTightness;
    }
    damageNow = stage().damage * (stage().state == kBroken ? f : 1.0f);
}

float BrokenVein::sway(CCSprite* sp, bool closed, bool held, float& timer, bool& dir, float open, float target,
                       float dt) {
    // a half not clamped nor held sways, by its openness, every swayEvery s
    // (but when both halves are about in line)
    if (closed) {
        timer = 0;
        return sp->getRotation();
    }
    if (held) return sp->getRotation();
    const float r0 = sprites[0]->getRotation(), r1 = sprites[1]->getRotation();
    const bool near0 = target1 + tolerance * -1.3f < r0 && r0 < target1 + tolerance * 1.3f;
    const bool near1 = target2 + tolerance * -1.3f < r1 && r1 < target2 + tolerance * 1.3f;
    float turn;
    if (near0 && near1) {
        dir = false;
        turn = sp->getRotation();
    } else {
        timer += dt;
        if (swayEvery <= timer) {
            timer = 0;
            dir = !dir;
        }
        turn = dir ? sp->getRotation() + open : sp->getRotation() - open;
    }
    if (target - range < turn && turn < target + range) sp->setRotation(turn);
    return turn;
}

void BrokenVein::vf4(float dt) {
    if (!hasStage() || stage().state != kBroken || sprites.size() < 2 || !halves) return;
    if (CCDirector::sharedDirector()->getTotalFrames() % 10 == 0) traceHalves(*this, halves, sprites, target1, target2);
    bothClamped = false;
    if (spurt1) spurt1->setPosition(endOf(sprites[0], 2.2));
    if (spurt2) spurt2->setPosition(endOf(sprites[1], 2.2));
    std::vector<HitAreaState*> clampAreas = areasOf(kBroken, true);
    // each end's clamp: shut, it stems its spurt (the first time, the good
    // popup; after, the coins one); taken off, it hurts
    float opens[2] = {1, 1};
    bool closed[2] = {false, false};
    float* last[2] = {&open1, &open2};
    bool* popup[2] = {&popup1, &popup2};
    ParticleSystem* spurts[2] = {spurt1, spurt2};
    Clamp** held[2] = {&clamp1, &clamp2};
    for (int k = 0; k < 2; ++k) {
        Clamp* c = nullptr;
        if (k < static_cast<int>(clampAreas.size()) && !clampAreas[k]->points.empty()) c = clampAreas[k]->points[0].clamp;
        *held[k] = c;
        if (c) {
            opens[k] = openness(c);
            if (opens[k] == 0) {
                closed[k] = true;
                if (*last[k] != 0 && *last[k] != -1) {
                    if (!*popup[k]) {
                        kit::postGoodPopup(*this);
                        *popup[k] = true;
                    } else {
                        InjuryEvent e;
                        e.type = InjuryEvent::kTagPopup;
                        e.kind = 5;
                        e.useTouch = true;
                        post(e);
                    }
                }
            }
        } else if (*last[k] == 0) {
            kit::postHurt(*this, hurt);
        }
        spurtAt(spurts[k], opens[k], emission, total, speed, minSpeed);
        // its clamp point (and clamp) at the half's end
        if (k < static_cast<int>(clampAreas.size()) && !clampAreas[k]->points.empty()) {
            const CCPoint w = halves->convertToWorldSpace(endOf(sprites[k], k == 0 ? 2.5 : 3.5));
            HitPointState& p = clampAreas[k]->points[0];
            p.position = ccp(std::floor(w.x), std::floor(w.y));
            if (CCDirector::sharedDirector()->getTotalFrames() % 10 == 0)  // (for test scripts)
                log("injury #%d clamp point %d at %.0f,%.0f%s", soundChannel, k, p.position.x, p.position.y,
                    c ? (closed[k] ? " clamped" : " loose") : "");
            const float d = p.def->radius * ResolutionMgr::sharedResolutionMgr()->canvasScale;
            p.rect = CCRectMake(p.position.x - d * 0.5f, p.position.y - d * 0.5f, d, d);
            if (p.marker) p.marker->setPosition(p.position);
            if (c && c->node) c->node->setPosition(w);
        }
    }
    sway(sprites[0], closed[0], held1, swayTimer1, swayDir1, opens[0], target1, dt);
    sway(sprites[1], closed[1], held2, swayTimer2, swayDir2, opens[1], target2, dt);
    if (closed[0] && closed[1]) bothClamped = true;
    open1 = opens[0];
    open2 = opens[1];
}

HitResult BrokenVein::hitTest(const CCPoint& touch, ToolState* tool) {
    held1 = held2 = false;
    HitResult r;
    if (!takesTool(tool->type)) return r;
    if (stage().state == kBroken) {
        if (tool->type != kTongs) {
            // (the clamp's own test is clampHitTest; the clamp areas are not worn here)
            return r;
        }
        if (tool->target && tool->target != this) return r;
        if (!halves || sprites.size() < 2) return r;
        const CCPoint local = halves->convertToNodeSpace(touch);
        CCSprite* turned = nullptr;
        float target = 0;
        if (onHalf(sprites[0], local)) {
            swayTimer1 = 0;
            held1 = true;
            turned = sprites[0];
            target = target1;
        } else if (onHalf(sprites[1], local)) {
            swayTimer2 = 0;
            held2 = true;
            turned = sprites[1];
            target = target2;
        }
        if (turned) {
            const float a = turnTo(turned, local);
            if (!(target - range < a && a < target + range)) {
                r.deselectTool = true;
                return r;
            }
            turned->setRotation(a);
            if (turned == sprites[0]) {
                swayTimer1 = 0;
                lastAngle = a;
            }
            r.injury = this;
            r.hit = true;
        }
        // both in line and clamped: set
        const float a0 = sprites[0]->getRotation(), a1 = sprites[1]->getRotation();
        if (target1 - tolerance < a0 && a0 < target1 + tolerance && target2 - tolerance < a1 &&
            a1 < target2 + tolerance && bothClamped) {
            r.deselectTool = true;
            sprites[0]->setRotation(target1);
            sprites[1]->setRotation(target2);
            r.stageDone = true;
            kit::postGoodPopup(*this);
            finishStage();
            SoundMgr::sharedSoundMgr()->playRandomSound("limb_replacement", 2, false);
        }
        return r;
    }
    if (tool->type == kTongs) return r;
    r.hit = kit::wearAll(*this, touch, tool);
    if (kit::allAreasDone(*this) && r.hit) {
        r.stageDone = true;
        kit::postGoodPopup(*this);
        finishStage();
    }
    if (r.hit) r.injury = this;
    return r;
}

void BrokenVein::completeStage() {
    kit::markAreasDone(*this);
    bothClamped = true;
    if (hasStage() && stage().state == kBroken && sprites.size() >= 2) {
        sprites[0]->setRotation(target1);
        sprites[1]->setRotation(target2);
        spurtAt(spurt1, 0, emission, total, speed, 0);
        spurtAt(spurt2, 0, emission, total, speed, 0);
    }
    finishStage();
}

void BrokenVein::toolStopped(int) {
    lastAngle = -1;
    held1 = held2 = false;
}

void BrokenVein::removeClamp(Clamp* c) {
    // (vf24) a clamp taken off: spurting again, it hurts, and it is broken again
    if (hasStage() && stage().state == kClamped) {
        if (!stage().tools.empty() && stage().tools[0] == kClamp) {
            kit::postGoodPopup(*this);
            if (clamps.size() == 1) finishStage();
        }
    } else {
        spurtAt(spurt1, 1, emission, total, speed, minSpeed);
        spurtAt(spurt2, 1, emission, total, speed, minSpeed);
        kit::postHurt(*this, hurt);
        while (m_stage > 1) {
            const int before = m_stage;
            backToPreviousStage();
            if (m_stage == before) break;
        }
    }
    InjuryState::removeClamp(c);
}

int BrokenVein::toolFor(const CCPoint& touch) {
    if (!hasStage()) return kNoTool;
    if (!bothClamped || stage().state == kClamped) {
        for (HitAreaState* a : areasOf(stage().state, true))
            if (a->def->forClamp && wear(*a, touch, 0, true, true)) return kClamp;
    } else {
        for (HitAreaState* a : areasOf(stage().state, false))
            if (wear(*a, touch, 0, true, true)) return stage().tools.empty() ? kNoTool : stage().tools[0];
    }
    if (halves && sprites.size() >= 2) {
        const CCPoint local = halves->convertToNodeSpace(touch);
        if (onHalf(sprites[0], local) || onHalf(sprites[1], local)) return kTongs;
    }
    return kNoTool;
}

void BrokenVein::pause() {
    kit::pauseParticles(spurt1);
    kit::pauseParticles(spurt2);
}

void BrokenVein::resume() {
    kit::resumeParticles(spurt1);
    kit::resumeParticles(spurt2);
}

// ---- Artery

Artery::Artery() {
    // (0x908c4 case 1)
    priority = 2;
    sendsGraphicsEvent = true;
    m_fd94 = true;
    zOrder = 50;
}

Artery::~Artery() { kit::dropParticles(spurt); }

void Artery::start() {
    // (its spurt only: no stage events nor graphics event)
    if (hasStage()) state = stage().state;
    // (Android 1.43, Artery::onCreated 0x328288) the Hospital's coffeeartery
    // (child class 147) spurts coffee
    spurt = kit::particles(def->classIndex == 147 ? "particle_coffeeFX" : "particle_arteryFX", position);
    if (spurt) {
        spurt->setDuration(-1);
        spurt->setEmissionRate(emission);
        spurt->setTotalParticles(static_cast<unsigned>(total));
        spurt->setSpeed(speed);
    }
}

void Artery::vf3() {
    m_flag58 = true;
    if (Animation* a = firstAnimation(*this)) a->play("static", -1, false);
}

void Artery::update(float dt) {
    // (vf5) its damage, less the tighter its clamp
    damageNow = 0;
    if (!hasStage()) return;
    damageTimer += dt;
    if (damageTimer < stage().damageTimer) return;
    damageTimer = 0;
    damageNow = openness(clamps.empty() ? nullptr : clamps[0]) * stage().damage;
}

void Artery::vf4(float) {
    Animation* a = firstAnimation(*this);
    if (m_stage == 3) playIfNot(a, "burnt");
    else if (m_stage == 4) playIfNot(a, "healed");
    if (spurt && spurt->getParent() != topNode) kit::reparent(topNode, spurt);
    if (m_stage >= 4) return;
    if (clamps.empty()) {
        playIfNot(a, "static");
        return;
    }
    Clamp* c = clamps[0];
    if (c->maxTightness <= c->tightness) {
        // clamped shut: stemmed; the first stage done (the good popup the
        // first time, the coins one after)
        if (spurt) spurt->setEmissionRate(0);
        if (m_stage == 1) {
            if (!popupGiven) {
                kit::postGoodPopup(*this);
                popupGiven = true;
            } else {
                InjuryEvent e;
                e.type = InjuryEvent::kTagPopup;
                e.kind = 5;
                e.useTouch = true;
                post(e);
            }
            finishStage();
        }
        return;
    }
    if (m_stage > 1) {
        // loosened after it was shut: hurt, and open again
        kit::postHurt(*this, static_cast<int>(stage().damage));
        backToPreviousStage();
    }
    spurtAt(spurt, openness(c), emission, total, speed, minSpeed);
}

void Artery::completeStage() {
    spurtAt(spurt, 0, emission, total, speed, 0);
    kit::markAreasDone(*this);
    finishStage();
}

void Artery::onFinished() {
    InjuryState::onFinished();
    if (Animation* a = firstAnimation(*this)) a->play("healed", -1, false);
}

void Artery::removeClamp(Clamp* c) {
    // (vf24) a clamp taken off before it is healed: open again, it hurts
    if (hasStage() && !stage().tools.empty() && stage().tools[0] == kClamp && stage().state == kClamped) {
        kit::postGoodPopup(*this);
        finishStage();
    }
    if (hasStage() && stage().state != kClamped) {
        spurtAt(spurt, 1, emission, total, std::max(speed, minSpeed), minSpeed);
        kit::postHurt(*this, hurt);
        while (m_stage > 1) {
            const int before = m_stage;
            backToPreviousStage();
            if (m_stage == before) break;
        }
    }
    InjuryState::removeClamp(c);
}

int Artery::toolFor(const CCPoint& touch) {
    if (!hasStage()) return kNoTool;
    const InjuryStageDef& st = stage();
    if ((!st.tools.empty() && st.tools[0] == kClamp) || st.state == kClamped) {
        for (HitAreaState* a : areasOf(st.state, true))
            if (wear(*a, touch, 0, true, true)) return kClamp;
        if (st.state == kClamped) return kClamp;
    }
    for (HitAreaState* a : areasOf(st.state, false))
        if (wear(*a, touch, 0, true, true)) return st.tools.empty() ? kNoTool : st.tools[0];
    if (st.useSizeRect && kit::inInjury(*this, touch)) return st.tools.empty() ? kNoTool : st.tools[0];
    return kNoTool;
}

void Artery::pause() { kit::pauseParticles(spurt); }

void Artery::resume() { kit::resumeParticles(spurt); }

}  // namespace mt
