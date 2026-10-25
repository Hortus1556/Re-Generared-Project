// The surgery's data (re/as3/notes/surgery.md): an injury class's
// definition (Data/Injuries/<class>.json, read by 0x8c898), a surgery's file
// (its stages and their injuries, 0x75f58), and the game's enums for them
// (static maps in the binary).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Json.h"
#include "engine/cocos/CCGeometry.h"
#include "engine/cocos/ccTypes.h"

namespace mt {

// The tools (0x42bd98), in the order of the map's values.
enum Tool {
    kLighter, kCream, kStapler, kCutter, kTazer, kTongs, kChainsaw, kInjection, kVacuum, kClamp, kScanner,
    kToolCount, kNoTool = kToolCount
};
extern const char* const kToolNames[kToolCount];
int toolByName(const std::string& name);  // kNoTool if unknown (the original: 0, std::map's [])

// An injury's base class (0x42bdb0): which C++ class runs it.
enum InjuryBase {
    kAngledExtraction, kArtery, kBat, kBlackFrog, kBoil, kBurn, kCementDrop, kFireMaggot, kFlea, kIncision,
    kLargeCut, kLeech, kMiniBat, kMiniFrog, kPassiveParticle, kPoisonArea, kRobotScorpion, kSmallCut, kSnake,
    kSoakableParticle, kTransplant, kUnderSurfaceExtraction, kBrokenBone, kBrokenVein, kFire, kPoisonFrog,
    kSurfaceExtraction, kIceAnt, kLargeCutRobot, kSmallCutRobot, kIncisionRobot, kRust, kSnakeCut,
    kSuperPoisonArea, kSuperPoisonScab, kFrostBite, kSpikeBase, kMiniWorm, kGiantWorm,
    // The Hospital's (Android 1.43; iOS 1.45 has them too, after IceAnt: re/as3flair/enums145.txt)
    kSpider, kGhost,
    // Le Flairground Fantastique's (iOS 1.45: Locust and Jellyfish after Leech, Pygmy last). 1.45's
    // table has no LargeCutRobot, SmallCutRobot, IncisionRobot or Rust (above, kept so the port's
    // numbers stay): no data names them as a base (their files say LargeCut, SmallCut, Incision, Burn)
    kLocust, kJellyfish, kPygmy, kInjuryBaseCount
};
extern const char* const kInjuryBaseNames[kInjuryBaseCount];
// Its class name's index (0x42bdc8, 114 names); 0 if unknown.
int injuryClassIndex(const std::string& name);

// An injury stage's state (0x42bdf8, 82 names); 0 (Open) if unknown.
enum InjuryStateKind {
    kOpen, kOpenWithClamp, kClosed, kBurnt, kFixed, kFree, kReplacing, kReplaced, kExtracting,
    // ... the rest by their index (see kStateNames)
};
// 114 of iOS 1.02, the Hospital's 34, the Flairground's 32 (iOS 1.45's 180 names in the port's order)
constexpr int kClassNameCount = 180;
// 82 of iOS 1.02, the Hospital's 7, the Flairground's 10 (iOS 1.45's 99 in the port's order)
constexpr int kStateCount = 99;
extern const char* const kStateNames[kStateCount];
int stateByName(const std::string& name);

// One point of a hit area (8 bytes in the original: x, y, radius, angle).
struct HitPoint {
    int x = 0, y = 0;
    int radius = 25;  // 35 on a small screen for some bases, 85 for a clamp's
    int angle = 0;    // degrees
};

// A hit area: the points a tool works along, for one state (0x1a0 bytes).
struct HitArea {
    int state = 0;
    bool forClamp = false;
    std::string name;
    std::vector<HitPoint> points;  // 50 at most
};

// An injury stage (0x78 bytes).
struct InjuryStageDef {
    std::vector<int> tools;  // 10 at most
    float damageTimer = 0, damage = 0, particleTimer = 0;
    bool needsGraphic = false;
    std::vector<std::string> animationFilenames;  // 2 at most
    int particle = 0;
    float eventTimer = 0;
    bool canBeGrabbed = false, showHitAreas = false, useSizeRect = false;
    cocos2d::CCPoint createAtStartOffset, createAtEndOffset;
    std::string createAtStart, createAtEnd, createOnExtraction;
    std::string stateName;
    int state = 0;
};

// A piece of an injury (a sprite of it: 0xa08 bytes).
struct InjuryPieceDef {
    int kind = 0;  // 6: a replacement piece
    std::string sprite;
    cocos2d::CCRect activeRect;
    bool isReplacement = false;
    int zOrder = 0;
    bool interactable = true;
    bool notReplacement = false;  // +0x10 of the original's piece record
    // (the port's) its picture's colour, "tint": [r, g, b] (white: as drawn)
    cocos2d::ccColor3B tint = {255, 255, 255};
    std::vector<HitArea> dependencies;  // the hit areas named by its dependencies (0x81a64)
};

struct InjuryDef {  // 0xabc4 bytes in the original
    std::string name;               // +0: the class name
    int base = 0;                   // +4
    int classIndex = 0;             // +8
    std::string baseName;           // +0xc
    bool removeOnComplete = false;  // +0x10
    std::vector<InjuryStageDef> stages;  // +0x14
    std::vector<HitArea> hitAreas;       // +0x4c8
    cocos2d::CCSize size;                // +0x151c: canvas points × the canvas scale
    std::vector<InjuryPieceDef> pieces;  // +0x1524
    int aux1 = 0;                        // +0xaba0
    bool embedded = true;                // +0xaba4
    int depth = 0;                       // +0xaba8
    bool isLimb = false;                 // +0xabac
    std::string passiveFxRef, soakableFxRef, bubbleFxRef;          // +0xabb0, +0xabb4, +0xabb8
    std::string passiveSpriteRef, soakableSpriteRef;               // +0xabbc, +0xabc0

