#include "as3/ToolEffects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"

namespace mt {

using namespace cocos2d;

// ---------------------------------------------------------------- LighterEffect

LighterEffect* LighterEffect::create() {
    LighterEffect* e = new LighterEffect;
    e->initLighter();
    e->autorelease();
    return e;
}

bool LighterEffect::initLighter() {
    // both systems read with their rates kept, silent until a touch
    m_smoke = retainedParticles("particle_smokeFX");
    m_smokeRate = emissionRate(m_smoke);
    setEmissionRate(m_smoke, 0);
    if (m_smoke) addChild(m_smoke);
    m_flame = retainedParticles("particle_lighterFX");
    m_flameRate = emissionRate(m_flame);
    setEmissionRate(m_flame, 0);
    if (m_flame) addChild(m_flame);
    return true;
}

LighterEffect::~LighterEffect() {
    releaseParticles(m_smoke);
    releaseParticles(m_flame);
    removeAllChildrenWithCleanup(true);
}

bool LighterEffect::isAlive() {
    return emissionRate(m_smoke) > 0 || hasParticles(m_smoke) || emissionRate(m_flame) > 0 || hasParticles(m_flame);
}

void LighterEffect::touchBegan(const CCPoint& p) {
    setEmissionRate(m_smoke, m_smokeRate);
    setEmissionRate(m_flame, m_flameRate);
    setPosition(p);
}

void LighterEffect::touchEnded(const CCPoint&) {
    setEmissionRate(m_smoke, 0);
    setEmissionRate(m_flame, 0);
}

void LighterEffect::setPosition(const CCPoint& p) {
    // (the original added offsets to the point, both times the canvas
    // scale times 0)
    place(m_flame, p);
    place(m_smoke, p);
}

// ---------------------------------------------------------------- CreamEffect

CreamEffect* CreamEffect::create() {
    CreamEffect* e = new CreamEffect;
    e->initCream();
    e->autorelease();
    return e;
}

bool CreamEffect::initCream() {
    char name[32];
    for (int i = 0; i < 4; ++i) {
        std::snprintf(name, sizeof name, "particle_cream%dFX", i + 1);
        ParticleSystem* p = retainedParticles(name);
        m_rates[i] = emissionRate(p);
        setEmissionRate(p, 0);
        if (p) addChild(p, 4 - i);  // the first on top
        m_systems[i] = p;
    }
    return true;
}

CreamEffect::~CreamEffect() {
    for (ParticleSystem*& p : m_systems) releaseParticles(p);
    removeAllChildrenWithCleanup(true);
}

bool CreamEffect::isAlive() {
    bool alive = false;
    for (ParticleSystem* p : m_systems) alive = alive || emissionRate(p) > 0 || hasParticles(p);
    return alive;
}

void CreamEffect::touchBegan(const CCPoint& p) {
    for (int i = 0; i < 4; ++i) setEmissionRate(m_systems[i], m_rates[i]);
    setPosition(p);
}

void CreamEffect::touchEnded(const CCPoint&) {
    for (ParticleSystem* p : m_systems) setEmissionRate(p, 0);
}

void CreamEffect::setPosition(const CCPoint& p) {
    for (ParticleSystem* s : m_systems) place(s, p);  // (offsets times 0 again)
}

// ---------------------------------------------------------------- ScannerEffect

ScannerEffect* ScannerEffect::create(float interval) {
    ScannerEffect* e = new ScannerEffect;
    e->m_rate = 1.0f / interval;
    e->m_time = interval;  // the first tick comes at once
    e->m_interval = interval;
    e->autorelease();
    return e;
}

ScannerEffect::~ScannerEffect() {
    for (ParticleSystem* p : m_blips) {
        p->setEmissionRate(0);
        p->stopSystem();
        p->release();
    }
    m_blips.clear();
    removeAllChildrenWithCleanup(true);
}

bool ScannerEffect::isAlive() {
    for (ParticleSystem* p : m_blips) {
        if (p->getEmissionRate() > 0) return true;
        if (p->getParticleCount() != 0) return true;
    }
    return false;
}

void ScannerEffect::update(float dt) { m_time += dt; }

bool ScannerEffect::tick() {
    bool due = m_interval <= m_time;
    if (due) m_time = 0;
    return due;
}

void ScannerEffect::addBlip(const CCPoint& at, int colour) {
    m_rate = 1.0f / m_interval;
    ParticleSystem* p = ParticleSystem::create("particle_scannerFX");
    if (!p) return;
    p->setTotalParticles(1);
    // the colour fades out to nothing over the particle's life
    ccColor4F start = {0, 0, 0, 1}, end = {0, 0, 0, 0};
    switch (colour) {
    case kBlack:  // nothing found
        break;
    case kRed:
        start = {1, 0, 0, 1};
        end = {1, 0, 0, 0};
        break;
    case kOrange:
        start = {1, 0.8f, 0, 1};
        end = {1, 0.8f, 0, 0};
        break;
    case kGreen:
        start = {0, 1, 0, 1};
        end = {0, 1, 0, 0};
        break;
    default:  // (no colour set: the file's)
        break;
    }
    if (colour >= kBlack && colour <= kGreen) {
        p->setStartColor(start);
        p->setEndColor(end);
    }
    p->retain();
    fillEmitCounter(p);
    addChild(p);
    p->setPosition(at);
    m_blips.push_back(p);
}

void ScannerEffect::restart() {
    for (ParticleSystem* p : m_blips) {
        p->resetSystem();
        p->setEmissionRate(1.0f / p->getEmissionRate());
        fillEmitCounter(p);
    }
}

void ScannerEffect::clear() {
    for (ParticleSystem* p : m_blips) {
        p->stopSystem();
        p->release();
    }
    removeAllChildrenWithCleanup(true);
    m_blips.clear();
}

void ScannerEffect::touchBegan(const CCPoint&) {
    // Bug fixed: the original also released each blip here, which the
    // list still held (a second release when they are cleared). Its list
    // is always empty by then (a touch's end clears it), so nothing changes.
    for (ParticleSystem* p : m_blips) {
        p->stopSystem();
        p->setEmissionRate(m_rate);
    }
}

void ScannerEffect::touchEnded(const CCPoint&) {
    for (ParticleSystem* p : m_blips) {
        p->setEmissionRate(0);
        p->stopSystem();
        p->release();
    }
    removeAllChildrenWithCleanup(true);
    m_blips.clear();
}

// ---------------------------------------------------------------- ChainsawTrailEffect

ChainsawTrailEffect* ChainsawTrailEffect::create() {
    ChainsawTrailEffect* e = new ChainsawTrailEffect;
    e->initTrail();
    e->autorelease();
    return e;
}

bool ChainsawTrailEffect::initTrail() {
    m_trail = retainedParticles("particle_chainsawTrail");
    m_trailRate = emissionRate(m_trail);
    setEmissionRate(m_trail, 0);
    place(m_trail, CCPointZero);
    if (m_trail) addChild(m_trail);
    m_sparks1 = retainedParticles("particle_chainsawFX");
    m_sparksRate = emissionRate(m_sparks1);
    setEmissionRate(m_sparks1, 0);
    place(m_sparks1, CCPointZero);
    if (m_sparks1) addChild(m_sparks1);
    m_sparks2 = retainedParticles("particle_chainsawFX");  // (the same file: the same rate)
    setEmissionRate(m_sparks2, 0);
    place(m_sparks2, CCPointZero);
    if (m_sparks2) addChild(m_sparks2);
    return true;
}

ChainsawTrailEffect::~ChainsawTrailEffect() {
    releaseParticles(m_trail);
    releaseParticles(m_sparks1);
    releaseParticles(m_sparks2);
    removeAllChildrenWithCleanup(true);
}

void ChainsawTrailEffect::setPosition(const CCPoint& p) {
    CCNode::setPosition(p);
    // the way it moved: its angle from the x axis (the x step taken as a
    // whole number, as the original did), below the axis when it moved down
    float dx = p.x - m_last.x, dy = p.y - m_last.y;
    float length = std::sqrt(dx * dx + dy * dy);
    // Bug fixed: unmoved, the original divided 0 by 0 and turned the sparks
    // to NaN, which threw them nowhere; they keep their angle instead.
    if (length > 0) {
        float angle = radToDeg(std::acos(static_cast<float>(static_cast<int>(dx)) / length));
        if (p.y < m_last.y) angle = -angle;
        if (m_sparks1) m_sparks1->setAngle(angle + 90);
        if (m_sparks2) m_sparks2->setAngle(angle - 90);
    }
    m_last = p;
}

void ChainsawTrailEffect::setVisible(bool visible) {
    if (visible) {
        CCNode::setVisible(true);
        setEmissionRate(m_trail, m_trailRate);
        setEmissionRate(m_sparks1, m_sparksRate);
        setEmissionRate(m_sparks2, m_sparksRate);
        return;
    }
    setEmissionRate(m_trail, 0);
    setEmissionRate(m_sparks1, 0);
    setEmissionRate(m_sparks2, 0);
}

// ---------------------------------------------------------------- ChainsawShakeEffect

ChainsawShakeEffect* ChainsawShakeEffect::create(const std::string& background, CCNode* bg, CCNode* injuryLayer,
                                                 CCNode* bgExtras) {
    ChainsawShakeEffect* e = new ChainsawShakeEffect;
    e->m_bg = background.find(".png") == std::string::npos ? nullptr : bg;
    e->m_injuryLayer = injuryLayer;
    e->m_bgExtras = bgExtras;
    e->m_angle = 0;
    e->m_held = false;
    e->m_alive = false;
    e->m_ghostsMade = false;
    e->autorelease();
    return e;
}

ChainsawShakeEffect::~ChainsawShakeEffect() { removeGhosts(); }

void ChainsawShakeEffect::update(float dt) {
    if (!m_alive) return;
    float strength = 1;
    if (m_held) {
        // it grows over 2 s
        if (m_strength < 2) {
            m_strength = std::min(2.0f, m_strength + dt);
            strength = m_strength * 0.5f;
        }
    } else if (m_strength >= 0) {
        // let go: it dies down, then the copies go
        m_strength = std::max(0.0f, m_strength - dt);
        strength = m_strength;
        if (m_strength <= 0) {
            m_alive = false;
            cleanup();
            return;
        }
    }
    // 720 degrees a second, in whole degrees
    m_angle = static_cast<int>(dt * 720 + static_cast<float>(m_angle)) % 360;
    float scale = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    if (m_ghosts.empty()) return;
    // (the sine of the whole degrees taken as radians, as the original did:
    // a quick, uneven jitter)
    float offset = static_cast<float>(
        static_cast<int>(static_cast<double>(strength) * (std::sin(static_cast<double>(m_angle)) * 10.0 * scale)));
    for (Ghost& g : m_ghosts) {
        CCSprite* parent = dynamic_cast<CCSprite*>(g.sprite->getParent());
        if (!parent) continue;
        // the sprite's frame changed (an animation): the copy follows
        CCSpriteFrame* shown = g.sprite->displayFrame();
        CCSpriteFrame* wanted = parent->displayFrame();
        if (shown != wanted) g.sprite->setDisplayFrame(wanted);
        g.sprite->setPosition(ccp(offset * g.direction.x, offset * g.direction.y));
    }
}

void ChainsawShakeEffect::touchBegan(const CCPoint&) {
    if (!m_ghostsMade) {
        m_ghostsMade = true;
        if (m_bg) addGhosts(m_bg);
        addGhosts(m_bgExtras);
        addGhosts(m_injuryLayer);
    }
    if (!m_alive) {
        m_angle = 0;
        m_alive = true;
        m_strength = 0;
    }
    m_held = true;
}

void ChainsawShakeEffect::toolStopped() {
    if (!m_held) return;
    m_held = false;
    m_strength = std::min(1.0f, m_strength);
}

void ChainsawShakeEffect::cleanup() {
    removeGhosts();
    CCNode::cleanup();
}

void ChainsawShakeEffect::removeGhosts() {
    for (Ghost& g : m_ghosts) {
        g.sprite->removeFromParentAndCleanup(true);
        g.sprite->release();
    }
    m_ghosts.clear();
    m_ghostsMade = false;
}

void ChainsawShakeEffect::addGhosts(CCNode* node) {
    if (!node) return;
    // its children first (the copy made below is not one of them yet)
    if (CCArray* children = node->getChildren()) {
        std::vector<CCNode*> list;
        for (unsigned i = 0; i < children->count(); ++i)
            if (CCNode* child = dynamic_cast<CCNode*>(children->objectAtIndex(i))) list.push_back(child);
        for (CCNode* child : list) addGhosts(child);
    }
    CCSprite* sprite = dynamic_cast<CCSprite*>(node);
    if (!sprite) return;
    if (m_ghosts.size() >= kMaxGhosts) {
        logError(
            "The array that contains all the injuries and their effects is full. Increment MaxInjurySpriteCount if "
            "necessary");
        return;
    }
    CCSprite* ghost = CCSprite::create();
    ghost->retain();
    ghost->setDisplayFrame(sprite->displayFrame());
    ghost->setOpacity(0x80);
    ghost->setAnchorPoint(ccp(0, 0));
    ghost->setPosition(ccp(0, 0));
    ghost->setFlipX(sprite->isFlipX());
    ghost->setFlipY(sprite->isFlipY());
    sprite->addChild(ghost);
    Ghost g;
    g.sprite = ghost;
    float c = std::cos(degToRad(sprite->getRotation()));
    g.direction = ccp(c, 1 - c);
    m_ghosts.push_back(g);
}

void ChainsawShakeEffect::refreshGhost(CCNode* sprite) {
    CCSprite* s = dynamic_cast<CCSprite*>(sprite);
    if (!s) return;
    for (Ghost& g : m_ghosts)
        if (g.sprite->getParent() == sprite) {
            g.sprite->setDisplayFrame(s->displayFrame());
            return;
        }
}

}  // namespace mt
