#include "as2/menus/Results.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

#include "as2/AS2App.h"
#include "as2/Connector.h"
#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/Loader.h"
#include "as2/menus/Bandages.h"
#include "as2/menus/CutScene.h"
#include "as2/menus/FlashStage.h"
#include "as2/menus/MainMenu.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

// Every mission's best score, added up.
long long totalScore() {
    long long sum = 0;
    for (int level = 1; level <= 30; ++level) sum += SaveDetails::GetScoreForLevel(level);
    return sum;
}

const char* levelSelect() { return gameController->mLevelPack == 1 ? "CFOLevelSelect" : "LevelSelect"; }

// The iPad's summaries' text: Marker Felt, turned a little.
void markerFelt(ui::TextView* t, float rotation) {
    if (!t) return;
    t->fontName = "MarkerFelt-Thin";
    t->fontSize = 32;
    if (rotation != 0) t->transform = ui::Affine::rotation(rotation);
}

std::string minutesSeconds(int frames) {
    int seconds = static_cast<int>(static_cast<float>(frames) / 30.0f);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%i:%02d", seconds / 60, seconds % 60);
    return buf;
}

void hideLeaderboard(ui::View* root) {
    // The leaderboard (OpenFeint) button: removed with the online services.
    if (ui::View* lb = root->viewWithTag(100)) lb->hidden = true;
}

}  // namespace

// -------------------------------------------------------------- CFOMissionWin

void CFOMissionWin::connectOutlet(const std::string& name, ui::View* v) {
    if (name == "mTextName") mTextName = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextScore") mTextScore = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextTimeLeft") mTextTimeLeft = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextTotalScore") mTextTotalScore = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextAway") mTextAway = dynamic_cast<ui::TextView*>(v);
    else if (name == "imgGrade") imgGrade = dynamic_cast<ui::ImageView*>(v);
    else if (name == "imgPoints") imgPoints = dynamic_cast<ui::ImageView*>(v);
    else if (name == "imgAwayGrade") imgAwayGrade = dynamic_cast<ui::ImageView*>(v);
}

void CFOMissionWin::viewDidLoad() {
    // Bango's "mission won" call is removed.
    setBackground(view(), "win_BG2.png");
    setupSummary();
    if (imgGrade) imgGrade->startAnimating();
    hideLeaderboard(view());
}

void CFOMissionWin::setupSummary() {
    for (ui::TextView* t : {mTextName, mTextScore, mTextTimeLeft, mTextTotalScore, mTextAway}) markerFelt(t, -0.05235988f);
    GameController& gc = *gameController;
    int score = gc.winScore, level = gc.winLevel;
    // The grade (A+ to D): the frames of the grade drawing shown (50 for
    // the best; for D, a share of 25 by how near C it came), the points to
    // the next grade and its letter.
    int frames, grade, away = 0;
    const char* next = "";
    if (score < gc.getRequiredScoreForGrade(3, level)) {
        away = gc.getRequiredScoreForGrade(3, level) - score;
        frames = score * 25 / std::max(1, gc.getRequiredScoreForGrade(3, level));
        grade = 5;
        next = "D";
    } else if (score < gc.getRequiredScoreForGrade(2, level)) {
        frames = 29;
        grade = 4;
        next = "C";
        away = gc.getRequiredScoreForGrade(2, level) - score;
    } else if (score < gc.getRequiredScoreForGrade(1, level)) {
        frames = 33;
        grade = 3;
        next = "B";
        away = gc.getRequiredScoreForGrade(1, level) - score;
    } else if (score < gc.getRequiredScoreForGrade(0, level)) {
        frames = 37;
        grade = 2;
        next = "A";
        away = gc.getRequiredScoreForGrade(0, level) - score;
    } else {
        frames = 50;
        grade = 1;
    }
    if (imgGrade) {
        std::vector<ui::Image*> images;
        for (int i = 1; i <= frames; ++i) {
            char name[32];
            std::snprintf(name, sizeof name, "grade%04d.png", i);
            images.push_back(ui::Image::named(name));
        }
        imgGrade->setAnimationImages(images);
        imgGrade->animationDuration = frames * 2.5 / 50.0;
        imgGrade->animationRepeatCount = 1;
        if (!images.empty()) imgGrade->setImage(images.back());
        for (ui::Image* i : images) releaseRef(i);
    }
    if (mTextName) mTextName->text = gc.getPatientName(level);
    if (mTextScore) mTextScore->text = std::to_string(score);
    if (mTextTimeLeft) mTextTimeLeft->text = minutesSeconds(gc.winTime);
    if (grade == 1) {
        for (ui::View* v : {static_cast<ui::View*>(mTextAway), static_cast<ui::View*>(imgAwayGrade),
                            static_cast<ui::View*>(imgPoints)})
            if (v) v->hidden = true;
    } else {
        if (mTextAway) mTextAway->text = std::to_string(away);
        if (imgAwayGrade) {
            ui::Image* img = ui::Image::named(std::string(next) + ".png");
            imgAwayGrade->setImage(img);
            releaseRef(img);
        }
    }
    if (mTextTotalScore) mTextTotalScore->text = std::to_string(totalScore());
}

