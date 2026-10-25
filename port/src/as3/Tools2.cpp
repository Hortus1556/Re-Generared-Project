#include "as3/Tools2.h"

#include <cmath>
#include <ctime>

#include "as3/Log.h"
#include "platform/Platform.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr float kPi = 3.14159265358979f;

SoundMgr* sounds() { return SoundMgr::sharedSoundMgr(); }

}  // namespace

bool toolMisuseDue(ToolState* t) {
    // (0x652ac: 3 s; compared with whole seconds of time())
    long now = static_cast<long>(platform::passTime());  // (time(): the pass clock, see Injection::now)
    if (static_cast<float>(now - t->lastTime) <= 3.0f) return false;
    t->lastTime = now;
    return true;
}

void postToolDamageEffect(ToolState* t) {
    InjuryEvent d;
    d.type = InjuryEvent::kEffect;
    d.effect = 0;  // DamageEffect
    t->post(d);
}

CCPoint toolCanvasPoint(const CCPoint& p) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    return ccp(p.x / s + (640.0f - r->width / s) * 0.5f, p.y / s + (384.0f - r->height / s) * 0.5f);
}

// ---- Cutter ----

void Cutter::moved(const CCPoint&) {
    // (the channel keeps the loop going while it is dragged: not restarted)
    sounds()->playOnChannel("pizza_cutter_loop", channel(), false, false, false, false);
}

void Cutter::stop() {
    if (!active) return;
    ToolState::stop();
    sounds()->stopChannel(channel(), true);
}

void Cutter::misuse(const CCPoint&) {
    if (toolMisuseDue(this)) postToolDamageEffect(this);
}

void Cutter::setEfficiency(float e) {
    power *= e;
    damage *= (1.0f - e) + 1.0f;
}

// ---- Chainsaw ----

Chainsaw::Chainsaw() {
    damage = 0.02f;
    lastTime = 0;
    zOrder = 9;
}

void Chainsaw::begin(const CCPoint& touch) {
    // (vf2) not while it slides on by itself; no cooldown
    if (slipping) return;
    heldTime = 0;
    warmUp = warmUp0;
    start();
    slideTime = 0;
    slipping = false;
    slideStep = CCPointZero;
    followTool = false;
    last = toolCanvasPoint(touch);
}

void Chainsaw::setPosition(const CCPoint& p) {
    // (vf7) the way it moved is measured on the canvas, from where it was
    if (slipping) return;
    last = toolCanvasPoint(position);
    position = p;
}

void Chainsaw::update(float dt) {
    // (vf5, each frame even let go) sliding: a step a frame, for a second
    m_damageFrame = 0;
    if (!slipping) return;
    position.x += slideStep.x;
    position.y += slideStep.y;
    slideTime += dt;
    if (slideDuration <= slideTime) slipping = false;
}

void Chainsaw::frame(float dt) {
    // (vf6) moved faster than it can be held (its squared canvas distance
    // over the frame's time) once held for a moment: it gets away
    heldTime += dt;
    CCPoint now = toolCanvasPoint(position);
    float dx = now.x - last.x, dy = now.y - last.y;
    float distSq = dx * dx + dy * dy;
    if (!slipping && maxSpeed < distSq / dt && warmUpTime < heldTime) {
        slipping = true;
        usefulThisFrame = false;
        followTool = true;
        slideStep = ccp(dx * 6.0f, dy * 6.0f);
        heldTime = 0;
        loseControl();
    }
    m_damageFrame = damage;
}

void Chainsaw::loseControl() {
    // (0x6b080) the "lost control" popup (tag popup kind 3), the patient's
    // damage and scream; let go (its sound stays: no power-off)
    InjuryEvent pop;
    pop.type = InjuryEvent::kTagPopup;
    pop.kind = 3;
    pop.x = std::floor(position.x);
    pop.y = std::floor(position.y);
    post(pop);
    InjuryEvent d;
    d.type = InjuryEvent::kDamage;
    d.damage = wrongUsageDamage;
    post(d);
    InjuryEvent s;
    s.type = InjuryEvent::kEvent19;  // the scream
    post(s);
    sounds()->playOnChannel("chainsaw_lose_control", channel(), false, true, false, false);
    ToolState::stop();
    log("chainsaw lost control at %.0f,%.0f", position.x, position.y);  // (for test scripts)
}

