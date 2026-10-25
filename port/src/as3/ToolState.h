// mt::ToolState (vtable 0x415220; 0x520 bytes): a surgery tool. The screen
// keeps one of each the surgery offers (made by 0x65318 with the upgrades the
// player owns, then given its tools.json settings: 0x4182c) and the one
// selected gets the touches: pressed (vf2 begin: if not cooling down, start
// it, vf9, and its cooldown starts), dragged (the screen hit-tests the
// injuries under it each frame with it and wears their hit areas by its
// power), released (vf4: stop, vf10). Like an injury it posts events
// (InjuryEvent) for the screen to run.
#pragma once

#include <string>
#include <vector>

#include "as3/InjuryState.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

class ToolState {
public:
    ToolState() = default;
    virtual ~ToolState();
    static ToolState* create(int tool);  // 0x65318
    // tuned as if every upgrade were owned (Crumpet's power; 0x667fc)
    void applyAllUpgrades();

    virtual void begin(const cocos2d::CCPoint& touch);  // vf2: pressed
    virtual void moved(const cocos2d::CCPoint&) {}      // vf3
    virtual void end(const cocos2d::CCPoint&) { stop(); }  // vf4: released
    virtual void update(float) {}                       // vf5
    virtual void frame(float) { m_damageFrame = 0; }    // vf6: each frame while held
    virtual void setPosition(const cocos2d::CCPoint& p) { position = p; }  // vf7
    // vf8: used where it does no good (the screen's misuse, 0x34d44); the
    // base moves the injury it holds
    virtual void misuse(const cocos2d::CCPoint& touch);
    virtual void start() { active = true; }             // vf9
    virtual void stop();                                // vf10
    virtual void setEfficiency(float e);                // vf11 (tool_mods; per tool)
    virtual void useOn(const HitResult&) {}             // vf12
    virtual void countUse() { ++uses; }                 // vf13

    void post(const InjuryEvent& e);  // 0x74838
    // its sounds' channel (the original keyed a channel by the tool's address)
    int channel() const { return 100 + type; }

    int uses = 0;               // +4
    int upgradeLevel = 0;       // +8
    int type = 0;               // +0xc
    float cooldown = 0;         // +0x10
    bool hasCooldown = false;   // +0x14
    int healthToGive = 0;       // +0x18
    int wrongUsageDamage = 0;   // +0x1c
    float timeBetweenWrongUsages = 0;  // +0x20
    float turnOnGrace = 0;      // +0x24
    float afterHealGrace = 0;   // +0x28
    bool multipleInjuries = false;  // +0x2c
    bool active = false;        // +0x30
    float cooldownLeft = 0;     // +0x34
    // +0x39: the screen's cursor (the chainsaw's trail) follows the tool's
    // position, not the touch (the chainsaw sliding away: 0x34c40)
    bool followTool = false;
    float power = 1;            // +0x3c: what a frame of it wears off a hit point
    cocos2d::CCPoint position;  // +0x44
    float damage = 0;           // +0x4c: the patient's damage a frame when it is no use
    float m_damageFrame = 0;    // +0x50
    std::vector<InjuryEvent> events;  // +0x54
    cocos2d::CCSprite* sprite = nullptr;  // +0x508
    long lastTime = 0;          // +0x50c: time() of its last misuse effect
    InjuryState* target = nullptr;  // +0x510
    bool usefulThisFrame = false;   // +0x514
    int zOrder = 0;             // +0x51c
};

}  // namespace mt

namespace mt {

// The tools with their own classes (vtables 0x4152c0, 0x415270, 0x415360).
class Lighter : public ToolState {
public:
    void start() override;   // vf9 0x671a4
    void stop() override;    // vf10 0x67388
    void misuse(const cocos2d::CCPoint& touch) override;  // vf8 0x673a8: burns the patient
    void setEfficiency(float e) override;  // vf11 0x67894
};

class Cream : public ToolState {
public:
    void start() override;   // vf9 0x66d80
    void stop() override;    // vf10 0x66ebc
    void frame(float dt) override;  // vf6 0x66ef4: the gel upgrade heals
    void setEfficiency(float e) override;  // vf11 0x66edc
    float healTimer = 0;     // +0x520
    float healEvery = 0;     // +0x524: 1 s with the gel upgrade
};

class Tongs : public ToolState {
public:
    Tongs();  // 0x7572c
    void start() override;   // vf9 0x6ba98
    void stop() override;    // vf10 0x6bbcc
    void end(const cocos2d::CCPoint&) override { stop(); }  // vf4 0x6bc08
    void frame(float) override {}  // vf6 0x6bc10
    void misuse(const cocos2d::CCPoint& touch) override;  // vf8 0x6bc14
    void setEfficiency(float e) override { reach *= e; }  // vf11 0x6bbec
    PieceState* piece = nullptr;  // +0x520: what it holds
    bool holdingPiece = false;    // +0x524
    float reach = 0;              // +0x528: the farthest a frame's pull keeps hold
    bool dropped = false;         // +0x52c
};

class Stapler : public ToolState {
public:
    void start() override;   // vf9 0x678c4
    void stop() override;    // vf10 0x679f8
    void frame(float dt) override;  // vf6 0x67a08: one staple a press, without the upgrade
    void misuse(const cocos2d::CCPoint& touch) override;  // vf8 0x67b80
    void setEfficiency(float e) override;  // vf11 0x67b50
};

}  // namespace mt
