// The effects an injury or a tool asks for with an "effect" event (event 3,
// its kind at +0x14; run by the screen's 0x4c0c8 / 0x4b294, made by
// EffectList::eventEffect): a hurt flash on the screen's edges, a shock, an
// electric burst, blood, a poison puff, an explosion, a colour wash over the
// screen, the tools frozen or poisoned.
#pragma once

#include <vector>

#include "as3/Animation.h"
#include "as3/Effect.h"
#include "as3/UI.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

// Red at the screen's left and right edges, fading out after a second
// (0xd4 bytes; 0x64ff0; kind 0).
class DamageEffect : public Effect {
public:
    static DamageEffect* create();  // 0x64ff0
    ~DamageEffect() override;       // 0x64f18
    bool isAlive() override;        // 0x65284: either side still fading

private:
    bool initDamage();  // 0x650c8
    cocos2d::CCLayerGradient* m_left = nullptr;   // +0xcc
    cocos2d::CCLayerGradient* m_right = nullptr;  // +0xd0
};

// Blue at the screen's edges for 0.7 s (0xd0 bytes; 0x127ec; kind 1). The
// edges sit in a sprite that blinks its opacity three times then fades to
// nothing; but a sprite's opacity does not reach its children in cocos2d-x
// 2.1.1, so the edges show steadily until the effect ends, in the original
// too (kept: no one reported it).
class ShockEffect : public Effect {
public:
    static ShockEffect* create();  // 0x1277ec
    ~ShockEffect() override;       // 0x127724
    bool isAlive() override;       // 0x127a98

private:
    bool initShock();  // 0x1278c4
    cocos2d::CCSprite* m_holder = nullptr;  // +0xcc
};

// Six electric bursts about a point, at random angles a sixth of a turn
// apart and random sizes (0xe4 bytes; 0xaa228; kind 2).
class ElectricityEffect : public Effect {
public:
    static ElectricityEffect* create();  // 0xaa228
    ~ElectricityEffect() override;       // 0xaa144
    bool isAlive() override;             // 0xaa3bc
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0xaa3f8

private:
    bool initElectricity();  // 0xaa30c
    ParticleSystem* m_systems[6] = {};  // +0xcc particle_electricityFX
};

// Six blood bursts of 0.4 s at a point (0xd8 bytes; 0x1116f4; kind 3).
class BloodSpurtEffect : public Effect {
public:
    static BloodSpurtEffect* create();  // 0x1116f4
    ~BloodSpurtEffect() override;       // 0x111878
    bool isAlive() override;            // 0x11196c
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x1119ac

private:
    bool initSpurt();  // 0x1117ec
    std::vector<ParticleSystem*> m_systems;  // +0xcc particle_bloodFX
};

// A frog's poison puff at a point (0xd8 bytes; 0x1270ac; kind 4).
class BluePoisonCloudEffect : public Effect {
public:
    static BluePoisonCloudEffect* create();  // 0x1270ac
    ~BluePoisonCloudEffect() override;       // 0x1271f4
    bool isAlive() override;                 // 0x127304
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x127344

private:
    std::vector<ParticleSystem*> m_systems;  // +0xcc particle_frogpoisonpuffFX
};

// An explosion of four systems at a point (0xdc bytes; 0x16fb5c; kind 6).
// It ends when the four stop emitting (their particles go with it).
class ExplosionEffect : public Effect {
public:
    static ExplosionEffect* create();  // 0x16fb5c
    ~ExplosionEffect() override;       // 0x16fa68
    bool isAlive() override;           // 0x16fd18
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x16fd68

private:
    bool initExplosion();  // 0x16fc40
    ParticleSystem* m_systems[4] = {};  // +0xcc particle_explosion1FX..4FX
};

// A colour washed over the whole screen: it fades in, holds, then fades out
// in a second. Three classes in the binary with the same code
// (BlueScreenEffect, GreenScreenEffect, WhiteScreenEffect, 0xd8 bytes
// each); the port shares it here. While one has not begun fading out, the
// screen renews it instead of making another (kind 5: EffectList).
class ScreenWashEffect : public Effect {
public:
    ~ScreenWashEffect() override;
    bool isAlive() override;            // vf87: before the hold's end, or fading
    void renew() override { m_time = 0; }  // vf88: the hold starts again
    bool isEnding() override;           // vf89: fading out
    void update(float dt) override;     // the hold, then the fade out

protected:
    // the colour at 0x7f, made transparent, fading in to 90 over `fadeIn`
    // seconds, held until `hold` seconds
    bool initWash(int type, const cocos2d::ccColor3B& colour, float fadeIn, float hold);

private:
    cocos2d::CCLayerColor* m_layer = nullptr;  // +0xcc
    float m_time = 0;       // +0xd0
    bool m_fading = false;  // +0xd4
    float m_hold = 0;
};

// Blue for 8 s, fading in over 3 (0x127c0c; vf87 0x127dd8, vf88 0x127dd0,
// vf89 0x127e04, update 0x127d74; kind 5).
class BlueScreenEffect : public ScreenWashEffect {
public:
    static BlueScreenEffect* create();  // 0x127c0c
};

// Green for 4.25 s (0x27d5cc; vf87 0x27d798, vf88 0x27d790, vf89 0x27d7c4,
// update 0x27d734; kind 11).
class GreenScreenEffect : public ScreenWashEffect {
public:
    static GreenScreenEffect* create();  // 0x27d5cc
};

// White for 3.75 s (0x17617c; vf87 0x176344, vf88 0x17633c, vf89 0x176370,
// update 0x1762e0; kind 8).
class WhiteScreenEffect : public ScreenWashEffect {
public:
    static WhiteScreenEffect* create();  // 0x17617c
};

// Ice over each tool's button for 1.75 s, then it thaws (0x104 bytes;
// 0x170130; kind 7). The screen gives it its HUD (addToolAnimations).
class FrozenToolsEffect : public Effect {
public:
    static FrozenToolsEffect* create();  // 0x170130
    ~FrozenToolsEffect() override;
    bool isAlive() override;          // 0x1703d0
    void update(float dt) override;   // 0x170360
    // a frostbite animation on each of the HUD's "tooleffect" items (0x170404)
    void addToolAnimations(UIInstance& hud);

private:
    bool initFrozen();  // 0x170214
    static constexpr int kMaxAnimations = 11;
    float m_time = 0;     // +0xcc
    bool m_thawed = false;  // +0xd0
    std::vector<Animation*> m_animations;  // +0xd4 (count +0x100; children)
};

// Poison bubbling over each tool's button for 3.75 s (0x104 bytes;
// 0x282fe0; kind 10).
class PoisonedToolsEffect : public Effect {
public:
    static PoisonedToolsEffect* create();  // 0x282fe0
    bool isAlive() override;          // 0x283158
    void update(float dt) override;   // 0x2830cc
    // a poison animation, growing, on each of the HUD's "tooleffect" items (0x28319c)
    void addToolAnimations(UIInstance& hud);

private:
    static constexpr int kMaxAnimations = 11;
    float m_time = 0;     // +0xcc
    bool m_done = false;  // +0xd0
    std::vector<Animation*> m_animations;  // +0xd4 (count +0x100; children)
};

}  // namespace mt
