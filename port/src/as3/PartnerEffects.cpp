#include "as3/PartnerEffects.h"

#include <cmath>

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCDrawNode.h"

namespace mt {

using namespace cocos2d;

namespace {

// the tag partners' ids and their art's names (0x4368d0, 0x4368ec)
const char* const kPartnerIds[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};
const char* const kPartnerArt[8] = {"Giblets", "Saturday", "Reverand", "Flow", "Bot", "Nowe", "Crumpet", "Brutality"};

CCSprite* sprite(const std::string& frame) {
    CCSprite* s = CCSprite::createWithSpriteFrameName(frame.c_str());
    if (!s) logError("Effect: frame %s not found", frame.c_str());
    return s;
}

// the screen's size in cocos points
CCSize screenSize() {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    return CCSizeMake(static_cast<float>(r->width), static_cast<float>(r->height));
}

// where a canvas point shows, without setPositionOnCanvas's node (the
// original's inlined arithmetic: (c - margin) * canvasScale)
CCPoint onCanvas(float x, float y) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float s = r->canvasScale;
    return ccp(s * (x - (640 - r->width / s) * 0.5f), s * (y - (384 - r->height / s) * 0.5f));
}

}  // namespace

// ---------------------------------------------------------------- HighFiveEffect

HighFiveEffect* HighFiveEffect::create(const std::string& partner) {
    HighFiveEffect* e = new HighFiveEffect;
    e->initHighFive(partner);
    e->autorelease();
    return e;
}

