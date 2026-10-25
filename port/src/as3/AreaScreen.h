// mt::AreaScreen (made by 0x1f1d8; 0x4258 bytes; vtable 0x4142c8): an
// area's map, its patients' case files laid out on its picture, with the
// top bar (its map layout: the area's name and stars won of the most), the
// upgrades and tag partner buttons and back.
//
// A patient never treated shows an unknown file; a treated one their own,
// with their stars: three yellow for the main surgery, blue and red for the
// two challenges. The area's boss stays locked until its three other
// patients are treated. The Bunker (the first area) shows its bonus surgery
// hubris2 only once the game is finished, on a second map; before the
// second patient (prxy4) is treated it sparkles over them and hides the
// upgrades and partner buttons, and its back goes to the title rather than
// the world map. Picking a file opens the patient's case (InfoScreen), a
// locked one is refused. Coming from the world map to an area not yet
// played, its introduction comes first (NewAreaScreen, over this screen),
// with back and the other buttons hidden until it is closed.
//
// The ambience is the area's own (its bgm, on the ambience channel); the
// music stops. The Exile Clinic snows.
//
// Each second the top bar (GlobalHud) shows the blood packs and the slot's
// are recharged (BloodPacks.h). Removed (docs/as3.md): the ads.
#pragma once

#include <string>

#include "as3/AreasDefUtils.h"
#include "as3/GlobalHud.h"
#include "as3/Scene.h"
#include "as3/SurgeryInfo.h"

namespace mt {

class ParticleSystem;

class AreaScreen : public Scene {
public:
    // `from` 1: from the world map (an unplayed area is introduced first)
    static AreaScreen* create(const AreaDef& area, int from);  // 0x1f1d8
    void load() override;       // vf87, 0x1f3a0
    void onShown() override;    // vf91, 0x23d00
    void onRemoved() override;  // vf92, 0x243c8

private:
    AreaScreen();  // 0x2c520
    void onButton(cocos2d::CCObject* sender);  // 0x23294
    void addSnow();                            // 0x23c60
    void tick(float dt);                       // 0x23cd8: the top bar, each second
    void hideButtons();

    int m_bgZ = 0;                         // +0x420
    int m_layoutZ = 100;                   // +0x424
    UIDefinition m_def;                    // +0x428
    GlobalHud m_hud;                       // +0x2134
    AreaDef m_area;                        // +0x4194
    ParticleSystem* m_highlight = nullptr; // +0x4244: over the Bunker's second patient
    bool m_fromWorld = false;              // +0x4250
    bool m_introduced = false;             // +0x4251: the NewAreaScreen was shown
};

}  // namespace mt
