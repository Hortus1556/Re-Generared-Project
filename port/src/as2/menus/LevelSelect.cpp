#include "as2/menus/LevelSelect.h"

#include <cstdio>

#include "as2/Connector.h"
#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/MainMenu.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

ui::Image* image(const char* fmt, int n) {
    char name[64];
    std::snprintf(name, sizeof name, fmt, n);
    return ui::Image::named(std::string(name) + ".png");
}

void setBackground(ui::Button* b, ui::Image* img, int state) {
    if (b) b->setBackgroundImage(img, state);
    releaseRef(img);
}

// The grade a score earns on a mission (0-based): A..D, else F.
const char* gradeImage(int score, int mission) {
    static const char* const names[4] = {"A", "B", "C", "D"};
    for (int g = 0; g < 4; ++g)
        if (score >= gameController->getRequiredScoreForGrade(g, mission)) return names[g];
    return "F";
}

}  // namespace

int upgradesBought() {
    int n = 0;
    for (const char* tool : GameData::toolNames3) n += SaveDetails::GetToolLevel(tool) - 1;
    return n;
}

// ----------------------------------------------------------------- FileSelect

void FileSelect::connectOutlet(const std::string& name, ui::View* v) {
    if (name == "mBtn1") mBtn1 = dynamic_cast<ui::Button*>(v);
    else if (name == "mBtn2") mBtn2 = dynamic_cast<ui::Button*>(v);
    // debugSwitch (a developers' UISwitch) is not in the nib.
}

void FileSelect::viewDidLoad() {
    if (upgradesBought() < 5 && mBtn2) {
        // The CFO pack is hard: a warning on its folder until 5 upgrades.
        ui::Image* img = ui::Image::named("episodesWarnCFO.png");
        mBtn2->setBackgroundImage(img, ui::Button::kNormal);
        releaseRef(img);
    }
    if (mBtn1) mBtn1->transform = ui::Affine::rotation(-0.06981317f);
    if (mBtn2) mBtn2->transform = ui::Affine::rotation(0.2792527f);
}

void FileSelect::action(const std::string&, ui::View* sender) {
    MenuManager& mm = MenuManager::sharedMenuManager();
    if (sender->tag == 0) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
        gameController->mLevelPack = 0;
        int done = 0;
        for (int level = 1; level <= 20; ++level)
            if (SaveDetails::GetScoreForLevel(level) != 0) ++done;
        int upgrades = upgradesBought();
        if (upgrades < done && upgrades < 20) {
            // An upgrade is owed for every mission done: buy it first.
            gameController->winScore = 0;
            mm.setupNewViewController("UpgradeTool", "UpgradeTool");
        } else {
            mm.killViewController();
            mm.setupNewViewController("LevelSelect", "LevelSelect");
        }
    } else if (sender->tag == 1) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT2");
        gameController->mLevelPack = 1;
        mm.setupNewViewController("CFOLevelSelect", "LevelSelect");
    }
}

// ---------------------------------------------------------------- LevelSelect

void LevelSelect::connectOutlet(const std::string& name, ui::View* v) {
    int n = 0;
    if (std::sscanf(name.c_str(), "mBtn%d", &n) == 1 && n >= 1 && n <= 10) mBtnPtr[n - 1] = dynamic_cast<ui::Button*>(v);
    else if (std::sscanf(name.c_str(), "mGrade%d", &n) == 1 && n >= 1 && n <= 10)
        mGradePtr[n - 1] = dynamic_cast<ui::ImageView*>(v);
    else if (name == "mBtnPrevious") mBtnPrevious = dynamic_cast<ui::Button*>(v);
    else if (name == "mBtnNext") mBtnNext = dynamic_cast<ui::Button*>(v);
}

void LevelSelect::viewDidLoad() {
    // The leaderboards button (tag 102, OpenFeint): removed with the online
    // services.
    if (ui::View* lb = view()->viewWithTag(102)) lb->hidden = true;
    mLevelSelected = false;
    Connector& c = Connector::sharedConnector();
    if (gameController->mLevelPack == 0 && c.current_mission_id >= 20) c.set_current_mission_id(0);
    else if (gameController->mLevelPack == 1 && c.current_mission_id < 20) c.set_current_mission_id(20);
    int id = c.current_mission_id;
    if (id < 6) mPage = 0;
    else if (id < 10) mPage = 1;
    else if (id < 16) mPage = 2;
    else if (id < 20) mPage = 3;
    else if (id < 26) mPage = 4;
    else if (id <= 29) mPage = 5;
    as2::setBackground(view(), "levelSelectBg" + std::to_string(mPage + 1) + ".png");
    setupPage();
}

