#include "as3/SurgeryDebugStart.h"

#include <cstdlib>
#include <string>

#include "as3/AchievementsScreen.h"
#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/CreditsScreen.h"
#include "as3/DialogueScreen.h"
#include "as3/EditorLevel.h"
#include "as3/EditorLevelsScreen.h"
#include "as3/EditorPlay.h"
#include "as3/EditorScreen.h"
#include "as3/InfoScreen.h"
#include "as3/PartnerSelectScreen.h"
#include "as3/PreOpScreen.h"
#include "as3/SaveSlotScreen.h"
#include "as3/UpgradeScreen.h"
#include "as3/WelcomeScreen.h"
#include "as3/WorldScreen.h"
#include "as3/Log.h"
#include "as3/SurgeryInfo.h"
#include "as3/SurgeryScreen.h"
#include "platform/Platform.h"

namespace mt {

Scene* surgeryFromEnv() {
    if (!platform::debugMode()) return nullptr;
    // (AS_EDITED=<file>: a surgery editor's level instead, EditorPlay.h)
    if (Scene* edited = editor::editedLevelFromEnv()) return edited;
    const char* spec = std::getenv("AS_SURGERY");
    if (!spec || !*spec) return nullptr;
    std::string id = spec;
    SurgeryParams params;
    std::string::size_type colon = id.find(':');
    if (colon != std::string::npos) {
        params.mode = std::atoi(id.c_str() + colon + 1);
        id = id.substr(0, colon);
    }
    const AreaDef* area = nullptr;
    for (const AreaDef& a : AreasDefUtils::shared()->areas)
        for (const std::string& s : a.surgeries)
            if (s == id) area = &a;
    if (!area) {
        logError("AS_SURGERY: no area has %s", id.c_str());
        return nullptr;
    }
    return SurgeryScreen::create(loadSurgeryInfo(id, *area), params);
}

Scene* screenFromEnv() {
    if (!platform::debugMode()) return nullptr;
    const char* spec = std::getenv("AS_SCREEN");
    if (!spec || !*spec) return nullptr;
    std::string name = spec, id;
    SurgeryParams params;
    std::string::size_type colon = name.find(':');
    if (colon != std::string::npos) {
        id = name.substr(colon + 1);
        name = name.substr(0, colon);
        std::string::size_type m = id.find(':');
        if (m != std::string::npos) {
            params.mode = std::atoi(id.c_str() + m + 1);
            id = id.substr(0, m);
        }
    }
    if (name == "title") return WelcomeScreen::create();
    if (name == "saveslots") return SaveSlotScreen::create();
    if (name == "world") return WorldScreen::create();
    if (name == "upgrades") return UpgradeScreen::create("world");
    if (name == "partners") return PartnerSelectScreen::create("world", SurgeryInfo(), SurgeryParams());
    if (name == "achievements") return AchievementsScreen::create();
    if (name == "credits") return CreditsScreen::create();
    if (name == "editor") return EditorLevelsScreen::create();
    if (name == "editor-new") {
        editor::session() = editor::Session();
        editor::session().level = editor::newLevel();
        return EditorScreen::create();
    }
    const AreaDef* area = nullptr;
    for (const AreaDef& a : AreasDefUtils::shared()->areas)
        for (const std::string& s : a.surgeries)
            if (s == id) area = &a;
    if (!area) {
        logError("AS_SCREEN: %s: no area has %s", name.c_str(), id.c_str());
        return nullptr;
    }
    if (name == "area") return AreaScreen::create(*area, 0);
    if (name == "info") return InfoScreen::create(id, *area);
    if (name == "dialogue") return DialogueScreen::create(loadSurgeryInfo(id, *area), params);
    if (name == "preop") return PreOpScreen::create(loadSurgeryInfo(id, *area), params);
    logError("AS_SCREEN: no screen %s", name.c_str());
    return nullptr;
}

int surgeryDebugStage(int stages) {
    if (!platform::debugMode()) return 0;
    const char* spec = std::getenv("AS_SURGERY");
    if (!spec) return 0;
    std::string s = spec;
    std::string::size_type a = s.find(':');
    if (a == std::string::npos) return 0;
    std::string::size_type b = s.find(':', a + 1);
    if (b == std::string::npos) return 0;
    int stage = std::atoi(s.c_str() + b + 1);
    return stage >= 2 && stage <= stages ? stage : 0;
}

}  // namespace mt
