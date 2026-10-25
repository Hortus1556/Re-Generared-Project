#include "as3/UpgradeScreen.h"

#include <string>

#include "as3/BloodPacks.h"
#include "as3/Cheats.h"
#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/Log.h"
#include "as3/PreOpScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/UpgradeInfoUtils.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

namespace {

// what the machine says before an upgrade is chosen (0x4331ac)
const char* kWelcome = "Welcome to the Upgrades Machine - select an item to the left to view its details.";

// the colour of a column's icons (1 blue, 2 orange, 3 red, 4 green, 5 purple)
const char* columnColour(int column) {
    switch (column) {
        case 2: return "orange";
        case 3: return "red";
        case 4: return "green";
        case 5: return "purple";
        default: return "blue";
    }
}

// the upgrade at a column and row, -1 if none (0x134674)
int upgradeAt(int column, int row) {
    const std::vector<UpgradeDef>& u = UpgradeInfoUtils::shared()->upgrades;
    for (size_t i = 0; i < u.size(); ++i)
        if (u[i].row == row && u[i].column == column) return static_cast<int>(i);
    return -1;
}

bool bought(const UpgradeDef& u) {
    return SaveData::shared().currentSlot().owns(SavedGame::kItemUpgrade, upgradeIndex(u.id));
}

// an upgrade bought, if the coins allow and it is not yet (0xe89a4)
void buyUpgrade(int index, int cost) {
    SavedGame& slot = SaveData::shared().currentSlot();
    if (cost <= slot.coins() && !slot.owns(SavedGame::kItemUpgrade, index))
        slot.addTransaction(-cost, SavedGame::kItemUpgrade, index, 1);
}

}  // namespace

UpgradeScreen* UpgradeScreen::create(const std::string& from) {
    UpgradeScreen* s = new UpgradeScreen;
    s->setName("UpgradeScreen");
    if (s->init()) {
        s->m_bought = false;
        s->autorelease();
        s->m_from = from;
        return s;
    }
    delete s;
    return nullptr;
}

void UpgradeScreen::load() {
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopChannel(SoundMgr::kAmbienceChannel, false);
    sound->stopMusic(true);
    sound->playOnChannel("tech_tree_ambience", SoundMgr::kAmbienceChannel, true, false, false, false);
    m_row = 0;
    m_column = 0;
    StatsMgr::sharedInstance()->catchUp();  // (an older slot's counters: StatsMgr.h)
    loadSheets(this, {m_sheet});
    if (CCSprite* bg = CCSprite::createWithSpriteFrameName("upgrades_background.png")) {
        setPositionOnCanvas(bg, 320, 192);
        addChild(bg);
    }
    m_def = loadUIDefinition("common/ui_techtree.json");
    buildUI(ui, this, m_def, this, menu_selector(UpgradeScreen::onButton));
    m_hud.build(this, this, false, menu_selector(UpgradeScreen::onButton));
    refreshTree();
    // nothing chosen yet
    ui.setVisible("upgrades_blueframe", false);
    ui.setText("upgrades_iconstext", kWelcome);
    ui.setText("upgrades_iconstitle", "");
    ui.setText("upgrades_starsprice", "");
    ui.setVisible("upgrades_coin", false);
    ui.setVisible("ui_button_buy", false);
    ui.setVisible("upgrades_screenicon", false);
    ui.setVisible("upgrades_tickbox", false);
    m_hud.refresh();
    schedule(schedule_selector(UpgradeScreen::tick), 1.0f);
}

void UpgradeScreen::tick(float) {
    m_hud.refresh();
    GameStateUtils::UpdateBloodPacksAndSave();  // (1.45 updateStatus 0x197fb4)
}

void UpgradeScreen::onShown() {
    Scene::onShown();
    tick(0);
    // (the original showed its ads here)
}

void UpgradeScreen::onRemoved() { unschedule(schedule_selector(UpgradeScreen::tick)); }

