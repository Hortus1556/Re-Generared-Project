// A Flash game's operation (the main timeline's clip "game": AS1's sprite
// 4950, Christmas's 6233) runs by its own scripts. This reads and nudges their state for debugging:
//
// - the trace (AS_TRACE) the automated play-tests aim by, once per pass:
//   the injuries as they come, move on and go (with their hit pieces, the
//   glass to pull, a puzzle's pieces and slots), the mission's stage and
//   body level, a tool pressed, the corkscrew's and vacuum's moving hit, an
//   organ put on the tray, the heart stopping, the patient won or lost;
// - the debug menu's acts on the operation: a stage skipped, the patient
//   won, healed or killed, more time, god mode.
//
// It reads the scripts' variables and calls their own functions; it changes
// one rule of the original's: a death is final (the operation's `health`
// is bound here, so nothing raises it once the heart monitor flatlines).
#pragma once

#include <map>
#include <memory>
#include <string>

#include "flash/Swf.h"
#include "surgeon/Script.h"

class StageScreen;
class SceneClip;

class OperationProbe {
public:
    OperationProbe(StageScreen& stage, int operationSprite) : stage(stage), operationSprite(operationSprite) {}

    void update();   // once per pass, after the stage's
    bool running();  // an operation is being played (its game frame)

    void skipStage();
    void win();
    void heal();
    void kill();
    void addTime(int seconds);
    bool godMode = false;  // the patient's health never drops below 50

private:
    StageScreen& stage;
    int operationSprite;
    std::shared_ptr<SceneClip> game();  // the operation's clip, while on its game frame

    struct Known {
        std::weak_ptr<script::Clip> clip;
        int id = 0, stage = 0, level = 0, holderFrame = 0;
        float x = 0, y = 0;
        std::string make, frame;
        double health = 0;
        bool seen = false;
    };
    std::map<const script::Clip*, Known> known;
    int nextId = 1;
    int missionStage = -1, bodyLevel = -1;
    bool paralyzed = false, pressed = false;
    bool playing = false;  // on its game frame last pass
    unsigned long long boundNode = 0;  // the operation clip whose health is bound
    double health = 0;                  // its health (the scripts' variable)
    std::weak_ptr<script::Clip> organ;  // the organ on the tray last told of
    std::string organMake;              // the injury it goes in, while carried
    flash::Rect organHit;               // that injury's hit area (stage)
    void traceInjury(SceneClip& c, Known& k, const char* what);
    void trace(SceneClip& g);
    void traceEnd(SceneClip* g);
};

