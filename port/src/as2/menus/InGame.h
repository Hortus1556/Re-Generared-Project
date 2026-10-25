// The screens over a mission: PauseMenu (the Flash AS2's pause clipboard:
// sound and music switches, quit with "are you sure?", the help filofax,
// resume) and TutorialPopup (a tutorial's text in a box; its button goes
// on).
#pragma once

#include "as2/menus/FlashScreen.h"
#include "engine/ui/UIKit.h"

namespace as2 {

// Flash's PauseScreen: its `pauseScreen` clip at stage (39, 6) slides in
// and stops (frame 21); resume plays it out (to its last frame) and the
// mission goes on. Quit asks first (its yes and no shown on the press),
// and leaves under the bandages. Help opens the filofax (its `help` clip
// plays to 11): a tool's button shows its page (`pages` at the button's
// name), back closes the page, then the filofax (on to 21 and round to 1).
class PauseMenu : public FlashScreen {
public:
    explicit PauseMenu(const std::string& nib);
    // Its art made ahead (FlashScreen::prefetch), with the switches either way.
    static void prefetch();

protected:
    void onFrame() override;
    void dress(VectorLook& look) override;
    void onClick(const std::string& part) override;
    void onPress(const std::string& part) override;
    std::vector<std::string> clickable() const override;

private:
    Clip* help = nullptr;   // the filofax
    bool asking = false;    // quit's yes and no shown
    bool resuming = false;  // playing out
    std::string page;       // the help page shown (a button's name), if any
};

// CFOPauseMenu: the CFO pack's own pause (the iPad's nib, as the user
// asked for missions 21-30): quit with "are you sure?", help (HelpScreen),
// sound and music switches, resume.
class CFOPauseMenu : public ui::ViewController {
public:
    explicit CFOPauseMenu(const std::string& nib) : ViewController(nib) {}
    void setMusicButtonImage();
    void setSoundFXButtonImage();

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::Button* mMusic = nullptr;
    ui::Button* mSoundFX = nullptr;
    ui::Button* mQuitYes = nullptr;
    ui::Button* mQuitNo = nullptr;
    ui::ImageView* mImgViewQuit = nullptr;
};

class TutorialPopup : public ui::ViewController {
public:
    explicit TutorialPopup(const std::string& nib) : ViewController(nib) {}

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::TextView* mText = nullptr;
};

}  // namespace as2
