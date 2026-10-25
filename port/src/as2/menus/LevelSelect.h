// FileSelect (the episode: the main game or the "CFO" pack) and
// LevelSelect (its missions, six or four a page, with grades and locks).
#pragma once

#include "engine/ui/UIKit.h"

namespace as2 {

// Upgrades bought so far: each tool's level above 1, summed.
int upgradesBought();

class FileSelect : public ui::ViewController {
public:
    explicit FileSelect(const std::string& nib) : ViewController(nib) {}

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::Button* mBtn1 = nullptr;
    ui::Button* mBtn2 = nullptr;
};

class LevelSelect : public ui::ViewController {
public:
    explicit LevelSelect(const std::string& nib) : ViewController(nib) {}
    void setupPage();

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    ui::Button* mBtnPtr[10] = {};
    ui::ImageView* mGradePtr[10] = {};
    ui::Button* mBtnPrevious = nullptr;
    ui::Button* mBtnNext = nullptr;
    int mPage = 0;
    bool mLevelSelected = false;
};

}  // namespace as2
