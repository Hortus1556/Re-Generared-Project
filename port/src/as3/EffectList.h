// The surgery screen's effects (mt::Effect), kept as the original keeps them
// in SurgeryScreen (its +0x2d84 array of 50, count +0x2e4c), for the screen
// to embed (re/as3/notes/surgery.md, "Effects"):
// - add (0x34fec): the effect retained and added as the screen's child at
//   its type's z order (275 under the HUD, which is at 350, or 400 over it:
//   the table at 0x3f2b20), then listed (0x3617c);
// - update (0x35c4c), each frame: every effect's update, then those no
//   longer alive (but the selected tool's) are told to the screen
//   (Listener::effectFinished, the screen's 0x35cf0), taken off the list,
//   removed and released;
// - three of them have their index kept: the selected tool's effect
//   (+0x2e50, its touches), the tag partner's cursor (+0x2e54, the
//   Reverand's light: its touches) and the high five (+0x2e58: while the
//   partner comes in, it alone is updated). Taking one off the list moves
//   the last into its place and fixes the indices (0x360e8).
// The screen's own pointers to effects sit beside the list: the tag
// partner button's sparkle (+0x2d48), the dying fade (+0x2d4c), the selected
// tool's effect (+0xfa8).
//
// The effects: Effect.h (the base), ToolEffects.h (Lighter, Cream, Scanner,
// ChainsawTrail, ChainsawShake), EventEffects.h (Damage, Shock,
// Electricity, BloodSpurt, BluePoisonCloud, Explosion, the colour washes,
// FrozenTools, PoisonedTools), HudEffects.h (DyingFade, Success,
// OutOfTime, Highlight, Sparkle, Tag, CoinTag, TutorialArrow,
// TutorialGradient), PartnerEffects.h (HighFive, Giblets, Saturday's,
// Bot, FlowMaster, Brutality's, the Reverand's, ToolsSheen; the Super Serum's went with the revive).
#pragma once

#include <string>
#include <vector>

#include "as3/Effect.h"
#include "as3/UI.h"

namespace mt {

class DyingFadeEffect;
class SparkleEffect;

class EffectList {
public:
    static constexpr int kMaxEffects = 50;  // DepthConstants::MaxEffectCount

    // What the screen does when an effect ends (its 0x35cf0, by type: the
    // high five's end starts the partner's power, the Saturday heart's
    // makes its sparkle, the baton's strikes...). Called before the effect
    // leaves the list.
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void effectFinished(Effect* effect) = 0;
    };

    // What the tools' effects need of the screen (0x3df88's reads).
    struct ToolContext {
        cocos2d::CCPoint touch;                // +0x5a8: the touch, for the old effect's end
        std::string stageBackground;           // the stage's background's name (+0x580 + stage * 0x40)
        cocos2d::CCNode* bg = nullptr;         // +0x100c
        cocos2d::CCNode* injuryLayer = nullptr;  // +0x1014
        cocos2d::CCNode* bgExtras = nullptr;   // +0x1010
        float scannerInterval = 0.9f;          // the Scanner tool's +0x52c: 0.9 s, 0.5 with its cooldown upgrade
    };

    // `screen`: the node the effects are added to (the surgery screen)
    explicit EffectList(cocos2d::CCNode* screen) : m_screen(screen) {}
    // its references let go (the screen, going, lets its children go)
    ~EffectList();
    EffectList(const EffectList&) = delete;
    EffectList& operator=(const EffectList&) = delete;

