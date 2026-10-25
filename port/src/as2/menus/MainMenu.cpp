#include "as2/menus/MainMenu.h"

#include "as2/AS2App.h"
#include "as2/Connector.h"
#include "as2/GameController.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"

namespace as2 {

void setBackground(ui::View* v, const std::string& png) {
    if (PCGImageView* p = dynamic_cast<PCGImageView*>(v)) {
        ui::Image* img = png.empty() ? nullptr : ui::Image::named(png);  // (none: no picture)
        p->setImage(img);
        releaseRef(img);
    }
}

MainMenu::~MainMenu() {
    if (timerPlayBackgroundMusic) {
        timerPlayBackgroundMusic->invalidate();
        releaseRef(timerPlayBackgroundMusic);
    }
}

void MainMenu::connectOutlet(const std::string& name, ui::View* v) {
    auto button = [&](ui::Button*& b) { b = dynamic_cast<ui::Button*>(v); };
    if (name == "mImgViewLegalText") mImgViewLegalText = dynamic_cast<ui::ImageView*>(v);
    else if (name == "mImgViewSubMenu") mImgViewSubMenu = dynamic_cast<ui::ImageView*>(v);
    else if (name == "mBtnYeah") button(mBtnYeah);
    else if (name == "mBtnNoWay") button(mBtnNoWay);
    else if (name == "mBtnNewGame") button(mBtnNewGame);
    else if (name == "mBtnContinue") button(mBtnContinue);
    else if (name == "mBtnLeaderBoard") button(mBtnLeaderBoard);
    else if (name == "mBtnAchievement") button(mBtnAchievement);
    else if (name == "mBtnMoreGames") button(mBtnMoreGames);
    else if (name == "mBtnCredits") button(mBtnCredits);
}

void MainMenu::viewDidLoad() {
    setBackground(view(), "background.png");
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    if (!al.isPlayingSoundWithKey("background")) al.playMusicWithKey("background");
    const float H = gameController->ActualScreenSize.height;
    // Everything starts below the screen.
    if (mImgViewSubMenu) mImgViewSubMenu->center.y = H + 140;
    if (mBtnYeah) mBtnYeah->center.y = H + 220;
    if (mBtnNoWay) mBtnNoWay->center.y = H + 220;
    if (mImgViewSubMenu) mImgViewSubMenu->hidden = false;
    if (mBtnYeah) mBtnYeah->hidden = false;
    if (mBtnNoWay) mBtnNoWay->hidden = false;
    if (mBtnNewGame) mBtnNewGame->center.y = H + 50;
    if (mBtnContinue) mBtnContinue->center.y = H + 50;
    if (mBtnContinue)
        mBtnContinue->enabled = SaveDetails::GetScoreForLevel(1) >= 1 || SaveDetails::GetScoreForLevel(21) >= 1;
    // (The view's turn to landscape is the port's screen already.)
    // The online buttons are removed.
    for (ui::Button* b : {mBtnLeaderBoard, mBtnAchievement, mBtnMoreGames})
        if (b) b->hidden = true;
    if (!al.isPlayingSoundWithKey("background")) {
        timerPlayBackgroundMusic = retainRef(ui::Timer::scheduled(0.75, true, [this](ui::Timer*) { checkPlayBGMusic(); }));
    }
    if (gameController->mCurrentSplashLevel == 4) dealWithCredits();
}

void MainMenu::checkPlayBGMusic() {
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    if (!al.isPlayingSoundWithKey("background")) al.playMusicWithKey("background");
    if (al.isPlayingSoundWithKey("background") && timerPlayBackgroundMusic) {
        timerPlayBackgroundMusic->invalidate();
        releaseRef(timerPlayBackgroundMusic);
        timerPlayBackgroundMusic = nullptr;
    }
}

void MainMenu::checkContinueButton() {
    if (mBtnContinue) mBtnContinue->enabled = SaveDetails::GetScoreForLevel(1) > 0;
}

void MainMenu::dealWithCredits() {
    if (!gameController->returnedFromCredits) {
        bLegalTextDone = false;
        retainRef(this);
        ui::Timer::scheduled(2.5, false, [this](ui::Timer*) {
            legalOutAnimation();
            releaseRef(this);
        });
    } else {
        gameController->returnedFromCredits = false;
        bLegalTextDone = true;
        if (mImgViewLegalText) mImgViewLegalText->hidden = true;
        moveStartIn();
    }
}

void MainMenu::legalOutAnimation() {
    if (mImgViewLegalText) {
        ui::Animation* a = ui::Animation::basic("opacity");
        a->duration = 1.0;
        a->hasFrom = a->hasTo = true;
        a->from[0] = 1;
        a->to[0] = 0;
        mImgViewLegalText->addAnimation(a, "animateOpacity");
        releaseRef(a);
        mImgViewLegalText->alpha = 0;
    }
    retainRef(this);
    ui::Timer::scheduled(1.0, false, [this](ui::Timer*) {
        moveStartIn();
        releaseRef(this);
    });
}

void MainMenu::slide(ui::View* v, float fromY, float toY, float endY) {
    if (!v) return;
    ui::Animation* a = ui::Animation::keyframe("position");
    a->removedOnCompletion = true;
    a->path = {{v->center.x, fromY}, {v->center.x, toY}};
    a->duration = 0.25;
    v->addAnimation(a, "moveButtonToPosition");
    releaseRef(a);
    v->center.y = endY;
}

void MainMenu::moveStartIn() {
    bLegalTextDone = true;
    const float H = gameController->ActualScreenSize.height;
    slide(mBtnNewGame, H + 150, H - 50, H - 50);
    slide(mBtnContinue, H + 150, H - 50, H - 50);
    ui::Animation* fade = ui::Animation::basic("opacity");
    fade->duration = 0.25;
    fade->hasFrom = fade->hasTo = true;
    fade->from[0] = 0;
    fade->to[0] = 1;
    // The "more games", leaderboard and achievement buttons faded in here
    // too: removed with the online services.
    if (mBtnCredits) {
        mBtnCredits->addAnimation(fade, "animateOpacity");
        mBtnCredits->alpha = 1;
        mBtnCredits->hidden = false;
    }
    releaseRef(fade);
    if (mBtnContinue) mBtnContinue->enabled = SaveDetails::GetScoreForLevel(1) > 0;
}

void MainMenu::moveMenuInNewGame() {
    const float H = gameController->ActualScreenSize.height;
    slide(mBtnNewGame, H + 150, H - 50, H - 50);
    slide(mImgViewSubMenu, H - 140, H + 140, H + 140);
    slide(mBtnYeah, H - 60, H + 220, H + 220);
    // The original leaves this one at H + 110 (still below the screen).
    slide(mBtnNoWay, H - 60, H + 220, H + 110);
}

void MainMenu::moveMenuOutNewGame() {
    const float H = gameController->ActualScreenSize.height;
    if (mBtnNewGame) slide(mBtnNewGame, mBtnNewGame->center.y, H + 150, H + 170);
    slide(mImgViewSubMenu, H + 140, H - 140, H - 140);
    slide(mBtnYeah, H + 220, H - 60, H - 60);
    slide(mBtnNoWay, H + 220, H - 60, H - 60);
}

void MainMenu::PlayMovie() {
    gameController->mLevelPack = 0;
    MyOpenAL::sharedMyOpenAL().stopSoundWithKey("background");
    Connector::sharedConnector().set_current_mission(0);
    Connector::sharedConnector().set_current_mission_id(0);
    // The iPad build took the 480 x 320 movie (ScreenScaleFactor 1); the
    // port plays the same movie at 960 x 640.
    retainRef(this);
    playMovie("MovieHi.m4v", [this] {
        endVideo();
        releaseRef(this);
    });
}

void MainMenu::endVideo() {
    MenuManager::sharedMenuManager().setupNewViewController(
        gameController->mLevelPack == 1 ? "CFOCutScene" : "CutScene", "CutScene");
}

void MainMenu::action(const std::string&, ui::View* sender) {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    MenuManager& mm = MenuManager::sharedMenuManager();
    switch (sender->tag) {
        case 1:
            if (SaveDetails::GetScoreForLevel(1) < 1) PlayMovie();
            else moveMenuOutNewGame();
            break;
        case 2:
            mm.killViewController();
            mm.setupNewViewController("FileSelect", "FileSelect");
            break;
        case 3:
            mm.killViewController();
            mm.setupNewViewController("Credits", "Credits");
            break;
        case 10:
            moveMenuInNewGame();
            break;
        case 11:
            SaveDetails::ClearDownForNewGame();
            PlayMovie();
            break;
        default:  // 4 more games, 5 leaderboard, 6 achievements: removed
            break;
    }
}

}  // namespace as2
