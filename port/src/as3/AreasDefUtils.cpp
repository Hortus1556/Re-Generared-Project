#include "as3/AreasDefUtils.h"

#include <algorithm>
#include <map>

#include "as3/Content.h"

namespace mt {

namespace {

// Json::Value's asString/asInt/asFloat as the original read them
std::string asString(const json::Value& v) { return v.type == json::Value::kString ? v.string : ""; }
int asInt(const json::Value& v) { return v.integer(); }
float asFloat(const json::Value& v) { return static_cast<float>(v.num()); }

void readStrings(const json::Value& array, std::vector<std::string>& out, size_t most) {
    for (const json::Value& v : array.items)
        if (out.size() < most) out.push_back(asString(v));
}

}  // namespace

AreasDefUtils* AreasDefUtils::shared() {
    static AreasDefUtils* a = [] {
        AreasDefUtils* u = new AreasDefUtils;
        u->load();
        return u;
    }();
    return a;
}

void AreasDefUtils::load() {
    std::map<std::string, int> costs;
    json::Value unlocks, root;
    if (readContent("area_unlocks.json", unlocks))
        for (const json::Value& u : unlocks.items) costs[asString(u["id"])] = asInt(u["area_unlock_cost"]);
    if (!readContent("areas.json", root)) return;
    const json::Value& list = root["areas"];
    // (1.45 0x1457e0) eight at most: Le Flairground Fantastique first, then
    // the Hospital, then the others in their order (each read by loadArea
    // 0x146294). Android 1.43 had seven, the Hospital first.
    std::vector<const json::Value*> order;
    const size_t most = std::min<size_t>(list.size(), 8);
    for (const char* first : {"area_pocketgods", "area_hospital"})
        for (size_t i = 0; i < most; ++i)
            if (asString(list[i]["area_id"]) == first) order.push_back(&list[i]);
    for (size_t i = 0; i < most; ++i) {
        const std::string id = asString(list[i]["area_id"]);
        if (id != "area_pocketgods" && id != "area_hospital") order.push_back(&list[i]);
    }
    for (const json::Value* item : order) {
        const json::Value& a = *item;
        AreaDef d;
        d.areaId = asString(a["area_id"]);
        d.title = asString(a["title"]);
        d.worldMapIcon = asString(a["world_map_icon"]);
        d.bgSprite = asString(a["bg_sprite"]);
        d.mapUiDefinitionFile = asString(a["map_ui_definition_file"]);
        d.bgSpritesheet = asString(a["bg_spritesheet"]);
        d.mapSpritesheet = asString(a["map_spritesheet"]);
        d.bossSurgery = asString(a["boss_surgery"]);
        d.bgm = asString(a["bgm"]);
        d.leaderboardId = asString(a["leaderboardId"]);
        d.pointsMultiplier = asFloat(a["points_multiplier"]);
        d.injuryDamageMultiplier = asFloat(a["injury_damage_multiplier"]);
        d.completeDialogueFile = asString(a["area_complete_dialogue_file"]);
        d.newTitle = asString(a["area_new_title"]);
        d.newBody = asString(a["area_new_body"]);
        d.introDialogueFile = asString(a["area_intro_dialogue_file"]);
        readStrings(a["area_intro_dialogue_spritesheets"], d.introDialogueSpritesheets, 6);
        auto cost = costs.find(d.areaId);
        d.unlockCost = cost == costs.end() ? 0 : cost->second;
        readStrings(a["area_complete_dialogue_spritesheets"], d.completeDialogueSpritesheets, 6);
        readStrings(a["surgeries"], d.surgeries, 8);
        readStrings(a["surgeries_shortnames"], d.surgeryShortNames, 8);
        for (const json::Value& v : a["surgeries_base_numbers"].items) d.surgeryBaseNumbers.push_back(asInt(v));
        areas.push_back(d);
    }
}

const AreaDef* AreasDefUtils::byId(const std::string& id) const {
    for (const AreaDef& a : areas)
        if (a.areaId == id) return &a;
    return nullptr;
}

}  // namespace mt
