#include "as3/Content.h"

#include <map>

#include "as3/EditorPlay.h"
#include "as3/Log.h"
#include "engine/cocos/CCFileUtils.h"

namespace mt {

bool readJson(const std::string& path, json::Value& out) {
    std::string text, error;
    if (!cocos2d::CCFileUtils::sharedFileUtils()->readText(path, text)) {
        logError("Cannot read %s", path.c_str());
        return false;
    }
    if (!json::parse(text, out, &error)) {
        logError("%s is not JSON: %s", path.c_str(), error.c_str());
        return false;
    }
    return true;
}

bool readContent(const std::string& name, json::Value& out) {
    // (the surgery editor's level in play, EditorPlay.h: the original's
    // edited version, SurgeryUtils::getSurgeryToLoadFullPath 0x75af90)
    if (editor::isEditedName(name)) return editor::readEditedLevel(name, out);
    // the files copyData wrote from default_data.json, by the array each holds
    static const std::map<std::string, std::string> kArrays = {
        {"upgrades.json", "upgrades"},
        {"partners.json", "tag_partners"},
        {"upgrade_unlocks.json", "upgrade_unlocks"},
        {"area_unlocks.json", "area_unlock_star"},
        {"settings.json", "settings"},
        {"free_daily_partner_list.json", "free_daily_partner_list"},  // (1.45 copyData 0x364614)
    };
    auto it = kArrays.find(name);
    if (it == kArrays.end()) return readJson("Data/" + name, out);  // copied as they are
    static json::Value defaults;
    static bool read = readJson("Data/default_data.json", defaults);
    if (!read) return false;
    out = defaults[it->second];
    return true;
}

}  // namespace mt
