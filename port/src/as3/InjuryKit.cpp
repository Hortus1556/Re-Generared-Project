#include "as3/InjuryKit.h"

#include <cmath>

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/ToolState.h"

namespace mt {
namespace kit {

using namespace cocos2d;

bool inTurnedRect(const CCPoint& p, const CCRect& r, float rotation, const CCPoint& center) {
    // (0x799f0: a = (90 - (rotation + 90)) times -pi/180 (0x79af0), the touch
    // turned back by the rect's turn; the offset as ints. Fixed: the port's
    // first version had +pi/180, which turned it the wrong way, so the halves
    // of a broken bone or vein were missed when turned)
    const double a = (90.0 - (rotation + 90.0)) * -0.017453292519943295;
    const double dx = static_cast<int>(p.x - center.x), dy = static_cast<int>(p.y - center.y);
    const float x = center.x + static_cast<float>(dx * std::cos(a) - dy * std::sin(a));
    const float y = center.y + static_cast<float>(dx * std::sin(a) + dy * std::cos(a));
    return r.origin.x < x && x < r.origin.x + r.size.width && r.origin.y < y && y < r.origin.y + r.size.height;
}

InjuryEvent createEvent(const std::string& name, float x, float y, float rotation) {
    InjuryEvent e;
    e.type = InjuryEvent::kCreateInjury;
    e.name = name;
    e.x = static_cast<float>(static_cast<int>(x));
    e.y = static_cast<float>(static_cast<int>(y));
    e.rotation = static_cast<float>(static_cast<int>(rotation));
    return e;
}

InjuryEvent damageEvent(int amount) {
    InjuryEvent e;
    e.type = InjuryEvent::kDamage;
    e.damage = amount;
    return e;
}

InjuryEvent effectEvent(int kind, float x, float y) {
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = kind;
    e.x = static_cast<float>(static_cast<int>(x));
    e.y = static_cast<float>(static_cast<int>(y));
    return e;
}

void postGoodPopup(InjuryState& i) {
    InjuryEvent e;
    e.type = InjuryEvent::kTagPopup;
    e.kind = 0;
    e.useTouch = true;
    i.post(e);
}

void postHurt(InjuryState& i, int amount) {
    i.post(damageEvent(amount));
    InjuryEvent fx;
    fx.type = InjuryEvent::kEffect;
    fx.effect = 0;
    fx.useTouch = true;  // (the original left its place unset; the damage effect is the screen's flash)
    i.post(fx);
    InjuryEvent p;
    p.type = InjuryEvent::kTagPopup;
    p.kind = 2;
    p.useTouch = true;
    i.post(p);
}

void finishNow(InjuryState& i) {
    // (0x83624: 0x836f0 (createAtEnd), the stage past the last, finished, vf20)
    if (i.hasStage() && !i.stage().createAtEnd.empty()) {
        // (posted as the base's postCreateAtEnd does)
        const InjuryStageDef& st = i.stage();
        const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
        InjuryEvent e = createEvent(st.createAtEnd, i.position.x + st.createAtEndOffset.x * s,
                                    i.position.y + st.createAtEndOffset.y * s, i.rotation);
        e.countsTowardCap = false;
        e.allowedOverLock = true;
        i.post(e);
    }
    i.m_stage = static_cast<int>(i.def->stages.size()) + 1;
    i.finished = true;
    i.onFinished();
}

void countPoints(InjuryState& i, int& done, int& total) {
    done = total = 0;
    if (!i.hasStage()) return;
    for (HitAreaState* a : i.areasOf(i.stage().state, false))
        for (const HitPointState& p : a->points) {
            ++total;
            if (p.done) ++done;
        }
}

void markAreasDone(InjuryState& i) {
    if (!i.hasStage()) return;
    for (HitAreaState* a : i.areasOf(i.stage().state, false)) {
        a->done = true;
        for (HitPointState& p : a->points) {
            if (p.marker) p.marker->setVisible(false);
            p.done = true;
        }
    }
}

bool wearAll(InjuryState& i, const CCPoint& touch, ToolState* tool) {
    bool hit = false;
    if (!i.hasStage()) return false;
    for (HitAreaState* a : i.areasOf(i.stage().state, false))
        if (i.wear(*a, touch, tool->power, false, tool->type != kStapler)) hit = true;
    return hit;
}

bool allAreasDone(InjuryState& i) {
    if (!i.hasStage()) return true;
    for (HitAreaState* a : i.areasOf(i.stage().state, false))
        if (!i.areaDone(*a)) return false;
    return true;
}

ParticleSystem* particles(const std::string& name, const CCPoint& at, bool keepDead) {
    ParticleSystem* p = ParticleSystem::create(name.c_str());
    if (!p) return nullptr;
    p->setPosition(at);
    if (keepDead) p->setKeepDeadParticles(true);
    p->retain();
    return p;
}

void dropParticles(ParticleSystem*& p) {
    if (!p) return;
    p->setKeepDeadParticles(false);
    if (p->getParent()) p->removeFromParentAndCleanup(true);
    p->release();
    p = nullptr;
}

void pauseParticles(ParticleSystem* p) {
    if (p) p->unscheduleUpdate();
}

void resumeParticles(ParticleSystem* p) {
    if (p && p->getParent()) p->scheduleUpdate();
}

void addOnce(CCNode* parent, CCNode* child) {
    if (parent && child && !child->getParent()) parent->addChild(child);
}

void reparent(CCNode* parent, CCNode* child, int z) {
    if (!parent || !child) return;
    if (child->getParent() == parent) return;
    child->retain();
    if (child->getParent()) child->removeFromParentAndCleanup(false);
    parent->addChild(child, z);
    child->release();
}

}  // namespace kit
}  // namespace mt
