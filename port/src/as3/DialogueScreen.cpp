#include "as3/DialogueScreen.h"

#include <algorithm>

#include "as3/AreaScreen.h"
#include "as3/BunkerPreviewScreen.h"
#include "as3/Content.h"
#include "as3/Log.h"
#include "as3/PreOpScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryEndScreen.h"
#include "as3/WorldScreen.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr float kCanvasWidth = 640, kCanvasHeight = 384;

// the tag partners' ids and their sheets' suffixes (0x42e3d8, 0x42e3f8)
const char* const kPartnerIds[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};
const char* const kPartnerSheets[8] = {"Giblets", "Saturday", "Reverand", "Flow", "Bot", "Nowe", "Crumpet", "Brutality"};

// what the screen leaves out of the canvas on each side, in canvas points
float marginX() {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    return (kCanvasWidth - r->width / r->canvasScale) * 0.5f;
}
float marginY() {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    return (kCanvasHeight - r->height / r->canvasScale) * 0.5f;
}

std::string asString(const json::Value& v) { return v.type == json::Value::kString ? v.string : ""; }

// a file name without its extension, as the sound manager takes it
std::string soundName(const std::string& file) { return file.substr(0, file.find('.')); }

}  // namespace

DialogueScreen::DialogueScreen() {
    m_partnerPictures = {
        {"giblets", "dialogueChar_Bunker_MrGiblets_normal.png"},
        {"saturday", "dialogueChar_Mumbojungle_Saturday_basic.png"},
        {"reverend", "dialogueChar_Robotopia_Reverand_angry.png"},
        {"flow", "dialogueChar_tag_Flow_meditating.png"},
        {"bot", "dialogueChar_tag_Bot_toasting.png"},
        {"nowe", "dialogueChar_tag_Nowe_basic.png"},
        {"crumpet", "dialogueChar_tag_Crumpet_basic.png"},
        {"brutality", "dialogueChar_tag_Brutality_basic.png"},
    };
}

