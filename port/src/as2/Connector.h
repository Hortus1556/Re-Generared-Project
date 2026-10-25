// The missions (levels.txt, read by the game's lenient JSON reader SPJSON:
// the engine's reader accepts the same), the mission being played, and the
// injury and tool catalogue (Library; data/library.json, made from the
// binary by port/tools/as2_data.py).
#pragma once

#include <string>
#include <vector>

#include "core/Json.h"
#include "core/Object.h"

namespace as2 {

// One injury placed in a stage.
class Injury : public Object {
public:
    static Injury* initFromDictionary(const json::Value& d);
    int mID = 0;
    int x = 0, y = 0;
    int rotation = 0;
    int scale = 0;
    bool offset = false;
    // Placed by levels.txt: its point on the Flash AS2's stage (where the
    // Flash AS2 had it, else levels.txt's numbers).
    bool staged = false;
    int stageX = 0, stageY = 0;
};

class MissionStage : public Object {
public:
    static MissionStage* initFromDictionary(const json::Value& d);
    int body_level = 0;
    std::string bg;
    int time = 0;
    int health = 0;
    Array<Injury> injuries;
    bool torch = false;
};

class Mission : public Object {
public:
    static Mission* initFromDictionary(const json::Value& d);
    std::string name;
    json::Value data;  // mission_data: the stages, as dictionaries
    std::vector<int> tools;
};

class Connector {
public:
    static Connector& sharedConnector();
    Mission* get_current_mission() const;
    const Array<Mission>& get_mission_list() const { return missions; }
    void set_current_mission(int m) { current_mission = m; }
    void set_current_mission_id(int id) { current_mission_id = id; }

    int current_mission = 0;
    int current_mission_id = 0;
    MissionStage* current_mission_stage = nullptr;  // not retained

private:
    Connector();
    Array<Mission> missions;
};

class InjuryData : public Object {
public:
    std::string linkage;
    int mId = 0;
    std::string name, description;
    std::string type;  // the class
};

class ToolData : public Object {
public:
    std::string linkage;
    int mId = 0;
    std::string name, description;
    std::string type;
    bool enterFrame = false;
};

}  // namespace as2
