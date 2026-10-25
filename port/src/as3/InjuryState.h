// mt::InjuryState (re/as3/notes/surgery.md): one injury of a surgery, run by
// its class's definition (SurgeryData's InjuryDef). Its stages each take
// some tools; a tool used along the stage's hit areas wears their points
// down, and when every area is done the injury goes to its next stage, or is
// finished after the last. What it does to the screen (a new injury, a heal,
// an effect, time) it asks for by posting events (InjuryEvent), which the
// screen runs after its update.
//
// This is the base class (0x81b84): the C++ subclasses of the creatures and
// special injuries (Bat, Flea, Transplant...) override its virtuals; the
// factory (0x908c4) makes the right one for a base class.
#pragma once

#include <string>
#include <vector>

#include "as3/SurgeryData.h"
#include "engine/cocos/CCNode.h"
#include "engine/cocos/CCSprite.h"

namespace mt {

class ToolState;
class InjuryState;
struct Clamp;

// What an injury or a tool asks the screen to do (0x78 bytes; the queue of
// 15 at +0x5efc, run by 0x4c0c8).
struct InjuryEvent {
    enum Type {
        kAddTime = 0, kHeal = 1, kCreateInjury = 2, kEffect = 3, kSetFrame = 4, kHealTimes25 = 5, kHealOne = 6,
        kNone7 = 7, kEvent8 = 8, kDamage = 9, kCombo = 10, kTagPopup = 11, kGraphics = 12, kEvent13 = 13,
        kClamps = 14, kLiquid = 15, kEvent17 = 17, kEvent18 = 18, kEvent19 = 19
    };
    int type = 0;
    std::string name;                  // +4: the injury class to create
    bool flag8 = false, flag9 = false; // +8, +9
    int i0c = 0;                       // +0xc
    bool allowedOverLock = false;      // +0x10
    int effect = 0;                    // +0x14 (kEffect: which)
    std::string frame;                 // +0x18 (kSetFrame: its name)
    cocos2d::CCSprite* sprite = nullptr;  // +0x24 (kSetFrame)
    float value = 0;                   // +0x28: time, health
    float x = 0, y = 0;                // +0x2c, +0x30 (ints in the original)
    bool useTouch = false;             // +0x34
    float rotation = 0;                // +0x38 (an int)
    int damage = 0;                    // +0x40
    int kind = 0;                      // +0x44 (kTagPopup)
    cocos2d::CCRect rect;              // +0x48 (kGraphics)
    float f58 = 0;                     // +0x58
    float delay = 0;                   // +0x3c (kCreateInjury: a SoakableParticle's soak time, 0x19f8bc)
    std::vector<std::string> names;    // +0x60 (kCreateInjury: what a SoakableParticle makes when it soaks)
    bool countsTowardCap = true;       // +0x6c
    // (kCreateInjury) the new injury waits for this one to be finished
    // before any tool works on it (+0x70: an embedded object's wound and an
    // incision's cut wait for them), or is hidden until then (+0x74; no
    // class sets it) (0x4c0c8 case 2)
    InjuryState* interactionDependency = nullptr;  // +0x70
    InjuryState* visualDependency = nullptr;       // +0x74
};

// A hit area of the current state, with its points' wear (0x96c bytes).
struct HitPointState {
    const HitPoint* def = nullptr;
    cocos2d::CCPoint position;  // on the screen (shorts in the original)
    bool done = false;          // +0xc
    float health = 1;           // +0x14..
    cocos2d::CCRect rect;       // where a touch wears it
    cocos2d::CCSprite* marker = nullptr;  // its hit area marker (shown with show_hit_areas)
    struct Clamp* clamp = nullptr;        // +0x10: the clamp on it (ClampsController.h)
    bool clampTagged = false;             // +0x2c: a clamp was put on it once (its popup)
};
struct HitAreaState {
    const HitArea* def = nullptr;
    std::vector<HitPointState> points;
    bool done = false;  // +0x968
};

// A piece of an injury as it is on the screen (0xa08 bytes at +0x6704): its
// sprite, and whether a tool holds it.
struct PieceState {
    const InjuryPieceDef* def = nullptr;
    cocos2d::CCSprite* sprite = nullptr;  // +8
    bool held = false;                    // +0x9f8
    bool onTray = false;                  // +0x9fb
    cocos2d::CCPoint grabOffset;          // +0xa00
    // where a touch takes it, on the screen as the injury would be unturned
    cocos2d::CCRect rect;
};

// What a tool's touch did to an injury (0x20 bytes: InjuryState::hitTest).
struct HitResult {
    class InjuryState* injury = nullptr;  // +0
    PieceState* piece = nullptr;            // +4
    bool hit = false;           // +8: worked on it
    bool touched = false;       // +9
    cocos2d::CCPoint point;     // +0xc
    bool stageDone = false;     // +0x14
    bool comboAction = false;   // +0x15: a completed action for the combo
    bool comboBonus = false;    // +0x16
    HitPointState* hitPoint = nullptr;  // +0x18
    bool useful = false;        // +0x1c: the tool did not hurt the patient
    bool b1d = false;
    bool deselectTool = false;  // +0x1e
};

class InjuryState {
public:
    InjuryState();  // 0x81b84
    virtual ~InjuryState();

