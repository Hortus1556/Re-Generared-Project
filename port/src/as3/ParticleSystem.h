// mt::ParticleSystem (vtable 0x4146c0, 0x1fc bytes): the game's particle
// system, a cocos2d CCParticleSystemQuad read from the screen's art folder
// by name, with one addition: it can keep a copy of each particle that dies
// (its last position, colour, size...), which the injuries that leave
// something where their particles land read and clear (CementDrop's cement,
// Fire, PoisonArea, SuperPoisonArea, SpikeBase).
//
// Its users (the decompile's callers of create, 0x52510): the area map
// (AreaScreen: particle_surgeryHighlightFX on the next surgery, the Exile
// Clinic's particle_exilesnowFX), WorldScreen, SurgeryEndScreen, about
// twenty effects (BloodSpurtEffect's six particle_bloodFX, ExplosionEffect,
// LighterEffect, CreamEffect, ElectricityEffect, HighFiveEffect,
// ChainsawTrailEffect, ScannerEffect, SparkleEffect, GibletsEffect, the
// tag partners' effects...) and the injuries (Artery, BrokenVein,
// CementDrop, Fire, FireMaggot, GiantWorm, Leech, PoisonArea,
// SuperPoisonArea, SuperPoisonScab, RobotScorpion, SpikeBase, Transplant).
// They set its position (and duration, emission rate...) and add it to a
// node; some stop it (stopSystem) and let it run out.
#pragma once

#include <vector>

#include "engine/cocos/CCParticleSystemQuad.h"

namespace mt {

class ParticleSystem : public cocos2d::CCParticleSystemQuad {
public:
    // "<name>.plist" in the screen's art folder (ResolutionMgr::artPath:
    // "4.0x/particle_bloodFX.plist"), its sizes, speeds and accelerations
    // scaled to the canvas (scaleParticleSystemToCanvas); autoreleased, or
    // nullptr if it cannot be read (0x52510)
    static ParticleSystem* create(const char* name);
    ~ParticleSystem() override;  // 0x524cc

    // CCParticleSystem::update, keeping each particle that dies when asked to (0x526f4)
    void update(float dt) override;

    // whether a copy of each particle that dies is kept (off at first; 0x526ec)
    void setKeepDeadParticles(bool keep) { m_bKeepDeadParticles = keep; }
    // the particles that died since they were last cleared, in the order
    // they died, as they were at their last update: `pos` in the system's
    // own terms (with `startPos`, the emitter's world position when it was
    // emitted, for a free system) (0x52cd0)
    std::vector<cocos2d::tCCParticle>& getDeadParticles() { return m_deadParticles; }
    void clearDeadParticles() { m_deadParticles.clear(); }  // 0x52504

private:
    ParticleSystem() = default;

    bool m_bKeepDeadParticles = false;                // +0x1ec
    std::vector<cocos2d::tCCParticle> m_deadParticles;  // +0x1f0
};

// A particle system made in canvas points, shown in this screen's cocos
// points: its start and end sizes (and their variances), and in
// radius mode its radii, or in gravity mode its gravity, accelerations and
// speed, times the cocos points a canvas point is (ResolutionMgr's
// canvasScale: 1 on the iPhones and the PC, 2 on an iPad) (0x2caf8, in
// ResolutionMgr's unit).
void scaleParticleSystemToCanvas(cocos2d::CCParticleSystem* system);

}  // namespace mt
