#include "as3/GlobalHud.h"

#include <algorithm>

#include "as3/BloodPacks.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"

namespace mt {

void GlobalHud::build(cocos2d::CCNode* scene, cocos2d::CCObject* target, bool onMap,
                      cocos2d::SEL_MenuHandler selector) {
    map = onMap;
    loadSheets(scene, {"global_ui_sheet"});
    def = loadUIDefinition(map ? "common/ui_globals_map.json" : "common/ui_globals_main.json");
    buildUI(ui, scene, def, target, selector);
    // removed on the PC: the coins and the blood packs bought with money
    ui.setVisible("ui_global_button_getsp", false);  // (coins bought with money)
    ui.setVisible("ui_global_button_getbp", false);  // (1.45: blood packs bought with money)
    // the blood packs' most, "/3" (1.45 updateStatusBar 0x3cba94; the
    // layout's placeholder says "/5")
    ui.setText("global_maxbludpaxm", "/" + std::to_string(GameStateUtils::MaxBloodPacks()));
    refresh();
}

void GlobalHud::refresh() {
    const SavedGame& slot = SaveData::shared().currentSlot();
    ui.setText("global_spbalance", std::to_string(slot.coins()));
    ui.setText("global_totalstars", std::to_string(slot.totalStars()));
    // (1.45 0x3cba94) the blood packs, and the time to the next or "Full"
    // (which recharges the slot, saving when one came back)
    const std::string next = GameStateUtils::GetTimeUntilNextBloodpack();
    ui.setText("global_totalbludpax", std::to_string(std::max(0, slot.numBloodPacks)));
    ui.setText("global_bludpaxtimer", next);
}

bool GlobalHud::owns(cocos2d::CCObject* sender) {
    return ui.indexOf.count(dynamic_cast<cocos2d::CCNode*>(sender)) != 0;
}

void GlobalHud::setMapInfo(const std::string& name, const std::string& stars) {
    ui.setText("global_mapname", name);
    ui.setText("global_mapstars", stars);
}

}  // namespace mt
