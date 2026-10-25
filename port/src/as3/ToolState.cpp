#include "as3/ToolState.h"

#include <cmath>

#include "as3/Log.h"
#include "platform/Platform.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SoundMgr.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/ClampsController.h"
#include "as3/Injection.h"
#include "as3/Tools2.h"

#include <ctime>

namespace mt {

namespace {

// the factory asks only what was bought, not the surgery's all-upgrades flag
bool owns(const char* id) {
    return SaveData::shared().currentSlot().owns(SavedGame::kItemUpgrade, upgradeIndex(id));
}

}  // namespace

namespace {

// a misuse's damage effect at the touch: at most every 3 whole seconds of time() (0x652ac)
bool misuseDue(ToolState* t) {
    long now = static_cast<long>(platform::passTime());  // (time(): the pass clock, see Injection::now)
    if (static_cast<float>(now - t->lastTime) <= 3.0f) return false;
    t->lastTime = now;
    return true;
}

}  // namespace

ToolState::~ToolState() {
    if (sprite) sprite->release();
}

ToolState* ToolState::create(int tool) {
    // (0x65318: each tool's class, tuned by the upgrades owned)
    ToolState* t;
    switch (tool) {
    case kLighter:
        t = new Lighter;
        t->zOrder = 2;
        break;
    case kCream:
        t = new Cream;
        t->zOrder = 1;
        break;
    case kStapler:
        t = new Stapler;
        break;
    case kTongs:
        t = new Tongs;
        break;
    case kCutter:
        t = new Cutter;
        break;
    case kTazer:
        t = new Tazer;
        break;
    case kChainsaw:
        t = new Chainsaw;
        break;
    case kInjection:
        t = new Injection;
        break;
    case kVacuum:
        t = new Vacuum;
        break;
    case kClamp:
        // two clamps, four with the upgrade
        t = new ClampsController(owns("binding_4_clamp_upgrade") ? 4 : 2);
        break;
    case kScanner:
        t = new Scanner;
        break;
    default:
        t = new ToolState;
        break;
    }
    t->type = tool;
    switch (tool) {
    case kLighter:
        t->power = 0.5f;
        if (owns("binding_lighter_first_upgrade")) {
            t->upgradeLevel = 1;
            t->power = 0.75f;
        }
        if (owns("binding_lighter_use_upgrade")) {
            t->upgradeLevel = 2;
            t->power = 1.0f;
        }
        break;
    case kCream:
        if (owns("healing_gel_upgrade")) {
            t->upgradeLevel = 1;
            static_cast<Cream*>(t)->healEvery = 1.0f;
        }
        break;
    case kStapler:
        if (owns("binding_staple_upgrade")) t->upgradeLevel = 1;
        break;
    case kCutter:
        t->power = 0.7f;
        t->damage = 0.05f;
        if (owns("cutting_cutter_use_upgrade")) {
            t->upgradeLevel = 1;
            t->power = 1.0f;
        }
        if (owns("cutting_cutter_damage_upgrade")) {
            t->upgradeLevel = 2;
            t->damage = 0;
        }
        // (the original also asked "cutting_cut_timer_boost", an id it does
        // not have: std::map's [] made it upgrade 0, the cutter's first)
        if (owns("cutting_cutter_use_upgrade")) t->upgradeLevel = 3;
        break;
    case kTazer:
        if (owns("cutting_tazer_use_upgrade")) t->upgradeLevel = 1;
        break;
    case kTongs:
        if (owns("extraction_speed_upgrade")) {
            t->upgradeLevel = 1;
            t->setEfficiency(1.2f);
        } else {
            t->setEfficiency(0.7f);
        }
        break;
    case kChainsaw: {
        // without the chainsaw upgrade it gets away at 60% of the speed
        Chainsaw* c = static_cast<Chainsaw*>(t);
        if (owns("cutting_chainsaw_upgrade")) {
            t->upgradeLevel = 1;
            c->maxSpeed = c->maxSpeed0;
        } else {
            c->maxSpeed = c->maxSpeed0 * 0.6f;
        }
        if (owns("cutting_chainsaw_damage_upgrade")) {
            t->upgradeLevel = 2;
            t->setEfficiency(1.0f);
        } else {
            t->setEfficiency(0.6f);
        }
        break;
    }
    case kInjection:
        if (owns("healing_injection_fluid_upgrade")) t->upgradeLevel = 1;
        if (owns("healing_injection_cooldown_upgrade")) t->upgradeLevel = 2;
        if (owns("healing_injection_speed_upgrade")) t->upgradeLevel = 3;
        if (owns("healing_injection_use_upgrade")) t->upgradeLevel = 4;
        break;
    case kVacuum:
        if (owns("extraction_vacuum_upgrade")) {
            t->upgradeLevel = 1;
            t->setEfficiency(1.0f);
        } else {
            t->setEfficiency(0.5f);
        }
        break;
    case kClamp:
        // the use upgrade tightens twice as fast
        if (owns("binding_clamp_use_upgrade")) {
            t->upgradeLevel = 1;
            static_cast<ClampsController*>(t)->rate *= 2;
        }
        break;
    case kScanner:
        if (owns("extraction_scanner_cooldown_upgrade")) {
            t->upgradeLevel = 1;
            static_cast<Scanner*>(t)->interval = 0.5f;
            static_cast<Scanner*>(t)->interval2 = 0.5f;
        }
        if (owns("extraction_scanner_arrow_upgrade")) t->upgradeLevel = 2;
        if (owns("extraction_scanner_global_upgrade")) t->upgradeLevel = 3;
        break;
    default:
        logError("Unknown tool!: %d", tool);
        delete t;
        return nullptr;
    }
    return t;
}

void ToolState::applyAllUpgrades() {
    // (0x667fc) Crumpet's power: each tool as with every upgrade bought.
    // Fixed where the original's wasn't: its setEfficiency(1.0) on the
    // tongs, vacuum and chainsaw multiplies (vf11 0x6bbec, 0x6e188,
    // 0x6ae34), so a tool without its upgrade stayed as it was (the factory
    // had already scaled it by 0.7, 0.5, 0.6); it doubled the clamps' rate
    // again when their upgrade was owned (4 times as fast); it gave the
    // tazer a 2 a frame harm whenever a zap was of no use (+0x4c, 0x34470),
    // so a miss hurt 4 where the upgrade's 2 was meant; and it left the
    // scanner's interval (+0x52c) as it was.
    switch (type) {
    case kLighter:
        upgradeLevel = 1;
        power = 1.0f;
        break;
    case kCream:
        upgradeLevel = 1;
        static_cast<Cream*>(this)->healEvery = 1.0f;
        break;
    case kStapler:
        upgradeLevel = 1;
        break;
    case kCutter:
        power = 1.0f;
        damage = 0;
        upgradeLevel = 3;
        break;
    case kTazer:
        upgradeLevel = 1;
        wrongUsageDamage = 2;
        break;
    case kTongs:
        // (the speed upgrade's 1.2 rather than the factory's 0.7)
        if (upgradeLevel < 1) setEfficiency(1.2f / 0.7f);
        upgradeLevel = 1;
        break;
    case kVacuum:
        // (the upgrade's 1.0 rather than the factory's 0.5)
        if (upgradeLevel < 1) setEfficiency(2.0f);
        upgradeLevel = 1;
        break;
    case kChainsaw: {
        // (the damage upgrade's 1.0 rather than the factory's 0.6, which
        // made its harm 1.4 times)
        if (upgradeLevel < 2) damage /= 1.4f;
        upgradeLevel = 2;
        Chainsaw* c = static_cast<Chainsaw*>(this);
        c->maxSpeed = c->maxSpeed0;
        break;
    }
    case kInjection:
        upgradeLevel = 4;
        break;
    case kClamp: {
        ClampsController* c = static_cast<ClampsController*>(this);
        if (upgradeLevel < 1) c->rate *= 2;
        c->maxClamps = 4;
        upgradeLevel = 1;
        break;
    }
    case kScanner:
        upgradeLevel = 3;
        static_cast<Scanner*>(this)->interval = 0.5f;
        static_cast<Scanner*>(this)->interval2 = 0.5f;
        break;
    default:
        break;
    }
    // (traced for test scripts: what the tool now works with)
    float tuned = type == kTongs ? static_cast<Tongs*>(this)->reach
                  : type == kVacuum ? static_cast<Vacuum*>(this)->suction
                  : type == kClamp ? static_cast<ClampsController*>(this)->rate
                  : type == kScanner ? static_cast<Scanner*>(this)->interval
                                     : power;
    log("tool %s all upgrades: level %d damage %.3f misuse %d tuned %.3f", type >= 0 && type < kToolCount ? kToolNames[type] : "?",
        upgradeLevel, damage, wrongUsageDamage, tuned);
}

void ToolState::begin(const cocos2d::CCPoint&) {
    // (vf2) not while cooling down
    if (cooldownLeft > 0) return;
    start();
    if (hasCooldown) cooldownLeft = cooldown;
}

void ToolState::misuse(const cocos2d::CCPoint& touch) {
    // (vf8) the injury it holds follows
    if (target) target->dragged(type, touch);
}

void ToolState::stop() {
    // (vf10)
    active = false;
    target = nullptr;
    m_damageFrame = 0;
    uses = 0;
}

void ToolState::setEfficiency(float) {
    // (vf11: the base does nothing; tools with a power scale it)
}

void ToolState::post(const InjuryEvent& e) {
    if (events.size() < 15) events.push_back(e);
}

// ---- Lighter ----

void Lighter::start() {
    active = true;
    SoundMgr::sharedSoundMgr()->playOnChannel("lighter_start", channel(), false, true, false, false);
    SoundMgr::sharedSoundMgr()->playOnChannel("lighter_loop", channel(), true, false, false, false);
}

void Lighter::stop() {
    if (!active) return;
    ToolState::stop();
    SoundMgr::sharedSoundMgr()->stopChannel(channel(), true);
}

void Lighter::misuse(const cocos2d::CCPoint& touch) {
    if (!misuseDue(this)) return;
    InjuryEvent d;
    d.type = InjuryEvent::kEffect;
    d.effect = 0;  // DamageEffect
    post(d);
    InjuryEvent b;
    b.type = InjuryEvent::kCreateInjury;
    b.name = "Burn";
    b.x = std::floor(touch.x);
    b.y = std::floor(touch.y);
    b.rotation = 0;
    post(b);
}

void Lighter::setEfficiency(float e) {
    power *= e;
    damage *= (1.0f - e) + 1.0f;
}

// ---- Cream ----

void Cream::start() {
    active = true;
    SoundMgr::sharedSoundMgr()->playOnChannel("pain_gel_loop", channel(), true, true, false, false);
}

void Cream::stop() {
    if (!active) return;
    ToolState::stop();
    SoundMgr::sharedSoundMgr()->stopChannel(channel(), true);
}

void Cream::frame(float dt) {
    if (!active || healEvery <= 0) return;
    healTimer -= dt;
    if (healTimer <= 0) {
        healTimer = healEvery;
        InjuryEvent h;
        h.type = InjuryEvent::kHealOne;
        post(h);
    }
}

void Cream::setEfficiency(float e) { power *= e; }

// ---- Tongs ----

Tongs::Tongs() {
    // its reach: 50 points, 70 on a small screen, in screen points
    reach = (isSmallScreen() ? 70.0f : 50.0f) * ResolutionMgr::sharedResolutionMgr()->canvasScale;
    zOrder = 11;
}

void Tongs::start() {
    active = true;
    SoundMgr::sharedSoundMgr()->playRandomSound("tongs", 4, false);
}

void Tongs::stop() {
    if (!active) return;
    ToolState::stop();
    SoundMgr::sharedSoundMgr()->stopChannel(channel(), false);
}

void Tongs::misuse(const cocos2d::CCPoint&) {
    if (misuseDue(this)) {
        InjuryEvent d;
        d.type = InjuryEvent::kEffect;
        d.effect = 0;
        post(d);
    }
    stop();
}

// ---- Stapler ----

void Stapler::start() {
    active = true;
    SoundMgr::sharedSoundMgr()->playRandomSound("stapler", 5, false);
}

void Stapler::stop() { ToolState::stop(); }

void Stapler::frame(float) {
    // without the upgrade it lets go after one frame: a staple a press; with
    // it, it staples as it is swiped
    if (upgradeLevel == 0) stop();
    if (active) SoundMgr::sharedSoundMgr()->playRandomSound("stapler", 5, false);
}

void Stapler::misuse(const cocos2d::CCPoint&) {
    if (active && (upgradeLevel == 0 || uses == 0) && misuseDue(this)) {
        InjuryEvent d;
        d.type = InjuryEvent::kEffect;
        d.effect = 0;
        post(d);
    }
    stop();
}

void Stapler::setEfficiency(float e) {
    power *= e;
    damage *= (1.0f - e) + 1.0f;
}

}  // namespace mt
