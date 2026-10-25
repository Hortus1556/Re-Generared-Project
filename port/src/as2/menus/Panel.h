// Panel: the mission's view over the GL scene: the tutorial panel, and the
// touches: on the HUD (Flash's: menus/Hud) a tool card picks its tool and
// the pause button pauses; on the patient a finger with a tool works it
// (the world point under it: half the screen point), without one it drags
// the view; two fingers zoom (1 to 2 times). The nib's own tool and pause
// buttons are hidden.
#pragma once

#include "engine/ui/UIKit.h"

namespace as2 {

class Panel : public ui::ViewController {
public:
    explicit Panel(const std::string& nib) : ViewController(nib) {}
    ~Panel() override;

    // The mission's tools offered (traced).
    void setupButtons();
    // Tool `index` (0..9 for tool 1..10, if the mission offers it): picked,
    // or put down when it was the one in hand (a click on its card also
    // sounds). SelectButton: the nib's button (tag 0..9).
    void selectTool(int index, bool clicked);
    void SelectButton(ui::View* sender);
    void Pause();
    // A view controller over the HUD (the tutorial popup), by nib and class.
    void addPopupView(const std::string& name);
    void removePopupView();
    // Keeps the zoomed world covering the screen.
    void KeepInBounds();
    // PC keys, as Flash's: the number keys pick tools 1..10 (0 the tenth),
    // Escape pauses (not while a tutorial popup is up).
    void toolKey(int n);
    void pauseKey();
    bool hasPopup() const { return mPopup != nullptr; }

    void touchesBegan(const ui::Touches& t) override;
    void touchesMoved(const ui::Touches& t) override;
    void touchesEnded(const ui::Touches& t) override;

    ui::ImageView* mImgViewTutPanel = nullptr;
    ui::TextView* mTextViewTutorial = nullptr;

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::Button* mBtn[10] = {};  // cutter, tongs, lighter, cream, stitch, vacuum,
                                // chainsaw, injection, battery, scanner
    ui::Button* mBtnPause = nullptr;
    ui::ViewController* mPopup = nullptr;  // retained
    unsigned long long mToolHash = 0;      // the touch working the tool
};

}  // namespace as2
