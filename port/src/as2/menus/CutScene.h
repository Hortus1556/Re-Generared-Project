// CutScene: a mission's story before it (cutscene_start_N.json) or after it
// (cutscene_end_N.json): a background, four characters (the speaker lit,
// the others darkened), a line of text; NEXT or ten seconds go on, SKIP ends
// it. Each background has its ambient loop. The places the Flash AS2 had
// (its cut scenes' `bg` clip: bathroom, shack, bedroom, home, the office
// lit and dark, the graveyard) are its animated art; the others (the CFO's
// tent) the iPad's pictures. The first pack's characters are the Flash AS2's
// too (its holder's `char1`–`char4`, each on its character's label, what
// plays in it playing: Alan thinking, the aide panicking...), the ones not
// speaking tinted dark as Flash's (60% black); the CFO's, and a character
// Flash had no label for, the iPad's pictures, darkened by half.
#pragma once

#include "core/Json.h"
#include "engine/ui/UIKit.h"

namespace as2 {

// The Flash cut scenes' backgrounds (their holder's `bg` clip, a frame
// labelled by place), drawn scaled to cover the screen: the dialogues' and
// the operation's results'.
namespace cutscene {
// The frame of a place ("bathroom"), 0 if Flash had none.
int backgroundFrame(const std::string& place);
// Draws it, its inner animations `age` Flash frames on.
void drawBackground(int frame, long age);
// Has the places of a mission's story (its start or end) made ahead, kept
// 40 seconds: a place's art rasterised as it first showed stalled a frame.
void prefetchStory(int mission, bool end);
}  // namespace cutscene

class CutScene : public ui::ViewController {
public:
    explicit CutScene(const std::string& nib) : ViewController(nib) {}
    ~CutScene() override;
    void setupScene();

protected:
    void connectOutlet(const std::string& name, ui::View* v) override;
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;

private:
    void playAmbient(const char* key, int track);
    void showCharacter(int slot, const std::string& name, const std::vector<std::string>& lit, float baseX,
                       float baseY);
    void createAlertView1();
    void createAlertView2();
    void alertDismissed(int index);
    void scheduleNext();
    void drawBackground();
    // The Flash characters over the Flash place.
    void drawCharacters();

    ui::ImageView* mImgViewPos[4] = {};  // pos1..pos4
    Rect mSlot[4];                        // their frames in the nib
    ui::TextView* mTextView = nullptr;
    ui::TextView* mTextView2 = nullptr;
    json::Value mCutSceneDictionary;
    int dialogPosition = 0;
    ui::Timer* animationTimer = nullptr;  // retained
    int playingTrack = 0;
    bool isActive = false;
    int flashBg = 0;       // the Flash background's frame (0: the picture)
    long frames = 0;       // Flash frames since the scene opened
    long bgSince = 0;      // the frame the background's place came on
    double clock = -1;
    // Each slot's Flash character (char1..char4: pos1..pos4): its label
    // ("nothing": no one; empty: the iPad's picture shows instead), whether
    // it speaks, and the frame it came on.
    struct FlashCharacter {
        std::string label;
        bool lit = false;
        long since = 0;
    };
    FlashCharacter flashCharacters[4];
};

// StartScene: the mission's start screen after its story; START begins it.
class StartScene : public ui::ViewController {
public:
    explicit StartScene(const std::string& nib) : ViewController(nib) {}

protected:
    void viewDidLoad() override;
    void action(const std::string& selector, ui::View* sender) override;
};

}  // namespace as2
