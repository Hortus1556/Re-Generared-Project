#include "as3/Injection.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "as3/Log.h"
#include "platform/Platform.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/Tools2.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kPlungerLength = 45;  // canvas points of a full push
// the plunger's grab rect in the syringe (canvas points; 0x44f4e8: x, y, w, h)
constexpr float kGrabX = -35, kGrabY = 144, kGrabW = 70, kGrabH = 45;

CCSprite* spriteNamed(const char* name) {
    CCSprite* s = CCSprite::createWithSpriteFrameName(name);
    return s ? s : CCSprite::create();
}

}  // namespace

Injection::Injection() {
    zOrder = 10;
    hasCooldown = true;
    upgradeLevel = 0;
    m_mask = spriteNamed("tool_injection-mask.png");
    m_plastic = spriteNamed("tool_injection-plastic.png");
    m_plunger = spriteNamed("tool_injection-plunger.png");
    m_tip = spriteNamed("tool_injection-tip.png");
    m_mask->retain();
    m_fluidWidth = m_plunger->getContentSize().width * 0.5f;
    m_fluidHeight = static_cast<float>(m_plunger->getContentSize().height / 3.85);
    m_fluid = CCLayerColor::create(ccc4(0, 0, 0, 255), m_fluidWidth, m_fluidHeight);
    const CCPoint anchor = ccp(0.52f, 0.08f);
    m_fluid->setAnchorPoint(ccp(0.5f, 0));
    m_mask->setAnchorPoint(anchor);
    m_plunger->setAnchorPoint(anchor);
    m_plastic->setAnchorPoint(anchor);
    m_tip->setAnchorPoint(anchor);
    // its node, hidden until it is stuck in: the tip, the fluid, the
    // plunger, the plastic over them
    sprite = CCSprite::create();
    sprite->retain();
    sprite->setVisible(false);
    sprite->addChild(m_tip);
    sprite->addChild(m_fluid);
    sprite->addChild(m_plunger);
    sprite->addChild(m_plastic);
    m_fluid->setPositionX(static_cast<float>(m_fluidWidth / -1.8));
    m_fluid->setPositionY(static_cast<float>(m_fluidHeight / 1.4));
}

Injection::~Injection() {
    if (m_mask) m_mask->release();
}

double Injection::now() {
    // (CFAbsoluteTimeGetCurrent: seconds; the port reads the pass clock, as
    // touches come once a pass: it also holds in fast-forwarded test runs)
    return platform::passTime();
}

void Injection::push(float amount) {
    if (m_pushes.size() >= 16) m_pushes.pop_front();
    m_pushes.push_back({amount, now()});
}

float Injection::recent() const {
    double t = now();
    float sum = 0;
    for (const Push& p : m_pushes)
        if (t - p.time <= window) sum += p.amount;
    return sum;
}

void Injection::start() {
    // (vf9) stuck in; its settings for the mode
    target = nullptr;
    active = true;
    planted = true;
    sprite->setVisible(true);
    if (mode == 1) {
        // (the original kept the out mode's full settings once it had been
        // used; the in mode starts from its own here: 1.45's constructor's,
        // 0x8e230: 0.7, 0.85, 0.8; 1.02's were 0.5, 0.8, 0.6)
        maxRate = 0.7f;
        healRate = 0.85f;
        travel = 0.8f;
        cooldown = 4.0f;
        if (upgradeLevel > 0) {
            healRate = 1.0f;
            travel = 1.0f;
            if (upgradeLevel > 1) {
                cooldown = 2.0f;
                if (upgradeLevel > 2) {
                    maxRate = 1.0f;
                    if (upgradeLevel > 3) healRate = 1.2f;
                }
            }
        }
        fill = 0;
    } else {
        maxRate = 1.0f;
        healRate = 1.0f;
        travel = 1.0f;
        fill = travel;
    }
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    m_plungerOffset = s * (-(fill * travel * kPlungerLength) - (1.0f - travel) * kPlungerLength);
    drawPlunger();
}

void Injection::stop() {
    // (vf10)
    if (planted) log("injection out fill %.2f cooldown %.1f", fill, cooldownLeft);  // (for test scripts: the bars on its button)
    SoundMgr::sharedSoundMgr()->stopChannel(channel(), false);
    active = false;
    planted = false;
    pulling = false;
    sprite->setVisible(false);
    target = nullptr;
    m_damageFrame = 0;
}

void Injection::frame(float) {
    m_damageFrame = 0;
    drawPlunger();
}

void Injection::drawPlunger() {
    // the plunger down by its fill, the fluid above it; in, the fluid goes
    // from green to red as it is pushed faster
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    float y = s * (-(fill * travel * kPlungerLength) - (1.0f - travel) * kPlungerLength);
    m_plunger->setPositionY(y);
    m_fluid->setContentSize(CCSizeMake(m_fluidWidth, y + m_fluidHeight));
    if (mode == 1) {
        float r = recent() / maxRate;
        auto byte = [](float v) { return static_cast<GLubyte>(std::min(255.0f, std::max(0.0f, v))); };
        m_fluid->setColor(ccc3(byte(r * 255.0f), byte((r * -0.5f + 1.0f) * 255.0f), 0));
    }
}

void Injection::setFluidColour(const ccColor3B& c) { m_fluid->setColor(c); }