bool HighFiveEffect::initHighFive(const std::string& partner) {
    m_type = kEffectHighFive;
    const float cs = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCSize size = screenSize();
    const float w = size.width, h = size.height;
    setAnchorPoint(CCPointZero);
    setContentSize(size);

    // the two halves: the left in front, the right clipped along a slant
    // a sixth of the screen wide
    float band = static_cast<float>(static_cast<int>(w / 6.0f));
    float halfWidth = w * 0.5f + band;
    m_left = CCLayerColor::create(m_leftColour, halfWidth, h);
    m_left->setPosition(CCPointZero);
    addChild(m_left);
    m_right = CCLayerColor::create(m_rightColour, halfWidth, h);
    m_right->setPosition(CCPointZero);
    float halfBand = static_cast<float>(static_cast<int>(band) / 2);
    float middle = w * 0.5f - halfBand;

    // 15 rays a 24th of a turn apart in each half, turning about a point
    // above the top
    {
        float length = std::sqrt(h * h + middle * middle) + cs * (m_rayY + 5);
        CCPoint at = ccp(middle + m_rayX * cs, h + m_rayY * cs);
        for (int i = 0; i < 15; ++i) {
            CCSprite* ray = sprite("ray.png");
            if (!ray) break;
            ray->setColor(m_leftRays);
            ray->setAnchorPoint(ccp(0, 0.5f));
            ray->setScale(length / ray->getContentSize().width);
            ray->setPosition(at);
            ray->setRotation(static_cast<float>(i) * 24);
            m_leftRayList.push_back(ray);
            m_left->addChild(ray);
        }
    }
    {
        float d = middle - w;
        float length = std::sqrt(h * h + d * d) + cs * (m_rayY + 5);
        CCPoint at = ccp(-(m_rayX * cs), h + m_rayY * cs);
        for (int i = 0; i < 15; ++i) {
            CCSprite* ray = sprite("ray.png");
            if (!ray) break;
            ray->setColor(m_rightRays);
            ray->setAnchorPoint(ccp(0, 0.5f));
            ray->setScale(length / ray->getContentSize().width);
            ray->setPosition(at);
            ray->setRotation(static_cast<float>(i) * 24);
            m_rightRayList.push_back(ray);
            m_right->addChild(ray);
        }
    }

    // the right half seen through a slanted shape
    m_clip = CCClippingNode::create();
    m_clip->setContentSize(CCSizeMake(halfWidth, h));
    m_clip->setAnchorPoint(CCPointZero);
    m_clip->setPosition(ccp(middle, 0));
    CCDrawNode* shape = CCDrawNode::create();
    CCPoint corners[4] = {ccp(band, 0), ccp(halfWidth, 0), ccp(halfWidth, h), ccp(0, h)};
    ccColor4F white = {1, 1, 1, 1};
    shape->drawPolygon(corners, 4, white, 1, white);
    m_clip->setStencil(shape);
    m_clip->addChild(m_right);
    addChild(m_clip);

    // the white bar along the slant
    float slant = std::sqrt(h * h + band * band);
    float angle = radToDeg(std::acos(-h / slant));
    int overlap = static_cast<int>(cs * 10);
    float barWidth = static_cast<float>(m_barWidth) * cs;
    m_bar = CCLayerColor::create(m_barColour, barWidth, slant + static_cast<float>(overlap));
    m_bar->setRotation(angle);
    m_bar->setPosition(ccp(w * 0.5f - barWidth * 0.5f, static_cast<float>(-overlap)));
    addChild(m_bar);

    // lines streaming in from both sides
    m_leftLine = retainedParticles("particle_lineFX");
    place(m_leftLine, ccp(0, h * 0.5f));
    if (m_leftLine) addChild(m_leftLine);
    m_rightLine = retainedParticles("particle_lineFX");
    place(m_rightLine, ccp(w, h * 0.5f));
    if (m_rightLine) {
        m_rightLine->setRotation(180);
        addChild(m_rightLine);
    }

    // Ophelia and the partner, each with an arm turning about its shoulder
    m_ophelia = sprite("Ophelia_body.png");
    if (m_ophelia) {
        m_ophelia->setAnchorPoint(CCPointZero);
        addChild(m_ophelia);
        m_opheliaArm = sprite("Ophelia_arm.png");
        if (m_opheliaArm) {
            m_opheliaArm->setAnchorPoint(ccp(0.3808824f, 0.3608491f));
            CCSize body = m_ophelia->getContentSize();
            m_opheliaArm->setPosition(ccp(body.width * 0.3808824f, body.height * 0.3608491f));
            m_ophelia->addChild(m_opheliaArm, -1);
        }
    }
    std::string art;
    for (int i = 0; i < 8; ++i)
        if (partner == kPartnerIds[i]) art = kPartnerArt[i];
    m_partner = sprite(art + "_body.png");
    if (m_partner) {
        m_partner->setAnchorPoint(CCPointZero);
        addChild(m_partner);
        m_partnerArm = sprite(art + "_arm.png");
        if (m_partnerArm) {
            m_partnerArm->setAnchorPoint(ccp(0.5632353f, 0.5825472f));
            // (by the arm's own size, where Ophelia's goes by her body's)
            CCSize arm = m_partnerArm->getContentSize();
            m_partnerArm->setPosition(ccp(arm.width * 0.5632353f, arm.height * 0.5825472f));
            m_partner->addChild(m_partnerArm, -1);
        }
    }
    m_hands = retainedParticles("particle_taghandFX");
    if (m_hands) setPositionOnCanvas(m_hands, 315, 260);
    SoundMgr::sharedSoundMgr()->playSound("hi_5_fanfare", false);
    slideIn();
    return true;
}

HighFiveEffect::~HighFiveEffect() {
    releaseParticles(m_leftLine);
    releaseParticles(m_rightLine);
    releaseParticles(m_hands);
    removeAllChildrenWithCleanup(true);
}

void HighFiveEffect::slideIn() {
    m_time = m_duration;
    CCSize size = screenSize();
    // both slide to the canvas's corner (their art is the canvas's size)
    CCPoint home = onCanvas(0, 0);
    if (m_ophelia) {
        m_ophelia->setPosition(ccp(m_ophelia->getContentSize().width * -0.5f, size.height * -0.5f));
        m_ophelia->runAction(CCEaseOut::create(CCMoveTo::create(m_slideTime, home), 2));
    }
    if (m_opheliaArm) {
        m_opheliaArm->setRotation(-90);
        m_opheliaArm->runAction(CCRotateTo::create(m_slideTime, 0));
    }
    if (m_partner) {
        m_partner->setPosition(ccp(size.width, size.height * 0.5f));
        m_partner->runAction(CCEaseOut::create(CCMoveTo::create(m_slideTime, home), 2));
    }
    if (m_partnerArm) {
        m_partnerArm->setRotation(90);
        m_partnerArm->runAction(CCRotateTo::create(m_slideTime, 0));
    }
}

