// The surgery editor's levels (docs/as3.md, "Surgery editor"): a surgery
// made in the game's editor, in the format of Data/Surgeries/<id>.json so
// the game's own surgery code plays it (SurgeryScreen, loadSurgeryDef).
//
// The original's editor (Android 1.43 only, never reachable: EditorScreen
// 0x446874.., docs/as3-unused.md) kept the level as the game's own records
// (SurgeryDef +0x634, SurgeryState +0x958) and wrote them back to JSON with
// SurgeryUtils::createSurgeryJson (0x759858) into Documents/editedLevels/
// as <surgery>_v<version>_<Y_M_D_h_m_s>.json (saveSurgeryNew 0x44f958).
// The port keeps a plain model of the file (Level) instead, with:
//  - the catalog of what a level may use (Catalog): the injury classes,
//    backgrounds and music of the shipped surgeries, and the sprite sheets
//    each needs (a class's or a background's "home" surgery's sheets);
//  - strict reading of a level (fromJson): it is untrusted input (a shared
//    code, a file dropped in the save folder), so every key, name and
//    number is checked and anything else refused with a message;
//  - the share code: "AS3L1:" + base64url(zlib(the level's compact JSON));
//  - the files in <save folder>/editedLevels/ (never state.json).
#pragma once

#include <string>
#include <vector>

#include "core/Json.h"

namespace mt {

struct AreaDef;

namespace editor {

// The limits of a level (the original's records: StaticVector<SurgeryStageDef, 10>;
// the shipped surgeries have at most 45 injuries a stage, 6 extra
// backgrounds, 101 injuries in all).
constexpr int kMaxStages = 10;
constexpr int kMaxInjuriesPerStage = 50;
constexpr int kMaxInjuries = 200;
constexpr int kMaxExtraBgs = 8;
constexpr int kMaxTitle = 32;
constexpr int kMaxName = 32;
constexpr int kCanvasW = 640, kCanvasH = 384;  // the design canvas, the files' coordinates
constexpr const char* kCodePrefix = "AS3L1:";  // the share code's format and version
constexpr size_t kMaxCodeChars = 32768;
constexpr size_t kMaxJsonBytes = 131072;       // a level's JSON (a code inflated, a file)

struct LevelInjury {
    std::string className, name;
    int x = 320, y = 192;  // canvas points (y up)
    int rotation = 0;      // degrees
    int toSurgeryStage = 0;   // the stage it opens (an incision): 0 none
    int stageEndToSwap = 0;   // its stage whose end jumps there: 0 its last
    bool allowNonEssential = true;
    std::vector<std::string> visualDeps, interactionDeps;  // names in the same stage
};

struct LevelBg {  // "additional_bgs"
    std::string filename;
    int x = 320, y = 192, rotation = 0;
    bool flipX = false, flipY = false;
};

struct LevelStage {
    std::string bg;
    bool flipX = false, flipY = false;
    std::string bgm;        // "" keeps the music on
    int bodyLevel = 1;
    int health = -1;        // -1 keeps the heart as it is
    int time = -1;          // -1 keeps the clock
    int toSurgeryStage = 0; // the stage after this one (0: the first left to do)
    std::vector<LevelBg> bgs;
    std::vector<LevelInjury> injuries;
};

struct Level {
    std::string title = "My Surgery";
    bool imported = false;  // came from a code or a file (shown as shared)
    bool isChallenge = false, isSuddenDeath = false, hideExtraction = false;
    int minScore[3] = {0, 40, 400}, minTime[3] = {0, 240, 180};
    float damageMultiplier = -1;  // "injury_damage_multiplier": -1 the area's
    std::vector<std::string> tools;  // their names (kToolNames)
    std::string scream = "male";
    std::vector<LevelStage> stages;
};

// A new level: one stage on a patient's chest, every tool.
Level newLevel();

// The level as the game's surgery JSON (createSurgeryJson 0x759858's keys,
// plus "editor": its title; "sprite_sheets" and "sounds" made from the
// catalog). `compact`: the share code's form, without the derived keys.
json::Value toJson(const Level& level, bool compact);
std::string toText(const Level& level, bool pretty, bool compact);
// Reads a level, checking everything (see above). `fromShipped`: one of the
// game's own surgeries as a template (its tutorial keys are dropped, not
// refused). False with a message the player can read.
bool fromJson(const json::Value& root, Level& out, std::string& error, bool fromShipped = false);
bool fromText(const std::string& text, Level& out, std::string& error);
// The level can be played: a heart and a clock on its first stage, an
// injury, a tool. (A level being made may lack them; a code or an imported
// file may not.)
bool playable(const Level& level, std::string& why);

// The share code of a level, and a level from one (spaces and line breaks
// in a pasted code are ignored).
std::string encodeCode(const Level& level);
bool decodeCode(const std::string& code, Level& out, std::string& error);

// What a level may use: read once from the shipped surgeries.
struct Catalog {
    struct Class {
        std::string name;   // the injury class (Data/Injuries/<name>.json)
        std::string home;   // the first surgery that has it: its sheets draw it
        int stages = 1;     // its definition's stages (stage_end_to_swap's range)
        int base = 0;       // its base class (InjuryBase): the palette's groups
    };
    struct Bg {
        std::string frame;  // a sheet's frame, or "(r,g,b)" a colour
        std::string home;
    };
    struct Patient {        // a shipped surgery, to start a level from
        std::string id, title;
    };
    std::vector<Class> classes;         // in the game's order of first use
    std::vector<Bg> bgs;                // a stage's backgrounds
    std::vector<Bg> extraBgs;           // the extra pictures over one
    std::vector<std::string> music;     // "" and the stages' music files
    std::vector<std::string> screams;   // the patients' screams
    std::vector<Patient> patients;      // every main surgery

