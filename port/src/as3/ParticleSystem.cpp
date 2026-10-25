#include "as3/ParticleSystem.h"

#include <algorithm>
#include <cmath>

#include "as3/ResolutionMgr.h"

namespace mt {

using namespace cocos2d;

void scaleParticleSystemToCanvas(CCParticleSystem* p) {
    float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;  // +0x24
    p->setStartSize(s * p->getStartSize());
    p->setStartSizeVar(s * p->getStartSizeVar());
    p->setEndSize(s * p->getEndSize());
    p->setEndSizeVar(s * p->getEndSizeVar());
    if (p->getEmitterMode() == kCCParticleModeRadius) {
        p->setStartRadius(s * p->getStartRadius());
        p->setStartRadiusVar(s * p->getStartRadiusVar());
        p->setEndRadius(s * p->getEndRadius());
        p->setEndRadiusVar(s * p->getEndRadiusVar());
    } else {
        const CCPoint& g = p->getGravity();
        p->setGravity(ccp(s * g.x, s * g.y));
        p->setRadialAccel(s * p->getRadialAccel());
        p->setRadialAccelVar(s * p->getRadialAccelVar());
        p->setTangentialAccel(s * p->getTangentialAccel());
        p->setTangentialAccelVar(s * p->getTangentialAccelVar());
        p->setSpeed(s * p->getSpeed());
        p->setSpeedVar(s * p->getSpeedVar());
    }
}

ParticleSystem* ParticleSystem::create(const char* name) {
    ParticleSystem* p = new ParticleSystem;
    std::string path = ResolutionMgr::sharedResolutionMgr()->artPath(name, "plist");
    if (!p->initWithFile(path.c_str())) {
        delete p;
        return nullptr;
    }
    p->autorelease();
    p->m_bKeepDeadParticles = false;
    scaleParticleSystemToCanvas(p);
    return p;
}

ParticleSystem::~ParticleSystem() {}

// CCParticleSystem::update (0x32b754) as the game copied it, with the dead
// particles kept.
void ParticleSystem::update(float dt) {
    if (m_bIsActive && m_fEmissionRate) {
        float rate = 1.0f / m_fEmissionRate;
        if (m_uParticleCount < m_uTotalParticles) m_fEmitCounter += dt;
        while (m_uParticleCount < m_uTotalParticles && m_fEmitCounter > rate) {
            addParticle();
            m_fEmitCounter -= rate;
        }
        m_fElapsed += dt;
        if (m_fDuration != -1 && m_fDuration < m_fElapsed) stopSystem();
    }

    m_uParticleIdx = 0;
    CCPoint currentPosition = emitterPosition();
    m_tQuadsPosition = currentPosition;  // (the port's: what draw shifts the quads by)

    if (m_bVisible) {
        while (m_uParticleIdx < m_uParticleCount) {
            tCCParticle* p = &m_pParticles[m_uParticleIdx];
            p->timeToLive -= dt;
            if (p->timeToLive > 0) {
                if (m_nEmitterMode == kCCParticleModeGravity) {
                    CCPoint tmp, radial, tangential;
                    radial = CCPointZero;
                    if (p->pos.x || p->pos.y) radial = ccpNormalize(p->pos);
                    tangential = radial;
                    radial = ccpMult(radial, p->modeA.radialAccel);
                    float newy = tangential.x;
                    tangential.x = -tangential.y;
                    tangential.y = newy;
                    tangential = ccpMult(tangential, p->modeA.tangentialAccel);
                    tmp = ccpAdd(ccpAdd(radial, tangential), modeA.gravity);
                    tmp = ccpMult(tmp, dt);
                    p->modeA.dir = ccpAdd(p->modeA.dir, tmp);
                    tmp = ccpMult(p->modeA.dir, dt);
                    p->pos = ccpAdd(p->pos, tmp);
                } else {
                    p->modeB.angle += p->modeB.degreesPerSecond * dt;
                    p->modeB.radius += p->modeB.deltaRadius * dt;
                    p->pos.x = -cosf(p->modeB.angle) * p->modeB.radius;
                    p->pos.y = -sinf(p->modeB.angle) * p->modeB.radius;
                }
                p->color.r += (p->deltaColor.r * dt);
                p->color.g += (p->deltaColor.g * dt);
                p->color.b += (p->deltaColor.b * dt);
                p->color.a += (p->deltaColor.a * dt);
                p->size += (p->deltaSize * dt);
                p->size = std::max(0.0f, p->size);
                p->rotation += (p->deltaRotation * dt);

                CCPoint newPos;
                if (m_ePositionType == kCCPositionTypeFree || m_ePositionType == kCCPositionTypeRelative) {
                    CCPoint diff = ccpSub(currentPosition, p->startPos);
                    newPos = ccpSub(p->pos, diff);
                } else {
                    newPos = p->pos;
                }
                if (m_pBatchNode) {
                    newPos.x += m_obPosition.x;
                    newPos.y += m_obPosition.y;
                }
                updateQuadWithParticle(p, newPos);
                ++m_uParticleIdx;
            } else {
                // died: a copy kept first, if asked for
                if (m_bKeepDeadParticles) m_deadParticles.push_back(*p);
                if (m_uParticleIdx != m_uParticleCount - 1) m_pParticles[m_uParticleIdx] = m_pParticles[m_uParticleCount - 1];
                --m_uParticleCount;
                if (m_uParticleCount == 0 && m_bIsAutoRemoveOnFinish) {
                    unscheduleUpdate();
                    // may delete it: nothing of it is used after
                    if (m_pParent) m_pParent->removeChild(this, true);
                    return;
                }
            }
        }
        m_bTransformSystemDirty = false;
    }
    if (!m_pBatchNode) postStep();
}

}  // namespace mt
