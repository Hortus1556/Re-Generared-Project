// MenuManager: swaps the screen's view controller, with a CATransition on
// the window (ReplaceViewController) or at once (setupNewViewController).
// A screen is a nib and the class of its view controller (MenuTypes:
// "CFOCutScene" is a CutScene on the CFO pack's nib); the class is made by
// name (NSClassFromString), from the classes the game registers
// (registerViewController).
#pragma once

#include <functional>
#include <string>

#include "engine/ui/UIKit.h"

namespace as2 {

class MenuManager : public Object, public ui::AnimationDelegate {
public:
    static MenuManager& sharedMenuManager();

    // A view controller class: made with its nib's name.
    static void registerViewController(const std::string& className,
                                       std::function<ui::ViewController*(const std::string& nib)> make);

    // transition: "fade" (0.5 s) or another CATransition type (0.25 s),
    // direction its subtype.
    void ReplaceViewController(const std::string& nibName, const std::string& className,
                               const std::string& transition, const std::string& direction);
    void setupNewViewController(const std::string& nibName, const std::string& className);
    // A view controller of the class on the nib (NSClassFromString,
    // initWithNibName:), not shown; nullptr for a class not registered.
    static ui::ViewController* make(const std::string& className, const std::string& nibName);
    void killCurrentViewController();
    void killViewController();
    void spinView();

    ui::ViewController* mCurrentViewController = nullptr;  // retained
    const std::string& mCurrentMenu() const { return currentMenu; }

    void animationDidStart(ui::Animation*) override;
    void animationDidStop(ui::Animation*, bool finished) override;

private:
    ui::ViewController* CreateViewController(const std::string& className, const std::string& nibName);
    void replaceSubview(ui::View* oldView, ui::View* newView, const std::string& transition,
                        const std::string& direction, double duration);
    ui::ViewController* mNewViewController = nullptr;  // retained
    std::string currentMenu;
    bool transitioning = false, wasEnabled = true;
};

}  // namespace as2
