// The tag partners' effects: the high five when one is called in, each
// partner's power (Mr Giblets' tongue, Lady Saturday's heart, the Reverand's
// blessing, Flow Master's glow, Bot's time slowing, Brutality's baton, the
// tools' boost). (The Super Serum's syringe, the revive's, went with it.)
#pragma once

#include <string>
#include <vector>

#include "as3/Effect.h"
#include "as3/InjuryState.h"
#include "as3/UI.h"
#include "engine/cocos/CCClippingNode.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

// The partner called in (0x1a8 bytes; 0x167fa8, made by 0x33758; the
// screen keeps it at +0x2e58 and, while it runs, updates it alone; when it
// ends the partner's power starts, 0x35cf0): the screen split in two
// colours along a slanted white bar, rays turning in each half, Ophelia
// sliding in from the left and the partner from the right, high-fiving
// over a burst of particles, with a fanfare. 1.6 s.
class HighFiveEffect : public Effect {
public:
    // `partner`: the tag partner's id ("giblets"...: its art's name) (0x167fa8)
    static HighFiveEffect* create(const std::string& partner);
    ~HighFiveEffect() override;  // 0x1692e8
    bool isAlive() override { return m_time > 0; }  // 0x169178
    void update(float dt) override;                  // 0x1691d4
    // (never forwarded by the screen) it starts again (0x169190)
    void touchEnded(const cocos2d::CCPoint&) override;

private:
    bool initHighFive(const std::string& partner);  // 0x168158
    void slideIn();                                 // 0x168f98
    void stopSliding();                             // 0x1691a4

    float m_raySpeed = 90;  // +0xcc: degrees a second
    cocos2d::ccColor4B m_leftColour = {0xed, 0x86, 0x18, 0xff};   // +0xd0
    cocos2d::ccColor4B m_rightColour = {0x40, 0x77, 0x9c, 0xff};  // +0xd4
    cocos2d::ccColor4B m_barColour = {0xff, 0xff, 0xff, 0xff};    // +0xd8
    cocos2d::ccColor3B m_leftRays = {0xff, 0xd6, 0x5c};           // +0xdc
    cocos2d::ccColor3B m_rightRays = {0x89, 0xf0, 0xa8};          // +0xdf
    float m_rayX = 20, m_rayY = 40;  // +0xe4, +0xe8: the rays' centre off the corner, canvas points
    int m_barWidth = 17;             // +0xec, canvas points
    float m_slideTime = 0.7f;        // +0xf0
    float m_duration = 1.6f;         // +0xf4
    cocos2d::CCLayerColor* m_left = nullptr;     // +0xf8
    cocos2d::CCLayerColor* m_right = nullptr;    // +0xfc
    cocos2d::CCClippingNode* m_clip = nullptr;   // +0x100
    cocos2d::CCLayerColor* m_bar = nullptr;      // +0x104
    cocos2d::CCSprite* m_ophelia = nullptr;      // +0x108
    cocos2d::CCSprite* m_opheliaArm = nullptr;   // +0x10c
    cocos2d::CCSprite* m_partner = nullptr;      // +0x110
    cocos2d::CCSprite* m_partnerArm = nullptr;   // +0x114
    ParticleSystem* m_leftLine = nullptr;        // +0x118 particle_lineFX
    ParticleSystem* m_rightLine = nullptr;       // +0x11c
    ParticleSystem* m_hands = nullptr;           // +0x120 particle_taghandFX, added at the high five
    float m_time = 0;                            // +0x124
    std::vector<cocos2d::CCSprite*> m_leftRayList;   // +0x128 (15)
    std::vector<cocos2d::CCSprite*> m_rightRayList;  // +0x168 (15)
};

// Mr Giblets' tongue licking across the screen with a trail of giblets
// (0xd8 bytes; 0x1e673c). The screen stops the giblets when the power ends.
class GibletsEffect : public Effect {
public:
    static GibletsEffect* create();  // 0x1e673c
    ~GibletsEffect() override;       // 0x1e6674
    bool isAlive() override;         // 0x1e6c10: giblets left, or a second
    void update(float dt) override { m_time += dt; }  // 0x1e6be0
    void onRemoved() override;       // 0x1e6c08: its sound stops
    void stopGiblets();              // 0x1e6bf8
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint&) override {}  // 0x1e6c44: it places itself

