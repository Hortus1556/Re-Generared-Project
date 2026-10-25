// Game: one mission being played: its stages (a stage ends when its
// injuries are all treated), the effects, the torch of the dark stages, won
// or failed; and the hit tests that find what the tool is on.
#pragma once

#include <string>
#include <vector>

#include "as2/Connector.h"
#include "core/Object.h"

namespace as2 {

class Sprite;
class ITool;
class InjuryGraphic;
class ToolEffect;
struct HitBox;

class Game : public Object {
public:
    // Sets the global game, makes the GameState.
    static Game* initWithMissionData(Mission* m);
    ~Game() override;

    void StartGame();
    // A mission frame (GameController updateScene).
    void checkRender();
    // The torch's light where the device's tilt points it.
    void drawTorchWithAccelerometer(const double* accel);

    void render(int frame);
    void updateInjuries();
    void missionQuit();
    // timeUp: the clock ran out (else the heart stopped).
    void missionFailed(bool timeUp);
    void missionCompleted();
    // The next stage is another part of the body: the old art fades out.
    bool needToFade() const;
    bool finishedMission() const;
    // A stage with no injuries left moves on.
    void checkInjuries(int reason);

    // The effects (ToolEffect classes by name); "flameSpray" makes nothing,
    // nor does a coloured "SmokeEffect".
    void addEffect(const std::string& name, int x, int y, float r, float g, float b);
    void addEffect(const std::string& name, int x, int y);
    // A piece of a tool's trail (ITool::trail): shown at `at` within this
    // pass, and gone sooner than the effect alone (ToolEffect::makeTrail).
    void addTrailEffect(const std::string& name, int x, int y, float at);
    void addTrailEffect(const std::string& name, int x, int y, float r, float g, float b, float at);
    void addSplatAtXY(float x, float y, float r, float g, float b);

    // The injuries under the finger the current tool treats (mFindCorrectInjury;
    // only the first with `single`).
    Array<InjuryGraphic>& findCorrectInjury(bool single);
    Array<InjuryGraphic>& findHiddenInjury(bool single);
    // Treats the injury's hitboxes under the tool (by its heal amount, or
    // at once): true when all are healed. With hitTestOnly nothing is
    // treated: true when one is under the tool.
    bool getAllHealedInjury(InjuryGraphic* injury, ITool* tool, bool instantHeal, bool updateToolEffect,
                            bool hitTestOnly);
    // Is the finger in one of the boxes (each turned with its guide sprite)?
    bool performHitTestOnHitBoxes(const HitBox* boxes, int count) const;

    Mission* mission_data = nullptr;       // retained
    MissionStage* mission_stage = nullptr;  // the first stage (its time)
    bool torch = false;
    Sprite* sprTorch[5] = {};  // the light, then four black edges
    bool mKillGame = false;
    bool mFailed = false;
    bool mWon = false;
    float torchXBound = 0, torchYBound = 0;
    Array<InjuryGraphic> mFindCorrectInjury;
    Array<InjuryGraphic> mFindHiddenInjury;
    Array<ToolEffect> effectsList;

private:
    Game() = default;
};

}  // namespace as2
