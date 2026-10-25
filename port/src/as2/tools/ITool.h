// ITool: a surgical tool (the ten of the HUD), turned on while the finger
// is down, with its upgrade level's strength. Each mission frame the tool
// that is on looks for the injuries it treats under the finger
// (Game::findCorrectInjury) and, when there are none for long enough, hurts
// the patient (wrongUsage).
#pragma once

#include <functional>
#include <string>

#include "as2/Connector.h"
#include "core/Object.h"

namespace as2 {

class InjuryGraphic;

class ITool : public Object {
public:
    // initWithToolData: of the class the tool data names (NSClassFromString);
    // nullptr for a class not ported.
    static ITool* create(ToolData* data);

    ToolData* toolData = nullptr;  // not retained (the library keeps them)
    int lowEffectTimer = 0, maxLowEffectTimer = 3, maxEffectTimer = 3;
    int bloodTimer = 0, maxBloodTimer = 3;
    float damage = 0;
    int level = 0;
    bool getSingleInjury = false;
    int hitBuffer = 0;
    int healAmount = 0;
    int effectTimer = 0;
    int wrongUsageTimer = 0, maxWrongUsageTimer = 45;
    int hitDist = 40;
    int bloodVel = 5;
    bool turnedOn = false;
    bool mDoTurnOn = false, mDoTurnOff = false, mRelease = false;
    int cursorHalfSize = 1;
    // The stroke being trailed: where it reached, how far since its last
    // piece, the pass it was last called on.
    float trailX = 0, trailY = 0, trailCarry = 0;
    double trailPass = -1;
    int trailSprays = 0;  // the trail's pieces so far (every third a spray)

    bool isTurnedOn() const { return turnedOn; }
    // The finger's stroke since the last pass, filled: emit(x, y, at) every
    // `spacing` points along it, `at` (0..1) when within the pass the finger
    // was there, so a fast stroke leaves a continuous trail as at the
    // display's rate rather than a puff a pass. (x, y): the finger now, in
    // the effects' coordinates. A pass without a call ends the stroke.
    // Returns how many were made.
    int trail(float x, float y, float spacing, const std::function<void(float, float, float)>& emit);
    virtual void turnOn() { turnedOn = true; }
    virtual void turnOff(bool /*release*/) { turnedOn = false; }
    // A mission frame while on: the injuries under the finger become the
    // current ones; none for maxWrongUsageTimer frames is a wrong use.
    virtual void render(int frame);
    virtual void setToolLevel() {}
    virtual void wrongUsage();
    virtual void onMouseDown() {}
    virtual void onMouseUp() {}
    // Whether the HUD's cursor is hidden (the system's pointer shown).
    virtual bool hidesPointer() const { return false; }

    // +[ITool wrongUsage]: the patient's scream (none in the graveyard or in
    // shock).
    static void scream();

protected:
    explicit ITool(ToolData* data) : toolData(data) {}
    // The tools' common turnOn: not while the big centipede attacks
    // (mission 19); on; an injury under the finger that takes anything
    // reacts instead (false then); the cursor's size by level.
    bool beginTurnOn();
    // The common treatment: each current injury's hitboxes under the
    // finger are treated (stopping at an invincible one); all healed moves
    // the injury on (updateInjury).
    void treatCurrentInjuries(bool updateToolEffect, bool (*instant)(InjuryGraphic*) = nullptr);
    // A player-made injury (a burn, a cut...) where the finger is.
    void addInjuryAtFinger(int id, int rotation);
    // Blood where the finger is.
    void addBloodAtFinger(int velocity);
    // Upgrades: the level saved for the tool, its hit buffer (20/50/80).
    void readLevel(const char* saveName);
};

// A tool class made by name.
void registerToolClass(const std::string& name, std::function<ITool*(ToolData*)> make);

}  // namespace as2
