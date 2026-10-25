// The top bar of the menu screens (0x2060 bytes, a member of the pre-op,
// world, area, upgrade, partner and shop screens; made by 0x2c6fc, built by
// 0x2b94a4): the coins, every star won, the blood packs and their timer,
// and a button to buy coins. On the map (the world and area screens) its
// layout also has the area's name and stars.
//
// The blood packs are free on the PC (BloodPacks.h): their count, most and
// timer are shown, their buy button hidden. The coins button is hidden too
// (it opened the coin packs sold for money, ShopCurrencyScreen).
#pragma once

#include <string>

#include "as3/UI.h"

namespace mt {

class GlobalHud {
public:
    GlobalHud() = default;
    GlobalHud(const GlobalHud&) = delete;
    GlobalHud& operator=(const GlobalHud&) = delete;
    // its sheet loaded and its layout built into `scene`; its buttons call
    // target->selector (0x2b94a4)
    void build(cocos2d::CCNode* scene, cocos2d::CCObject* target, bool map, cocos2d::SEL_MenuHandler selector);
    void refresh();  // the numbers (0x2b9a64)
    // whether a clicked item is one of its own (0x2ba67c)
    bool owns(cocos2d::CCObject* sender);
    // the area's name and stars, on the map (0x2ba37c)
    void setMapInfo(const std::string& name, const std::string& stars);

    UIDefinition def;  // +8
    UIInstance ui;     // +0x1d14
    bool map = false;  // +0x2050
};

}  // namespace mt