    // the factory (0x908c4): the class for a definition's base
    static InjuryState* create(const InjuryDef* def);
    // made for a surgery (0x75ee0): placed, its first stage, started
    // (`graphicsEvent`: 0x75ee0's last argument, an event's +8: it sends its
    // graphics event 12 on start, +0xfd97)
    void setup(const cocos2d::CCPoint& position, float rotation, bool graphicsEvent);

    virtual void start();                    // vf2
    // vf3: its pictures are built (the end of 0x77c4c; the creatures start
    // their animations here)
    virtual void vf3() { m_flag58 = true; }
    virtual void vf4(float) {}               // vf4: each frame, after update (0x32e38)
    // (the port's) shown from the stage's start although its visual
    // dependencies are not finished (the screen hides the rest until they
    // are: 0x77c4c, 0x33244)
    virtual bool shownWhileWaiting() const { return false; }
    virtual void update(float dt);           // vf5: the stage's timers
    virtual void finishStage();              // vf6
    virtual void backToPreviousStage();      // vf7
    virtual HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool);  // vf8
    // vf9: the injection's hit test (0x398e4 calls it for tool 7 with the
    // injection's mode, +0x520, and fill, +0x528); the base touches nothing (0x84c48)
    virtual HitResult injectionHitTest(const cocos2d::CCPoint&, int /*mode*/, float /*fill*/) { return HitResult(); }
    // vf10: the clamps' hit test (0x84c6c, tool 9): a clamp point of the
    // current state under the touch (ClampsController.cpp)
    virtual HitResult clampHitTest(const cocos2d::CCPoint& touch, ToolState* tool);
    // vf24: a clamp taken off it (ClampsController.cpp)
    virtual void removeClamp(struct Clamp* clamp);
    // its clamps forgotten and every clamp asked off (event 14) (0x856bc;
    // the clamped classes call it when done with them)
    void releaseClamps();
    virtual bool takesTool(int tool) const;  // vf12
    virtual bool pieceContains(const PieceState& piece, const cocos2d::CCPoint& touch) const;  // vf13
    // wears a hit area's points under the touch (vf14): the point worn
    virtual HitPointState* wear(HitAreaState& area, const cocos2d::CCPoint& touch, float amount, bool evenIfDone,
                                bool allPoints);
    virtual bool areaDone(const HitAreaState& area) const;  // vf15
    virtual void resetArea(HitAreaState& area);             // vf16
    virtual void onTouch(const cocos2d::CCPoint&) {}         // vf17
    virtual void completeStage();                            // vf18
    virtual void onFinished() {}                             // vf20
    virtual void dragged(int /*tool*/, const cocos2d::CCPoint&) {}  // vf22: held by a tool
    // vf23: the tool `tool` was put down (let go of: 0x34cbc, for every
    // injury not finished nor hidden)
    virtual void toolStopped(int /*tool*/) {}
    virtual void setRotation(float degrees);                 // vf21
    virtual void onStaple(const cocos2d::CCPoint&, ToolState*) {}  // vf31
    virtual int toolFor(const cocos2d::CCPoint& touch);      // vf27
    // vf28/vf29: its particle systems' updates stopped and started again
    // (unscheduleUpdate/scheduleUpdate; the injuries with systems)
    virtual void pause() {}
    virtual void resume() {}
    virtual void placeNodes();                               // vf30

