// mt::Effect (no vtable of its own in the binary: every effect inlines its
// constructor; 0xc8 + 4 bytes): a visual effect of the surgery screen. A node
// the screen adds as its own child, at a z order picked by its type
// (EffectList::add, 0x34fec), keeps in its effect list and updates each
// frame (0x35c4c) until it says it is no longer alive (vf87), then removes.
// It derives from cocos2d::CCNode and adds the virtuals vf87..vf94 (CCNode's
// slots end at 86):
//   vf87 0x15c isAlive        while true the screen keeps it
//   vf88 0x160 renew          the colour washes start their time again
//   vf89 0x164 isEnding       the colour washes are fading out (0 by default)
//   vf90 0x168 onRemoved      taken off the list (0x360e8): the effects with
//                             a sound channel stop it
//   vf91 0x16c touchBegan     the screen's touches, forwarded to the selected
//   vf92 0x170 touchMoved     tool's effect and to the tag partner's cursor
//   vf93 0x174 touchEnded     (EffectList)
//   vf94 0x178 toolStopped    the selected tool stopped (0x34cbc; the chainsaw
//                             lets its shake die down)
// The names are the port's (the binary has none). Its type (+0xc8, the
// binary's +200) is 3 unless the effect sets its own (EffectType).
//
// The effects are ported in the files named in EffectList.h.
#pragma once

#include <cstdint>
#include <cstdlib>

#include "as3/ParticleSystem.h"
#include "engine/cocos/CCNode.h"

namespace mt {

// An effect's type (+0xc8): what the screen tells them apart by (its z
// order, the effects removed or renewed together, what happens when one
// ends). The values are the effects' constructors'; the names the port's.
enum EffectType {
    kEffectBluePoisonCloud = 0,
    kEffectBlueScreen = 1,
    kEffectFlash = 2,          // DamageEffect, ShockEffect
    kEffectGeneric = 3,        // the default: the tools', BloodSpurt, Electricity, Explosion...
    kEffectFrozenTools = 4,
    kEffectHighFive = 5,       // the tag partner coming in; when it ends the partner's power starts
    kEffectHighlight = 6,      // the tutorial's
    kEffectSparkle = 7,        // on the tag partner button (the screen's +0x2d48)
    kEffectTag = 8,            // the feedback popups
    kEffectTutorialArrow = 9,
    kEffectWhiteScreen = 10,
    kEffectReverandTools = 11,
    kEffectReverandCursor = 12,
    kEffectWin = 13,           // none made in 1.02; when one ended the surgery was won
    kEffectSuperSerum = 14,  // (the revive's: removed, the type kept)
    kEffectFlowMaster = 15,
    kEffectGiblets = 16,
    kEffectBot = 17,
    kEffectSaturdayHeart = 18,
    kEffectSaturdayParticle = 19,
    kEffectToolsSheen = 20,
    kEffectReverandEnd = 21,
    kEffectGreenScreen = 22,
    kEffectPoisonedTools = 23,
    kEffectEndPopup = 24,      // OutOfTimeEffect, SuccessEffect
    kEffectTutorialGradient = 26,
    kEffectBatonIn = 27,
    kEffectBatonOut = 28,
    kEffectCoinTag = 29,
    kEffectBrutality = 30,
    kEffectDyingFade = 31,
};

class Effect : public cocos2d::CCNode {
public:
    int getType() const { return m_type; }

    virtual bool isAlive() = 0;                          // vf87
    virtual void renew() {}                              // vf88
    virtual bool isEnding() { return false; }            // vf89 (0x63534)
    virtual void onRemoved() {}                          // vf90
    // iOS 1.45's new slot 91 (0x16c; the touches and toolStopped after it
    // are its 92..95): told to wind down before the screen removes it (the
    // stage's effect at a stage switch: BubbleEffect)
    virtual void setToEnd() {}
    virtual void touchBegan(const cocos2d::CCPoint&) {}  // vf91
    virtual void touchMoved(const cocos2d::CCPoint&) {}  // vf92
    virtual void touchEnded(const cocos2d::CCPoint&) {}  // vf93
    virtual void toolStopped() {}                        // vf94

    // the effect's own sound channel: the original keyed a channel by the
    // effect's address (SoundMgr 0x305780)
    int soundChannel() const { return static_cast<int>(reinterpret_cast<intptr_t>(this)); }

protected:
    int m_type = kEffectGeneric;  // +0xc8
};

// --- what the effects share (their names are the port's)

// lo..hi, both included: rand() % (hi - lo + 1) (0x644b0)
inline int randomInt(int lo, int hi) { return lo + std::rand() % (hi - lo + 1); }
// degrees to radians and back (0x64518, 0x64550)
inline float degToRad(float d) { return static_cast<float>(d * 3.141592653589793 / 180.0); }
inline float radToDeg(float r) { return static_cast<float>(r * 180.0f / 3.141592653589793); }

// A particle system by name, retained (the effects keep their systems and
// release them when they go); nullptr if it cannot be read (the original
// crashed; the port logs it and the effect goes on without it).
ParticleSystem* retainedParticles(const char* name);
void releaseParticles(ParticleSystem*& p);

// What the effects ask a system, tolerating one that could not be read.
inline float emissionRate(cocos2d::CCParticleSystem* p) { return p ? p->getEmissionRate() : 0; }
inline void setEmissionRate(cocos2d::CCParticleSystem* p, float r) {
    if (p) p->setEmissionRate(r);
}
inline bool hasParticles(cocos2d::CCParticleSystem* p) { return p && p->getParticleCount() != 0; }
inline bool isActive(cocos2d::CCParticleSystem* p) { return p && p->isActive(); }
inline void place(cocos2d::CCNode* n, const cocos2d::CCPoint& at) {
    if (n) n->setPosition(at);
}
// whether a node runs actions (CCNode::numberOfRunningActions, 0x314ab4)
inline bool runsActions(cocos2d::CCNode* n) { return n && n->numberOfRunningActions() != 0; }

// Mediatonic's addition to cocos2d's particle system (0x32c094): the
// emission counter filled to a whole particle (1 / emission rate), so the
// next update emits at once instead of a particle's time later.
void fillEmitCounter(cocos2d::CCParticleSystem* p);

}  // namespace mt