void CFOMissionWin::action(const std::string&, ui::View* sender) {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    if (sender->tag != 0) return;  // 100: the leaderboard, removed
    MyOpenAL::sharedMyOpenAL().playMusicWithKey("background");
    MenuManager::sharedMenuManager().setupNewViewController(levelSelect(), "LevelSelect");
}

// ----------------------------------------------------------- CFOMissionFailed

void CFOMissionFailed::connectOutlet(const std::string& name, ui::View* v) {
    if (name == "mTextName") mTextName = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextScore") mTextScore = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextTimeLeft") mTextTimeLeft = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextTotalScore") mTextTotalScore = dynamic_cast<ui::TextView*>(v);
}

void CFOMissionFailed::viewDidLoad() {
    setBackground(view(), "failbg2.png");
    for (ui::TextView* t : {mTextName, mTextScore, mTextTimeLeft, mTextTotalScore}) markerFelt(t, 0);
    GameController& gc = *gameController;
    if (mTextName) mTextName->text = gc.getPatientName(gc.winLevel);
    if (mTextScore) mTextScore->text = std::to_string(gc.winScore);
    if (mTextTimeLeft) mTextTimeLeft->text = minutesSeconds(gc.winTime);
    if (mTextTotalScore) mTextTotalScore->text = std::to_string(totalScore());
    hideLeaderboard(view());
}

void CFOMissionFailed::action(const std::string&, ui::View* sender) {
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    al.stopSoundWithKey("FLATLINE");
    al.playSFXWithKey("SELECT");
    MenuManager& mm = MenuManager::sharedMenuManager();
    if (sender->tag == 100) return;  // the leaderboard, removed
    if (game) game->mKillGame = true;
    platform::debugTrace("failed %s", sender->tag == 0 ? "again" : "menu");
    if (sender->tag == 0) {
        // Again.
        mm.killViewController();
        mm.setupNewViewController("CFOStartScene", "StartScene");
    } else {
        mm.setupNewViewController("MainMenu", "MainMenu");
    }
}

// ------------------------------------------------------------- EndStoryScreen

namespace {
// The patients the Flash clipboards have a page for (holder.patient: the
// Flash AS2's 20 missions, in order); the CFO's are written in, in their
// fields' hand.
constexpr int kPatientFrames = 20;
// The cut scenes' holder's pieces: the dialogue's, its background (drawn
// apart, covering the screen), the upgrade, the summary, the banners, the
// outro, the speech panel.
const char* const kHolderPieces[] = {"/char1", "/char2",  "/char3",   "/char4",  "/speech", "/skip",
                                     "/next",  "/underBut", "/upgrade", "/summary", "/banner", "/outro",
                                     "/bg",    "#1212",    "#1213",    "#1225",   "#1228",  "#1229"};
// The grade bar's labels, worst to best.
const char* const kGrades[5] = {"F", "D", "C", "B", "A"};
}  // namespace

EndStoryScreen::EndStoryScreen(const std::string& shown)
    : FlashScreen("flash-cutscene", "cutSceneHolder"), shown(shown) {
    char name[64];
    std::snprintf(name, sizeof name, "cutscene_end_%i.json", gameController->winLevel + 1);
    std::string text;
    json::Value story;
    if (assets::readText(name, text)) {
        json::parse(text, story);
        const json::Value& screens = story["dialog"]["screen"];
        if (screens.size() > 0) place = screens[static_cast<size_t>(screens.size() - 1)]["bg"].str();
    }
    placeFrame = cutscene::backgroundFrame(place);
}

void EndStoryScreen::showOnly(VectorLook& look, const std::string& shown) {
    for (const char* p : kHolderPieces)
        if ("/" + shown != p) look.hidden.push_back(p);
}

void EndStoryScreen::viewDidLoad() {
    FlashScreen::viewDidLoad();
    // A place Flash had no background for: the iPad's picture under it all.
    if (placeFrame || place.empty()) return;
    ui::Image* img = ui::Image::named(place + ".png");
    if (!img) return;
    ui::ImageView* v = new ui::ImageView;
    v->setImage(img);
    v->setFrame({{0, 0}, {view()->bounds.width, view()->bounds.height}});
    v->userInteractionEnabled = false;
    view()->insertSubviewAtIndex(v, 0);
    releaseRef(v);
    releaseRef(img);
}