    // its nodes (0x77c4c): a root with three layers (its pictures, its hit
    // areas' markers, a top one), its pieces, then its pictures
    void buildGraphics();
    // the pictures (0x77c4c by base): the usual case is the state's picture
    // (`injury-<class>-<state>.png`) and the stage's animations
    // (`common/<name>.json`) at the injury's place and turn; the classes
    // drawn otherwise override it
    virtual void buildPictures();
    // the hit areas' markers, when the stage shows them (0x796dc)
    // (always: whatever the stage says, the tutorial's ShowInjuryHitAreas)
    void buildMarkers(bool always = false);
    // the debug trace of its stage for test scripts: its state, the tools
    // it takes, its hit points still to treat (cocos points)
    void traceStage() const;
    // traceStage again if hit points were treated since it was traced
    void traceIfWorn() const;

    // goes to the stage of a state (0x8364c): the end of the current one's
    // events first, the new one's start events after, if asked
    void goToState(int state, bool startEvents, bool endEvents);
    void rebuildHitAreas();  // 0x852e4
    void placeHitPoints();   // 0x84ec0
    void post(const InjuryEvent& e);  // 0x97bc0
    // the hit areas of a state's points (0x82258); clamp ones only if asked
    std::vector<HitAreaState*> areasOf(int state, bool withClamp);
    const InjuryStageDef& stage() const { return def->stages[m_stage - 1]; }
    bool hasStage() const { return m_stage >= 1 && m_stage <= static_cast<int>(def->stages.size()); }
    // the frame name of the current state's picture (0x8153c)
    std::string frameName() const;

    const InjuryDef* def = nullptr;  // +0x14
    std::string name;                // +8
    int toSurgeryStage = 0;          // +0xc
    int stageEndToSwap = 0;          // +0x10
    cocos2d::CCPoint position;       // +0x18
    cocos2d::CCRect rect;            // +0x20
    float rotation = 0;              // +0x30
    int m_stage = 1;                 // +0x34 (1-based)
    bool finished = false;           // +0x38
    bool stageFound = false;         // +0x39
    int state = 0;                   // +0x3c
    float damageNow = 0;             // +0x40: the patient's damage this frame
    float damageTimer = 0;           // +0x44
    bool m_flag48 = false;           // +0x48
    float particleTimer = 0;         // +0x4c
    float eventTimer = 0;            // +0x54
    bool m_flag58 = false;           // +0x58
    cocos2d::CCNode* root = nullptr;     // +0x5c
    cocos2d::CCNode* spritesNode = nullptr;  // +0x60
    std::vector<cocos2d::CCSprite*> sprites;  // +0x64
    cocos2d::CCNode* markersNode = nullptr;   // +0xa4
    cocos2d::CCNode* topNode = nullptr;       // +0xa8
    std::vector<cocos2d::CCNode*> animations; // +0xac
    std::vector<HitAreaState> areas;          // +0xc0
    std::vector<PieceState> pieces;           // +0x6704: its pieces on the screen
    std::vector<InjuryEvent> events;          // +0x5efc
    std::vector<InjuryState*> visualDependencies, interactionDependencies;  // +0x6608, +0x6684
    // +0x6700: it waits for its interaction dependencies to be finished: not
    // hit-tested meanwhile (cleared by the screen, 0x33244)
    bool hasDependencies = false;
    int zOrder = 25;                          // +0xfd80
    std::vector<struct Clamp*> clamps;        // +0xfd84: the clamps on it (the clamps tool's)
    int priority = 1;                         // +0xfda0: the factory's per class (the tutorial's and partners' pick, 0x3c1xx)
    float pointHealth = 1;                    // +0xfd90
    bool m_fd94 = false, m_fd95 = false, m_fd96 = false;
    bool sendsGraphicsEvent = false;          // +0xfd97
    bool grabbed = false, m_fd99 = false;  // +0xfd98..
    // +0xfd9a: an extraction still perfect (never slipped) with the heart
    // rate upgrade (AngledExtraction vf2 0x8790c, SurfaceExtraction vf2
    // 0x883cc); finishStage's +2 s and +2 health. (Fixed: the port's
    // extractions kept their own copy, so the upgrade never paid out.)
    bool perfect = false;
    bool allowNonEssentialCreation = true;    // +0xfd9b
    bool createdLater = false;                // +0xfda4
    // 0x77c4c's third argument: the picture the event that made it asked for
    // (its +0x18), shown instead of the state's (the particles' landings)
    std::string graphicFrame;
    float upgradeHealBoost = 1;               // the heal boost's health (1)
    int soundChannel;                         // its sounds' channel (the original keyed one by its address)

protected:
    void postCreateAtStart();  // 0x83ac0
    void postCreateAtEnd();    // 0x836f0
    void postFrameChange();    // 0x82540
};

}  // namespace mt
