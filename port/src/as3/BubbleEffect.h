// mt::BubbleEffect (iOS 1.45; 0xd8 bytes, vtable 0x645010; type 0x20):
// bubbles rising from the bottom of the screen (particle_bubbleFX), the
// effect a surgery stage names by its "effect" key (SurgeryData's
// SurgeryStageDef::effect), shown while that stage is (SurgeryScreen's stage
// switch, 1.45 runSurgeryStageSwitch 0x4aae8, which also loops bubblesFX on
// the ambience channel). It lives while its system emits; setToEnd stops
// and hides it. Its place is the system's own (the plist's source position):
// setPosition does nothing.
//
// Made by name (stageEffect: 1.45 EffectUtils::createEffectForEnum 0x7b0b4
// with the effect names' map, whose 0x20 is "BubbleEffect"; it makes no
// other). No surgery file shipped with 1.45 names an effect: the whale's
// files list bubblesFX among their sounds, but nothing plays it.
#pragma once

#include <string>

#include "as3/Effect.h"

namespace mt {

constexpr int kEffectBubble = 0x20;

class BubbleEffect : public Effect {
public:
    static BubbleEffect* create();  // 1.45 0x4d48bc (constructor 0x4dcd28)
    ~BubbleEffect() override;
    bool isAlive() override;        // vf87 1.45 0x4d49a0: its system still emits
    void onRemoved() override {}    // vf90 1.45 0x4d49c8
    void setToEnd() override;       // vf91 1.45 0x4d49cc: emission off, hidden
    using cocos2d::CCNode::setPosition;
    void setPosition(const cocos2d::CCPoint&) override {}  // 1.45 0x4d49f0: nothing

private:
    ParticleSystem* m_bubbles = nullptr;  // +0xcc: particle_bubbleFX
};

// The effect a stage names (1.45 EffectUtils::createEffectForEnum 0x7b0b4 of
// the name's enum): a BubbleEffect for "BubbleEffect", else none.
Effect* stageEffect(const std::string& name);

}  // namespace mt