    static const Catalog& shared();
    const Class* findClass(const std::string& name) const;
    const Bg* findBg(const std::string& frame) const;
    const Bg* findExtraBg(const std::string& frame) const;
    std::vector<std::string> sheetsOf(const std::string& surgery) const;
    // the sheets a level needs: its classes', its backgrounds'
    std::vector<std::string> sheetsFor(const Level& level) const;
    std::vector<std::string> sheetsForClass(const std::string& name) const;
    std::vector<std::string> sheetsForBg(const std::string& frame) const;
    // the area a level plays in (its first background's): its difficulty
    const AreaDef* areaFor(const Level& level) const;
    // a shipped surgery's main file as a level to edit
    bool levelFromSurgery(const std::string& id, Level& out, std::string& error) const;

private:
    std::vector<std::pair<std::string, std::vector<std::string>>> m_sheets;  // surgery -> its sheets
    std::vector<std::pair<std::string, std::string>> m_areaOf;               // surgery -> its area
};

// The files: <save folder>/editedLevels/<id>_v<version>_<Y_M_D_h_m_s>.json
// (the original's naming, saveSurgeryNew 0x44f958).
struct LevelFile {
    std::string file;   // its name in editedLevels/
    std::string id;     // the level's (the file's name before "_v")
    int version = 1;
    std::string title;  // "" when it can't be read
    bool imported = false;
    std::string error;  // why it can't be played ("" when fine)
};
std::string levelsDir();  // with a trailing separator; made if missing
std::vector<LevelFile> listLevels();  // newest first
bool loadLevel(const std::string& file, Level& out, std::string& error);
// Writes the level: over `file` when given (Save), else as a new version of
// `id` (Save new; a new id from the title when `id` is empty). `file` and
// `id` are set to what was written.
bool saveLevel(const Level& level, std::string& id, std::string& file, bool newVersion, std::string& error);
bool deleteLevel(const std::string& file);
// The levels dropped in editedLevels/import/ (*.json, or *.txt holding a
// code) become levels; the files read are taken out, the bad ones stay.
// The message says what happened.
int importFolder(std::string& message);
// The level as editedLevels/export/<id>.json; the path written.
bool exportLevel(const Level& level, const std::string& id, std::string& path, std::string& error);

}  // namespace editor
}  // namespace mt
