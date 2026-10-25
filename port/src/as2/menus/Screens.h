// The smaller menus: OperationDetails (a mission's patient form, best score
// and time), Credits (two pages) and HelpScreen (the tool guide, from the
// pause menu).
#pragma once

#include "engine/ui/UIKit.h"

namespace as2 {

class OperationDetails : public ui::ViewController {
public:
    explicit OperationDetails(const std::string& nib) : ViewController(nib) {}

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::TextView* bestScore = nullptr;
    ui::TextView* bestTime = nullptr;
};

class Credits : public ui::ViewController {
public:
    explicit Credits(const std::string& nib) : ViewController(nib) {}

protected:
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    int creditPage = 1;
};

class HelpScreen : public ui::ViewController {
public:
    explicit HelpScreen(const std::string& nib) : ViewController(nib) {}
    void setBackgroundScreen(int page);

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::Button* mBtn[9] = {};
    int helpPage = 1;
};

}  // namespace as2