void HighFiveEffect::stopSliding() {
    m_time = 0;
    for (CCNode* n : {static_cast<CCNode*>(m_ophelia), static_cast<CCNode*>(m_partner),
                      static_cast<CCNode*>(m_opheliaArm), static_cast<CCNode*>(m_partnerArm)})
        if (n) n->stopAllActions();
}

void HighFiveEffect::touchEnded(const CCPoint&) {
    stopSliding();
    slideIn();
}

void HighFiveEffect::update(float dt) {
    m_time -= dt;
    if (m_time > 0) {
        for (CCSprite* r : m_leftRayList) r->setRotation(r->getRotation() - m_raySpeed * dt);
        for (CCSprite* r : m_rightRayList) r->setRotation(r->getRotation() + m_raySpeed * dt);
    }
    // the hands meet: their burst
    if (m_time < m_duration - m_slideTime && m_hands && !m_hands->getParent()) addChild(m_hands);
}

// ---------------------------------------------------------------- GibletsEffect

GibletsEffect* GibletsEffect::create() {
    GibletsEffect* e = new GibletsEffect;
    e->m_type = kEffectGiblets;
    e->initGiblets();
    e->autorelease();
    return e;
}

bool GibletsEffect::initGiblets() {
    m_time = 0;
    m_giblets = retainedParticles("particle_taggiblets2FX");
    if (m_giblets) {
        m_giblets->resetSystem();
        addChild(m_giblets);
    }
    SoundMgr::sharedSoundMgr()->playOnChannel("mr_giblets_tag", soundChannel(), true, false, false, false);
    m_tongue = sprite("anim_giblets_tongue_1.png");
    CCSize size = screenSize();
    const float w = size.width, h = size.height;
    if (m_tongue) {
        // from the right edge's middle up past the top left, in 1.5 s
        CCSize t = m_tongue->getContentSize();
        m_tongue->setAnchorPoint(ccp(0.5f, 0.5f));
        m_tongue->setPosition(ccp(w + t.width / 7.0f, h * 0.5f));
        m_tongue->setRotation(65);
        addChild(m_tongue);
        m_tongue->runAction(CCMoveTo::create(1.5f, ccp(w / 7.0f, h + t.height * 0.5f)));
        if (m_giblets) {
            // the giblets a little behind it
            m_giblets->setPosition(
                ccp(static_cast<float>(static_cast<double>(w + t.width / 7.0f) + t.width / -1.6), h * 0.25f));
            m_giblets->runAction(CCMoveTo::create(
                1.5f, ccp(static_cast<float>(w / 7.0f - t.width / 1.1), static_cast<float>(h + t.height / 1.1))));
        }
    }
    return true;
}

GibletsEffect::~GibletsEffect() {
    releaseParticles(m_giblets);
    removeAllChildrenWithCleanup(true);
}

bool GibletsEffect::isAlive() { return hasParticles(m_giblets) || m_time < 1; }

void GibletsEffect::onRemoved() { SoundMgr::sharedSoundMgr()->stopChannel(soundChannel(), false); }

void GibletsEffect::stopGiblets() { setEmissionRate(m_giblets, 0); }

// ---------------------------------------------------------------- Saturday

SaturdayHeartEffect* SaturdayHeartEffect::create() {
    SaturdayHeartEffect* e = new SaturdayHeartEffect;
    e->m_type = kEffectSaturdayHeart;
    e->m_heart = sprite("ingame_comboheart-normal.png");  // (0x21aa54)
    if (!e->m_heart) e->m_heart = CCSprite::create();
    e->m_heart->setAnchorPoint(ccp(0.5f, 0.5f));
    e->addChild(e->m_heart);
    e->m_heart->retain();
    e->autorelease();
    return e;
}

SaturdayHeartEffect::~SaturdayHeartEffect() {
    CC_SAFE_RELEASE(m_heart);
    removeAllChildrenWithCleanup(true);
}

void SaturdayHeartEffect::moveTo(const CCPoint& p) {
    m_heart->runAction(CCEaseIn::create(CCMoveTo::create(0.35f, p), 0.5f));
}