private:
    bool initGiblets();  // 0x1e6818
    float m_time = 0;                       // +0xcc
    cocos2d::CCSprite* m_tongue = nullptr;  // +0xd0
    ParticleSystem* m_giblets = nullptr;    // +0xd4 particle_taggiblets2FX
};

// Lady Saturday's heart flying to the combo meter's heart (0xd0 bytes;
// 0x21a97c): a revival used (0x35138).
class SaturdayHeartEffect : public Effect {
public:
    static SaturdayHeartEffect* create();  // 0x21a97c
    ~SaturdayHeartEffect() override;
    bool isAlive() override { return runsActions(m_heart); }  // 0x21aab0
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override { m_heart->setPosition(p); }  // 0x21aac4
    void moveTo(const cocos2d::CCPoint& p);  // 0x21aad0: 0.35 s, easing in

private:
    cocos2d::CCSprite* m_heart = nullptr;  // +0xcc
};

// Lady Saturday's sparkle following the heart (0xd8 bytes; 0x21af70; made
// when the heart arrives, 0x35cf0). The screen stops it (stop).
class SaturdayParticleEffect : public Effect {
public:
    static SaturdayParticleEffect* create();  // 0x21af70
    ~SaturdayParticleEffect() override;
    bool isAlive() override;          // 0x21b090: particles, or half a second
    void update(float dt) override { m_time += dt; }  // 0x21b0d4
    void stop();                      // 0x21b0c4
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override { place(m_particles, p); }  // 0x21b0ec
    // (the screen uses the heart's move on it too: 0x21aad0 moves its +0xcc)
    void moveTo(const cocos2d::CCPoint& p);

private:
    ParticleSystem* m_particles = nullptr;  // +0xcc particle_tagsaturdayFX
    float m_time = 0;                       // +0xd4
};

// Bot slowing time: a nuclear glow at the combo meter's heart and its
// sounds (0xd4 bytes; 0x214ad0). end (0x214e18) when the power ends.
class BotEffect : public Effect {
public:
    static BotEffect* create();  // 0x214ad0
    ~BotEffect() override;
    bool isAlive() override;  // 0x214de4: particles, or a second
    void update(float dt) override { m_time += dt; }  // 0x214dcc
    void end();               // 0x214e18: the toaster's ding, the glow stops
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x214f68: at (0.4 x, 0.95 y)

private:
    float m_time = 0;                       // +0xcc
    ParticleSystem* m_particles = nullptr;  // +0xd0 particle_nuclearFX
};

// Flow Master's glow pulsing on the combo meter's score box (0xd0 bytes;
// 0x21440c). Until the screen removes it.
class FlowMasterEffect : public Effect {
public:
    static FlowMasterEffect* create();  // 0x21440c
    ~FlowMasterEffect() override;       // 0x214344
    bool isAlive() override { return true; }  // 0x2149cc
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x214588

private:
    cocos2d::CCSprite* m_glow = nullptr;  // +0xcc
};

// Brutality's cloud and sparkles where the baton struck (0xd8 bytes; 0x2daa0c).
class BrutalityParticlesEffect : public Effect {
public:
    static BrutalityParticlesEffect* create();  // 0x2daa0c
    ~BrutalityParticlesEffect() override;
    bool isAlive() override;  // 0x2dab58: particles, or a second
    void update(float dt) override { m_time += dt; }  // 0x2dab40
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x2daba0

private:
    float m_time = 0;                     // +0xcc
    ParticleSystem* m_cloud = nullptr;    // +0xd0 particle_BrutalityCloudFX
    ParticleSystem* m_sparkle = nullptr;  // +0xd4 particle_BrutalitySparkleFX
};

// Brutality's baton swung onto an injury (0xe0 bytes; 0x2c93a4): a hand and
// truncheon at a random angle, swung 15 degrees in 0.1 s; its tip at the
// point set. When it ends the screen strikes (0x35cf0: BatonOutEffect,
// BrutalityParticlesEffect, a TagEffect, the injury done).
class BatonInEffect : public Effect {
public:
    static BatonInEffect* create();  // 0x2c93a4
    ~BatonInEffect() override;       // 0x2c92dc
    bool isAlive() override { return runsActions(m_baton); }  // 0x2c96e0
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x2c95d0
    void setInjury(InjuryState* injury) { m_injury = injury; }  // 0x2c95c8
    InjuryState* injury() const { return m_injury; }            // +0xcc
    int angle() const { return m_angle; }                       // +0xd0
    cocos2d::CCNode* baton() const { return m_baton; }          // +0xd4

private:
    bool initBaton();  // 0x2c947c
    InjuryState* m_injury = nullptr;       // +0xcc (not owned)
    int m_angle = 0;                       // +0xd0
    cocos2d::CCNode* m_baton = nullptr;    // +0xd4
    cocos2d::CCSprite* m_hand = nullptr;   // +0xd8
    cocos2d::CCSprite* m_truncheon = nullptr;  // +0xdc
};

