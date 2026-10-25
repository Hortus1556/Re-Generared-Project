// A game as its Flash movie ran it: the main timeline (symbol 0) is the
// screen. Its frames, clips and buttons run their translated scripts from
// the package's index (Scene.h, Script.h) with Flash's classes
// (FlashClasses.h), its text fields show the scripts' variables, and the
// movie's own transitions, menus and banners play as authored.
//
// A game's native modules take over the clips whose scripts are real logic
// (an operation): a module is made when an instance of its sprite appears
// on the main timeline and dropped when it goes; it is drawn in that clip's
// place (between the pieces below and above it) and may take the pointer.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "surgeon/FlashClasses.h"
#include "surgeon/Scene.h"
#include "surgeon/Screen.h"

class ClipWatch;
class StageScreen;

class StageModule : public Object {
public:
    // Drawn instead of its clip (else over it: an addition to the clip).
    virtual bool replacesClip() const { return true; }
    // It takes the pointer at this point before any button of the timeline
    // (a button of its own).
    virtual bool hits(float /*x*/, float /*y*/) { return false; }
    virtual void update() {}
    virtual void draw() {}
    // It takes the pointer where no button of the timeline above it is.
    virtual bool takesInput() const { return false; }
    virtual void touchDown(float /*x*/, float /*y*/) {}
    virtual void touchMove(float /*x*/, float /*y*/) {}
    virtual void touchUp(float /*x*/, float /*y*/) {}
    virtual void numberKey(int /*n*/) {}
    virtual void escapeKey() {}
    virtual bool takesEscape() const { return false; }  // Escape goes to it first
    virtual int hoveredButton() const { return -1; }
    virtual bool drawsPointer() const { return false; }
    // What the scripts reach as this clip (null: the clip as the art has it).
    virtual script::ClipRef clip() { return nullptr; }
};

class StageScreen : public Screen {
public:
    explicit StageScreen(SurgeonGame& game);
    ~StageScreen() override;

    // Native code for the instances of `sprite` on the main timeline.
    using Factory = std::function<StageModule*(StageScreen& stage, const std::string& path)>;
    void addModule(int sprite, Factory make) { factories[sprite] = std::move(make); }

    void update() override;
    void draw() override;
    void touchDown(float x, float y) override;
    void touchMove(float x, float y) override;
    void touchUp(float x, float y) override;
    void numberKey(int n) override;
    void escapeKey() override;
    int hoveredButton() const override { return hoverModule ? hoverModule->hoveredButton() : hover.button; }
    bool drawsPointer() const override;

    // A _root variable naming the frame a transition is taking the main
    // timeline to (Christmas's ftb and bandage set `fTarg`): that frame's
    // art is made while the transition hides the screen.
    std::string transitionTarget;
    // The path of the clip the game keeps at the mouse as its pointer (AS1's
    // operation cursor, "game.cursor"): drawn as the pointer (ArtNode::pointer)
    // whenever it is there.
    std::string pointerClip;
    // The clip in it the tools test what they touch with (AS1's and
    // Christmas's `hit`, a 9.5-pixel square): on a touch device it is made
    // three times as wide and high, a fingertip, which hides what it
    // touches and lands less exactly than a mouse.
    std::string pointerHit;
    bool preparing() const override { return preparingFrame > 0 && preparedAges <= 25; }

    // The main timeline to a frame or label (debug: a mission).
    void gotoFrame(const script::Value& frame) { root->gotoFrame(frame, false); }

    // The game's own code run each pass after the stage's (debug probes).
    std::function<void()> afterUpdate;

    script::Runner runner;
    Scene scene;
    FlashClasses classes;
    ArtNode* stage;
    std::shared_ptr<SceneClip> root;  // _root

private:
    struct Running {
        std::string path;
        unsigned long long instance;
        int depth;
        StageModule* module;
    };
    std::vector<Running> running;  // by depth
    bool started = false;
    int tracedFrame = 0;
    long censusPasses = 0;
    ArtNode* pointerNode = nullptr;  // pointerClip's node this pass (not retained)
    unsigned long long pointerHitScaled = 0;  // (the pointer node's serial its hit was scaled for)
    void findPointer();
    std::unique_ptr<ClipWatch> watch;  // debug: the wounds' places (ClipWatch.h)
    int preparingFrame = 0;  // the frame a transition goes to, while it runs
    long preparedAges = 0;
    void prefetch();
    std::unordered_map<int, Factory> factories;
    void syncModules();
    StageModule* inputModule() const;
    bool escapeTaken();

    // Buttons, as Flash ran them: press, release, releaseOutside, rollOver,
    // rollOut, dragOver, dragOut, with `this` the clip the button is in.
    struct Hit {
        int button = -1;
        ArtNode* node = nullptr;  // the node the clip is in
        std::string clip;
        bool operator==(const Hit& o) const { return button == o.button && node == o.node && clip == o.clip; }
    };
    bool onStage(const ArtNode* n) const;
    Hit hitAt(float x, float y);
    void event(const Hit& h, const char* name);
    void showButtons();
    Hit hover, pressed;
    bool pressedOver = false;
    StageModule* pointerModule = nullptr;  // the module the pointer went down on
    StageModule* hitModule(float x, float y) const;  // the topmost whose own button is there
    StageModule* hoverModule = nullptr;

    bool fieldText(const std::string& clip, const std::string& variable, std::string& text);
};
