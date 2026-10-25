// mt::InfoScreen (made by 0xaa890; 0x230c bytes; vtable 0x4160e8): a
// patient's case file, opened from the area's map, over the area's picture.
//
// Never treated, it shows the first page (the case in brief) and start.
// Once treated: their photo, their stars on the folder, and three post-its
// to turn the page: the main surgery (observations, symptoms, actions, by
// Ophelia and Alan), and the two bonus challenges (their title and text);
// a challenge not yet won pulses, and the bonus link turns to the first
// challenge not won. Start plays the page's surgery: the main one through
// its story (DialogueScreen), a challenge straight to its pre-op
// (PreOpScreen), the slot's selectedMode set to 1, 2 or 3 and saved. Back
// returns to the area's map. The ambience is the area's own.
//
// Removed (docs/as3.md): the ads.
#pragma once

#include "as3/Scene.h"
#include "as3/SurgeryInfo.h"

namespace mt {

class InfoScreen : public Scene {
public:
    static InfoScreen* create(const std::string& surgery, const AreaDef& area);  // 0xaa890
    void load() override;  // vf87, 0xaac04

private:
    InfoScreen() = default;  // 0xb6c70
    void onButton(cocos2d::CCObject* sender);  // 0xadb60
    // the page shown: 0 the case in brief, 1 the main surgery, 2 and 3 the
    // challenges (0xaed84)
    void showPage(int page);
    void turnTo(int page);  // the start buttons for it, and the page

    UIDefinition m_def;       // +0x420
    SurgeryInfo m_surgery;    // +0x2134
    AreaDef m_area;           // +0x2258
    int m_page = 0;           // +0x2308
};

}  // namespace mt