// The baton swung away again, with one of Brutality's grunts (0xdc bytes;
// 0x2cefd8; at the BatonInEffect's baton, given its angle).
class BatonOutEffect : public Effect {
public:
    static BatonOutEffect* create(int angle);  // 0x2cefd8
    ~BatonOutEffect() override;
    bool isAlive() override { return runsActions(m_baton); }  // 0x2cf3b8
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override { m_baton->setPosition(p); }  // 0x2cf3ac

private:
    bool initBaton(int angle);  // 0x2cf0b4
    int m_angle = 0;                       // +0xcc
    cocos2d::CCNode* m_baton = nullptr;    // +0xd0
    cocos2d::CCSprite* m_hand = nullptr;   // +0xd4
    cocos2d::CCSprite* m_truncheon = nullptr;  // +0xd8
};

// The Reverand's holy light under the finger while his power lasts (0xd4
// bytes; 0x1f0d88; the screen's partner cursor, +0x2e54).
class ReverandCursorEffect : public Effect {
public:
    static ReverandCursorEffect* create();  // 0x1f0d88
    ~ReverandCursorEffect() override;       // 0x1f0cc0
    bool isAlive() override { return true; }  // 0x1f67ac
    void touchBegan(const cocos2d::CCPoint& p) override;  // 0x1f0ea0
    void touchMoved(const cocos2d::CCPoint& p) override { setPosition(p); }  // 0x1f0ec0
    void touchEnded(const cocos2d::CCPoint&) override { setEmissionRate(m_light, 0); }  // 0x1f0ec8
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override { place(m_light, p); }  // 0x1f0ed8

private:
    ParticleSystem* m_light = nullptr;  // +0xcc particle_tagreverandFX
    float m_rate = 0;                   // +0xd0
};

// The Reverand's blessing over each tool's button (0xdc bytes; 0x1f67b0).
class ReverandToolsEffect : public Effect {
public:
    static ReverandToolsEffect* create();  // 0x1f67b0
    ~ReverandToolsEffect() override;       // 0x1f68b8
    bool isAlive() override { return true; }  // 0x1f7030
    void onRemoved() override;  // (the port's: its twinkle stops)
    // a particle_tagreverand2FX on each of the HUD's "tooleffect" items (0x1f69c8)
    void addToolParticles(UIInstance& hud);
    // (the port's) moved with the buttons when the HUD's layout changes
    // (SurgeryHud.cpp)
    void followTools(UIInstance& hud);

private:
    std::vector<ParticleSystem*> m_systems;  // +0xd0
};

// White flashing twice at the screen's edges as the Reverand's power ends
// (0xd4 bytes; 0x24350c).
class ReverandEndEffect : public Effect {
public:
    static ReverandEndEffect* create();  // 0x24350c
    ~ReverandEndEffect() override;
    bool isAlive() override { return runsActions(m_left) || runsActions(m_right); }  // 0x2437b8

private:
    bool initEnd();  // 0x2435e4
    cocos2d::CCLayerGradient* m_left = nullptr;   // +0xcc
    cocos2d::CCLayerGradient* m_right = nullptr;  // +0xd0
};

// The tools' boost: a sheen sweeping over each tool's button again and
// again, with its charge and loop (0x104 bytes; 0x21b160; made with the
// power, 0x36204).
class ToolsSheenEffect : public Effect {
public:
    static ToolsSheenEffect* create();  // 0x21b160
    bool isAlive() override { return true; }  // 0x221374
    void onRemoved() override;  // 0x21b404: its sound stops (the port's: then tool_boost_end)
    // a sheen over each of the HUD's "tooleffect" items (0x21b410)
    void addToolSheens(UIInstance& hud);
    // (the port's) moved and scaled with the buttons when the HUD's layout
    // changes (SurgeryHud.cpp)
    void followTools(UIInstance& hud);

private:
    static constexpr int kMaxSheens = 10;
    std::vector<cocos2d::CCSprite*> m_sheens;  // +0xcc (count +0xf8)
    cocos2d::CCPoint m_start;                  // +0xfc: the last sheen's start
};

}  // namespace mt
