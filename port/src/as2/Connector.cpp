#include "as2/Connector.h"

#include "as2/GameData.h"
#include "as2/Library.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

namespace as2 {

Injury* Injury::initFromDictionary(const json::Value& d) {
    Injury* i = new Injury;
    i->mID = d["id"].integer();
    int x = d["x"].integer();
    i->x = x;
    i->y = d["y"].integer();
    // levels.txt is laid out on the Flash game's 600 x 400 stage: the
    // iPhone's 480 x 320 takes x at 0.8 and y 50 higher (-1, -1: placed by
    // the game).
    if (i->x != -1 || i->y != -1) {
        // Its point on the Flash stage: the Flash AS2's own where it had
        // the injury ("stageX", "stageY": the Connector's), else these.
        i->staged = true;
        i->stageX = d["stageX"].integer(i->x);
        i->stageY = d["stageY"].integer(i->y);
        i->x = static_cast<int>(static_cast<double>(static_cast<float>(x)) * 0.8);
        i->y = i->y - 50;
    }
    i->rotation = d["rotation"].integer();
    i->scale = d["scale"].integer();
    i->offset = false;
    return i;
}

MissionStage* MissionStage::initFromDictionary(const json::Value& d) {
    MissionStage* s = new MissionStage;
    s->bg = d["bg"].str();
    s->body_level = d["body_level"].integer();
    s->time = d["time"].integer();
    s->health = d["health"].integer();
    s->torch = d["torch"].flag(false) || d["torch"].integer(0) != 0;
    for (const json::Value& v : d["injuries"].items) {
        Injury* i = Injury::initFromDictionary(v);
        s->injuries.addObject(i);
        releaseRef(i);
    }
    return s;
}

Mission* Mission::initFromDictionary(const json::Value& d) {
    Mission* m = new Mission;
    m->name = d["name"].str();
    m->data = d["mission_data"];
    for (const json::Value& t : d["tools"].items) m->tools.push_back(t.integer());
    return m;
}

Connector& Connector::sharedConnector() {
    static Connector* instance = new Connector;
    return *instance;
}

Connector::Connector() {
    std::string text;
    json::Value doc;
    std::string err;
    if (!assets::readText("levels.txt", text) || !json::parse(text, doc, &err))
        platform::fatal("levels.txt cannot be read: " + err);
    // Where the Flash AS2 placed its missions' injuries (library.json's
    // "flashLevels", from its levels.txt): given to each injury of the
    // same mission, stage, place in the list and id as "stageX", "stageY".
    json::Value lib;
    if (assets::readText("data/library.json", text)) json::parse(text, lib);
    const json::Value& flash = lib["flashLevels"];
    auto member = [](json::Value& v, const char* key) -> json::Value* {
        for (auto& [k, m] : v.members)
            if (k == key) return &m;
        return nullptr;
    };
    if (json::Value* levels = member(doc, "levels"))
        for (size_t n = 0; n < levels->items.size(); ++n) {
            json::Value* stages = member(levels->items[n], "mission_data");
            for (size_t s = 0; stages && s < stages->items.size(); ++s) {
                json::Value* injuries = member(stages->items[s], "injuries");
                for (size_t k = 0; injuries && k < injuries->items.size(); ++k) {
                    const json::Value& at = flash[n][s][k];
                    json::Value& injury = injuries->items[k];
                    if (at.size() != 3 || at[0].integer(-1) != injury["id"].integer()) continue;
                    injury.members.emplace_back("stageX", at[1]);
                    injury.members.emplace_back("stageY", at[2]);
                }
            }
        }
    for (const json::Value& v : doc["levels"].items) {
        Mission* m = Mission::initFromDictionary(v);
        missions.addObject(m);
        releaseRef(m);
    }
}

Mission* Connector::get_current_mission() const {
    return current_mission >= 0 && current_mission < missions.count() ? missions[current_mission] : nullptr;
}

// ------------------------------------------------------------------ Library

Library& Library::sharedLibrary() {
    static Library* instance = new Library;
    return *instance;
}

Library::Library() {
    std::string text;
    json::Value doc;
    if (!assets::readText("data/library.json", text) || !json::parse(text, doc))
        platform::fatal("data/library.json is missing: run port/tools/as2_data.py");
    for (const json::Value& v : doc["injuries"].items) {
        InjuryData* d = new InjuryData;
        d->mId = v["id"].integer();
        d->linkage = v["linkage"].str();
        d->type = v["class"].str();
        d->name = v["name"].str();
        d->description = v["description"].str();
        injury_library.addObject(d);
        releaseRef(d);
    }
    for (const json::Value& v : doc["tools"].items) {
        ToolData* t = new ToolData;
        t->mId = v["id"].integer();
        t->linkage = v["linkage"].str();
        t->type = v["class"].str();
        t->name = v["name"].str();
        t->description = v["description"].str();
        t->enterFrame = v["enterFrame"].flag();
        tool_library.addObject(t);
        releaseRef(t);
    }
}

InjuryData* Library::getInjuryByID(int id) const {
    for (InjuryData* d : injury_library)
        if (d->mId == id) return d;
    return nullptr;
}

ToolData* Library::getToolByID(int id) const {
    for (ToolData* t : tool_library)
        if (t->mId == id) return t;
    return nullptr;
}

const int* Library::getInjuryBoundsForID(int id) const {
    static const int none[4] = {0, 0, 0, 0};
    return id >= 0 && id < 160 ? GameData::boundBox[id] : none;
}

}  // namespace as2