SaturdayParticleEffect* SaturdayParticleEffect::create() {
    SaturdayParticleEffect* e = new SaturdayParticleEffect;
    e->m_type = kEffectSaturdayParticle;
    e->m_particles = retainedParticles("particle_tagsaturdayFX");  // (0x21b048)
    if (e->m_particles) {
        e->addChild(e->m_particles);
        e->m_particles->setDuration(-1);
    }
    e->m_time = 0;
    e->autorelease();
    return e;
}

SaturdayParticleEffect::~SaturdayParticleEffect() {
    releaseParticles(m_particles);
    removeAllChildrenWithCleanup(true);
}

bool SaturdayParticleEffect::isAlive() { return hasParticles(m_particles) || m_time < 0.5f; }

void SaturdayParticleEffect::stop() { setEmissionRate(m_particles, 0); }

void SaturdayParticleEffect::moveTo(const CCPoint& p) {
    if (m_particles) m_particles->runAction(CCEaseIn::create(CCMoveTo::create(0.35f, p), 0.5f));
}

// ---------------------------------------------------------------- BotEffect

BotEffect* BotEffect::create() {
    BotEffect* e = new BotEffect;
    e->m_type = kEffectBot;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();  // (0x214ba8)
    sound->playOnChannel("bot_time_slow", e->soundChannel(), false, true, false, false);
    sound->playOnChannel("bot_time_loop", e->soundChannel(), true, false, false, false);
    e->m_time = 0;
    e->m_particles = retainedParticles("particle_nuclearFX");
    if (e->m_particles) e->addChild(e->m_particles);
    e->autorelease();
    return e;
}

BotEffect::~BotEffect() {
    releaseParticles(m_particles);
    removeAllChildrenWithCleanup(true);
}

bool BotEffect::isAlive() { return hasParticles(m_particles) || m_time < 1; }

void BotEffect::end() {
    SoundMgr::sharedSoundMgr()->playOnChannel("bot_time_toaster", soundChannel(), false, true, false, false);
    setEmissionRate(m_particles, 0);
}

void BotEffect::setPosition(const CCPoint& p) {
    place(m_particles, ccp(static_cast<float>(p.x * 0.4), static_cast<float>(p.y * 0.95)));
}

// ---------------------------------------------------------------- FlowMasterEffect

FlowMasterEffect* FlowMasterEffect::create() {
    FlowMasterEffect* e = new FlowMasterEffect;
    e->m_type = kEffectFlowMaster;
    e->m_glow = sprite("ingame_combometer_nuclearGlow.png");  // (0x2144e4)
    if (!e->m_glow) e->m_glow = CCSprite::create();
    e->m_glow->setAnchorPoint(ccp(0.5f, 0.5f));
    e->m_glow->setOpacity(0xff);
    e->m_glow->setBlendFunc({GL_ONE, GL_ONE});  // additive
    e->m_glow->runAction(CCRepeatForever::create(static_cast<CCActionInterval*>(
        CCSequence::create(CCFadeTo::create(0.5f, 0x7f), CCFadeTo::create(0.5f, 0xff), nullptr))));
    e->addChild(e->m_glow);
    e->m_glow->retain();
    e->autorelease();
    return e;
}

FlowMasterEffect::~FlowMasterEffect() {
    CC_SAFE_RELEASE(m_glow);
    removeAllChildrenWithCleanup(true);
}

void FlowMasterEffect::setPosition(const CCPoint& p) {
    CCSize s = m_glow->getContentSize();
    m_glow->setPosition(
        ccp(static_cast<float>(p.x + s.width * 0.272), static_cast<float>(p.y + s.height * 0.263)));
}

// ---------------------------------------------------------------- BrutalityParticlesEffect

BrutalityParticlesEffect* BrutalityParticlesEffect::create() {
    BrutalityParticlesEffect* e = new BrutalityParticlesEffect;
    e->m_type = kEffectBrutality;
    e->m_time = 0;  // (0x2daae4)
    e->m_cloud = retainedParticles("particle_BrutalityCloudFX");
    if (e->m_cloud) e->addChild(e->m_cloud);
    e->m_sparkle = retainedParticles("particle_BrutalitySparkleFX");
    if (e->m_sparkle) e->addChild(e->m_sparkle);
    e->autorelease();
    return e;
}

