// The achievements (the port's; StatsMgr.h): the list and the banner.
//
// AchievementsScreen: opened by the title's top-left button, the one 1.45
// gave Game Center (WelcomeScreen::clickedButton 0x318238 →
// GamingSocialNetworkMgr::openAchievementsView 0x30bf88, Game Center's own
// list). It lists the current slot's 22 achievements on the Upgrades
// Machine's board (common/ui_techtree.json, upgrade_screen_sheet): a tile
// each in its frames (gold once earned, the icon greyed until then), and the
// machine's screen shows the one picked: its title, what it takes, the count
// so far, the tick once earned. Under the board, the Director's cut switch
// (RestoredContent.h), shut until the slot has earned enough of them.
//
// AchievementPopup: what 1.45 left to Game Center (unlockAchievement
// 0x30be6c set showsCompletionBanner): a banner slides down from the top
// over whatever screen is on (the director's notification node, so it
// outlives a change of screen), with the achievement's title, then goes
// back up; one at a time, in turn. Its art is the top bar's plate and star
// (global_ui_sheet), its sound the Upgrades Machine's "upgrade_confirm".
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "as3/Scene.h"
#include "as3/StatsMgr.h"

namespace mt {

class AchievementsScreen : public Scene {
public:
    static AchievementsScreen* create();
    void load() override;
    void unload() override;

private:
    void onButton(cocos2d::CCObject* sender);
    void onTile(cocos2d::CCObject* sender);
    void onDirectorsCut(cocos2d::CCObject* sender);
    void select(int index);
    void refreshDirectorsCut();

    UIDefinition m_def;
    std::vector<cocos2d::CCNode*> m_frames;  // each tile's frame (the layout's)
    cocos2d::CCSprite* m_bigIcon = nullptr;  // on the machine's screen
    cocos2d::CCMenuItemSprite* m_cut = nullptr;
    cocos2d::CCMenu* m_tiles = nullptr;
    int m_selected = -1;
};

class AchievementPopup : public cocos2d::CCNode {
public:
    // an achievement earned: its banner, after any already showing
    static void show(const AchievementDef& a);

private:
    static AchievementPopup* shared();
    void tick(float dt);
    void startNext();
    void finish();

    std::deque<AchievementDef> m_queue;
    cocos2d::CCNode* m_banner = nullptr;
    float m_time = -1;  // the banner's time shown; -1 none
};

}  // namespace mt