    // the z order an effect of `type` is added at (0x3f2b20, else 275)
    static int zOrderOf(int type);
    // Adds and lists `effect`; its index, or -1. When the list is full,
    // `replace` makes room (the first effect, or the second if the first is
    // the selected tool's) (0x34fec, 0x3617c).
    int add(Effect* effect, bool replace = true);
    // Takes the effect at `index` off the list (it is told: onRemoved) and
    // moves the last into its place; not removed from the screen nor
    // released (0x360e8).
    void removeAt(int index);
    // Every effect removed and released, the indices cleared (0x2fa58).
    void clear();
    // A frame (0x35c4c).
    void update(float dt, Listener* listener);
    // While the partner comes in (the screen's +0x8f8): the high five alone
    // runs; when it ends it is removed and told (0x3b2ec). Its end in the
    // original: removed, unlisted, the screen's 0x33e04(0), then 0x35cf0,
    // then released; the listener stands for both screen calls.
    void updateHighFive(float dt, Listener* listener);
    // the effects of a type all removed (0x376b0)
    void removeType(int type);
    // the effects of a type renewed (vf88) (0x37710)
    void renewType(int type);
    // the effects of a type that are not ending (vf89 false) (0x37750)
    std::vector<Effect*> activeOfType(int type);
    // the tutorial's highlights and arrows removed (its action 9, 0x3c238)
    void removeTutorialEffects();

    int count() const { return static_cast<int>(m_effects.size()); }
    Effect* at(int index) const { return index >= 0 && index < count() ? m_effects[index] : nullptr; }

    // --- the selected tool's effect
    // The effect for a tool (0x3df88): the lighter's, the cream's, the
    // chainsaw's shake, the scanner's; none for the others. The current
    // tool effect, if any, is told the touch ended first.
    Effect* makeToolEffect(int tool, const ToolContext& context);
    // The tool's effect made, added and kept as the selected one (the part
    // of the screen's selectTool, 0x3900c, after the old tool stopped). The
    // old effect stays listed until it dies. Returns it (nullptr if none).
    // (Then the chainsaw's cursor, made by the screen: ChainsawTrailEffect.)
    Effect* selectTool(int tool, const ToolContext& context);
    Effect* toolEffect() const { return at(toolIndex); }
    Effect* partnerCursor() const { return at(partnerIndex); }
    Effect* highFive() const { return at(highFiveIndex); }

    // --- the touches, where the screen's handlers forward them
    // 0x2fc38 first: the partner's cursor
    void partnerTouchBegan(const cocos2d::CCPoint& p);
    // 0x2fc38, after the tool began, if it is on (ToolState::active)
    void toolTouchBegan(const cocos2d::CCPoint& p);
    // 0x30c58 first: the partner's cursor; then, if a tool is selected and
    // on, the tool's effect
    void partnerTouchMoved(const cocos2d::CCPoint& p);
    void toolTouchMoved(const cocos2d::CCPoint& p);
    // 0x2ff18 first: the partner's cursor; at its end, if a tool is
    // selected, the tool's effect (on or not)
    void partnerTouchEnded(const cocos2d::CCPoint& p);
    void toolTouchEnded(const cocos2d::CCPoint& p);
    // 0x34cbc: the selected tool stopped
    void toolStopped();

    // An effect event (event 3; 0x4c0c8 / 0x4b294) by its kind (+0x14):
    // 0 Damage (the caller decides: the original skips it for a tool's
    // event during the grace or in god mode), 1 Shock, 2 Electricity,
    // 3 BloodSpurt, 4 BluePoisonCloud, 6 Explosion at `at` (+0x2c, +0x30);
    // 5 BlueScreen if none is showing (not fading out); 7 FrozenTools,
    // 10 PoisonedTools over the HUD's tool buttons (`hud`, the screen's
    // +0xd4); 8 WhiteScreen; 11 GreenScreen. Made and added; nullptr for
    // the other kinds (9: none).
    Effect* eventEffect(int kind, const cocos2d::CCPoint& at, UIInstance* hud);

    int toolIndex = -1;       // +0x2e50
    int partnerIndex = -1;    // +0x2e54
    int highFiveIndex = -1;   // +0x2e58
    SparkleEffect* sparkle = nullptr;      // +0x2d48 (weak: cleared when it goes)
    DyingFadeEffect* dyingFade = nullptr;  // +0x2d4c (weak)
    Effect* currentToolEffect = nullptr;   // +0xfa8 (weak)

private:
    int listEffect(Effect* effect, bool replace);  // 0x3617c
    // an effect leaving: the screen's pointers to it cleared
    void forget(Effect* effect);

    cocos2d::CCNode* m_screen;
    std::vector<Effect*> m_effects;  // +0x2d84 (retained)
};

}  // namespace mt
