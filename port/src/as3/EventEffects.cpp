#include "as3/EventEffects.h"

#include <cstdio>

#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCDirector.h"

namespace mt {

using namespace cocos2d;

namespace {

// A gradient a quarter of the screen wide and its height, `colour` at
// `alpha` on its outer edge (where `along` points) fading to nothing
// inwards (the damage, shock and Reverand flashes; the original's
// CCLayerGradient::initWithColor, start transparent, end the colour).
CCLayerGradient* edge(const ccColor3B& colour, GLubyte alpha, float along, const CCSize& win) {
    CCLayerGradient* g = CCLayerGradient::create(ccc4(colour.r, colour.g, colour.b, 0),
                                                 ccc4(colour.r, colour.g, colour.b, alpha), ccp(along, 0));
    g->setContentSize(CCSizeMake(win.width * 0.25f, win.height));
    return g;
}

}  // namespace

// ---------------------------------------------------------------- DamageEffect

DamageEffect* DamageEffect::create() {
    DamageEffect* e = new DamageEffect;
    e->m_type = kEffectFlash;
    e->initDamage();
    e->autorelease();
    return e;
}

bool DamageEffect::initDamage() {
    CCSize win = CCDirector::sharedDirector()->getWinSize();
    m_left = edge(ccc3(255, 0, 0), 0x96, -1, win);
    m_right = edge(ccc3(255, 0, 0), 0x96, 1, win);
    m_right->setPosition(ccp(win.width * 0.75f, 0));
    m_left->retain();
    m_right->retain();
    // a second, then a fade out over two
    m_left->runAction(CCSequence::create(CCDelayTime::create(1), CCFadeOut::create(2), nullptr));
    addChild(m_left);
    m_right->runAction(CCSequence::create(CCDelayTime::create(1), CCFadeOut::create(2), nullptr));
    addChild(m_right);
    return true;
}

DamageEffect::~DamageEffect() {
    CC_SAFE_RELEASE(m_left);
    CC_SAFE_RELEASE(m_right);
    removeAllChildrenWithCleanup(true);
}

bool DamageEffect::isAlive() { return runsActions(m_left) || runsActions(m_right); }

// ---------------------------------------------------------------- ShockEffect

ShockEffect* ShockEffect::create() {
    ShockEffect* e = new ShockEffect;
    e->m_type = kEffectFlash;
    e->initShock();
    e->autorelease();
    return e;
}

bool ShockEffect::initShock() {
    m_holder = CCSprite::create();
    CCSize win = CCDirector::sharedDirector()->getWinSize();
    CCLayerGradient* left = edge(ccc3(0, 0, 200), 0x96, -1, win);
    CCLayerGradient* right = edge(ccc3(0, 0, 200), 0x96, 1, win);
    right->setPosition(ccp(win.width * 0.75f, 0));
    m_holder->addChild(left);
    m_holder->addChild(right);
    m_holder->setAnchorPoint(ccp(0, 0));
    m_holder->setOpacity(0x7f);
    m_holder->retain();
    // three blinks, then out (on the holder only: see the header)
    CCFiniteTimeAction* blink = CCSequence::create(CCFadeTo::create(0.1f, 0xff), CCFadeTo::create(0.1f, 0x7f), nullptr);
    m_holder->runAction(CCSequence::create(CCRepeat::create(blink, 3), CCFadeTo::create(0.1f, 0), nullptr));
    addChild(m_holder);
    return true;
}

ShockEffect::~ShockEffect() {
    CC_SAFE_RELEASE(m_holder);
    removeAllChildrenWithCleanup(true);
}

bool ShockEffect::isAlive() { return runsActions(m_holder); }

// ---------------------------------------------------------------- ElectricityEffect

ElectricityEffect* ElectricityEffect::create() {
    ElectricityEffect* e = new ElectricityEffect;
    e->initElectricity();
    e->autorelease();
    return e;
}

bool ElectricityEffect::initElectricity() {
    int angle = randomInt(0, 60);
    for (ParticleSystem*& p : m_systems) {
        p = ParticleSystem::create("particle_electricityFX");
        int spread = randomInt(-15, 15);
        int size = randomInt(70, 100);
        if (p) {
            p->setAngle(static_cast<float>(spread + angle));
            p->setScale(static_cast<float>(size) / 100.0f);
            addChild(p);
            p->retain();
        }
        angle += 60;
    }
    return true;
}

ElectricityEffect::~ElectricityEffect() {
    for (ParticleSystem*& p : m_systems) releaseParticles(p);
    removeAllChildrenWithCleanup(true);
}

bool ElectricityEffect::isAlive() {
    for (ParticleSystem* p : m_systems)
        if (isActive(p) || hasParticles(p)) return true;
    return false;
}

void ElectricityEffect::setPosition(const CCPoint& p) {
    for (ParticleSystem* s : m_systems) place(s, p);
}

// ---------------------------------------------------------------- BloodSpurtEffect

BloodSpurtEffect* BloodSpurtEffect::create() {
    BloodSpurtEffect* e = new BloodSpurtEffect;
    e->initSpurt();
    e->autorelease();
    return e;
}

bool BloodSpurtEffect::initSpurt() {
    for (int i = 0; i < 6; ++i) {
        ParticleSystem* p = ParticleSystem::create("particle_bloodFX");
        if (!p) continue;
        p->setDuration(0.4f);
        addChild(p);
        p->retain();
        m_systems.push_back(p);
    }
    return true;
}

BloodSpurtEffect::~BloodSpurtEffect() {
    // (the original cleaned each system up and released it; its children
    // went with the node)
    for (ParticleSystem* p : m_systems) {
        p->cleanup();
        p->release();
    }
}

bool BloodSpurtEffect::isAlive() {
    for (ParticleSystem* p : m_systems)
        if (p->isActive() || p->getParticleCount() != 0) return true;
    return false;
}

void BloodSpurtEffect::setPosition(const CCPoint& p) {
    for (ParticleSystem* s : m_systems) s->setPosition(p);
}

// ---------------------------------------------------------------- BluePoisonCloudEffect

BluePoisonCloudEffect* BluePoisonCloudEffect::create() {
    BluePoisonCloudEffect* e = new BluePoisonCloudEffect;
    e->m_type = kEffectBluePoisonCloud;
    if (ParticleSystem* p = ParticleSystem::create("particle_frogpoisonpuffFX")) {  // (0x127190)
        e->addChild(p);
        p->retain();
        e->m_systems.push_back(p);
    }
    e->autorelease();
    return e;
}

BluePoisonCloudEffect::~BluePoisonCloudEffect() {
    for (ParticleSystem* p : m_systems) {
        p->cleanup();
        p->release();
    }
}

bool BluePoisonCloudEffect::isAlive() {
    for (ParticleSystem* p : m_systems)
        if (p->isActive() || p->getParticleCount() != 0) return true;
    return false;
}

void BluePoisonCloudEffect::setPosition(const CCPoint& p) {
    for (ParticleSystem* s : m_systems) s->setPosition(p);
}

// ---------------------------------------------------------------- ExplosionEffect

ExplosionEffect* ExplosionEffect::create() {
    ExplosionEffect* e = new ExplosionEffect;
    e->initExplosion();
    e->autorelease();
    return e;
}

bool ExplosionEffect::initExplosion() {
    char name[32];
    for (int i = 0; i < 4; ++i) {
        std::snprintf(name, sizeof name, "particle_explosion%dFX", i + 1);
        m_systems[i] = retainedParticles(name);
        place(m_systems[i], CCPointZero);
    }
    for (ParticleSystem* p : m_systems)
        if (p) addChild(p);
    return true;
}

ExplosionEffect::~ExplosionEffect() {
    for (ParticleSystem*& p : m_systems) releaseParticles(p);
    removeAllChildrenWithCleanup(true);
}

bool ExplosionEffect::isAlive() {
    for (ParticleSystem* p : m_systems)
        if (isActive(p)) return true;
    return false;
}

void ExplosionEffect::setPosition(const CCPoint& p) {
    for (ParticleSystem* s : m_systems) place(s, p);
}

// ---------------------------------------------------------------- the colour washes

bool ScreenWashEffect::initWash(int type, const ccColor3B& colour, float fadeIn, float hold) {
    m_type = type;
    m_fading = false;
    m_time = 0;
    m_hold = hold;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    m_layer = CCLayerColor::create(ccc4(colour.r, colour.g, colour.b, 0x7f), static_cast<float>(r->width),
                                   static_cast<float>(r->height));
    m_layer->retain();
    m_layer->setOpacity(0);
    m_layer->runAction(CCFadeTo::create(fadeIn, 0x5a));
    addChild(m_layer);
    return true;
}

ScreenWashEffect::~ScreenWashEffect() {
    if (m_layer) m_layer->stopAllActions();
    CC_SAFE_RELEASE(m_layer);
    removeAllChildrenWithCleanup(true);
}

void ScreenWashEffect::update(float dt) {
    m_time += dt;
    if (m_fading || m_time < m_hold) return;
    if (m_layer->numberOfRunningActions() == 0) {
        m_fading = true;
        m_layer->runAction(CCFadeTo::create(1, 0));
    }
}

bool ScreenWashEffect::isAlive() {
    if (m_time < m_hold) return true;
    return m_layer->numberOfRunningActions() != 0;
}

bool ScreenWashEffect::isEnding() { return m_layer->numberOfRunningActions() != 0 && m_fading; }

BlueScreenEffect* BlueScreenEffect::create() {
    BlueScreenEffect* e = new BlueScreenEffect;
    e->initWash(kEffectBlueScreen, ccc3(0, 0, 255), 3, 8);  // (0x127cf4)
    e->autorelease();
    return e;
}

GreenScreenEffect* GreenScreenEffect::create() {
    GreenScreenEffect* e = new GreenScreenEffect;
    e->initWash(kEffectGreenScreen, ccc3(0, 255, 0), 1, 4.25f);  // (0x27d6b4)
    e->autorelease();
    return e;
}

WhiteScreenEffect* WhiteScreenEffect::create() {
    WhiteScreenEffect* e = new WhiteScreenEffect;
    e->initWash(kEffectWhiteScreen, ccc3(255, 255, 255), 1, 3.75f);  // (0x176264)
    e->autorelease();
    return e;
}

// ---------------------------------------------------------------- FrozenToolsEffect

FrozenToolsEffect* FrozenToolsEffect::create() {
    FrozenToolsEffect* e = new FrozenToolsEffect;
    e->m_type = kEffectFrozenTools;
    e->initFrozen();
    e->autorelease();
    return e;
}

bool FrozenToolsEffect::initFrozen() {
    SoundMgr::sharedSoundMgr()->playOnChannel("ice_crystals_spreading_loop", soundChannel(), false, true, false,
                                              false);
    m_time = 1.75f;
    m_thawed = false;
    return true;
}

FrozenToolsEffect::~FrozenToolsEffect() {}

void FrozenToolsEffect::update(float dt) {
    m_time -= dt;
    if (m_time <= 0 && !m_thawed) {
        // the ice melts: its animation backwards
        for (Animation* a : m_animations) a->play("frostbite", 1, true, -1);
        m_thawed = true;
    }
}

bool FrozenToolsEffect::isAlive() {
    if (m_time > 0) return true;
    // (the first animation stands for them all; the original read it even
    // with none made)
    if (m_animations.empty()) return false;
    return runsActions(m_animations[0]) || m_animations[0]->isPlaying();
}

void FrozenToolsEffect::addToolAnimations(UIInstance& hud) {
    for (auto& item : hud.byName) {
        if (item.first.find("tooleffect") == std::string::npos) continue;
        if (static_cast<int>(m_animations.size()) >= kMaxAnimations) break;
        Animation* a = Animation::create("common/anim_injury_frostbite_anim.json", 1);
        addChild(a);
        m_animations.push_back(a);
        a->play("frostbite", 1, false, -1);
        // (times the spot's scale: the port's bar across the top scales the
        // tools' buttons and their spots, SurgeryHud.cpp)
        a->setScale(0.5f * (item.second ? item.second->getScale() : 1));
        if (item.second) a->setPosition(item.second->getPosition());
    }
}

// ---------------------------------------------------------------- PoisonedToolsEffect

PoisonedToolsEffect* PoisonedToolsEffect::create() {
    PoisonedToolsEffect* e = new PoisonedToolsEffect;
    e->m_type = kEffectPoisonedTools;
    e->m_time = 3.75f;
    e->m_done = false;
    e->autorelease();
    return e;
}

void PoisonedToolsEffect::update(float dt) {
    m_time -= dt;
    if (m_time <= 0 && !m_done) {
        // a last bubbling as it shrinks away
        for (Animation* a : m_animations) {
            a->play("poison", 1, false, -1);
            a->runAction(CCScaleTo::create(0.75f, 0.1f));
        }
        m_done = true;
    }
}

bool PoisonedToolsEffect::isAlive() {
    if (m_time > 0) return true;
    if (m_animations.empty()) return false;  // (the original read the first even with none)
    if (runsActions(m_animations[0])) return true;
    if (!m_animations[0]->isPlaying()) return false;
    return !m_done;
}

void PoisonedToolsEffect::addToolAnimations(UIInstance& hud) {
    for (auto& item : hud.byName) {
        if (item.first.find("tooleffect") == std::string::npos) continue;
        if (static_cast<int>(m_animations.size()) >= kMaxAnimations) break;
        Animation* a = Animation::create("common/anim_injury_poison_anim.json", 1);
        addChild(a);
        m_animations.push_back(a);
        a->play("poison", -1, false, -1);
        // (times the spot's scale, as the frost's)
        const float k = item.second ? item.second->getScale() : 1;
        a->setScale(0.1f * k);
        a->runAction(CCScaleTo::create(0.75f, 0.4f * k));
        if (item.second) a->setPosition(item.second->getPosition());
    }
}

}  // namespace mt
