// The screens after a mission (the Flash AS2's): MissionWin (the discharge form:
// the patient, score, time left, total score, the grade drawn in and how
// far the next one is; on to the level select, or after the first pack's
// last mission its end movie),
// MissionFailed (the obituary: retry or the main menu) and UpgradeTool (the
// scrapbook: a first win's prize, one tool up a level).
#pragma once

#include "as2/menus/FlashScreen.h"
#include "engine/ui/UIKit.h"

namespace as2 {

// The Flash cut scenes' holder after the end story (CutScene's upgrade and
// summary): only its piece `shown` drawn, over the story's last place
// (cutscene::drawBackground, or the iPad's picture where Flash had none).
class EndStoryScreen : public FlashScreen {
protected:
    explicit EndStoryScreen(const std::string& shown);
    void viewDidLoad() override;
    // The rest of the holder hidden (a screen's own dress calls it first).
    void dress(VectorLook& look) override;
    void drawUnder(const VectorTransform& at) override;
    // A look of the holder showing only `shown` (its background drawn apart).
    static void showOnly(VectorLook& look, const std::string& shown);

    std::string shown;
    std::string place;   // the end story's last place
    int placeFrame = 0;  // its Flash background (0: the picture)
};

// Flash's cut scene summary (CutScene.createSummary): the discharge form on
// its clipboard comes in (frames 2-7), its grade bar steps a frame each
// Flash frame up to the grade's label (F, D, C, B, A); the patient's page
// (the CFO's written in), the score, the time left, the total; the points
// still to the next grade. Its "submit score" (online) is gone; continue
// goes on as the iPad's did.
class MissionWin : public EndStoryScreen {
public:
    explicit MissionWin(const std::string& nib);
    void PlayMovie();
    // Its art made ahead (FlashScreen::prefetch): the form coming in.
    static void prefetch();

protected:
    void dress(VectorLook& look) override;
    void drawOver(const VectorTransform& at) override;
    void onClick(const std::string& part) override;
    std::vector<std::string> clickable() const override;

private:
    Clip* summary = nullptr;
    Clip* gradeBar = nullptr;
    std::string nextGrade;  // the grade the score is short of ("": an A)
    int away = 0;           // by so many points
    bool chosen = false;
};

// Flash's FailedScreen over the stopped mission: its `failedScreen` clip
// (the obituary report on a clipboard) comes up and stops (frame 6), with
// the patient (holder.patient at the mission's frame: name and faded
// portrait), the time left and the score. Replay starts the mission again
// (by its start screen); quit leaves under the bandages.
class MissionFailed : public FlashScreen {
public:
    explicit MissionFailed(const std::string& nib);
    // Its art made ahead (FlashScreen::prefetch).
    static void prefetch();

protected:
    void onFrame() override;
    void dress(VectorLook& look) override;
    void drawOver(const VectorTransform& at) override;
    void onClick(const std::string& part) override;
    std::vector<std::string> clickable() const override;

private:
    bool chosen = false;
};

// Flash's upgrade (CutScene.createUpgrade): the scrapbook "The good old
// days... restoring my memories" comes in (frames 2-11), a card per tool at
// its level (`lvl<n>`); a press on one below level 3 picks it (`lvl<n>hov`;
// again, or another, puts it back), and "upgrade tool" (dimmed till then)
// takes it a level up. On to the summary, or (an upgrade owed from the file
// select) the level select.
// The CFO pack's own summaries (the iPad's nibs, as the user asked for
// missions 21-30). CFOMissionWin: the patient, score, time left, total
// score, the grade drawn in and how far the next one is; on to the level
// select. CFOMissionFailed: after the mission is unloaded (Loader), the
// patient, score, time left; again (its start screen) or the main menu.
class CFOMissionWin : public ui::ViewController {
public:
    explicit CFOMissionWin(const std::string& nib) : ViewController(nib) {}

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    void setupSummary();
    ui::TextView* mTextName = nullptr;
    ui::TextView* mTextScore = nullptr;
    ui::TextView* mTextTimeLeft = nullptr;
    ui::TextView* mTextTotalScore = nullptr;
    ui::TextView* mTextAway = nullptr;
    ui::ImageView* imgGrade = nullptr;
    ui::ImageView* imgPoints = nullptr;
    ui::ImageView* imgAwayGrade = nullptr;
};

class CFOMissionFailed : public ui::ViewController {
public:
    explicit CFOMissionFailed(const std::string& nib) : ViewController(nib) {}

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::TextView* mTextName = nullptr;
    ui::TextView* mTextScore = nullptr;
    ui::TextView* mTextTimeLeft = nullptr;
    ui::TextView* mTextTotalScore = nullptr;
};

class UpgradeTool : public EndStoryScreen {
public:
    explicit UpgradeTool(const std::string& nib);

protected:
    void viewDidLoad() override;
    void dress(VectorLook& look) override;
    void onPress(const std::string& part) override;
    void onClick(const std::string& part) override;
    std::vector<std::string> clickable() const override;

private:
    Clip* book = nullptr;
    int toolLevel[10] = {};
    int levelFrame[10][3][2] = {};  // each card's frame by level, picked or not
    int selected = -1;
    bool chosen = false;
};

}  // namespace as2
