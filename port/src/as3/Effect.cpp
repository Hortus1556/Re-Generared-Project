#include "as3/Effect.h"

#include "as3/Log.h"

namespace mt {

using namespace cocos2d;

namespace {

// CCParticleSystem's emission counter is protected: a pointer to it taken
// through a derived class reaches it on any system (the standard way in
// C++; the cocos layer is not changed for one Mediatonic addition)
struct EmitCounter : CCParticleSystem {
    static float CCParticleSystem::*member() { return &EmitCounter::m_fEmitCounter; }
};

}  // namespace

ParticleSystem* retainedParticles(const char* name) {
    ParticleSystem* p = ParticleSystem::create(name);
    if (!p) {
        logError("Effect: particle system %s not read", name);
        return nullptr;
    }
    p->retain();
    return p;
}

void releaseParticles(ParticleSystem*& p) {
    if (p) p->release();
    p = nullptr;
}

void fillEmitCounter(CCParticleSystem* p) {
    if (!p) return;
    p->*EmitCounter::member() = 1.0f / p->getEmissionRate();
}

}  // namespace mt
