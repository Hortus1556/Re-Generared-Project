#include "as2/MenuManager.h"

#include <map>

#include "as2/GameController.h"
#include "as2/menus/MainMenu.h"
#include "platform/Platform.h"

namespace as2 {

namespace {
std::map<std::string, std::function<ui::ViewController*(const std::string&)>>& classes() {
    static std::map<std::string, std::function<ui::ViewController*(const std::string&)>> c;
    return c;
}
}  // namespace

MenuManager& MenuManager::sharedMenuManager() {
    static MenuManager* instance = new MenuManager;
    return *instance;
}

void MenuManager::registerViewController(const std::string& className,
                                         std::function<ui::ViewController*(const std::string&)> make) {
    classes()[className] = std::move(make);
}

ui::ViewController* MenuManager::make(const std::string& className, const std::string& nibName) {
    auto it = classes().find(className);
    if (it == classes().end()) {
        platform::debugTrace("menu: no view controller class %s", className.c_str());
        return nullptr;
    }
    return it->second(nibName);
}

ui::ViewController* MenuManager::CreateViewController(const std::string& className, const std::string& nibName) {
    ui::ViewController* vc = make(className, nibName);
    if (!vc) return nullptr;
    currentMenu = nibName;
    platform::debugTrace("menu %s (%s)", className.c_str(), nibName.c_str());
    return vc;
}

void MenuManager::animationDidStart(ui::Animation*) {
    transitioning = true;
    wasEnabled = ui::Window::shared().userInteractionEnabled;
}

void MenuManager::animationDidStop(ui::Animation*, bool finished) {
    ui::Window& w = ui::Window::shared();
    transitioning = false;
    w.userInteractionEnabled = true;
    if (MainMenu* mm = dynamic_cast<MainMenu*>(mNewViewController)) {
        if (gameController->mCurrentSplashLevel != 4) {
            mm->dealWithCredits();
            gameController->mCurrentSplashLevel = 4;
        }
    }
    if (finished) {
        w.removeAnimationForKey("transitionViewAnimation");
        releaseRef(mCurrentViewController);
        mCurrentViewController = mNewViewController;
        mNewViewController = nullptr;
    }
}

void MenuManager::spinView() {
    ui::Animation* a = ui::Animation::basic("transform.rotation");
    a->hasTo = true;
    a->to[0] = 3.141593f;
    if (mCurrentViewController) mCurrentViewController->view()->addAnimation(a, "spinAnimation");
    releaseRef(a);
}

void MenuManager::replaceSubview(ui::View* oldView, ui::View* newView, const std::string& transition,
                                 const std::string& direction, double duration) {
    if (transitioning) return;
    ui::Window& w = ui::Window::shared();
    int index = 0;
    if (oldView && oldView->superview() == &w) {
        index = w.subviews().indexOfObject(oldView);
        oldView->removeFromSuperview();
    }
    if (newView && !newView->superview()) w.insertSubviewAtIndex(newView, index);
    ui::Animation* t = ui::Animation::transition();
    t->setDelegate(this, this);
    t->type = transition;
    if (transition != "fade") {
        // The directions are the iPad's portrait window's: held landscape
        // with the home button on the left, its top is the screen's right.
        t->subtype = direction == "fromTop"      ? "fromRight"
                     : direction == "fromLeft"   ? "fromTop"
                     : direction == "fromBottom" ? "fromLeft"
                     : direction == "fromRight"  ? "fromBottom"
                                                 : direction;
    }
    t->setValue("PCGOldView", mCurrentViewController);
    t->duration = duration;
    t->timing = ui::Animation::kEaseInEaseOut;
    w.addAnimation(t, "transitionViewAnimation");
    releaseRef(t);
}

void MenuManager::ReplaceViewController(const std::string& nibName, const std::string& className,
                                        const std::string& transition, const std::string& direction) {
    ui::ViewController* vc = CreateViewController(className, nibName);
    if (!vc) return;
    ui::Window& w = ui::Window::shared();
    w.userInteractionEnabled = false;
    if (!mCurrentViewController) w.insertSubviewAtIndex(vc->view(), 1);
    else w.insertSubviewBelow(vc->view(), mCurrentViewController->view());
    replaceSubview(mCurrentViewController ? mCurrentViewController->view() : nullptr, vc->view(), transition,
                   direction, transition == "fade" ? 0.5 : 0.25);
    releaseRef(mNewViewController);
    mNewViewController = vc;
}

void MenuManager::setupNewViewController(const std::string& nibName, const std::string& className) {
    ui::ViewController* vc = CreateViewController(className, nibName);
    if (!vc) return;
    ui::Window& w = ui::Window::shared();
    if (!mCurrentViewController) w.insertSubviewAtIndex(vc->view(), 1);
    else w.insertSubviewBelow(vc->view(), mCurrentViewController->view());
    killCurrentViewController();
    mCurrentViewController = vc;
}

void MenuManager::killCurrentViewController() {
    if (!mCurrentViewController) return;
    ui::View* v = mCurrentViewController->view();
    if (v->isDescendantOfView(&ui::Window::shared())) v->removeFromSuperview();
    releaseRef(mCurrentViewController);
    mCurrentViewController = nullptr;
}

void MenuManager::killViewController() {
    killCurrentViewController();
    currentMenu.clear();
}

}  // namespace as2
