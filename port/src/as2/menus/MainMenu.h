// MainMenu: the title screen. The legal notice fades out, the buttons rise
// in; NEW GAME (with progress: "are you sure?") plays the intro movie and
// goes to the first cut scene; CONTINUE goes to the episode choice.
//
// Removed with the online services: the leaderboard, achievement and
// "more games" buttons stay hidden.
#pragma once

#include <memory>

#include "engine/ui/UIKit.h"

class MoviePlayer;

namespace as2 {

// The game's UIView that draws an image scaled to its bounds (the menus'
// backgrounds).
class PCGImageView : public ui::View {
public:
    ~PCGImageView() override { releaseRef(image_); }
    void setImage(ui::Image* img) {
        retainRef(img);
        releaseRef(image_);
        image_ = img;
    }
    ui::Image* image() const { return image_; }

protected:
    void drawContent(float alpha) override { drawImage(image_, {{0, 0}, bounds}, alpha); }

private:
    ui::Image* image_ = nullptr;
};

// Sets a PCGImageView's image from a bundle PNG (the menus' viewDidLoad).
void setBackground(ui::View* v, const std::string& png);

class MainMenu : public ui::ViewController {
public:
    explicit MainMenu(const std::string& nib) : ViewController(nib) {}
    ~MainMenu() override;

    void dealWithCredits();
    void legalOutAnimation();
    void moveStartIn();
    void moveMenuInNewGame();
    void moveMenuOutNewGame();
    void PlayMovie();
    void endVideo();
    void checkContinueButton();
    void checkPlayBGMusic();

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    // A button or view rising from below (or the reverse) along a straight
    // path over 0.25 s, ending at its new centre.
    void slide(ui::View* v, float fromY, float toY, float endY);
    ui::ImageView* mImgViewLegalText = nullptr;
    ui::ImageView* mImgViewSubMenu = nullptr;
    ui::Button* mBtnYeah = nullptr;
    ui::Button* mBtnNoWay = nullptr;
    ui::Button* mBtnNewGame = nullptr;
    ui::Button* mBtnContinue = nullptr;
    ui::Button* mBtnLeaderBoard = nullptr;
    ui::Button* mBtnAchievement = nullptr;
    ui::Button* mBtnMoreGames = nullptr;
    ui::Button* mBtnCredits = nullptr;
    ui::Timer* timerPlayBackgroundMusic = nullptr;  // retained
    bool bLegalTextDone = false;
};

}  // namespace as2