BrutalityParticlesEffect::~BrutalityParticlesEffect() {
    releaseParticles(m_cloud);
    releaseParticles(m_sparkle);
    removeAllChildrenWithCleanup(true);
}

bool BrutalityParticlesEffect::isAlive() { return hasParticles(m_cloud) || hasParticles(m_sparkle) || m_time < 1; }

void BrutalityParticlesEffect::setPosition(const CCPoint& p) {
    place(m_cloud, p);
    place(m_sparkle, p);
}

// ---------------------------------------------------------------- the baton

BatonInEffect* BatonInEffect::create() {
    BatonInEffect* e = new BatonInEffect;
    e->m_type = kEffectBatonIn;
    e->initBaton();
    e->autorelease();
    return e;
}

bool BatonInEffect::initBaton() {
    m_baton = CCNode::create();
    m_baton->retain();
    m_baton->setAnchorPoint(ccp(0.5f, 0));
    m_angle = randomInt(0, 359);
    m_baton->setRotation(static_cast<float>(m_angle - 15));
    m_hand = sprite("anim_truncheon_hand_alone_1.png");
    m_truncheon = sprite("anim_truncheon_alone_1.png");
    for (CCSprite* s : {m_truncheon, m_hand})
        if (s) {
            s->setAnchorPoint(ccp(0.5f, 0));
            m_baton->addChild(s);
        }
    // swung 15 degrees and down to its size in a tenth of a second
    m_baton->runAction(CCEaseOut::create(CCRotateTo::create(0.1f, static_cast<float>(m_angle)), 0.05f));
    m_baton->setScale(1.5f);
    m_baton->runAction(CCEaseOut::create(CCScaleTo::create(0.1f, 1), 0.05f));
    addChild(m_baton);
    return true;
}

BatonInEffect::~BatonInEffect() {
    CC_SAFE_RELEASE(m_baton);
    removeAllChildrenWithCleanup(true);
}

void BatonInEffect::setPosition(const CCPoint& p) {
    // the baton's foot nine tenths of the hand's height back from the point
    m_baton->setPosition(p);
    double a = m_angle * 0.017453292519943295;
    double reach = static_cast<float>((m_hand ? m_hand->getContentSize().height : 0) * 0.9);
    double s = std::sin(a), c = std::cos(a);
    m_baton->setPosition(ccp(static_cast<float>(p.x + (c * 0.0 - s * reach)),
                             static_cast<float>(p.y + (s * 0.0 - c * reach))));
}

BatonOutEffect* BatonOutEffect::create(int angle) {
    BatonOutEffect* e = new BatonOutEffect;
    e->m_type = kEffectBatonOut;
    e->initBaton(angle);
    e->autorelease();
    return e;
}

bool BatonOutEffect::initBaton(int angle) {
    SoundMgr::sharedSoundMgr()->playRandomSound("Brutality_sample", 4, false);
    m_baton = CCNode::create();
    m_baton->retain();
    m_baton->setAnchorPoint(ccp(0.5f, 0));
    m_angle = angle - 15;
    m_baton->setRotation(static_cast<float>(angle));
    m_hand = sprite("anim_truncheon_hand_alone_1.png");
    m_truncheon = sprite("anim_truncheon_alone_1.png");
    for (CCSprite* s : {m_truncheon, m_hand})
        if (s) {
            s->setAnchorPoint(ccp(0.5f, 0));
            m_baton->addChild(s);
        }
    // swung back 15 degrees, growing
    m_baton->runAction(CCEaseOut::create(CCRotateTo::create(0.1f, static_cast<float>(m_angle)), 0.1f));
    m_baton->setScale(1);
    m_baton->runAction(CCEaseOut::create(CCScaleTo::create(0.1f, 1.5f), 0.1f));
    addChild(m_baton);
    return true;
}

BatonOutEffect::~BatonOutEffect() {
    CC_SAFE_RELEASE(m_baton);
    removeAllChildrenWithCleanup(true);
}

// ---------------------------------------------------------------- the Reverand

ReverandCursorEffect* ReverandCursorEffect::create() {
    ReverandCursorEffect* e = new ReverandCursorEffect;
    e->m_type = kEffectReverandCursor;
    e->m_light = retainedParticles("particle_tagreverandFX");  // (0x1f0e60)
    if (e->m_light) e->addChild(e->m_light);
    e->m_rate = emissionRate(e->m_light);
    e->autorelease();
    return e;
}

