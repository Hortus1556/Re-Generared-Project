// The surgery's feedback effects: the popups over a treated injury (TagEffect,
// CoinTagEffect), the end of the surgery (SuccessEffect, OutOfTimeEffect),
// the screen's edges whitening as the patient dies (DyingFadeEffect), the
// tag partner button's sparkle (SparkleEffect), and the tutorial's
// pointers (HighlightEffect, TutorialArrowEffect, TutorialGradientEffect).
#pragma once

#include <vector>

#include "as3/Effect.h"
#include "as3/InjuryState.h"
#include "as3/UI.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

// The edges of the screen whitening as the patient's health falls under 15
// (0xd4 bytes; 0x303de4). Made with the HUD (0x3f298; the screen keeps it
// at +0x2d4c) and given the health shown each frame (0x37850).
class DyingFadeEffect : public Effect {
public:
    static DyingFadeEffect* create();  // 0x303de4
    ~DyingFadeEffect() override;       // 0x303d0c
    bool isAlive() override { return true; }  // 0x3040f8
    // under 15 the edges whiten, up to 180 at 0 (100 inwards); else clear (0x304020)
    void setHealth(float health);

private:
    bool initFade();  // 0x303eb4
    cocos2d::CCLayerGradient* m_left = nullptr;   // +0xcc
    cocos2d::CCLayerGradient* m_right = nullptr;  // +0xd0
};

// "Success!" or "Out of time!" bouncing in at the screen's middle (0xcc
// bytes each; SuccessEffect 0x293e3c, made by 0x3d5a8 with the "Win_effect"
// sound; OutOfTimeEffect 0x2938e4, made by 0x3da74). They stay until the
// screen goes.
class EndPopupEffect : public Effect {
public:
    bool isAlive() override { return true; }  // 0x293fdc, 0x293a84

protected:
    bool initPopup(const char* frame);  // 0x293f20, 0x2939c8
};

class SuccessEffect : public EndPopupEffect {
public:
    static SuccessEffect* create();  // 0x293e3c: ingame_popup-success.png
};

class OutOfTimeEffect : public EndPopupEffect {
public:
    static OutOfTimeEffect* create();  // 0x2938e4: ingame_popup-outoftime.png
};

// Sparkles that follow an injury while it is not finished: the tutorial's
// pointer to it (0xd4 bytes; 0x1d9e2c; its "highlight" action, 0x3c238).
class HighlightEffect : public Effect {
public:
    static HighlightEffect* create(InjuryState* injury);  // 0x1d9e2c
    ~HighlightEffect() override;                          // 0x1d9f88
    bool isAlive() override { return !m_injury->finished; }  // 0x1da050
    void update(float dt) override;                       // 0x1da064: it follows the injury

private:
    ParticleSystem* m_particles = nullptr;  // +0xcc particle_highlightFX
    InjuryState* m_injury = nullptr;        // +0xd0 (not owned)
};

// Sparkles on the tag partner button while the partner can be called (the
// screen keeps it at +0x2d48; 0xd8 bytes; 0x111d68).
class SparkleEffect : public Effect {
public:
    static SparkleEffect* create();  // 0x111d68
    ~SparkleEffect() override;       // 0x111ec8
    bool isAlive() override;         // 0x112030
    void restart();                  // 0x111fd8
    void stop();                     // 0x112004
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x112070

private:
    std::vector<ParticleSystem*> m_systems;  // +0xcc particle_tagFX (endless)
};

// A feedback popup (0x2128 bytes; 0xa17b0): the box of
// common/ui_feedback_popups.json spun in and faded out over 1.55 s, with
// what it says:
//   kind 0, 1 (an injury treated, a combo): the box in the medal colour of
//     the points' tens (bronze 10.., silver, gold, diamond, rainbow), "+<points>"
//     and a word of praise as good as the medal;
//   kind 2 (a miss), 4 (the wrong tool): the "fail" box and "NOOO!",
//     "FAIL!", "MISS!" or "WRONG TOOL";
//   kind 3 (too fast): the "fail" box and "TOO FAST!" or "SLOW DOWN!";
//   kind 5 (a tag partner's bonus): the medal box and praise only.
// Its content size is the box's; the screen places it with its centre half
// that above the point (0x34d44, 0x4c0c8).
class TagEffect : public Effect {
public:
    enum Kind { kGood = 0, kCombo = 1, kMiss = 2, kTooFast = 3, kWrongTool = 4, kPartner = 5 };

    // `halve`: the points' half picks the medal (the tag partner's flag,
    // +0x1d) (0xa17b0)
    static TagEffect* create(int kind, int points, bool halve);
    ~TagEffect() override;
    bool isAlive() override;  // 0xa44ec: the box or its numbers fading
    // the word for a kind and level (0xa4468)
    static const char* word(int kind, int level);

private:
    bool initTag(int kind, int points, bool halve);  // 0xa187c

    UIInstance m_ui;          // +0xcc
    UIDefinition m_def;       // +0x408
    cocos2d::CCNode* m_root = nullptr;         // +0x2114
    cocos2d::CCNode* m_box = nullptr;          // +0x2118
    cocos2d::CCNode* m_text = nullptr;         // +0x211c
    cocos2d::CCNode* m_numbers = nullptr;      // +0x2120
    cocos2d::CCNode* m_failNumbers = nullptr;  // +0x2124
};

// The filthy coin bonus's sparkle and bleep (0xd4 bytes; 0x2d4d80; with a
// TagEffect of kind 5 when the partner is Nowe).
class CoinTagEffect : public Effect {
public:
    static CoinTagEffect* create();  // 0x2d4d80
    ~CoinTagEffect() override;       // 0x2d4cb8
    bool isAlive() override;         // 0x2d4fe0: particles, or 3 s
    void update(float dt) override { m_time += dt; }  // 0x2d4fc8
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint& p) override;  // 0x2d5014: a quarter higher

private:
    float m_time = 0;                       // +0xcc
    ParticleSystem* m_particles = nullptr;  // +0xd0 particle_tagfilthyFX
};

// The tutorial's arrow bobbing beside a HUD item, by the item's anchoring
// mode (UIMode; 0xcc bytes; 0x1df888): left of it, pointing right (an item
// on the right edge, 4), right of it pointing left (the left edge, 8),
// under it pointing up (the top, 1..3); any other mode is logged and shows
// nothing.
class TutorialArrowEffect : public Effect {
public:
    static TutorialArrowEffect* create(cocos2d::CCNode* item, int mode);  // 0x1df888
    bool isAlive() override { return true; }  // 0x1dfb74

private:
    bool initArrow(cocos2d::CCNode* item, int mode);  // 0x1df974
};

// The tutorial's shade at the top and bottom of the screen (0xd8 bytes;
// 0x2b8654): it fades in; fadeOut (0x2b88c4) lets it go.
class TutorialGradientEffect : public Effect {
public:
    static TutorialGradientEffect* create();  // 0x2b8654
    ~TutorialGradientEffect() override;       // 0x2b857c
    bool isAlive() override;                  // 0x2b8920
    void fadeOut();                           // 0x2b88c4

private:
    bool initGradient();  // 0x2b872c
    cocos2d::CCLayerGradient* m_bottom = nullptr;  // +0xcc
    cocos2d::CCLayerGradient* m_top = nullptr;     // +0xd0
    bool m_fading = false;                         // +0xd4
};

}  // namespace mt