void EndStoryScreen::dress(VectorLook& look) { showOnly(look, shown); }

void EndStoryScreen::drawUnder(const VectorTransform&) { cutscene::drawBackground(placeFrame, age); }

// ----------------------------------------------------------------- MissionWin

MissionWin::MissionWin(const std::string&) : EndStoryScreen("summary") {
    // (Bango's "mission won" call is removed.)
    GameController& gc = *gameController;
    int score = gc.winScore, level = gc.winLevel;
    // The grade (the scores to beat: 3 D .. 0 A) and how far the next is.
    int grade = 4;
    for (int g = 3; g >= 0; --g)
        if (score < gc.getRequiredScoreForGrade(g, level)) {
            grade = 3 - g;
            nextGrade = kGrades[4 - g];
            away = gc.getRequiredScoreForGrade(g, level) - score;
            break;
        }
    summary = &inner("summary");
    summary->stops = {7};
    summary->gotoAndPlay(2);
    gradeBar = &inner("summary.holder.grade");
    int bar = art.innerSymbol(symbol, 1, 0, "/summary.holder.grade");
    gradeBar->stops = {std::max(1, bar >= 0 ? art.frameOf(bar, kGrades[grade]) : 1)};
    gradeBar->gotoAndPlay(1);
}

void MissionWin::prefetch() {
    VectorLook look;
    showOnly(look, "summary");
    look.hidden.push_back("summary.holder.submit");
    for (int f = 2; f <= 7; ++f) {
        look.frames["/summary"] = f;
        look.frames["/summary.holder.grade"] = f < 7 ? 1 : 50;
        FlashScreen::prefetch("flash-cutscene", "cutSceneHolder", {0, 0}, &look);
    }
}

void MissionWin::dress(VectorLook& look) {
    GameController& gc = *gameController;
    EndStoryScreen::dress(look);
    look.hidden.push_back("summary.holder.submit");
    if (gc.winLevel < kPatientFrames) look.frames["summary.holder.patient"] = gc.winLevel + 1;
    else look.hidden.push_back("summary.holder.patient");
    look.texts["summary.holder.time.txt"] = flashstage::formatTime(gc.winTime / 30);
    look.texts["summary.holder.score.txt"] = flashstage::prettyScore(gc.winScore);
    look.texts["summary.holder.tscore.txt"] = flashstage::prettyScore(totalScore());
    if (nextGrade.empty()) {
        for (const char* p : {"summary.holder.ngrade", "summary.holder.nscore", "summary.holder.paf"})
            look.hidden.push_back(p);
    } else {
        look.texts["summary.holder.ngrade"] = nextGrade;
        look.texts["summary.holder.nscore"] = flashstage::prettyScore(away);
    }
}

void MissionWin::drawOver(const VectorTransform& at) {
    GameController& gc = *gameController;
    if (gc.winLevel < kPatientFrames) return;
    VectorLook look;
    dress(look);
    writeIn("summary.holder.tscore.txt", gc.getPatientName(gc.winLevel),
            art.partBounds(symbol, main.frame, age, main.inFrame, at, &look, "summary.holder.patient"), 1.0f, at, look);
}

std::vector<std::string> MissionWin::clickable() const {
    if (chosen || summary->playing) return {};
    return {"summary.holder.start"};
}

void MissionWin::onClick(const std::string& part) {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    platform::debugTrace("win %s", part.c_str());
    chosen = true;
    if (Connector::sharedConnector().current_mission_id == 18) {
        PlayMovie();
        return;
    }
    MyOpenAL::sharedMyOpenAL().playMusicWithKey("background");
    MenuManager::sharedMenuManager().setupNewViewController(levelSelect(), "LevelSelect");
}

void MissionWin::PlayMovie() {
    // The first pack's ending: then the level select.
    retainRef(this);
    playMovie("EndMovieHi.m4v", [this] {
        MenuManager::sharedMenuManager().setupNewViewController(levelSelect(), "LevelSelect");
        MyOpenAL::sharedMyOpenAL().playMusicWithKey("background");
        releaseRef(this);
    });
}

// -------------------------------------------------------------- MissionFailed

void MissionFailed::prefetch() { FlashScreen::prefetch("flash-shell", "failedScreen", {0, 0}); }

MissionFailed::MissionFailed(const std::string&) : FlashScreen("flash-shell", "failedScreen") {
    main.stops = {6};
    bandages::prefetch();
}

void MissionFailed::onFrame() {
    if (age % (20 * 30) == 0) bandages::prefetch();
}

