// What the injury classes (CutInjuries, ParticleInjuries, SkinInjuries,
// Extractions, BoneInjuries) share: the original's small helpers around
// InjuryState (their addresses), kept out of the shared base.
#pragma once

#include <cmath>
#include <string>

#include "as3/InjuryState.h"
#include "as3/ParticleSystem.h"

namespace mt {
namespace kit {

// A point in a rect turned by `rotation` degrees about `center` (0x799f0):
// the point is turned back by the rotation about the centre (its offset
// truncated to whole points, as the original's ints), then strictly inside.
bool inTurnedRect(const cocos2d::CCPoint& p, const cocos2d::CCRect& r, float rotation, const cocos2d::CCPoint& center);
// the touch over the injury's own rect, turned with it (0x799f0 on +0x20,
// +0x30, +0x18)
inline bool inInjury(const InjuryState& i, const cocos2d::CCPoint& p) {
    return inTurnedRect(p, i.rect, std::floor(i.rotation), i.position);
}

// A create-injury event (2) for `name` at (x, y) turned `rotation`, as the
// classes post them: whole points, not counted toward the cap of 40.
InjuryEvent createEvent(const std::string& name, float x, float y, float rotation);
// a damage event (9) of `amount`
InjuryEvent damageEvent(int amount);
// an effect event (3) of `kind` at (x, y)
InjuryEvent effectEvent(int kind, float x, float y);
// The base's vf25 (0x83e10): the "good" popup (event 11, kind 0, at the touch).
void postGoodPopup(InjuryState& i);
// The base's vf26 (0x84084): hurt by `amount` (event 9), the damage effect
// (event 3, kind 0) and the "bad" popup (event 11, kind 2, at the touch).
void postHurt(InjuryState& i, int amount);
// 0x83624: finished at once (its injury_to_create_at_end, then onFinished)
void finishNow(InjuryState& i);

// The current state's hit points, done and in all (the classes that fade as
// their points are worn).
void countPoints(InjuryState& i, int& done, int& total);
// Marks every hit area of the current state done (the tutorial's and the
// partners' completeStage, vf18): the areas and their points, their markers
// hidden.
void markAreasDone(InjuryState& i);
// Wears every hit area of the current state with the tool (the base vf8's
// loop: its power; the stapler a point a press): whether any point was hit.
bool wearAll(InjuryState& i, const cocos2d::CCPoint& touch, ToolState* tool);
// every hit area of the current state done (vf15 on each)
bool allAreasDone(InjuryState& i);
// The combo for a completed action (+0x15 for the stage's own injuries,
// +0x16 for the ones made on the way: 0xfda4).
inline void comboFor(const InjuryState& i, HitResult& r) {
    if (!i.createdLater) r.comboAction = true;
    else r.comboBonus = true;
}

// A particle system by name at `at`, retained (the injuries keep theirs and
// let them go when they go); nullptr if it cannot be read (the original
// crashed; logged by ParticleSystem::create)
ParticleSystem* particles(const std::string& name, const cocos2d::CCPoint& at, bool keepDead = false);
// let go of one: off its parent and released
void dropParticles(ParticleSystem*& p);
// an injury's systems' updates stopped or started (vf28/vf29)
void pauseParticles(ParticleSystem* p);
void resumeParticles(ParticleSystem* p);
// what the injuries ask a system, tolerating one that could not be read
inline bool running(ParticleSystem* p) { return p && (p->isActive() || p->getParticleCount() != 0); }
inline void stop(ParticleSystem* p) {
    if (p) p->stopSystem();
}
inline void reset(ParticleSystem* p) {
    if (p) p->resetSystem();
}
// added to `parent` unless it already has one
void addOnce(cocos2d::CCNode* parent, cocos2d::CCNode* child);
// added to `parent`, off any it had (its pictures built again: a stage
// shown again makes a new root)
void reparent(cocos2d::CCNode* parent, cocos2d::CCNode* child, int z = 0);

}  // namespace kit
}  // namespace mt
