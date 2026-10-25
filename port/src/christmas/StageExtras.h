// What the port adds to the original Christmas game on the stage
// (StageScreen.h), asked for by its players: the level select's BACK tag
// (the original had no way back to the title from it; Escape too), and the
// van's engine staying where it landed in the workshop.
#pragma once

#include <string>

#include "surgeon/StageScreen.h"

class LevelsBack : public StageModule {
public:
    LevelsBack(StageScreen& stage, std::string path);

    bool replacesClip() const override { return false; }
    bool hits(float x, float y) override;
    void draw() override;
    void touchDown(float x, float y) override;
    void touchMove(float x, float y) override;
    void touchUp(float x, float y) override;
    bool takesEscape() const override { return true; }
    void escapeKey() override;
    // Stale once a card opens over it, with no later touchMove to clear
    // `over`: gated on cardOpen() too, not just the last hit test.
    int hoveredButton() const override { return over && !cardOpen() ? kBack : -1; }

private:
    static constexpr int kBack = 4251;  // the credits' BACK tag
    StageScreen& stage;
    std::string path;  // the level select's clip on the main timeline
    VectorTransform at;
    bool over = false, down = false, gone = false;
    bool cardOpen() const;
    void back();
};

// The story scenes' holder (5688): in the workshop scenes after the first,
// the van's engine stays in the window where it crashed. (In the Flash game
// those scenes have their own copy of the engine, never played: the engine
// was gone again.)
class WorkshopEngine : public StageModule {
public:
    WorkshopEngine(StageScreen& stage, std::string path);
    bool replacesClip() const override { return false; }
    void update() override;

private:
    StageScreen& stage;
    std::string bg;  // the scene's backdrop, on the main timeline
    int engineSymbol, firstWorkshop;
};
