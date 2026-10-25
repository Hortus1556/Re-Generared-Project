// InjuryGraphic: something on the patient to treat, stage by stage, each
// stage asking for a tool (its stageArray, from the binary's static table:
// data/library.json "stages"); its sprites, its hitboxes (where the tool must
// go, each with a guide sprite and the amount of treatment left), the damage
// it does while untreated and its "special" (bleeding, a new cut).
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "as2/Connector.h"
#include "as2/SceneManager.h"
#include "core/Object.h"

namespace as2 {

class FlashCreature;

// stageArray_t: one stage.
struct StageInfo {
    std::string tool;        // the tool (or "a, b" tools) that treats it
    std::string wrongTool;   // "back", "nothing"...
    int i8 = 0;
    int damagePeriod = 0;    // frames between damage
    float damage = 0;        // health lost: twice this, each period
    int specialPeriod = 0;   // frames to the special
    std::string effect;      // the special: "blood", "smallCut", "nothing"
    std::string s1c;
    bool c20 = false;
    std::string lineColour;  // the guide: "invisible", "yellow", "black", "green"
    bool c28 = false;
    int i2c = 0;
};

// The stage table of an injury class (Library), empty when it has none.
const std::vector<StageInfo>& stageTable(const std::string& className);
// The Flash AS2's (its class's name there: "Barnicle" for Barnacle), for
// the monsters whose logic is the Flash AS2's; the iPad's if it has none.
const std::vector<StageInfo>& flashStageTable(const std::string& className);

// hitbox_t: where the tool must go.
struct HitBox {
    Rect rect;               // world points, unrotated (the sprite carries the turn)
    Sprite* sprite = nullptr;  // the guide (retained); its position and turn
                               // are the box's frame
    bool active = false;     // +0x14
    int colour = 0;          // +0x18: the guide line's (0x66ff33, as the
                             // original always ended up setting)
    float health = 0;        // +0x1c: treatment left; healed at 0
    int i20 = 0;
};

class InjuryGraphic;

// DraggableWrapper: a thing the tongs can hold (an extraction's object, a
// transplant's organ...): its sprite, the boxes the finger must be on,
// where it was.
class DraggableWrapper : public Object {
public:
    InjuryGraphic* injury = nullptr;  // not retained (its owner)
    Sprite* mSprite = nullptr;        // not retained (the injury's)
    Sprite* healthyReplacementSprite = nullptr;
    HitBox* hitboxes = nullptr;       // boxes the owner keeps
    int hitBoxCount = 0;
    float xOrigin = 0, yOrigin = 0;
};

class InjuryGraphic : public Object {
public:
    // initWithInjuryData:Instance:PlayerCreated:Force: (the class's own
    // set-up runs in its constructor + init()).
    InjuryGraphic(InjuryData* data, Injury* instance, bool playerCreated, bool force);
    ~InjuryGraphic() override;

    // Made by class name (NSClassFromString), as the game did; velocity -1
    // for the four-argument initialiser.
    static InjuryGraphic* create(InjuryData* data, Injury* instance, bool playerCreated, bool force,
                                 int velocity = -1);

    virtual void render();
    virtual void setup();
    virtual void updateInjury() {}
    virtual void interact() {}
    virtual void special();
    virtual void extendHitBox() {}
    virtual void texturesUsed(void*) {}
    // The big centipede's state "attack" (no tool can be used then).
    virtual bool isAttacking() const { return false; }
    // Bleed (mission 20): a new touch on its thumb hold.
    virtual void checkThumbClick() {}
    virtual void createEffect(int count, int velocity);
    void initStageArray(const std::vector<StageInfo>& stages);
    // The stages of the injury's own class when it has a table of its own
    // (a kind that only differs by its stages: the vampire's BloodBurn, the
    // stake), else `fallback`'s; maxStages set to their number.
    void initOwnStages(const std::string& fallback);

    // Debug trace (-d): "injury <event> <class> id <id> stage <s>/<max>
    // tool <tool> alive <0|1> at <x>,<y> rot <degrees> hidden <0|1> #<serial>", then "  box <x>,<y> <health>"
    // for each box still to treat, in game-screen coordinates (where to
    // touch), for the play-test driver.
    void trace(const char* event) const;
    // The tool it takes now, for the trace: its stage's (a bat names two and
    // takes one at a time).
    virtual std::string toolNow() const { return current().tool; }
    // The trace's boxes (its hitboxes; a transplant's every part).
    virtual void traceBoxes() const;
    // "  box <x>,<y> <health><note>" for each of `count` boxes.
    static void traceBoxes(const HitBox* boxes, int count, const char* note);
    // Set when a tool wears one of its boxes down; on letting go of the
    // tool each such injury is traced again ("injury worked ...", its boxes'
    // health now: traceWorked), so the driver sees what is left to treat.
    bool worked = false;
    static void traceWorked();