ReverandCursorEffect::~ReverandCursorEffect() {
    releaseParticles(m_light);
    removeAllChildrenWithCleanup(true);
}

void ReverandCursorEffect::touchBegan(const CCPoint& p) {
    setPosition(p);
    setEmissionRate(m_light, m_rate);
}

ReverandToolsEffect* ReverandToolsEffect::create() {
    ReverandToolsEffect* e = new ReverandToolsEffect;
    e->m_type = kEffectReverandTools;
    // (the port's) the unused "auto_tool_twinkle_loop" (Sounds/: no code or
    // data of either build plays it) under the blessing while the Reverend
    // picks the tools
    SoundMgr::sharedSoundMgr()->playOnChannel("auto_tool_twinkle_loop", e->soundChannel(), true, true, false, false);
    e->autorelease();
    return e;
}

void ReverandToolsEffect::onRemoved() { SoundMgr::sharedSoundMgr()->stopChannel(soundChannel(), false); }

ReverandToolsEffect::~ReverandToolsEffect() {
    for (ParticleSystem* p : m_systems) p->release();
    removeAllChildrenWithCleanup(true);
}

void ReverandToolsEffect::addToolParticles(UIInstance& hud) {
    for (auto& item : hud.byName) {
        if (item.first.find("tooleffect") == std::string::npos) continue;
        ParticleSystem* p = ParticleSystem::create("particle_tagreverand2FX");
        if (!p) continue;
        p->setDuration(-1);
        addChild(p);
        p->retain();
        m_systems.push_back(p);
        if (item.second) p->setPosition(item.second->getPosition());
    }
}

void ReverandToolsEffect::followTools(UIInstance& hud) {
    // (the port's) each system on its spot again, in addToolParticles' order
    size_t i = 0;
    for (auto& item : hud.byName) {
        if (item.first.find("tooleffect") == std::string::npos) continue;
        if (i >= m_systems.size()) break;
        ParticleSystem* p = m_systems[i++];
        if (item.second) p->setPosition(item.second->getPosition());
    }
}

ReverandEndEffect* ReverandEndEffect::create() {
    ReverandEndEffect* e = new ReverandEndEffect;
    e->m_type = kEffectReverandEnd;
    e->initEnd();
    e->autorelease();
    return e;
}

bool ReverandEndEffect::initEnd() {
    CCSize win = CCDirector::sharedDirector()->getWinSize();
    m_left = CCLayerGradient::create(ccc4(255, 255, 255, 0), ccc4(255, 255, 255, 0x96), ccp(-1, 0));
    m_left->setContentSize(CCSizeMake(win.width * 0.25f, win.height));
    m_right = CCLayerGradient::create(ccc4(255, 255, 255, 0), ccc4(255, 255, 255, 0x96), ccp(1, 0));
    m_right->setContentSize(CCSizeMake(win.width * 0.25f, win.height));
    m_right->setPosition(ccp(win.width * 0.75f, 0));
    m_left->retain();
    m_right->retain();
    // twice: a tenth of a second, then out over half a second (a fade out
    // starts from full again)
    for (CCLayerGradient* g : {m_left, m_right}) {
        g->runAction(CCRepeat::create(CCSequence::create(CCDelayTime::create(0.1f), CCFadeOut::create(0.5f), nullptr), 2));
        addChild(g);
    }
    return true;
}

ReverandEndEffect::~ReverandEndEffect() {
    CC_SAFE_RELEASE(m_left);
    CC_SAFE_RELEASE(m_right);
    removeAllChildrenWithCleanup(true);
}

// ---------------------------------------------------------------- ToolsSheenEffect

ToolsSheenEffect* ToolsSheenEffect::create() {
    ToolsSheenEffect* e = new ToolsSheenEffect;
    e->m_type = kEffectToolsSheen;  // (its constructor, 0x221378)
    SoundMgr* sound = SoundMgr::sharedSoundMgr();  // (0x21b220)
    sound->playOnChannel("tool_boost_charge", e->soundChannel(), false, true, false, false);
    // (the port's: the unused "tool_boost_start" as the boost takes over
    // from the charge, before its loop)
    sound->playOnChannel("tool_boost_start", e->soundChannel(), false, false, false, false);
    sound->playOnChannel("tool_boost_loop", e->soundChannel(), true, false, false, false);
    e->autorelease();
    return e;
}

