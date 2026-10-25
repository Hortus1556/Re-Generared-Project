#include "as3/ParticleViewer.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "as3/ParticleSystem.h"
#include "as3/Scene.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

class ParticleViewer : public Scene {
public:
    explicit ParticleViewer(const std::string& spec) : m_spec(spec) {}

    void load() override {
        CCSize win = CCDirector::sharedDirector()->getWinSize();
        addChild(CCLayerColor::create(ccc4(40, 40, 40, 255)), -1);
        std::vector<std::string> items;
        size_t from = 0;
        while (from <= m_spec.size()) {
            size_t comma = m_spec.find(',', from);
            if (comma == std::string::npos) comma = m_spec.size();
            if (comma > from) items.push_back(m_spec.substr(from, comma - from));
            from = comma + 1;
        }
        for (size_t i = 0; i < items.size(); ++i) {
            std::string name = items[i];
            CCPoint at = ccp(win.width * (i + 1) / (items.size() + 1), win.height / 2);
            size_t atSign = name.find('@');
            if (atSign != std::string::npos) {
                std::string where = name.substr(atSign + 1);
                name = name.substr(0, atSign);
                size_t colon = where.find(':');
                if (colon != std::string::npos)
                    at = ccp(std::strtof(where.c_str(), nullptr), std::strtof(where.c_str() + colon + 1, nullptr));
            }
            if (name.compare(0, 9, "particle_") != 0) name = "particle_" + name;
            ParticleSystem* p = ParticleSystem::create(name.c_str());
            if (!p) {
                platform::debugTrace("particle viewer: %s not read", name.c_str());
                continue;
            }
            p->setPosition(at);
            p->setKeepDeadParticles(true);  // counted below
            addChild(p);
            m_systems.push_back(p);
            m_names.push_back(name);
            platform::debugTrace(
                "particle viewer: %s at %.0f,%.0f: emission rate %.3f/s, life %.4f +- %.4f s, total %u, duration %.3f, "
                "mode %d, blend %u %u",
                name.c_str(), at.x, at.y, p->getEmissionRate(), p->getLife(), p->getLifeVar(), p->getTotalParticles(),
                p->getDuration(), p->getEmitterMode(), p->getBlendFunc().src, p->getBlendFunc().dst);
        }
        scheduleUpdate();
    }

    // the living particles once a second (the systems are the viewer's children: none removes itself)
    void update(float dt) override {
        m_elapsed += dt;
        if (++m_frames % 60 != 0) return;
        for (size_t i = 0; i < m_systems.size(); ++i) {
            ParticleSystem* p = m_systems[i];
            platform::debugTrace("particle viewer: %s after %.2f s: %u particles, active %d, %u died in the second",
                                 m_names[i].c_str(), m_elapsed, p->getParticleCount(), p->isActive() ? 1 : 0,
                                 static_cast<unsigned>(p->getDeadParticles().size()));
            p->clearDeadParticles();
        }
    }

private:
    std::string m_spec;
    std::vector<ParticleSystem*> m_systems;  // weak: children
    std::vector<std::string> m_names;
    float m_elapsed = 0;
    int m_frames = 0;
};

}  // namespace

Scene* particleViewer() {
    if (!platform::debugMode()) return nullptr;
    const char* spec = std::getenv("AS_PARTICLES");
    if (!spec || !*spec) return nullptr;
    ParticleViewer* v = new ParticleViewer(spec);
    v->init();
    v->autorelease();
    return v;
}

}  // namespace mt
