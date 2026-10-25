// mt::PartnerSelectScreen (made by 0x1fd460; 0x432c bytes; vtable
// 0x419e98): the tag partners, their portraits in a row to swipe across
// (HorizontalScrollLayer), the chosen one ticked and shown big beside them.
//
// A portrait is the partner's own (normal), greyed with the time left while
// its power cools down (a PartnerCooldownInfoScreen under it, with a button
// to end it for coins), or locked while the story has not given it yet (Mr
// Giblets, Lady Saturday, the Reverend); a partner on sale has its price on
// a tag. Tapping one: on sale, or owned away from the pre-op screen, opens
// its file (PartnerInfoScreen: buy it, or take it as the partner); owned,
// from the pre-op screen, takes it at once; cooling down, offers to end the
// cooldown (CooldownInfoScreen); locked, is refused. The big portrait drops
// the partner.
//
// From the pre-op screen ("preop") it slides in over it with shades above
// and below; start (with a partner) or skip (without) begins the surgery,
// back slides it out again. From the world or an area's map it only shows
// the partners, with the upgrades button.
//
// After the first two surgeries, start or skip takes a blood pack; without
// one, the out-of-packs popup instead (BloodPacks.h).
//
// Removed (docs/as3.md): the shop of coins sold for money (the top bar's
// button hidden: GlobalHud.h), the ads and analytics.
#pragma once

#include <string>
#include <vector>

#include "as3/GlobalHud.h"
#include "as3/Scene.h"
#include "as3/SurgeryInfo.h"

namespace cocos2d {
class CCLayerGradient;
class CCSprite;
}

namespace mt {

class HorizontalScrollLayer;
class PartnerCooldownInfoScreen;

class PartnerSelectScreen : public Scene {
public:
    // `from`: "preop", "world" or "area" (0x1fd460)
    static PartnerSelectScreen* create(const std::string& from, const SurgeryInfo& surgery, const SurgeryParams& params);
    ~PartnerSelectScreen() override;
    void load() override;       // vf87, 0x1fd694
    void unload() override;     // vf88, 0x202944
    void onShown() override;    // vf91, 0x2026e0
    void onRemoved() override;  // vf92, 0x202968

    // the portraits (normal, grey with the time left, or locked), the tick
    // and the big portrait of the chosen one, start or skip (0x1ff3d0)
    void refreshPortraits();

private:
    using Scene::init;
    bool init(const SurgeryInfo& surgery, const SurgeryParams& params);  // 0x1fd60c
    void onButton(cocos2d::CCObject* sender);   // 0x1fe594
    void refreshPriceTags();                    // 0x2002c0
    void refreshUpgrades();                     // 0x2007a8: the upgrades that can be bought
    void tick(float dt);                        // 0x201a74: the top bar, each second (and, the port's, the cooldowns)
    void pagePressed(cocos2d::CCNode* page);    // 0x200c58
    void pageReleased(cocos2d::CCNode* page);   // 0x200ed8
    void pageSelected(cocos2d::CCNode* page);   // 0x201344
    void pageChanged(cocos2d::CCNode* page) {}  // 0x201a70: nothing
    void hideBrowseOnly();                      // 0x201a9c: away from the pre-op screen
    void hideForPreop();                        // 0x201f38
    void beforeCovered();                       // 0x2024d4: input off under a popup
    void fadeInShades();                        // 0x1fd65c
    void slidOut();                             // 0x202118: back to the pre-op screen

    UIDefinition m_def;                         // +0x420
    GlobalHud m_hud;                            // +0x212c
    HorizontalScrollLayer* m_scroll = nullptr;  // +0x418c
    std::vector<cocos2d::CCSprite*> m_portraits;   // +0x4190
    std::vector<cocos2d::CCNode*> m_pages;         // +0x419c: tagged with the partner's place
    std::vector<cocos2d::CCSprite*> m_priceTags;   // +0x41a8
    std::vector<cocos2d::CCNode*> m_prices;        // +0x41b4
    cocos2d::CCSprite* m_tick = nullptr;           // +0x41c0 (retained)
    std::string m_from;                            // +0x41c4
    std::vector<PartnerCooldownInfoScreen*> m_cooldowns;  // +0x41c8
    SurgeryInfo m_surgery;                         // +0x41d4
    SurgeryParams m_params;                        // +0x42f8
    bool m_leaving = false;                        // +0x4304: a file opening
    cocos2d::CCLayerGradient* m_shadeBottom = nullptr;  // +0x4308 (retained, from the pre-op screen)
    cocos2d::CCLayerGradient* m_shadeTop = nullptr;     // +0x430c
    std::string m_sheet = "partner_select_sheet";             // +0x4310
    // (1.45 ctor 0x2d22fc: the same sheet over the pre-op too; 1.02 had a
    // smaller one for it, partner_select_sheet_preop, still in the bundle)
    std::string m_preopSheet = "partner_select_sheet";  // +0x4314
};

}  // namespace mt