void LevelSelect::setupPage() {
    // (The trace's "level page <n>", for the play-tests.)
    platform::debugTrace("level page %d", mPage);
    if (mBtnNext) mBtnNext->hidden = mPage == 5 || mPage == 3;
    // Even pages show missions 1-6 of their ten, odd pages 7-10.
    int start = mPage == 2 || mPage == 3 ? 11 : 1;
    bool firstSix = mPage % 2 == 0;
    for (int i = 0; i < 10; ++i) {
        if (mBtnPtr[i]) mBtnPtr[i]->hidden = firstSix ? i >= 6 : i < 6;
        if (mGradePtr[i]) mGradePtr[i]->hidden = firstSix ? i >= 6 : i < 6;
    }
    auto unlocked = [&](int i, int level, int score) {
        mBtnPtr[i]->enabled = true;
        if (score == 0) {
            if (mGradePtr[i]) mGradePtr[i]->hidden = true;
            setBackground(mBtnPtr[i], image("unlocked-%i", level), ui::Button::kNormal);
            return;
        }
        setBackground(mBtnPtr[i], image("complete-%i", level), ui::Button::kNormal);
        if (mGradePtr[i]) {
            ui::Image* g = ui::Image::named(std::string(gradeImage(score, level - 1)) + ".png");
            mGradePtr[i]->setImage(g);
            releaseRef(g);
        }
    };
    if (gameController->mLevelPack == 0) {
        // The bonus missions (10 and 20) open only with an A on every mission
        // before them.
        int aGrades = 0;
        while (aGrades < 19 && SaveDetails::GetScoreForLevel(aGrades + 1) >=
                                   gameController->getRequiredScoreForGrade(0, aGrades))
            ++aGrades;
        for (int i = 0; i < 10; ++i) {
            int level = start + i;
            if (!mBtnPtr[i]) continue;
            int score = SaveDetails::GetScoreForLevel(level);
            bool open;
            if (level < 2 || SaveDetails::GetScoreForLevel(level - 1) > 0)
                open = !((level == 10 && aGrades < 9) || (level == 20 && aGrades < 19));
            else
                open = level == 11 && SaveDetails::GetScoreForLevel(9) > 0;
            if (open) {
                unlocked(i, level, score);
                continue;
            }
            mBtnPtr[i]->enabled = false;
            if (mGradePtr[i]) mGradePtr[i]->hidden = true;
            if (i == 9) {
                setBackground(mBtnPtr[9], ui::Image::named("bonus-mission.png"), ui::Button::kDisabled);
                break;
            }
            setBackground(mBtnPtr[i], ui::Image::named("locked.png"), ui::Button::kDisabled);
        }
    } else {
        for (int i = 0; i < 10; ++i) {
            int level = start + 20 + i;
            if (!mBtnPtr[i]) continue;
            if ((mPage == 4 && i < 6) || (mPage == 5 && i > 5)) {
                int score = SaveDetails::GetScoreForLevel(level);
                if (level < 22 || SaveDetails::GetScoreForLevel(level - 1) > 0 ||
                    (level == 11 && SaveDetails::GetScoreForLevel(9) > 0)) {
                    unlocked(i, level, score);
                } else {
                    mBtnPtr[i]->enabled = false;
                    if (mGradePtr[i]) mGradePtr[i]->hidden = true;
                    setBackground(mBtnPtr[i], ui::Image::named("levSelectLocked.png"), ui::Button::kDisabled);
                }
            } else if (mGradePtr[i]) {
                mGradePtr[i]->hidden = true;
            }
        }
    }
    as2::setBackground(view(), "levelSelectBg" + std::to_string(mPage + 1) + ".png");
}

void LevelSelect::action(const std::string&, ui::View* sender) {
    if (mLevelSelected) return;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    MenuManager& mm = MenuManager::sharedMenuManager();
    int tag = sender->tag;
    if (tag == 100) {
        if (mPage == 0 || mPage == 4) {
            mm.ReplaceViewController("FileSelect", "FileSelect", "moveIn", "fromTop");
            return;
        }
        --mPage;
        setupPage();
        return;
    }
    if (tag == 101) {
        ++mPage;
        setupPage();
        return;
    }
    if (tag == 102) return;  // the leaderboard: removed
    int mission = mPage < 2 ? tag - 1 : (mPage >> 1) * 8 + (mPage & ~1) - 1 + tag;
    Connector::sharedConnector().set_current_mission(mission);
    Connector::sharedConnector().set_current_mission_id(mission);
    gameController->isEndScene = false;
    mLevelSelected = true;
    bool cfo = gameController->mLevelPack == 1;
    if (SaveDetails::GetScoreForLevel(mission + 1) == 0) {
        // Never played: its story first.
        MyOpenAL::sharedMyOpenAL().stopSoundWithKey("background");
        mm.ReplaceViewController(cfo ? "CFOCutScene" : "CutScene", "CutScene", "moveIn", "fromTop");
    } else {
        mm.ReplaceViewController(cfo ? "CFOOperationDetails" : "OperationDetails", "OperationDetails", "moveIn",
                                 "fromLeft");
    }
}

}  // namespace as2
