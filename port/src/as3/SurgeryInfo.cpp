#include "as3/SurgeryInfo.h"

#include "as3/Content.h"

namespace mt {

SurgeryInfo loadSurgeryInfo(const std::string& surgery, const AreaDef& area) {
    SurgeryInfo s;
    s.id = surgery;
    json::Value root;
    readJson("Data/Surgeries/surgeries_info.json", root);
    const json::Value& i = root["surgeries_info"][surgery];
    auto str = [&](const char* key) { return i[key].type == json::Value::kString ? i[key].string : std::string(); };
    s.patientTitle = str("patient_title");
    s.locationTitle = str("location_title");
    s.initialDescription = str("initial_description");
    s.observationsOphelia = str("main_observations_ophelia");
    s.observationsAlan = str("main_observations_alan");
    s.symptomsOphelia = str("main_symptoms_ophelia");
    s.symptomsAlan = str("main_symptoms_alan");
    s.actionsOphelia = str("main_actions_ophelia");
    s.actionsAlan = str("main_actions_alan");
    s.bonus1Description = str("bonus1_description");
    s.bonus2Description = str("bonus2_description");
    s.bonus1Title = str("bonus1_title");
    s.bonus2Title = str("bonus2_title");
    s.bonus1Overview = str("bonus1_overview");  // (1.45)
    s.bonus2Overview = str("bonus2_overview");
    s.conditions = str("conditions");
    s.photoSuffix = str("photo_suffix");
    s.infoSpritesheet = str("info_spritesheet");
    s.preopBgSprite = str("preop_bg_sprite");
    s.preopSpritesheet = str("preop_spritesheet");
    s.dialogueIntroFile = str("dialogue_intro_file");
    s.dialogueOutroFile = str("dialogue_outro_file");
    s.preSurgerySfx = str("pre_surgery_sfx");
    for (const json::Value& v : i["dialogue_spritesheets"].items)
        if (s.dialogueSpritesheets.size() < 6) s.dialogueSpritesheets.push_back(v.str());
    s.area = area;
    return s;
}

}  // namespace mt
