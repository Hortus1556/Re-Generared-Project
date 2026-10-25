// An Amateur Surgeon Flash game (AS1, Christmas): the original movie run by
// its own scripts on the generic stage (StageScreen.h; the package
// games/<game>), its operation included (OperationProbe: the debug trace and
// acts on it). A game gives what is its own (FlashSurgeonGame::Setup); the
// rest is the same for each: the screens through the movie's own
// transitions, the _root state the port shares with the scripts, the save,
// the sounds, the debug menu. docs/engine.md, docs/as1.md, docs/christmas.md.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "surgeon/FlashClasses.h"
#include "surgeon/SaveData.h"
#include "surgeon/SurgeonGame.h"

class StageScreen;
class OperationProbe;

class FlashSurgeonGame : public SurgeonGame {
public:
    struct Setup {
        int labelText = -1;            // the dialogue pop's text field (the debug menu's labels)
        std::vector<int> noHandCursor;  // the buttons the scripts set useHandCursor = false on
        // The Flash SharedObject the scripts keep progress in, and the file
        // it is kept as (FlashClasses::setSaveFormat).
        std::string sharedObject;
        FlashClasses::SaveFormat saveFormat;
        std::vector<int> hiddenCharacters;  // the online score submission's button and label
        int operationSprite = 0;            // the main timeline's clip "game"
        int patients = 0;                   // missionArray1..n (the debug menu's)
        // The pause screen's music and sound buttons, off and on (the port's
        // settings press them).
        int musicOff = 0, musicOn = 0, effectsOff = 0, effectsOn = 0;
        // _root flags a game's own scripts keep besides the shared ones
        // (Christmas's `pressed`).
        std::vector<std::string> flags;
        // The game's own additions to the stage (modules: Christmas's BACK
        // tag on the level select).
        std::function<void(StageScreen&)> extend;
    };

    explicit FlashSurgeonGame(Setup setup);
    ~FlashSurgeonGame() override;
    // Its package and script index, read in the background during the
    // engine's splash, which waits for them.
    static void preload();
    static bool preloaded();

    // Screen changes, through the game's own transitions (its ftb fade
    // and bandage wipe to a main timeline label).
    void startIntro();
    void startDialogue(int speech);  // a story scene from dialogue.xml
    void startLevels(bool bandage);  // the level select (bandage wipe or fade)
    void startOpReady();             // the patient before an operation
    void startOperation();
    void startTitleScreen();  // ftb("start"): the title, from a game
    void startComplete3();    // ftb("complete3"): the end, after the last patient
    void openHelp();          // _root.help.play(): the help panel over the operation

    std::vector<DebugMenu::Item> debugItems() override;

    SaveData save;
    int level = 1;         // the patient being operated on
    int curSpeech = 0;     // the dialogue scene to show next
    bool won = false;      // the operation just played was won
    bool story = false;    // the story goes on after it (set on a win)
    int scene = 0;         // story scenes shown since the operation

    // What an operation leaves on _root.
    std::string cause;             // how the last patient died ("time", "blood"...)
    double lscore = 0, ltime = 0;  // the last operation's score and time left
    bool organPlaced = false, dontAllowInjury = false, ktg = false;
    bool hitMenu = false;  // the pause screen was opened for the help

private:
    Setup setup;
    StageScreen* stage_ = nullptr;
    std::unique_ptr<OperationProbe> probe;
    std::vector<std::pair<std::string, bool>> flags;  // setup.flags' values
    void startStage();
    void seedStoredTime(int patient);  // debug: what the patient before leaves a continuing one
    void stageGoto(const char* label, bool bandage = false);
};
