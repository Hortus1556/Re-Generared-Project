#include "as3/SettingsInfoUtils.h"

#include "as3/Content.h"
#include "as3/Log.h"

namespace mt {

const char* const kSettingIds[kSettingCount] = {
    "max_bloodpacks",      "start_bloodpacks",    "bloodpack_recharge_secs", "surgeon3coinpack1",
    "surgeon3coinpack2",   "surgeon3coinpack3",   "surgeon3coinpack4",       "surgeon3coinpack5",
    "surgeon3bloodpack1",  "surgeon3bloodpack2",  "surgeon3bloodpack3",      "surgeon3revivepack1",
    "surgeon3revivepack2", "surgeon3revivepack3", "throttle",                "facebook_connect_reward"};

SettingsInfoUtils* SettingsInfoUtils::shared() {
    static SettingsInfoUtils* s = [] {
        SettingsInfoUtils* x = new SettingsInfoUtils;
        x->load();
        return x;
    }();
    return s;
}

void SettingsInfoUtils::load() {
    json::Value list;
    if (!readContent("settings.json", list)) {
        logError("%s doen't exist", "Downloaded/settings.json");
        return;
    }
    for (const json::Value& s : list.items) {
        const std::string& id = s["id"].str();
        bool known = false;
        for (const char* k : kSettingIds) known = known || id == k;
        // ("version" and the urls, strings, are not of them)
        if (!known || settings.size() >= 24) continue;
        settings.push_back({id, s["value"].integer()});
    }
}

int SettingsInfoUtils::value(int setting) const {
    if (setting < 0 || setting >= kSettingCount) return -1;
    for (const Setting& s : settings)
        if (s.id == kSettingIds[setting]) return s.value;
    return -1;
}

}  // namespace mt