void ToolsSheenEffect::onRemoved() {
    SoundMgr::sharedSoundMgr()->stopChannel(soundChannel(), false);
    // (the port's: the unused "tool_boost_end" as the boost goes, at the
    // surgery's end: SurgeryScreen::win/lose take it away)
    SoundMgr::sharedSoundMgr()->playSound("tool_boost_end", false);
}

void ToolsSheenEffect::addToolSheens(UIInstance& hud) {
    for (auto& item : hud.byName) {
        if (item.first.find("tooleffect") == std::string::npos) continue;
        CCNode* button = item.second;
        if (!button || static_cast<int>(m_sheens.size()) >= kMaxSheens) continue;
        CCSprite* holder = CCSprite::create();
        holder->setAnchorPoint(CCPointZero);
        CCSprite* sheen = sprite("toolsheenFX.png");
        if (!sheen) continue;
        sheen->setAnchorPoint(CCPointZero);
        addChild(holder);
        m_sheens.push_back(sheen);
        CCSize s = sheen->getContentSize();
        // (scaled with the spot: the port's bar across the top scales the
        // tools' buttons and their spots, SurgeryHud.cpp; 1 at the sides)
        const float k = button->getScaleX(), ky = button->getScaleY();
        holder->setScaleX(k);
        holder->setScaleY(ky);
        holder->setPosition(ccp(button->getPositionX() + s.width * k * -0.5f, button->getPositionY() + s.height * ky * -0.5f));
        sheen->setPosition(ccp(sheen->getPositionX() + s.width * -0.5f, sheen->getPositionY() + s.height * 0.5f));
        m_start = sheen->getPosition();
        // The original showed the sheen through a CCScrollView (an
        // extension the port does not have) used only to clip it to the
        // button's face: a clipping node with a rectangle the scroll view's
        // size does the same.
        CCSize view = CCSizeMake(static_cast<float>(s.width * 0.6225), static_cast<float>(s.height * 0.548));
        CCClippingNode* clip = CCClippingNode::create();
        clip->setContentSize(view);
        clip->setAnchorPoint(CCPointZero);
        clip->setPosition(ccp(static_cast<float>(s.width * 0.217), static_cast<float>(s.height * 0.2425)));
        CCDrawNode* face = CCDrawNode::create();
        CCPoint corners[4] = {ccp(0, 0), ccp(view.width, 0), ccp(view.width, view.height), ccp(0, view.height)};
        ccColor4F white = {1, 1, 1, 1};
        face->drawPolygon(corners, 4, white, 0, white);
        clip->setStencil(face);
        clip->addChild(sheen);
        holder->addChild(clip);
        // across in 2 s, back at once, a pause of 1.5 s, for ever
        CCArray* steps = CCArray::create();
        steps->addObject(CCMoveTo::create(2, ccp(m_start.x + s.width * 1.5f, m_start.y - s.height * 1.5f)));
        steps->addObject(CCMoveTo::create(0, m_start));
        steps->addObject(CCDelayTime::create(1.5f));
        sheen->runAction(CCRepeatForever::create(CCSequence::create(steps)));
    }
}

void ToolsSheenEffect::followTools(UIInstance& hud) {
    // (the port's) each sheen's holder (its clip's parent) on its spot again,
    // the spots taken in the same order as addToolSheens
    size_t i = 0;
    for (auto& item : hud.byName) {
        if (item.first.find("tooleffect") == std::string::npos) continue;
        CCNode* button = item.second;
        if (!button || i >= m_sheens.size()) continue;
        CCSprite* sheen = m_sheens[i++];
        CCNode* holder = sheen->getParent() ? sheen->getParent()->getParent() : nullptr;
        if (!holder) continue;
        const CCSize s = sheen->getContentSize();
        const float k = button->getScaleX(), ky = button->getScaleY();
        holder->setScaleX(k);
        holder->setScaleY(ky);
        holder->setPosition(ccp(button->getPositionX() + s.width * k * -0.5f, button->getPositionY() + s.height * ky * -0.5f));
    }
}

}  // namespace mt
