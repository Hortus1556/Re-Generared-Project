// The tools' own effects (made for the selected tool by
// EffectList::selectToolEffect, the screen's 0x3df88; the chainsaw's trail is
// the tool's cursor instead): particles where the finger is while the tool
// is held, and the chainsaw's shake.
#pragma once

#include <string>
#include <vector>

#include "as3/Effect.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

// The lighter's flame and smoke under the finger (0xdc bytes; made by 0x635b0).
class LighterEffect : public Effect {
public:
    static LighterEffect* create();  // 0x635b0
    ~LighterEffect() override;       // 0x63748

    bool isAlive() override;  // 0x63820: emitting, or particles left
    // the systems emit again (at their files' rates) where the finger is (0x6389c)
    void touchBegan(const cocos2d::CCPoint& p) override;
    void touchMoved(const cocos2d::CCPoint& p) override { setPosition(p); }  // 0x638cc
    void touchEnded(const cocos2d::CCPoint&) override;                       // 0x638d4: they stop emitting
    // the systems placed (the node itself stays where it is) (0x638fc)
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;

private:
    bool initLighter();  // 0x63694

    ParticleSystem* m_smoke = nullptr;  // +0xcc particle_smokeFX
    float m_smokeRate = 0;              // +0xd0 its file's emission rate
    ParticleSystem* m_flame = nullptr;  // +0xd4 particle_lighterFX
    float m_flameRate = 0;              // +0xd8
};

// The cream's four particle systems under the finger (0xec bytes; 0x63d68).
class CreamEffect : public Effect {
public:
    static CreamEffect* create();  // 0x63d68
    ~CreamEffect() override;       // 0x63f0c

    bool isAlive() override;  // 0x63ff0
    void touchBegan(const cocos2d::CCPoint& p) override;  // 0x64054
    void touchMoved(const cocos2d::CCPoint& p) override { setPosition(p); }  // 0x64084
    void touchEnded(const cocos2d::CCPoint&) override;    // 0x6408c
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x640ac

private:
    bool initCream();  // 0x63e4c

    ParticleSystem* m_systems[4] = {};  // +0xcc particle_cream1FX..4FX (z 4..1)
    float m_rates[4] = {};              // +0xdc
};

// The scanner's blips (0xe4 bytes; 0x10b598). While the scanner is held the
// screen (0x39104) asks it every `interval` seconds (tick) to show what is
// under the finger: it clears the old blips (clear), adds one at the finger
// or at each thing found, coloured by what it is (addBlip), and starts them
// (restart). The interval is the scanner tool's (ScannerTool +0x52c).
class ScannerEffect : public Effect {
public:
    enum BlipColour { kBlack = 0, kRed = 1, kOrange = 2, kGreen = 3 };

    static ScannerEffect* create(float interval);  // 0x10b598
    ~ScannerEffect() override;                     // 0x10b6cc

    bool isAlive() override;           // 0x10b958
    void update(float dt) override;    // 0x10ba14: the time since the last scan
    bool tick();                       // 0x10ba2c: an interval has passed (and the time restarts)
    void addBlip(const cocos2d::CCPoint& at, int colour);  // 0x10b814
    void restart();                    // 0x10b9a8
    void clear();                      // 0x10ba4c
    void touchBegan(const cocos2d::CCPoint& p) override;  // 0x10ba8c
    void touchEnded(const cocos2d::CCPoint& p) override;  // 0x10bac8: the blips go

private:
    float m_interval = 0;                 // +0xcc
    float m_rate = 0;                     // +0xd0: 1 / interval
    float m_time = 0;                     // +0xd4
    std::vector<ParticleSystem*> m_blips;  // +0xd8 (retained)
};

// The chainsaw's trail: not a listed effect but the tool's cursor (the
// screen's +0x2d34, added at z 225 by 0x39d08 when the chainsaw is selected,
// 0x3900c), so it follows the finger as a node. A trail of particles, and
// sparks thrown out on both sides of the way it moves (0xe8 bytes; 0x9a174).
class ChainsawTrailEffect : public Effect {
public:
    static ChainsawTrailEffect* create();  // 0x9a174
    ~ChainsawTrailEffect() override;       // 0x9a388