ccColor3B Injection::colourForClass(int classIndex) {
    // (0x79afc; the class indices of 0x42bdc8)
    switch (classIndex) {
    case 0x11: return ccc3(0xb1, 0xe1, 0x1a);
    case 0x13: return ccc3(0x96, 0xa5, 0x00);
    case 0x1c: return ccc3(0x32, 0xa5, 0x00);
    case 0x20: return ccc3(0x96, 0xbb, 0x61);
    case 0x3d: return ccc3(0x61, 0x9e, 0xc0);
    default: return ccc3(0x96, 0xbb, 0x61);
    }
}

bool Injection::onPlunger(const CCPoint& touch) {
    // (0x6cb28) in the syringe's frame, canvas points, the grab rect moving
    // down with the plunger
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCPoint local = sprite->convertToNodeSpace(touch);
    float lx = local.x / s;
    float ly = local.y / s + fill * (travel * kPlungerLength);
    return kGrabX < lx && lx < kGrabX + kGrabW && kGrabY < ly && ly < kGrabY + kGrabH;
}

void Injection::begin(const CCPoint& touch) {
    // (vf2) stuck in already: a touch takes its plunger, or not
    if (planted) {
        pulling = onPlunger(touch);
        return;
    }
    if (cooldownLeft > 0 && mode == 1) return;
    start();
    // its body turned towards the middle of the canvas
    CCPoint c = toolCanvasPoint(touch);
    float a = std::atan2(192.0f - c.y, 320.0f - c.x);
    sprite->setRotation(90.0f - a * 180.0f / kPi);
    // (for test scripts: where its plunger is taken, screen points)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCPoint grab = sprite->convertToWorldSpace(
        ccp((kGrabX + kGrabW * 0.5f) * s, (kGrabY + kGrabH * 0.5f - fill * travel * kPlungerLength) * s));
    CCPoint top = sprite->convertToWorldSpace(ccp(0, 122.0f * s));
    CCPoint empty = sprite->convertToWorldSpace(ccp(0, (122.0f + travel * kPlungerLength) * s));
    log("injection in at %.0f,%.0f mode %d plunger %.0f,%.0f full %.0f,%.0f empty %.0f,%.0f", position.x, position.y,
        mode, grab.x, grab.y, top.x, top.y, empty.x, empty.y);
}

void Injection::setPosition(const CCPoint& p) {
    if (planted) return;
    position = p;
}

void Injection::moved(const CCPoint& touch) {
    // (vf3) the plunger dragged: down to inject, up to extract
    if (!pulling) return;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCPoint local = sprite->convertToNodeSpace(touch);
    float range = travel * kPlungerLength;
    float top = range + 122.0f;
    float y = std::max(122.0f, local.y / s);
    if (top < y) y = top;
    float f = (top - y) / range;
    float delta = f - fill;
    // (1.45 0x8ea38) a move the wrong way by up to 0.05 of the travel is let
    // pass, and the fill follows only a move the right way (1.02's 0x6ccd4
    // took the fill from every move and punished any wrong one)
    const float kSlack = 0.05f;
    SoundMgr* snd = SoundMgr::sharedSoundMgr();
    if (mode == 0) {
        snd->playOnChannel("Extraction_Syringe", channel(), false, false, false, false);
        if (delta <= kSlack) {
            if (!(delta < 0)) return;
            fill = std::min(1.0f, std::max(0.0f, f));
            push(-delta);
            if (recent() <= maxRate) {
                // (event 7: what was drawn; the screen does nothing with it,
                // the injury reads the fill in its hit test)
                InjuryEvent e;
                e.type = InjuryEvent::kNone7;
                e.value = -delta;
                e.x = std::floor(position.x);
                e.y = std::floor(position.y);
                post(e);
                if (fill == 0) stop();
            } else {
                misuse(touch);
                failed(wrongUsageDamage, true);
            }
        } else {
            misuse(touch);
            failed(wrongUsageDamage, false);
        }
    } else if (mode == 1) {
        snd->playOnChannel("Injection_Loop", channel(), false, false, false, false);
        if (delta < -kSlack) {
            cooldownLeft = cooldown;
            misuse(touch);
            failed(wrongUsageDamage, false);
            stop();
        } else if (delta > 0) {
            fill = std::min(1.0f, std::max(0.0f, f));
            push(delta);
            if (recent() <= maxRate) {
                InjuryEvent h;
                h.type = InjuryEvent::kHealTimes25;
                h.value = delta * healRate;
                h.x = std::floor(position.x);
                h.y = std::floor(position.y);
                post(h);
                if (0.995f <= fill) {
                    cooldownLeft = cooldown;
                    stop();
                }
            } else {
                cooldownLeft = cooldown;
                misuse(touch);
                failed(wrongUsageDamage, true);
                stop();
            }
        }
    }
}

void Injection::failed(int dmg, bool tooFast) {
    InjuryEvent pop;
    pop.type = InjuryEvent::kTagPopup;
    pop.kind = tooFast ? 3 : 2;
    pop.x = std::floor(position.x);
    pop.y = std::floor(position.y);
    post(pop);
    InjuryEvent d;
    d.type = InjuryEvent::kDamage;
    d.damage = dmg;
    post(d);
}

void Injection::misuse(const CCPoint&) {
    // (vf8) what it is stuck in follows it; a damage effect; out
    if (target) target->dragged(type, position);
    if (toolMisuseDue(this)) postToolDamageEffect(this);
    stop();
    m_damageFrame = 0;
}

}  // namespace mt