void MissionFailed::dress(VectorLook& look) {
    GameController& gc = *gameController;
    if (gc.winLevel < kPatientFrames) look.frames["holder.patient"] = gc.winLevel + 1;
    else look.hidden.push_back("holder.patient");
    look.texts["holder.time"] = flashstage::formatTime(gc.winTime / 30);
    look.texts["holder.score"] = flashstage::prettyScore(gc.winScore);
}

void MissionFailed::drawOver(const VectorTransform& at) {
    GameController& gc = *gameController;
    if (gc.winLevel < kPatientFrames) return;
    // The name on the patient's line, in the score's field's hand at the
    // patients' names' size, from where theirs start.
    VectorLook look;
    dress(look);
    writeIn("holder.score", gc.getPatientName(gc.winLevel),
            art.partBounds(symbol, main.frame, age, main.inFrame, at, &look, "holder.patient"), 1.4f, at, look);
}

std::vector<std::string> MissionFailed::clickable() const {
    if (chosen) return {};
    return {"holder.replay", "holder.quit"};
}

void MissionFailed::onClick(const std::string& part) {
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    al.stopSoundWithKey("FLATLINE");
    al.playSFXWithKey("SELECT");
    platform::debugTrace("failed %s", part.c_str());
    chosen = true;
    if (part == "holder.replay") {
        // Again: by the mission's start screen (the mission unloaded first).
        MenuManager::sharedMenuManager().killViewController();
        if (game) game->mKillGame = true;
        Loader::sharedLoader().startReplay();
    } else {
        bandages::play([] {
            if (game) game->missionQuit();
        });
    }
}

// ---------------------------------------------------------------- UpgradeTool

namespace {
std::string lowered(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string cardOf(int tool) { return "upgrade.holder." + lowered(GameData::toolNames[tool]); }
}  // namespace

UpgradeTool::UpgradeTool(const std::string&) : EndStoryScreen("upgrade") {
    book = &inner("upgrade");
    book->stops = {11};
    book->gotoAndPlay(2);
    for (int i = 0; i < 10; ++i) {
        toolLevel[i] = std::clamp(SaveDetails::GetToolLevel(GameData::toolNames[i]), 1, 3);
        int card = art.innerSymbol(symbol, 1, 0, "/" + cardOf(i));
        for (int l = 1; l <= 3; ++l)
            for (int hov = 0; hov < 2; ++hov) {
                std::string label = "lvl" + std::to_string(l) + (hov ? "hov" : "");
                levelFrame[i][l - 1][hov] = card >= 0 ? std::max(1, art.frameOf(card, label)) : 1;
            }
    }
}

void UpgradeTool::viewDidLoad() {
    EndStoryScreen::viewDidLoad();
    MyOpenAL::sharedMyOpenAL().stopSoundWithKey("background");
    MyOpenAL::sharedMyOpenAL().playMusicWithKey("bathroom");
}

void UpgradeTool::dress(VectorLook& look) {
    EndStoryScreen::dress(look);
    for (int i = 0; i < 10; ++i)
        look.frames[cardOf(i)] = levelFrame[i][toolLevel[i] - 1][i == selected && toolLevel[i] < 3 ? 1 : 0];
    if (selected < 0) look.alphas["upgrade.holder.restore"] = 0.3f;
}

std::vector<std::string> UpgradeTool::clickable() const {
    if (chosen || book->playing) return {};
    // (The cards of tools at their best take no press.)
    std::vector<std::string> v;
    for (int i = 0; i < 10; ++i)
        if (toolLevel[i] < 3) v.push_back(cardOf(i));
    if (selected >= 0) v.push_back("upgrade.holder.restore");
    return v;
}

void UpgradeTool::onPress(const std::string& part) {
    // selectTool (on the press): the card picked, or put back.
    for (int i = 0; i < 10; ++i)
        if (part == cardOf(i)) {
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
            selected = selected == i ? -1 : i;
            platform::debugTrace("upgrade pick %s", selected < 0 ? "none" : GameData::toolNames[i]);
            return;
        }
}

void UpgradeTool::onClick(const std::string& part) {
    if (part != "upgrade.holder.restore" || selected < 0) return;
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    al.playSFXWithKey("SELECT");
    chosen = true;
    SaveDetails::SaveToolLevel(GameData::toolNames[selected], toolLevel[selected] + 1);
    platform::debugTrace("upgrade %s to %d", GameData::toolNames[selected], toolLevel[selected] + 1);
    MenuManager& mm = MenuManager::sharedMenuManager();
    if (gameController->winScore == 0) {
        al.playMusicWithKey("background");
        mm.setupNewViewController(levelSelect(), "LevelSelect");
    } else {
        mm.setupNewViewController("MissionWin", "MissionWin");  // (the first pack's: the CFO's has no upgrades)
    }
}

}  // namespace as2
