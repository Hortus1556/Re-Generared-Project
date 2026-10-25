// mt::PreOpScreen (made by 0xd3cdc; 0x42dc bytes; vtable 0x4166e8): the
// patient before the surgery: their picture, name and the stars won on it
// (main, then the two challenges), the top bar, and buttons to start, go
// back to the area, or to the upgrades (with a count of those that can be
// bought now). Its ambience is the surgery's pre-surgery sound.
//
// Start: in the first two surgeries the surgery begins at once (the second,
// prxy4, gives Mr Giblets as the tag partner first); after them the tag
// partner is chosen first (PartnerSelectScreen over this one).
#pragma once

#include "as3/GlobalHud.h"
#include "as3/Scene.h"
#include "as3/SurgeryInfo.h"

namespace mt {

class PreOpScreen : public Scene {
public:
    static PreOpScreen* create(const SurgeryInfo& surgery, const SurgeryParams& params);
    void load() override;       // vf87
    void onShown() override;    // vf91
    void onRemoved() override;  // vf92

private:
    void onButton(cocos2d::CCObject* sender);  // 0xd5380
    void updateUpgrades();                     // 0xd639c
    void tick(float dt);                       // 0xd6c68: the top bar, each second

    UIDefinition m_def;      // +0x420
    GlobalHud m_hud;         // +0x212c
    SurgeryInfo m_surgery;   // +0x418c
    SurgeryParams m_params;  // +0x42b0
    // its setup (0xddc38): the surgery, the patient's name, the picture, the sheets
    std::string m_surgeryId;            // +0x42bc
    std::string m_name;                 // +0x42c0
    std::string m_picture;              // +0x42c4
    std::vector<std::string> m_sheets;  // +0x42c8
};

}  // namespace mt