void UpgradeScreen::refreshTree() {
    const UpgradeInfoUtils* info = UpgradeInfoUtils::shared();
    int stars = SaveData::shared().currentSlot().totalStars();
    // the rows: still shut, chained with their price in stars; open, the chain gone
    for (int r = 0; r < 6; ++r) {
        std::string cost = "ui_upgrades_starcost_row_" + std::to_string(r + 1);
        // (debug: every row open with cheat 16, 1.45 updateAvailable 0x196ba8; Cheats.h)
        if (stars < info->rowStars[r] && !cheat(Cheats::kAllUpgradeRows)) {
            ui.setText(cost, std::to_string(info->rowStars[r]));
        } else {
            ui.setVisible("upgrades_chain_row_" + std::to_string(r + 1), false);
            ui.setText(cost, "");
        }
    }
    // a slot with no upgrade (the left-out "Bleed Buddies") shows nothing
    for (int column = 1; column <= 5; ++column)
        for (int row = 1; row <= 6; ++row) {
            if (upgradeAt(column, row) >= 0) continue;
            std::string at = "_row_" + std::to_string(7 - row) + "_col_" + std::to_string(column);
            for (const std::string& n : {"upgrades_frame" + at, "upgrades_starbox" + at,
                                         std::string("upgrades_") + columnColour(column) + "icons0" + std::to_string(row)})
                ui.setVisible(n, false);
            ui.setText("ui_upgrades_starcost" + at, "");
        }
    // each upgrade (its frame and star box counted from the top: row 7 - r)
    for (const UpgradeDef& u : info->upgrades) {
        std::string at = "_row_" + std::to_string(7 - u.row) + "_col_" + std::to_string(u.column);
        if (bought(u)) {
            ui.setItemImage(m_def, "upgrades_frame" + at, "gold");
            ui.setVisible("upgrades_starbox" + at, false);
            ui.setText("ui_upgrades_starcost" + at, "");
            continue;
        }
        std::string icon = std::string("upgrades_") + columnColour(u.column) + "icons0" + std::to_string(u.row);
        // open: its row's stars, and the one under it bought (none under the first row)
        bool open = true;
        if (u.row >= 2) {
            int below = upgradeAt(u.column, u.row - 1);
            open = below >= 0 && bought(info->upgrades[below]);
        }
        int rowNeeds = u.row >= 1 && u.row <= 6 ? info->rowStars[u.row - 1] : 0;
        if ((open && stars >= rowNeeds) || cheat(Cheats::kAllUpgradeRows)) {
            ui.setItemImage(m_def, icon, "normal");
            ui.setVisible("upgrades_starbox" + at, true);
            ui.setText("ui_upgrades_starcost" + at, std::to_string(u.cost));
        } else {
            ui.setItemImage(m_def, icon, "grey");
            ui.setVisible("upgrades_starbox" + at, false);
            ui.setText("ui_upgrades_starcost" + at, "");
        }
    }
}

void UpgradeScreen::select(const std::string& icon) {
    // "upgrades_<colour>icons0<row>": the colour's first two letters, the last digit
    std::string c = icon.substr(9, 2);
    m_row = icon.empty() ? 0 : icon.back() - '0';
    m_column = c == "or" ? 2 : c == "re" ? 3 : c == "gr" ? 4 : c == "pu" ? 5 : 1;
}

void UpgradeScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (m_hud.owns(sender)) {
        // (the top bar's button to the shop of coins sold for money: removed; it opens
        // the shop of items for earned coins on its own, GlobalHud.h)
        sound->playSound("button_click", false);
        return;
    }
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SavedGame& slot = SaveData::shared().currentSlot();
    UpgradeInfoUtils* info = UpgradeInfoUtils::shared();
    if (name == "ui_button_backsmall") {
        sound->playSound("button_click", false);
        goBack();
    } else if (name == "ui_button_buy") {
        int i = upgradeAt(m_column, m_row);
        if (i < 0) return;
        const UpgradeDef u = info->upgrades[i];
        if (slot.coins() - u.cost < 0) {
            // (the original then opened the shop of coins sold for money: removed)
            sound->playSound("button_click_denied", false);
            return;
        }
        ui.setVisible("ui_button_buy", false);
        ui.setText("upgrades_starsprice", "");
        ui.setVisible("upgrades_coin", false);
        ui.setVisible("upgrades_tickbox", true);
        sound->playSound("upgrade_confirm", false);
        sound->playSound("button_click", false);
        m_bought = true;
        buyUpgrade(upgradeIndex(u.id), u.cost);
        // (1.45 clickedButton 0x193c8c: an upgrade bought, stat 17, StatsMgr.h;
        // its analytics: removed)
        StatsMgr::sharedInstance()->incrementStat(StatTypes::kUpgradeTech);
        refreshTree();
        tick(0);
        SaveData::shared().saveGame();
    } else if (name.find("icons0") != std::string::npos) {
        sound->playSound("button_click", false);
        select(name);
        // the blue frame over the icon
        CCNode* frame = ui.node("upgrades_blueframe");
        CCNode* icon = ui.node(name);
        if (frame && icon) {
            frame->setVisible(true);
            frame->setPosition(ccp(icon->getPositionX() - 2.4f, icon->getPositionY() + 0.7f));
        }
        int i = upgradeAt(m_column, m_row);
        if (i < 0) return;  // (the original read before its list)
        const UpgradeDef u = info->upgrades[i];
        bool owned = bought(u);
        bool belowOwned = true;
        if (m_row > 1) {
            int below = upgradeAt(m_column, m_row - 1);
            belowOwned = below >= 0 && bought(info->upgrades[below]);
        }
        int stars = slot.totalStars();
        if (owned) {
            ui.setVisible("ui_button_buy", false);
            ui.setText("upgrades_starsprice", "");
            ui.setVisible("upgrades_coin", false);
            ui.setVisible("upgrades_tickbox", true);
        } else if ((stars < info->rowStars[m_row - 1] || !belowOwned) && !cheat(Cheats::kAllUpgradeRows)) {
            // not open yet: its price, no buy button
            ui.setVisible("ui_button_buy", false);
            ui.setText("upgrades_starsprice", std::to_string(u.cost));
            ui.setVisible("upgrades_coin", true);
            ui.setVisible("upgrades_tickbox", false);
        } else {
            ui.setVisible("ui_button_buy", true);
            ui.setText("upgrades_starsprice", std::to_string(u.cost));
            ui.setVisible("upgrades_coin", true);
            ui.setVisible("upgrades_tickbox", false);
        }
        ui.setText("upgrades_iconstitle", u.name);
        ui.setText("upgrades_iconstext", u.description);
        ui.setItemImage(m_def, "upgrades_screenicon", std::string(columnColour(m_column)) + "0" + std::to_string(m_row));
        ui.setVisible("upgrades_screenicon", true);
    }
}

void UpgradeScreen::goBack() {
    SceneMgr* scenes = SceneMgr::sharedSceneMgr();
    if (m_from == "world") {
        scenes->replaceScene(WorldScreen::create());
        return;
    }
    SavedGame& slot = SaveData::shared().currentSlot();
    for (const AreaDef& area : AreasDefUtils::shared()->areas) {
        if (area.areaId != slot.lastSelectedArea) continue;
        if (m_from == "preop") {
            // the patient the player was on, in the mode chosen
            SurgeryParams params;
            params.mode = slot.selectedMode == 3 ? 2 : slot.selectedMode == 2 ? 1 : 0;
            scenes->replaceScene(PreOpScreen::create(loadSurgeryInfo(slot.selectedSurgery, area), params));
        } else {
            scenes->replaceScene(AreaScreen::create(area, 0));
        }
        return;
    }
}

}  // namespace mt