    // the stage of a state ("the stages' index + 1", the original's numbering);
    // 0 if none
    int stageOf(int state) const;
};

// Bigger touch areas for some injuries on a small screen (0x75eb0).
bool hasBigHitAreas(const InjuryDef& def);
// The screen counts as small (at most 480x320 canvas points: 0x75e5c).
bool isSmallScreen();

// Reads Data/Injuries/<className>.json (0x8c898).
InjuryDef loadInjuryDef(const std::string& className);

// A surgery's stage (0x40 bytes) and its injuries as the file gives them.
struct StageInjury {
    std::string className;
    std::string name;
    cocos2d::CCPoint position;  // screen points
    int rotation = 0;
    bool allowNonEssentialCreation = true;
    std::vector<std::string> visualDependencies, interactionDependencies;
    int toSurgeryStage = 0;
    int stageEndToSwap = 0;
};

struct AdditionalBg {
    std::string filename;
    int x = 0, y = 0, rotation = 0;
    bool flipX = false, flipY = false;
};

struct SurgeryStageDef {
    int name = 0;
    int toSurgeryStage = 0;
    std::string bg;
    bool bgFlipX = false, bgFlipY = false;
    // (iOS 1.45 loadSurgery 0x9abf4) an effect over the whole stage, by its
    // name ("BubbleEffect"), shown while it is (SurgeryScreen's stage
    // switch); "" none. No shipped surgery file sets it
    std::string effect;
    std::string bgm;
    int health = 0, time = 0, bodyLevel = 0;
    std::vector<AdditionalBg> additionalBgs;
    std::vector<StageInjury> injuries;
    float weight = 10;                   // +0x38 (10 for every stage)
    float injuryDamageMultiplier = -1;   // +0x3c: -1 if not given
};

struct SurgeryDef {
    std::vector<SurgeryStageDef> stages;
    std::vector<int> tools;
    bool isChallenge = false, isSuddenDeath = false, tagAllowed = true, hideExtraction = false;
    float injuryDamageMultiplier = -1;
    std::vector<std::string> sheets, sounds;
    std::string scream, tutorial;
    int minScore[3] = {}, minTime[3] = {};
};

// The surgery's file (0x79ed8): Downloaded/Surgeries/<id>[_challenge1|2].json,
// or the params' file when given.
std::string surgeryFile(const std::string& surgeryId, int mode, const std::string& customFile);
// Reads a surgery (0x75f58 with 0x32060's preloading left to the caller).
SurgeryDef loadSurgeryDef(const json::Value& root);

}  // namespace mt
