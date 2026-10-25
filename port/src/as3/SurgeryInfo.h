// What the screens before and after a surgery know of it (0x124 bytes; made
// by 0x2bc60, copied by 0x4d10c): its texts and art from
// Data/Surgeries/surgeries_info.json, and its area (0xb6e1c). The surgery
// itself (its injuries, stages) is Data/Surgeries/<id>.json.
#pragma once

#include <string>
#include <vector>

#include "as3/AreasDefUtils.h"

namespace mt {

struct SurgeryInfo {
    std::string id;                    // +0
    std::string patientTitle;          // +4
    std::string locationTitle;         // +8
    std::string initialDescription;    // +0xc
    std::string observationsOphelia, observationsAlan;  // +0x10, +0x14
    std::string symptomsOphelia, symptomsAlan;          // +0x18, +0x1c
    std::string actionsOphelia, actionsAlan;            // +0x20, +0x24
    std::string bonus1Description, bonus2Description;   // +0x28, +0x2c
    std::string bonus1Title, bonus2Title;               // +0x30, +0x34
    // (1.45 LoadSurgeryInfoDef 0xec8a8) a challenge's line for the pre-op's star panel
    std::string bonus1Overview, bonus2Overview;
    std::string conditions;            // +0x38
    std::string photoSuffix;           // +0x3c
    std::string infoSpritesheet;       // +0x40
    std::string preopBgSprite;         // +0x44
    std::string preopSpritesheet;      // +0x48
    std::string dialogueIntroFile;     // +0x4c
    std::string dialogueOutroFile;     // +0x50
    std::string preSurgerySfx;         // +0x54
    std::vector<std::string> dialogueSpritesheets;  // +0x58: 6 at most
    AreaDef area;                      // +0x74
};

// 0xb6e1c
SurgeryInfo loadSurgeryInfo(const std::string& surgery, const AreaDef& area);

// How a surgery is to be played (0xc bytes), passed from screen to screen.
struct SurgeryParams {
    int mode = 0;      // 0 the main surgery, 1 and 2 its challenges
    std::string name;  // +4: a surgery file to play instead (the level editor's), "" for none
    int value = 1;     // +8 (its use is still to find)
};

// How a surgery went (0x38 bytes, made by 0x4f384), from the surgery screen
// to the outro and the results screen.
struct SurgeryResult {
    std::string surgery;          // +0
    std::string area;             // +4: its area's id
    int timeLeft = 0;             // +8: seconds
    int score = 0;                // +0xc
    unsigned char stars[3] = {};  // +0x10: the main surgery's (1-3), then each challenge's (0/1), as now won
    int coinsFromScore = 0;       // +0x14
    int coinsBonus = 0;           // +0x18: for the stars won
    bool won = false;             // +0x1c
    bool firstTime = false;       // +0x1d: new stars on the main surgery
    bool tutorial = false;        // +0x1e: played with its tutorial
    std::string partner;          // +0x20: the tag partner's id ("" none)
    int starsGained = 0;          // +0x24
    int stage = 0;                // +0x28: the stage it ended on (0-based)
    bool b2c = false;             // +0x2c
    int progress = 0;             // +0x30: percent of the stages' weight done
    bool partnerFlag = false;     // +0x34 (the screen's +0xfd8)
};

}  // namespace mt