    bool isAlive() override { return false; }  // 0x9a46c (not in the list: not asked)
    // moves, and turns the sparks across the way it moved (0x9a470)
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;
    // shown: the systems emit at their files' rates; hidden: they stop (the
    // node stays visible, the particles left run out) (0x9a554)
    void setVisible(bool visible) override;

private:
    bool initTrail();  // 0x9a278

    ParticleSystem* m_trail = nullptr;   // +0xcc particle_chainsawTrail
    ParticleSystem* m_sparks1 = nullptr;  // +0xd0 particle_chainsawFX, at +90 degrees
    ParticleSystem* m_sparks2 = nullptr;  // +0xd4 particle_chainsawFX, at -90 degrees
    cocos2d::CCPoint m_last;             // +0xd8: the last position
    float m_trailRate = 0;               // +0xe0
    float m_sparksRate = 0;              // +0xe4
};

// The chainsaw's shake (0xa48 bytes; 0x647a8): while it runs, every sprite
// of the stage's background (if a picture), its extras and its injuries
// carries a half-transparent copy of itself that jitters to and fro, so
// the body seems to shake. Held, the shake grows over 2 s; let go, it dies
// down over a second, then the copies go.
class ChainsawShakeEffect : public Effect {
public:
    // `background`: the stage's background's name (the screen's per-stage
    // string, +0x580 + stage * 0x40): its sprite `bg` shakes only if it is a
    // picture (".png"); the injuries' layer and the background's extras
    // (the screen's +0x1014, +0x1010) always do (0x647a8)
    static ChainsawShakeEffect* create(const std::string& background, cocos2d::CCNode* bg,
                                       cocos2d::CCNode* injuryLayer, cocos2d::CCNode* bgExtras);
    ~ChainsawShakeEffect() override;  // 0x646f8

    bool isAlive() override { return m_alive; }  // 0x64b6c
    void update(float dt) override;              // 0x649b0
    void touchBegan(const cocos2d::CCPoint& p) override;  // 0x64bd0
    void touchMoved(const cocos2d::CCPoint&) override {}  // 0x64e0c
    void touchEnded(const cocos2d::CCPoint&) override { toolStopped(); }  // 0x64e10
    void toolStopped() override;                          // 0x64e18: let go
    // the copies taken off their sprites (0x64e54)
    void cleanup() override;

    // a copy of every sprite in `node` and under it (0x64c24); the screen
    // calls it for an injury made while the chainsaw is selected
    void addGhosts(cocos2d::CCNode* node);
    // the copy of `sprite` shows its frame again (0x64b74; the screen calls
    // it when an injury's sprite changes frame, event 4)
    void refreshGhost(cocos2d::CCNode* sprite);

private:
    static constexpr unsigned kMaxGhosts = 200;  // MaxInjurySpriteCount
    struct Ghost {                     // 0xc bytes at +0xd8
        cocos2d::CCSprite* sprite = nullptr;  // retained, a child of the sprite it copies
        cocos2d::CCPoint direction;     // (cos r, 1 - cos r) of its sprite's rotation r
    };
    void removeGhosts();

    cocos2d::CCNode* m_bg = nullptr;           // +0xcc (nullptr unless a picture)
    cocos2d::CCNode* m_injuryLayer = nullptr;  // +0xd0
    cocos2d::CCNode* m_bgExtras = nullptr;     // +0xd4
    std::vector<Ghost> m_ghosts;               // +0xd8, count +0xa38
    int m_angle = 0;          // +0xa3c: whole degrees
    float m_strength = 0;     // +0xa40: up to 2 held, 1 at the most once let go
    bool m_held = false;      // +0xa44
    bool m_alive = false;     // +0xa45
    bool m_ghostsMade = false;  // +0xa46
};

}  // namespace mt