void Chainsaw::misuse(const CCPoint&) {
    if (toolMisuseDue(this)) postToolDamageEffect(this);
}

void Chainsaw::start() {
    active = true;
    sounds()->playOnChannel("chainsaw_startup", channel(), false, true, false, false);
    sounds()->playOnChannel("chainsaw_loop_mid", channel(), true, false, false, false);
}

void Chainsaw::stop() {
    if (!active) return;
    heldTime = 0;
    ToolState::stop();
    sounds()->playOnChannel("chainsaw_poweroff", channel(), false, true, false, false);
}

void Chainsaw::setEfficiency(float e) { damage *= (1.0f - e) + 1.0f; }

// ---- Vacuum ----

void Vacuum::start() {
    active = true;
    sounds()->playOnChannel("vacuum_startup", channel(), false, true, false, false);
    sounds()->playOnChannel("vacuum_loop", channel(), true, false, false, false);
}

void Vacuum::stop() {
    if (!active) return;
    ToolState::stop();
    sounds()->playOnChannel("vacuum_poweroff", channel(), false, true, false, false);
}

// ---- Tazer ----

void Tazer::begin(const CCPoint& touch) {
    // (vf2) the base's start; the electricity shows even while cooling down
    ToolState::begin(touch);
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = 2;  // ElectricityEffect
    e.x = std::floor(touch.x);
    e.y = std::floor(touch.y);
    post(e);
}

void Tazer::start() {
    active = true;
    sounds()->playRandomSound("battery_zap", 3, false);
}

void Tazer::misuse(const CCPoint& touch) {
    // (vf8) a damage effect and a burn where it touched
    if (!toolMisuseDue(this)) return;
    postToolDamageEffect(this);
    InjuryEvent b;
    b.type = InjuryEvent::kCreateInjury;
    b.name = "Burn";
    b.x = std::floor(touch.x);
    b.y = std::floor(touch.y);
    b.rotation = 0;
    post(b);
}

void Tazer::setEfficiency(float e) { damage *= (1.0f - e) + 1.0f; }

// ---- Scanner ----

Scanner::Scanner() {
    zOrder = 9;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    range = 75.0f * s;
    rangeSq = range * range;
    nearSq = (25.0f * s) * (25.0f * s);
    // (its node: the arrow upgrade's arrow is drawn on it)
    sprite = CCSprite::create();
    sprite->retain();
}

void Scanner::setEfficiency(float e) {
    float r = range * e;
    rangeSq = r * r;
}

void Scanner::stop() {
    // (vf10) the arrow goes with the arrow upgrade
    ToolState::stop();
    if (upgradeLevel != 2) return;
    removeArrow();
}

int Scanner::colourFor(float distSq) const {
    if (distSq <= nearSq) return 3;   // green
    return rangeSq < distSq ? 1 : 2;  // red, orange
}

const char* Scanner::soundFor(float distSq) const {
    if (distSq <= nearSq) return "radar_blip_positive";
    if (distSq <= rangeSq) return "radar_blip_neutral";
    return "radar_blip_negative";
}

void Scanner::pointArrow(const CCPoint& from, const CCPoint& target) {
    if (!arrow) {
        arrow = CCSprite::create();
        if (CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("tool_scannerarrow.png"))
            arrow->setDisplayFrame(f);
        sprite->addChild(arrow);
    }
    float a = std::atan2(target.y - from.y, target.x - from.x);
    // (whole degrees, as the original)
    arrow->setRotation(90.0f - static_cast<float>(static_cast<int>(a * 180.0f / kPi)));
}

void Scanner::removeArrow() {
    if (arrow) sprite->removeChild(arrow, true);
    arrow = nullptr;
}

}  // namespace mt
