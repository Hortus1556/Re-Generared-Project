#include "as3/RestoredContent.h"

#include "as3/Log.h"
#include "as3/SaveData.h"
#include "as3/StatsMgr.h"

namespace mt {
namespace restored {

namespace {

// The definitions the package lacks, written as Data/Injuries/ files would
// be. LighterShiv: Dale Break's shanks' numbers (Toothbrush_*, Razor,
// Razorblade: all the same). OrganShard1-3: the Reverend's Organ's own
// shards' (RoborpionShard2/3: no depth, 0.1 a second, a robot's large cut
// under them), their hit rect the shape of their frames (264x388).
const char* const kLighterShiv = R"({
    "remove_on_complete": true, "base_class_name": "AngledExtraction", "class_name": "LighterShiv",
    "size": {"width": 58, "height": 170}, "depth": 15,
    "stages": [{"state": "Extracting", "needs_graphic": true, "can_be_grabbed": true, "show_hit_areas": false,
                "use_size_rect": true, "tools": ["Tongs"], "damage_timer": 1, "damage": 0.1, "particle_timer": 0,
                "particle": "", "event_timer": 10, "injury_to_create_at_start": "LargeCut"}]})";

const char* const kOrganShard = R"({
    "remove_on_complete": true, "base_class_name": "AngledExtraction", "class_name": "OrganShard%",
    "size": {"width": 26, "height": 62}, "extra_injuries": ["LargeCutRobot"], "depth": 0,
    "stages": [{"state": "Extracting", "needs_graphic": true, "can_be_grabbed": true, "show_hit_areas": false,
                "use_size_rect": true, "tools": ["Tongs"], "damage_timer": 1, "damage": 0.1, "particle_timer": 0,
                "particle": "", "event_timer": 10, "injury_to_create_at_start": "LargeCutRobot"}]})";

// A swap: in a surgery's main file, the injury of that name becomes that
// class (the mirror: its x turned about the canvas's middle, 640 wide).
struct Swap {
    const char* surgery;
    const char* injury;
    const char* className;
    bool mirror;
};
const Swap kSwaps[] = {
    {"surgery_shanksalot_dalebreak", "razorblade_2", "LighterShiv", false},
    {"surgery_shanksalot_dalebreak", "toothbrush_yellow_1", "Toothbrush_Green", false},
    {"surgery_robotopia_reverendsorgan", "roborpionshard2_2", "OrganShard1", false},
    {"surgery_robotopia_reverendsorgan", "roborpionshard3_2", "OrganShard2", false},
    {"surgery_robotopia_reverendsorgan", "roborpionshard4_1", "OrganShard3", false},
    {"surgery_bunker_mrgiblets", "MonitorShard3_1", "MonitorShard1", false},
    {"surgery_shanksalot_earnestpickle", "kneeltransplant_1", "kneeRTransplant", true},
    {"surgery_bunker_hubris2", "giantwormchestwrap_1", "GiantWormWrapChest", false},
};

// The finished tutorials no surgery names (Android 1.43 and iOS 1.45 ship
// them; 1.45's use 1.45's TimeElapsedOnStage condition, which the port has).
struct Tutorial {
    const char* surgery;
    const char* tutorial;
};
const Tutorial kTutorials[] = {
    {"surgery_lunacyfalls_batshift", "tutorial_lunacyfalls_batshift"},
    {"surgery_robotopia_reverendsorgan", "tutorial_robotopia_revsorgan"},
    {"surgery_exileclinic_usefulbrother1", "tutorial_exile_usefulbro1"},
};