DialogueScreen* DialogueScreen::create(const SurgeryInfo& surgery, const SurgeryParams& params) {
    DialogueScreen* s = new DialogueScreen;
    s->setName("DialogueScreen");
    s->m_surgery = surgery;
    s->m_params = params;
    // the scene's setup (0xd3560): the intro's file, the surgery's sheets
    if (s->init(kIntro, "Data/Dialogues/" + surgery.dialogueIntroFile, surgery.dialogueSpritesheets)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

DialogueScreen* DialogueScreen::createOutro(const SurgeryInfo& surgery, const SurgeryParams& params,
                                            const SurgeryResult& result) {
    DialogueScreen* s = new DialogueScreen;
    s->m_surgery = surgery;
    s->m_params = params;
    s->m_result = result;
    if (s->init(kOutro, "Data/Dialogues/" + surgery.dialogueOutroFile, surgery.dialogueSpritesheets)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

DialogueScreen* DialogueScreen::createAreaComplete(const AreaDef& area, int worldArea) {
    DialogueScreen* s = new DialogueScreen;
    s->m_area = area;
    s->m_worldArea = worldArea;
    // (0xd3930)
    if (s->init(kAreaComplete, "Data/Dialogues/" + area.completeDialogueFile, area.completeDialogueSpritesheets)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

DialogueScreen* DialogueScreen::createAreaIntro(const AreaDef& area) {
    DialogueScreen* s = new DialogueScreen;
    s->m_area = area;
    // (Android 1.43, sceneAreaIntro 0x41f418)
    if (s->init(kAreaIntro, "Data/Dialogues/" + area.introDialogueFile, area.introDialogueSpritesheets)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool DialogueScreen::init(int mode, const std::string& file, const std::vector<std::string>& sheets) {
    if (!CCScene::init()) return false;
    m_mode = mode;
    m_file = file;
    m_sheets = sheets;
    return true;
}

int DialogueScreen::completedInArea() {
    SavedGame& slot = SaveData::shared().currentSlot();
    int n = 0;
    for (const std::string& s : m_surgery.area.surgeries)
        if (slot.starsOf(s, 0) != 0) ++n;
    return n;
}

void DialogueScreen::load() {
    SavedGame& slot = SaveData::shared().currentSlot();
    const std::string& partner = slot.selectedTagPartner;
    if (partner.empty()) {
        m_partner = "";
    } else {
        m_partner = "";
        for (int i = 0; i < 8; ++i)
            if (partner == kPartnerIds[i]) m_partner = kPartnerSheets[i];
        loadSheets(this, {"dialogue_char_" + m_partner});
    }
    loadSheets(this, m_sheets);

    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale, mx = marginX(), my = marginY();
    // the picture: the canvas' size, its corner where the screen's is
    m_camera = CCSprite::create();
    m_camera->setContentSize(CCSizeMake(kCanvasWidth * s, kCanvasHeight * s));
    m_camera->setAnchorPoint(CCPointZero);
    addChild(m_camera);
    m_cameraAt = ccp(-(mx * s), -(my * s));
    m_bg = CCSprite::create();
    m_bg->setAnchorPoint(ccp(0.5f, 0));
    setPositionOnCanvas(m_bg, 320, my);
    m_camera->addChild(m_bg);
    for (int i = 0; i < 4; ++i) m_chars[i] = CCSprite::create();
    m_chars[1]->setScaleX(-1);
    m_chars[3]->setScaleX(-1);
    m_chars[0]->setAnchorPoint(CCPointZero);
    m_chars[1]->setAnchorPoint(ccp(1, 0));
    m_chars[2]->setAnchorPoint(CCPointZero);
    m_chars[3]->setAnchorPoint(ccp(1, 0));
    // 1 and 3 from the left edge, 2 and 4 (mirrored, a canvas wide) from the right
    setPositionOnCanvas(m_chars[0], mx + m_offsets[0], my);
    setPositionOnCanvas(m_chars[1], (kCanvasWidth - mx) - kCanvasWidth - m_offsets[1], my);
    setPositionOnCanvas(m_chars[2], mx + m_offsets[2], my);
    setPositionOnCanvas(m_chars[3], (kCanvasWidth - mx) - kCanvasWidth - m_offsets[3], my);
    m_camera->addChild(m_chars[1]);
    m_camera->addChild(m_chars[0]);
    m_camera->addChild(m_chars[2]);
    m_camera->addChild(m_chars[3]);

    const int completed = completedInArea();
    json::Value root;
    readJson(m_file, root);
    const json::Value& scene = root["scene"];
    if (m_mode == kAreaComplete) {
        Line first;
        first.narrative = "Back in Alan's control room...";
        m_lines.push_back(first);
    }
    std::string partnerPicture;
    if (!partner.empty()) partnerPicture = m_partnerPictures[partner];
    for (const json::Value& e : scene.items) {
        Line line;
        for (const json::Value& h : e["highlighted"].items) line.highlighted.push_back(h.integer());
        line.bgm = asString(e["bgm"]);
        line.sfx = asString(e["sfx"]);
        line.required = asString(e["required"]);
        line.bg = asString(e["bg"]);
        for (int i = 0; i < 4; ++i) line.chars[i] = asString(e["char_" + std::to_string(i + 1)]);
        line.text = asString(e["text"]);
        line.cameraX = static_cast<float>(e["camera_pos_x"].integer());
        line.cameraY = static_cast<float>(e["camera_pos_y"].integer());
        line.tagPartnerRequired = asString(e["tag_partner_required"]);
        for (int i = 0; i < 4; ++i) line.centred[i] = e["abs_" + std::to_string(i + 1)].flag();
        // (the original preloaded them by their file names, which the sound
        // manager's own ".caf" made wrong: by their names here)
        if (!line.bgm.empty()) SoundMgr::sharedSoundMgr()->preloadSound(soundName(line.bgm));
        if (!line.sfx.empty()) SoundMgr::sharedSoundMgr()->preloadSound(soundName(line.sfx));
        for (std::string& c : line.chars)
            if (c == "TAG") c = partnerPicture;
        if (!line.tagPartnerRequired.empty() && line.tagPartnerRequired != partner) continue;
        // (1.45 0x108508 added "Completed_0": the Flairground's triplets share
        // one story, told by how many of the area's patients are done)
        if (line.required.empty() || (line.required == "Completed_0" && completed == 0) ||
            (line.required == "Completed_1" && completed == 1) || (line.required == "Completed_2" && completed == 2) ||
            (line.required == "Completed_3" && completed == 3))
            m_lines.push_back(line);
    }

    m_index = 0;
    if (!m_lines.empty()) m_cameraAt = ccp(m_lines[0].cameraX, m_lines[0].cameraY);
    m_camera->setPosition(ccp(m_cameraAt.x * s, -(m_cameraAt.y * s)));
    m_def = loadUIDefinition("common/ui_dialogue_screen.json");
    buildUI(ui, this, m_def, this, menu_selector(DialogueScreen::onButton));
    // (the port's) what plays goes on into the dialogue unless it has an
    // ambience of its own, which takes over (left on if it is the one
    // playing already); the original stopped the music and the ambience
    // every time, so a dialogue without one was silent
    std::string own;
    for (const Line& l : m_lines)
        if (own.empty()) own = l.bgm;
    if (!own.empty()) {
        SoundMgr::sharedSoundMgr()->stopMusic(true);
        if (!SoundMgr::sharedSoundMgr()->ambiencePlays(soundName(own)))
            SoundMgr::sharedSoundMgr()->stopChannel(SoundMgr::kAmbienceChannel, false);
    }
    if (!m_lines.empty()) show(m_lines[m_index]);
}

void DialogueScreen::show(const Line& line) {
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale, mx = marginX(), my = marginY();
    // where each character stands: from its edge, or centred
    for (int i = 0; i < 4; ++i) {
        CCSprite* c = m_chars[i];
        bool right = i == 1 || i == 3;
        if (line.centred[i]) {
            c->setAnchorPoint(ccp(0.5f, 0));
            setPositionOnCanvas(c, 320, my);
        } else if (right) {
            c->setAnchorPoint(ccp(1, 0));
            setPositionOnCanvas(c, (kCanvasWidth - mx) - kCanvasWidth - m_offsets[i], my);
        } else {
            c->setAnchorPoint(CCPointZero);
            setPositionOnCanvas(c, mx + m_offsets[i], my);
        }
    }
    CCSpriteFrameCache* frames = CCSpriteFrameCache::sharedSpriteFrameCache();
    if (!line.bg.empty()) m_bg->setDisplayFrame(frames->spriteFrameByName(line.bg.c_str()));
    // the lit ones white, the others grey
    for (int i = 0; i < 4; ++i) {
        CCSprite* c = m_chars[i];
        if (line.chars[i].empty()) {
            c->setVisible(false);
            continue;
        }
        c->setDisplayFrame(frames->spriteFrameByName(line.chars[i].c_str()));
        bool lit = std::find(line.highlighted.begin(), line.highlighted.end(), i + 1) != line.highlighted.end();
        c->setColor(lit ? ccc3(255, 255, 255) : ccc3(127, 127, 127));
        c->setVisible(true);
    }
    ui.setText("dialogue", line.text);
    ui.setText("narrative", line.narrative);
    // a new ambience starts on its channel (behind the one playing, which it
    // doesn't replace)
    if (!line.bgm.empty() && line.bgm != m_ambience)
        SoundMgr::sharedSoundMgr()->playOnChannel(soundName(line.bgm), SoundMgr::kAmbienceChannel, true, false, false,
                                                  false);
    m_ambience = line.bgm;
    if (!line.sfx.empty()) SoundMgr::sharedSoundMgr()->playSound(soundName(line.sfx), false);
    // the camera moves over a second
    if (m_cameraAt.x != line.cameraX || m_cameraAt.y != line.cameraY) {
        m_camera->runAction(CCMoveTo::create(1.0f, ccp(line.cameraX * s, -(line.cameraY * s))));
        m_cameraAt = ccp(line.cameraX, line.cameraY);
    }
}

void DialogueScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    if (name == "ui_button_skip") {
        finish();
    } else if (name == "ui_button_next") {
        if (m_index + 1 < static_cast<int>(m_lines.size())) {
            ++m_index;
            show(m_lines[m_index]);
        } else {
            finish();
        }
    } else if (name == "ui_button_prev") {
        // (no layout has it; the original stepped back past the first line)
        if (m_index > 0) {
            --m_index;
            show(m_lines[m_index]);
        }
    }
}

void DialogueScreen::finish() {
    // (the ambience left on: the next screen sets its own, or keeps it if it
    // is the same; the original stopped it here)
    Scene* next = nullptr;
    if (m_mode == kIntro) {
        next = PreOpScreen::create(m_surgery, m_params);
    } else if (m_mode == kAreaComplete) {
        next = WorldScreen::create(m_worldArea);
    } else if (m_mode == kAreaIntro) {
        next = AreaScreen::create(m_area, 1);  // (Android 1.43, endScene 0x425558)
    } else {
        // the outro: the surgery's results; after the exile clinic's alanprobe
        // won for the first time, the bunker's preview first
        if (m_surgery.id == "surgery_exileclinic_alanprobe" && m_result.firstTime)
            next = BunkerPreviewScreen::create(m_surgery, m_params, m_result);
        else
            next = SurgeryEndScreen::create(m_surgery, m_params, m_result);
    }
    SceneMgr::sharedSceneMgr()->replaceScene(next);
}

}  // namespace mt
