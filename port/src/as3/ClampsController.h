// mt::ClampsController (vtable 0x415400; 0x554 bytes; ctor 0x75380): the
// clamps tool. A double tap on an injury's clamp hit point (its hit areas
// "for_clamp", found by the injury's clamp hit test, vf10) puts a clamp there
// (2 at most, 4 with the upgrade); turning a clamp's wheel round with the
// finger tightens it (each step of the wheel by the tool's rate, twice as
// fast with the use upgrade) until it is fully tightened, which marks its
// hit point done (the injury's class, e.g. Artery, then goes on). A double
// tap on a placed clamp takes it off. A clamp left loose shows, after 2 s,
// an arrow and a hand going round its wheel. The injuries' own clamps list
// (+0xfd84) holds the clamps on them; an injury done with them asks for
// them all off (event 14, InjuryState::releaseClamps), and a finished
// injury's clamps go with it (0x33188).
#pragma once

#include <vector>

#include "as3/Animation.h"
#include "as3/ToolState.h"

namespace mt {

// A clamp on an injury (0x68 bytes; made by 0x69aa8).
struct Clamp {
    cocos2d::CCNode* node = nullptr;        // +0: at the hit point, turned with it
    cocos2d::CCSprite* wheel = nullptr;     // +4
    Animation* anim = nullptr;              // +8: anim_tool_clamp_anim.json "close", by its tightness
    cocos2d::CCSprite* arrow = nullptr;     // +0xc: the hint
    cocos2d::CCSprite* hand = nullptr;      // +0x10: the hint
    cocos2d::CCSprite* cogs[3] = {};        // +0x14..+0x1c (the first two never shown)
    cocos2d::CCSprite* bolts[2] = {};       // +0x20, +0x24
    float tightness = 1;                    // +0x2c
    float maxTightness = 100;               // +0x30
    HitPointState* point = nullptr;         // +0x34
    InjuryState* injury = nullptr;          // +0x38
    bool hintShown = false;                 // +0x3c
    float hintTimer = 0;                    // +0x40
    float hintDelay = 2;                    // +0x44
    int hintSpeed = 4;                      // +0x48: degrees a frame
    float handAngle = 0;                    // +0x50: degrees
    float handRadius = 2.6f;                // +0x54: a frame's step
    float boltX[2] = {};                    // +0x58, +0x5c
    float boltTravel = 30;                  // +0x64: canvas points × the canvas scale
};

class ClampsController : public ToolState {
public:
    explicit ClampsController(int maxClamps);  // 0x75380
    ~ClampsController() override;              // 0x75158

    void begin(const cocos2d::CCPoint& touch) override;  // vf2 0x68e18: the taps' times
    void moved(const cocos2d::CCPoint& touch) override;  // vf3 0x68828: turning a wheel
    void end(const cocos2d::CCPoint& touch) override;    // vf4 0x68e90
    void update(float dt) override;                      // vf5 0x68c98: the hints
    void misuse(const cocos2d::CCPoint& touch) override; // vf8 0x6a784
    void stop() override;                                // vf10 0x6a728
    void setEfficiency(float e) override;                // vf11 0x6a6f4

    // the screen's use of it each frame (0x39624): a placed clamp under the
    // touch takes it (its wheel to turn, or a double tap takes it off); true
    // if one did (0x68ec0)
    bool touchClamps(const cocos2d::CCPoint& touch);
    // a clamp hit point under the touch (the injury's clamp hit test): a
    // double tap there puts a clamp on it (0x694cc)
    void place(const HitResult& r);
    // every clamp off (event 14: 0x6a290)
    void releaseAll();
    // the clamps of an injury off (it is finished: 0x6a2f8)
    void releaseFor(InjuryState* injury);
    // one clamp off: its point open again (event 13); freed (0x69150)
    void releaseClamp(Clamp* c);
    int count() const { return static_cast<int>(clamps.size()); }

    std::vector<Clamp*> clamps;  // +0x520
    int maxClamps = 2;           // +0x52c
    int frameCount = 29;         // +0x530: "close"'s last frame
    float rate = 0.1f;           // +0x534: tightness a degree of the wheel
    float doubleTap = 0.8f;      // +0x544: seconds between two taps

private:
    Clamp* clampAt(const cocos2d::CCPoint& touch);           // 0x69048
    bool onWheel(Clamp* c, const cocos2d::CCPoint& touch);   // 0x6875c
    Clamp* makeClamp(const cocos2d::CCPoint& at, int rotation);  // 0x69aa8
    void unclampPoint(Clamp* c);
    bool doubleTapped() const;
    static int angleOf(Clamp* c, const cocos2d::CCPoint& touch);

    std::vector<double> m_taps;  // +0x538: the last two presses' times
    Clamp* m_current = nullptr;  // +0x548: the clamp being turned
    int m_lastAngle = -1;        // +0x54c: the wheel's angle under the finger (-1 none)
    bool m_tapUsed = true;       // +0x550
};

}  // namespace mt