// A stage effect no surgery file sets: 1.45 reads a stage's "effect"
// (loadSurgery 0x9abf4; "BubbleEffect", mt::BubbleEffect, its bubbles over
// the stage and bubblesFX looping), and the whale's file preloads
// bubblesFX.caf, but none names it. The whale's stages in the tank's water
// (its mouth, 1-2, and its flank, 3-4 and 10; not the blubber and flesh
// inside), in all three modes: it changes nothing of the play.
struct StageEffect {
    const char* surgery;
    int stages[6];  // 1-based, 0 ends
    const char* effect;
};
const StageEffect kStageEffects[] = {
    {"surgery_pocketgods_whale", {1, 2, 3, 4, 10, 0}, "BubbleEffect"},
};

json::Value* member(json::Value& v, const std::string& key) {
    for (auto& m : v.members)
        if (m.first == key) return &m.second;
    return nullptr;
}

void setString(json::Value& v, const std::string& key, const std::string& s) {
    json::Value* m = member(v, key);
    if (!m) {
        v.members.emplace_back(key, json::Value());
        m = &v.members.back().second;
    }
    m->type = json::Value::kString;
    m->string = s;
}

}  // namespace

bool directorsCut() {
    return !SaveData::shared().savedGames.empty() && SaveData::shared().currentSlot().directorsCut;
}

bool directorsCutUnlocked() { return StatsMgr::sharedInstance()->earnedCount() >= kDirectorsCutAchievements; }

void setDirectorsCut(bool on) {
    SaveData::shared().currentSlot().directorsCut = on;
    SaveData::shared().saveGame();
    log("director's cut %s", on ? "on" : "off");
}

void applyToSurgery(const std::string& surgeryId, int mode, json::Value& root) {
    if (!directorsCut() || root.type != json::Value::kObject) return;
    json::Value* stages = member(root, "stages");
    // the stage effects, in every mode
    for (const StageEffect& e : kStageEffects) {
        if (surgeryId != e.surgery || !stages) continue;
        for (int st : e.stages) {
            if (st <= 0) break;
            if (st > static_cast<int>(stages->items.size())) continue;
            setString(stages->items[st - 1], "effect", e.effect);
            log("director's cut: %s stage %d effect %s", surgeryId.c_str(), st, e.effect);
        }
    }
    // the rest: the main surgery only
    if (mode != 0) return;
    for (const Tutorial& t : kTutorials) {
        if (surgeryId != t.surgery) continue;
        setString(root, "tutorial_filename", t.tutorial);
        log("director's cut: %s tutorial %s", surgeryId.c_str(), t.tutorial);
    }
    if (!stages) return;
    for (size_t si = 0; si < stages->items.size(); ++si) {
        json::Value* injuries = member(stages->items[si], "injuries");
        if (!injuries) continue;
        int bats = 0;
        for (json::Value& inj : injuries->items) {
            const std::string name = inj["name"].str();
            for (const Swap& s : kSwaps) {
                if (surgeryId != s.surgery || name != s.injury) continue;
                setString(inj, "class_name", s.className);
                if (s.mirror)
                    if (json::Value* x = member(inj, "x")) x->number = 640 - x->number;
                log("director's cut: %s stage %d %s is %s", surgeryId.c_str(), static_cast<int>(si) + 1, name.c_str(),
                    s.className);
            }
            // Batshift's tutorial wants its stage 3 bats as bat_1..3 (its
            // file has three called "bat")
            if (surgeryId == "surgery_lunacyfalls_batshift" && si == 2 && inj["class_name"].str() == "Bat" &&
                name == "bat")
                setString(inj, "name", "bat_" + std::to_string(++bats));
        }
    }
}

bool injuryJson(const std::string& className, json::Value& out) {
    std::string text;
    if (className == "LighterShiv") {
        text = kLighterShiv;
    } else if (className == "OrganShard1" || className == "OrganShard2" || className == "OrganShard3") {
        text = kOrganShard;
        text.replace(text.find('%'), 1, className.substr(10));
    } else {
        return false;
    }
    std::string error;
    if (!json::parse(text, out, &error)) {
        logError("restored %s: %s", className.c_str(), error.c_str());
        return false;
    }
    return true;
}

}  // namespace restored
}  // namespace mt