    // isKindOfClass: of the game's class (or one of its subclasses), by
    // the class tree in data/library.json.
    bool isKindOf(const std::string& className) const;

    // For an injury that moves by itself (the crab, the dove): "injury moved
    // #<serial> at <x>,<y> open <0|1>", its first box's middle on the screen
    // and whether it can be treated now (not invincible), each time it has
    // gone 12 points or that changed (the play-tests follow it).
    void traceMoved();
    const StageInfo& stage(int n) const;  // 1-based, as currentStage
    const StageInfo& current() const { return stage(currentStage); }

    Array<Sprite> mSpriteArray;
    InjuryData* mInjuryData = nullptr;  // the library's
    Injury* mInjury = nullptr;          // retained
    bool mPlayerCreated = false;
    bool mForce = false;
    std::string make;
    int maxStages = 0;
    std::vector<StageInfo> stageArray;
    bool alive = false;
    bool hidden = false;
    int damageTimer = 0;
    int specialTimer = 0;
    int currentStage = 1;
    bool invincible = false;
    int invincibleTimer = 0;
    int hitBoxCount = 0;
    HitBox* hitboxes = nullptr;  // hitBoxCount of them
    int box_width = 0, box_height = 0;
    bool slippery = false;
    bool isExtraction = false;
    bool isTransplant = false;
    bool beingHeld = false;
    float extractionOriginX = 0, extractionOriginY = 0;
    float distance = 0;
    int health = 0;
    bool forcePosition = false;
    bool anything = false;
    int removeCounter = 0;
    Point tracedAt{-1000, -1000};  // traceMoved's last place
    bool tracedOpen = true;
    // What the tongs take out of it (removable: one, or a transplant's
    // several).
    Array<DraggableWrapper> removable;

protected:
    // Frees the hitboxes and their guides (each class's setup remakes them).
    void freeHitBoxes();
    // The cut-like injuries' hitboxes: `count` boxes of box_width x
    // box_height in a column down from the injury's position, turned with
    // it, each 2 to heal, with a guide sprite of `guide` (layer 44) that is
    // drawn only when `shown` (in `layer`); the column's top `from` the
    // injury (unturned).
    void columnHitBoxes(int count, const std::string& guide, bool shown, int layer = 44, Point from = {0, 0});
    // A line of boxes from a table of (x, y, turn) about (x, y): each
    // box_width x box_height from its point down, with a 20x16 guide
    // (layer 33, shown) turned so and coloured by the stage's line.
    void lineHitBoxes(HitBox* out, const float (*points)[3], int count, float x, float y);
    // An insertion's piece (a pin, a plug, a pill...) put on the tongs'
    // tray (its removable, with these boxes), to be taken from there and
    // let go where it goes (Tongs::trayTarget).
    void waitOnTray(Sprite* piece, HitBox* boxes = nullptr, int count = 0);
    // A box's guide coloured by the stage's line colour (invisible ones
    // active and plain; yellow, black, green drawn so; others not active),
    // 2 to heal.
    void colourBox(HitBox& b);
    // The one box of the small injuries (a fire, a blob, a barnacle...):
    // box_width x box_height about (x, y), turned with the injury, `health`
    // to heal, with a guide of `guide` (layer 44, not drawn); made once,
    // placed again by each setup.
    void oneHitBox(float x, float y, const std::string& guide, float health = 2);
    // A monster whose logic is the Flash AS2's: its boxes are Flash's hit
    // areas (each movie clip of its `hit`) on a label's frame, about `at`
    // (the world), each `health` to work (Flash's own: the port's tools wear
    // boxes as Flash's did), not drawn ("injury boxed" in the trace); and
    // moved along with it.
    void flashHitBoxes(FlashCreature& flash, const std::string& label, Point at, float health);
    void moveFlashHitBoxes(Point at);
    // Where a monster whose art is the Flash AS2's stands (the world): the
    // stage point the Flash AS2's own levels placed it at (the iPad moved
    // many, and took x 0.8, y - 50 of them, to suit its smaller pictures);
    // else where it was made.
    Point flashPlace() const;
    // Where its one picture stands (setup puts it there): where it was made;
    // a monster that stands at its flashPlace says so (its boxes are there).
    virtual Point picturePlace() const { return {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)}; }
    std::vector<Point> flashBoxOffsets;  // each box's middle from its place
    // The guides out of the scene (before a setup remakes them).
    void hideGuides();
    // Healed: fading out over removeCounter frames, then removed (render).
    bool fadeOut(Sprite* s, bool fadeIn = false);
    // The tutorial pupil that follows the finger (Incision, LargeCut): from
    // (x, y), `scale` of the distance, at most `max`, 1.2 times sideways.
    void followFinger(Sprite* pupil, float x, float y, float scale, float max);
};

// A class of injury made by name.
void registerInjuryClass(const std::string& name,
                         std::function<InjuryGraphic*(InjuryData*, Injury*, bool, bool, int)> make);

}  // namespace as2
