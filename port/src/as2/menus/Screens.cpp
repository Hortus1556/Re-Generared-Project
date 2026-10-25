#include "as2/menus/CutScene.h"
#include "as2/menus/Screens.h"

#include <cstdio>

#include "as2/Connector.h"
#include "as2/GameController.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/MainMenu.h"

namespace as2 {

// ---------------------------------------------------------- OperationDetails

void OperationDetails::connectOutlet(const std::string& name, ui::View* v) {
    if (name == "bestScore") bestScore = dynamic_cast<ui::TextView*>(v);
    else if (name == "bestTime") bestTime = dynamic_cast<ui::TextView*>(v);
}

void OperationDetails::viewDidLoad() {
    int level = Connector::sharedConnector().current_mission_id + 1;
    cutscene::prefetchStory(level - 1, false);  // (the story next)
    setBackground(view(), "PatientForm" + std::to_string(level) + ".png");
    if (bestScore) bestScore->text = std::to_string(SaveDetails::GetScoreForLevel(level));
    // The time is kept in frames (30 a second).
    int seconds = SaveDetails::GetTimeForLevel(level) / 30;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%i:%02d", seconds / 60, seconds - seconds / 60 * 60);
    if (bestTime) bestTime->text = buf;
    for (ui::TextView* t : {bestScore, bestTime})
        if (t) {
            t->fontName = "MarkerFelt-Thin";
            t->fontSize = 28;
            t->transform = ui::Affine::rotation(-0.05235988f);
        }
    // The leaderboard button: removed with Game Center.
    if (ui::View* lb = view()->viewWithTag(100)) lb->hidden = true;
}

void OperationDetails::action(const std::string&, ui::View* sender) {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    MenuManager& mm = MenuManager::sharedMenuManager();
    bool cfo = gameController->mLevelPack == 1;
    if (sender->tag == 1) {
        mm.ReplaceViewController(cfo ? "CFOLevelSelect" : "LevelSelect", "LevelSelect", "moveIn", "fromRight");
    } else if (sender->tag == 2) {
        MyOpenAL::sharedMyOpenAL().stopSoundWithKey("background");
        mm.ReplaceViewController(cfo ? "CFOCutScene" : "CutScene", "CutScene", "moveIn", "fromTop");
    }
}

// -------------------------------------------------------------------- Credits

void Credits::viewDidLoad() {
    creditPage = 1;
    setBackground(view(), "credits1.png");
}

void Credits::action(const std::string&, ui::View* sender) {
    if (sender->tag == 0) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
        if (creditPage == 1) {
            gameController->returnedFromCredits = true;
            MenuManager::sharedMenuManager().setupNewViewController("MainMenu", "MainMenu");
            return;
        }
        creditPage = 1;
        setBackground(view(), "credits1.png");
    } else if (sender->tag == 1 && creditPage == 1) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
        creditPage = 2;
        setBackground(view(), "credits2.png");
    }
}

// ----------------------------------------------------------------- HelpScreen

void HelpScreen::connectOutlet(const std::string& name, ui::View* v) {
    int n = 0;
    if (std::sscanf(name.c_str(), "mBtn%d", &n) == 1 && n >= 1 && n <= 9) mBtn[n - 1] = dynamic_cast<ui::Button*>(v);
}

void HelpScreen::viewDidLoad() { setBackgroundScreen(1); }

void HelpScreen::setBackgroundScreen(int page) {
    setBackground(view(), "helpscreen" + std::to_string(page) + ".png");
    // The tools to pick on the first page; on the others, back to it.
    for (int i = 0; i < 8; ++i)
        if (mBtn[i]) mBtn[i]->hidden = page != 1;
    if (mBtn[8]) mBtn[8]->hidden = page == 1;
    helpPage = page;
}

void HelpScreen::action(const std::string&, ui::View* sender) {
    if (sender->tag == 0) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
        bool cfo = Connector::sharedConnector().current_mission_id >= 20;
        MenuManager::sharedMenuManager().setupNewViewController(cfo ? "CFOPauseMenu" : "PauseMenu",
                                                                cfo ? "CFOPauseMenu" : "PauseMenu");
    } else {
        setBackgroundScreen(sender->tag == 10 ? 1 : sender->tag + 1);
    }
}

}  // namespace as2
