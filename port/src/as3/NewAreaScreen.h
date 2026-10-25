// mt::NewAreaScreen (made by 0x294474; 0x2140 bytes; vtable 0x41c138): the
// letter introducing an area the first time its map is opened from the
// world map (its area_new_title and area_new_body, areas.json), dropped in
// with a bounce over the darkened map; OK slides it out and takes it off
// (SceneMgr::popScene), and the map shows its buttons again.
#pragma once

#include <string>

#include "as3/Scene.h"

namespace mt {

class NewAreaScreen : public Scene {
public:
    // `area` the area's id; `map` the AreaScreen under it (kept, unused)
    static NewAreaScreen* create(const std::string& area, Scene* map);  // 0x294474
    void load() override;  // vf87, 0x294628

private:
    NewAreaScreen() = default;  // 0x29b9d0
    void onButton(cocos2d::CCObject* sender);  // 0x294c30
    void close(cocos2d::CCNode* node);         // 0x294e90

    cocos2d::CCNode* m_shade = nullptr;   // +0x420
    UIDefinition m_def;                   // +0x424
    Scene* m_map = nullptr;               // +0x2130
    std::string m_area;                   // +0x2134
    cocos2d::CCNode* m_popup = nullptr;   // +0x2138
    std::string m_sheet = "new_area_sheet";  // +0x213c
};

}  // namespace mt
