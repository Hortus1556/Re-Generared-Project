#include "as3/SurgeryScreen.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#include "as3/AngledExtraction.h"
#include "as3/Animation.h"
#include "as3/Extractions.h"
#include "as3/Ghost.h"
#include "as3/ParticleInjuries.h"
#include "as3/ClampsController.h"
#include "as3/Injection.h"
#include "as3/AreasDefUtils.h"
#include "as3/BloodPacks.h"
#include "as3/BubbleEffect.h"
#include "as3/Cheats.h"
#include "as3/Content.h"
#include "as3/DialogueScreen.h"
#include "as3/EditorPlay.h"
#include "as3/GeneralTipPopupScreen.h"
#include "as3/HudEffects.h"
#include "as3/Log.h"
#include "as3/LifeWarningScreen.h"
#include "as3/ResolutionMgr.h"
#include "as3/RestoredContent.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/PartnerEffects.h"
#include "as3/PauseScreen.h"
#include "as3/PreOpScreen.h"
#include "as3/StatsMgr.h"
#include "as3/SurgeryDebugStart.h"
#include "as3/SurgeryEndScreen.h"
#include "as3/ToolEffects.h"
#include "as3/UpgradeInfoUtils.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCActionGrid.h"
#include "engine/cocos/CCClippingNode.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCDrawNode.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCMenu.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr float kCanvasWidth = 640, kCanvasHeight = 384;

// the tag partners' ids and their sheets' suffixes (0x429878, 0x429898)
const char* const kPartnerIds[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};
const char* const kPartnerSheets[8] = {"Giblets", "Saturday", "Reverand", "Flow", "Bot", "Nowe", "Crumpet", "Brutality"};

std::string partnerSheet(const std::string& id) {
    for (int i = 0; i < 8; ++i)
        if (id == kPartnerIds[i]) return kPartnerSheets[i];
    return "";
}

bool allFinished(const std::vector<InjuryState*>& list) {
    for (const InjuryState* d : list)
        if (!d->finished) return false;
    return true;
}

std::string soundName(const std::string& file) { return file.substr(0, file.find('.')); }

CCSpriteFrame* frame(const std::string& name) {
    return CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(name.c_str());
}

// the play area: the ResolutionMgr's rect (the patient's body, canvas
// points) on the screen, in screen points (0x4c0c8, 0x2ff18; the port first
// took the whole screen)
CCRect playArea() {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasWidth - static_cast<float>(r->width) / s) * 0.5f;
    const float my = (kCanvasHeight - static_cast<float>(r->height) / s) * 0.5f;
    return CCRectMake(s * (r->rect.origin.x - mx), s * (r->rect.origin.y - my), s * r->rect.size.width,
                      s * r->rect.size.height);
}

}  // namespace

SurgeryScreen* SurgeryScreen::create(const SurgeryInfo& surgery, const SurgeryParams& params) {
    SurgeryScreen* s = new SurgeryScreen;
    if (s->init(surgery, params)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

SurgeryScreen::~SurgeryScreen() {
    for (StageRun& r : m_runs) {
        for (InjuryState* i : r.injuries) delete i;
        for (InjuryState* i : r.treated) delete i;
    }
    for (auto& t : m_tools) delete t.second;
    CC_SAFE_RELEASE(m_trail);
    CC_SAFE_RELEASE(m_stageEffect);
}

bool SurgeryScreen::init(const SurgeryInfo& surgery, const SurgeryParams& params) {
    if (!CCScene::init()) return false;
    m_surgery = surgery;
    m_params = params;
    // (the original told its analytics the surgery had begun)
    return true;
}

void SurgeryScreen::load() {
    // (debug: the load tests, Android loadAssets 0x7a7d90: cheat 22 loads
    // nothing, cheat 23 only the sheets; onShown goes back to the pre-op)
    if (cheat(Cheats::kLoadTestNothing)) return log("load test: %s nothing loaded", m_surgery.id.c_str());
    SavedGame& slot = SaveData::shared().currentSlot();
    if (!slot.selectedTagPartner.empty())
        loadSheets(this, {"surgery_" + partnerSheet(slot.selectedTagPartner) + "_sheet"});
    // (0x41c1c used up the consumable items bought with money: removed)
    json::Value root;
    readContent(surgeryFile(m_surgery.id, m_params.mode, m_params.name), root);
    // (the port's: the Director's cut, RestoredContent.h; the achievements'
    // counters caught up with an older slot before this surgery adds to them)
    if (!editor::isEditedPlay(m_params)) restored::applyToSurgery(m_surgery.id, m_params.mode, root);
    StatsMgr::sharedInstance()->catchUp();
    buildLayers();
    loadTools();
    m_def = loadSurgeryDef(root);
    loadSheets(this, m_def.sheets);  // (0x75ce0)
    if (cheat(Cheats::kLoadTestSheets)) return log("load test: %s sheets loaded", m_surgery.id.c_str());
    m_ending = false;
    m_tool = nullptr;
    m_completedStages = 0;
    m_jumpToStage = 0;
    m_weightDone = 0;
    slot.allUpgrades = false;
    loadInjuryDefs(root);
    m_injuryDamageMultiplier = m_def.injuryDamageMultiplier;
    if (m_injuryDamageMultiplier == -1) m_injuryDamageMultiplier = m_surgery.area.injuryDamageMultiplier;
    buildStages();
    SoundMgr::sharedSoundMgr()->playMusic(m_music, true);
    buildTools();
    SoundMgr::sharedSoundMgr()->preloadGroup(SoundMgr::kSurgeryCommon);
    m_touch = CCPointZero;
    setPositionOnCanvas(m_bg, 320, 192);
    m_stage = 1;
    endStage(true);
    m_ending = false;
    beginStage(1);
    // (debug: AS_SURGERY's stage to start on, SurgeryDebugStart.h)
    if (int first = surgeryDebugStage(static_cast<int>(m_def.stages.size()))) {
        m_stage = first;
        beginStage(first);
    }
    // (the ambience stopped before the first stage shows, which may loop
    // its effect's sound there: 1.45 vf87 0x50468)
    SoundMgr::sharedSoundMgr()->stopChannel(SoundMgr::kAmbienceChannel, false);
    showStage();
    setupTagPartner();  // (SurgeryPartners.cpp)
    buildHud();
    startTutorial();  // (SurgeryTutorial.cpp)
    schedule(schedule_selector(SurgeryScreen::tick));
    CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, 0, true);
}

void SurgeryScreen::loadTools() {
    json::Value root;
    readJson("Data/tools.json", root);
    for (const json::Value& t : root["tools"].items) {
        int tool = toolByName(t["name"].str());
        if (tool == kNoTool) continue;
        ToolData d;
        d.cooldown = static_cast<float>(t["cool_down_timer"].integer());
        d.hasCooldown = d.cooldown > 0;
        d.wrongUsageDamage = t["wrong_usage_damage"].integer();
        d.timeBetweenWrongUsages = static_cast<float>(t["time_between_wrong_usages"].num());
        d.healthToGive = t["health_to_give"].integer();
        d.turnOnGrace = static_cast<float>(t["turn_on_grace_period"].num());
        d.afterHealGrace = static_cast<float>(t["after_heal_grace_period"].num());
        d.multipleInjuries = t["multiple_injuries_interaction"].flag();
        m_toolData[tool] = d;
    }
}

void SurgeryScreen::loadInjuryDefs(const json::Value& root) {
    // (0x32060) the surgery's sounds, its music (the first stage's), and
    // each injury class's definition once
    for (const json::Value& s : root["sounds"].items) SoundMgr::sharedSoundMgr()->preloadSound(soundName(s.str()));
    const json::Value& stages = root["stages"];
    for (size_t i = 0; i < stages.size(); ++i) {
        const json::Value& st = stages[i];
        if (i == 0) {
            m_music = soundName(st["bgm"].str());
            SoundMgr::sharedSoundMgr()->preloadSound(m_music);
        }
        for (const json::Value& inj : st["injuries"].items) {
            std::vector<std::string> names = {inj["class_name"].str()};
            for (const json::Value& e : inj["extra_injuries"].items) names.push_back(e.str());
            for (const std::string& n : names)
                if (!m_injuryDefs.count(n)) m_injuryDefs[n] = loadInjuryDef(n);
        }
    }
    // (0x32010: three injuries the screen makes itself)
    for (const char* n : {"Fire", "PassiveParticle", "SoakableParticle"})
        if (!m_injuryDefs.count(n)) m_injuryDefs[n] = loadInjuryDef(n);
}

void SurgeryScreen::buildLayers() {
    // (0x32cec)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    m_bg = CCSprite::create();
    m_bg->setContentSize(CCSizeMake(kCanvasWidth * s, kCanvasHeight * s));
    m_bgExtras = CCNode::create();
    m_injuryLayer = CCNode::create();
    m_toolLayer = CCNode::create();
    m_topLayer = CCSprite::create();
    addChild(m_bg, 0);
    addChild(m_bgExtras, 1);
    addChild(m_injuryLayer, 2);
    addChild(m_toolLayer, 250);
    addChild(m_topLayer, 350);
}

void SurgeryScreen::buildStages() {
    // every stage's injuries are made at the start (0x75f58)
    m_runs.assign(m_def.stages.size(), StageRun());
    for (size_t i = 0; i < m_def.stages.size(); ++i) {
        for (const StageInjury& si : m_def.stages[i].injuries) {
            auto d = m_injuryDefs.find(si.className);
            if (d == m_injuryDefs.end()) continue;
            InjuryState* inj = InjuryState::create(&d->second);
            inj->setup(si.position, static_cast<float>(si.rotation), false);
            inj->allowNonEssentialCreation = si.allowNonEssentialCreation;
            inj->name = si.name;
            inj->toSurgeryStage = si.toSurgeryStage;
            if (si.stageEndToSwap) inj->stageEndToSwap = si.stageEndToSwap;
            m_runs[i].injuries.push_back(inj);
        }
        // the dependencies by name, within the stage (the original warned of
        // names it could not find)
        for (size_t k = 0; k < m_def.stages[i].injuries.size() && k < m_runs[i].injuries.size(); ++k) {
            InjuryState* inj = m_runs[i].injuries[k];
            auto find = [&](const std::string& n) -> InjuryState* {
                for (InjuryState* o : m_runs[i].injuries)
                    if (o->name == n) return o;
                logError("Found dependency with unexisting injury \"%s\" in injury \"%s\"", n.c_str(),
                         inj->name.c_str());
                return nullptr;
            };
            for (const std::string& n : m_def.stages[i].injuries[k].visualDependencies)
                if (InjuryState* o = find(n)) inj->visualDependencies.push_back(o);
            for (const std::string& n : m_def.stages[i].injuries[k].interactionDependencies)
                if (InjuryState* o = find(n)) inj->interactionDependencies.push_back(o);
            inj->hasDependencies = !inj->interactionDependencies.empty();
        }
    }
}

void SurgeryScreen::buildTools() {
    // (0x4182c) one of each tool, with its tools.json settings; the
    // surgery's own tools are the ones it offers
    for (auto& d : m_toolData) {
        ToolState* t = ToolState::create(d.first);
        if (!t) continue;
        const ToolData& td = d.second;
        t->cooldown = td.cooldown;
        t->hasCooldown = td.hasCooldown;
        t->healthToGive = td.healthToGive;
        t->wrongUsageDamage = td.wrongUsageDamage;
        t->timeBetweenWrongUsages = td.timeBetweenWrongUsages;
        t->turnOnGrace = td.turnOnGrace;
        t->afterHealGrace = td.afterHealGrace;
        t->multipleInjuries = td.multipleInjuries;
        // the tazer's use upgrade: its misuse hurts 2, not tools.json's 5
        // (0x4182c; the port let the file's number win)
        if (d.first == kTazer && t->upgradeLevel > 0) t->wrongUsageDamage = 2;
        t->target = nullptr;
        t->cooldownLeft = 0;
        t->active = false;
        m_tools[d.first] = t;
        m_toolList.push_back(t);
        if (t->sprite) m_toolLayer->addChild(t->sprite, t->zOrder);
    }
}

void SurgeryScreen::buildHud() {
    // (0x3f298) the layout, in its own node over everything
    m_hud = CCNode::create();
    m_hudDef = loadUIDefinition("common/in_game_ui.json");
    buildUI(ui, m_hud, m_hudDef, this, menu_selector(SurgeryScreen::onButton));
    addChild(m_hud, 350);
    // the tools the surgery doesn't offer are left out
    for (int t = 0; t < kToolCount; ++t) {
        bool offered = std::find(m_def.tools.begin(), m_def.tools.end(), t) != m_def.tools.end();
        std::string n = kToolNames[t];
        ui.setVisible("ingame_tool_" + n, offered);
        ui.setVisible("tooleffect_" + n, false);
    }
    for (const char* n : {"ingame_injection_graph", "ingame_tool_Injection_out", "ingame_injection_bar_grey",
                          "ingame_injection_bar_green", "ingame_out_of_time_box", "ingame_out_of_time",
                          "ingame_tutorial_box", "ingame_tutorial_partner", "ingame_tutorial_ok",
                          "ingame_tutorial_pause_box", "ingame_saturday_left", "ingame_saturday_right",
                          "ingame_combometerfillWarning", "ingame_combometer_rainbowbar",
                          "ingame_combometer_nuclearGlow"})
        ui.setVisible(n, false);
    // (traced for test scripts: a sudden death challenge, as its case file
    // tells the player; the tool buttons' centres, cocos points)
    log("surgery %s%s", m_surgery.id.c_str(), m_def.isSuddenDeath ? " sudden death" : "");
    for (int t = 0; t < kToolCount; ++t)
        if (CCNode* b = ui.node(std::string("ingame_tool_") + kToolNames[t]))
            if (b->isVisible()) {
                CCRect bb = b->boundingBox();
                log("tool %s button %.0f,%.0f", kToolNames[t], bb.getMidX(), bb.getMidY());
            }
    if (CCNode* b = ui.node("ingame_pause")) {
        CCRect bb = b->boundingBox();
        log("pause button %.0f,%.0f", bb.getMidX(), bb.getMidY());
    }
    ui.setText("tutorial", "");
    ui.setText("out_of_times", "");
    hideClockWarningAtOnce();  // (0x3ab9c(1); the box's places kept)
    buildHeartMeter();
    // the screen's edges whiten as the patient dies (DyingFadeEffect)
    m_effects.dyingFade = DyingFadeEffect::create();
    m_effects.add(m_effects.dyingFade);
    // the tag partner's button (SurgeryPartners.cpp)
    buildTagButton();
    // (the port's: the tools in a bar across the top on a big screen,
    // SurgeryHud.cpp; it traces the buttons' places again)
    layoutHud(wantTopBar());
    if (m_tagButton && m_tagButton->isVisible()) {
        CCRect bb = m_tagButton->boundingBox();
        log("tag partner %s button %.0f,%.0f needs %.0f", m_partnerId.c_str(), bb.getMidX(), bb.getMidY(),
            m_tagNeeded);
    }
    refreshHud();
}

void SurgeryScreen::beginStage(int stage) {
    // (0x32b00) its music, and the first time: its heart and clock. The time
    // on the stage starts again (1.45: switchSurgeryStage 0x482a4 clears its
    // +0x1040 before setSurgeryStateForStage 0x4042c)
    m_stageTime = 0;
    const SurgeryStageDef& d = m_def.stages[stage - 1];
    if (!d.bgm.empty()) m_music = soundName(d.bgm);
    StageRun& r = m_runs[stage - 1];
    r.done = false;
    if (!r.begun) {
        if (d.health != -1) m_health = m_healthShown = static_cast<float>(d.health);
        if (d.time != -1) m_time = static_cast<float>(d.time);
    }
    r.begun = true;
    if (d.injuryDamageMultiplier != -1) m_injuryDamageMultiplier = d.injuryDamageMultiplier;
}

void SurgeryScreen::endStage(bool keepTool) {
    // (0x33d18)
    if (m_ending) return;
    if (!keepTool) {
        touchEnded();
        pauseInjuryAnimations(true);
        // (each injury's vf28: its particle systems stand still)
        for (InjuryState* i : run().injuries) i->pause();
    }
    m_ending = true;
}

void SurgeryScreen::showStage() {
    // (0x3ca50) the stage's background and injuries
    m_ending = false;
    log("stage %d", m_stage);
    m_bg->removeAllChildrenWithCleanup(true);
    m_bgExtras->removeAllChildrenWithCleanup(true);
    m_injuryLayer->removeAllChildrenWithCleanup(true);
    // The tray's organ and what the tongs hold point at an injury's pieces,
    // which its graphics remake (the original's are fixed records inside the
    // injury, rewritten in place): kept by their index across the rebuild.
    auto indexIn = [](InjuryState* inj, PieceState* p) -> int {
        if (!inj || !p) return -1;
        for (size_t k = 0; k < inj->pieces.size(); ++k)
            if (&inj->pieces[k] == p) return static_cast<int>(k);
        return -1;
    };
    Tongs* tongs = m_tool && m_tool->type == kTongs ? static_cast<Tongs*>(m_tool) : nullptr;
    const int trayIndex = indexIn(m_trayInjury, m_trayPiece);
    const int heldIndex = tongs ? indexIn(tongs->target, tongs->piece) : -1;
    // Fixed: the flag follows the stage shown. The original (iOS 1.02 and
    // Android 1.43) set it here and in 0x33244 but cleared it only as a
    // transplant was removed, so a transplant freed just before a stage jump
    // kept new injuries off in the stage jumped to (the Hospital boss's stage
    // 2 and 3 ghosts didn't split or leave frostbite until a pentagram was done).
    bool noNewInjuries = false;
    for (InjuryState* inj : run().injuries) {
        buildInjuryGraphics(inj);
        m_injuryLayer->addChild(inj->root, inj->zOrder);
        // hidden while its visual dependencies are there (a snake's hole
        // shows: Snake.cpp)
        if (!inj->visualDependencies.empty() && !allFinished(inj->visualDependencies) && !inj->shownWhileWaiting())
            inj->root->setVisible(false);
        // a transplant not waiting for others: no injury is made while it is
        // on (0x77bcc sets +0xf90; removeTreated clears it)
        if (inj->def->base == kTransplant && !inj->hasDependencies) noNewInjuries = true;
        // (traced for test scripts: where to treat it)
        inj->traceStage();
    }
    auto pieceAt =[](InjuryState* inj, int k) -> PieceState* {
        return inj && k >= 0 && k < static_cast<int>(inj->pieces.size()) ? &inj->pieces[static_cast<size_t>(k)] : nullptr;
    };
    m_noNewInjuries = noNewInjuries;
    if (m_trayPiece) m_trayPiece = pieceAt(m_trayInjury, trayIndex);
    if (tongs && tongs->piece) tongs->piece = pieceAt(tongs->target, heldIndex);
    showBackground();
    showStageEffect();
}

void SurgeryScreen::showStageEffect() {
    // (1.45 runSurgeryStageSwitch 0x4aae8) the effect the stage names: the
    // last stage's ended (setToEnd; no longer alive, the list lets it go);
    // none: the ambience channel stopped; the bubbles loop bubblesFX there
    // (another name stops it). (BubbleEffect.h: no shipped file names one)
    if (m_stageEffect) {
        m_stageEffect->setToEnd();
        CC_SAFE_RELEASE_NULL(m_stageEffect);
    }
    const std::string& name = stageDef().effect;
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (name.empty()) {
        sound->stopChannel(SoundMgr::kAmbienceChannel, false);
        return;
    }
    if (name == "BubbleEffect") sound->playOnChannel("bubblesFX", SoundMgr::kAmbienceChannel, true, false, false, false);
    else sound->stopChannel(SoundMgr::kAmbienceChannel, false);
    m_stageEffect = stageEffect(name);
    if (!m_stageEffect) return;
    m_stageEffect->retain();
    m_effects.add(m_stageEffect, true);
}

// (the port's) A background packed as one half of a symmetric picture (its
// frame trimmed to the half beside its middle, the other half shown by the
// same frame mirrored) is shown without its middle edge: the sheet's PVRTC
// blocks tint the texel columns there with what lies beside the frame in the
// sheet (two darker, two lighter by overshoot), and the mirrored pair made a
// line down the middle of the patient (fixed: hubris2's chest, bg_hubrisChest
// beside the forehead's dark edge in surgery_hubris1_sheet; the original's
// too). The rest of the half is stretched over them (4 texels in 1280,
// unseen) about the sprite's middle, which is that edge.
static void hideMiddleSeam(CCSprite* sp, CCSpriteFrame* f) {
    sp->setScaleX(1);
    const float k = 4;  // texels
    const CCRect r = f->getRectInPixels();
    const float w = r.size.width, ox = f->getOffsetInPixels().x;
    if (f->isRotated() || w <= 8 * k) return;
    const bool leftEdge = std::fabs(ox - w * 0.5f) < 0.5f, rightEdge = std::fabs(ox + w * 0.5f) < 0.5f;
    if (!leftEdge && !rightEdge) return;
    const CCRect cut = CCRectMake(leftEdge ? r.origin.x + k : r.origin.x, r.origin.y, w - k, r.size.height);
    const CCPoint offset = ccp(leftEdge ? (w - k) * 0.5f : -(w - k) * 0.5f, f->getOffsetInPixels().y);
    sp->setDisplayFrame(
        CCSpriteFrame::createWithTexture(f->getTexture(), cut, false, offset, f->getOriginalSizeInPixels()));
    sp->setScaleX(w / (w - k));
}

void SurgeryScreen::showBackground() {
    // (0x778fc) a picture, or "r,g,b" for a plain colour
    const SurgeryStageDef& d = stageDef();
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    m_bg->setScaleX(1);
    if (d.bg.find(".png") == std::string::npos) {
        // the three numbers, whatever surrounds them: the files write "40,55,58"
        // or "(255, 72, 136)" (atoi of "(255" read 0, losing the red)
        int rgb[3] = {0, 0, 0};
        int n = 0;
        for (size_t i = 0; i < d.bg.size() && n < 3;) {
            if (!std::isdigit(static_cast<unsigned char>(d.bg[i]))) {
                ++i;
                continue;
            }
            int v = 0;
            while (i < d.bg.size() && std::isdigit(static_cast<unsigned char>(d.bg[i]))) v = v * 10 + (d.bg[i++] - '0');
            rgb[n++] = std::min(v, 255);
        }
        CCLayerColor* c = CCLayerColor::create(
            ccc4(static_cast<GLubyte>(rgb[0]), static_cast<GLubyte>(rgb[1]), static_cast<GLubyte>(rgb[2]), 255),
            kCanvasWidth * s, kCanvasHeight * s);
        c->setPosition(CCPointZero);
        m_bg->addChild(c);
    } else if (CCSpriteFrame* f = frame(d.bg)) {
        m_bg->setDisplayFrame(f);
        m_bg->setFlipX(d.bgFlipX);
        m_bg->setFlipY(d.bgFlipY);
        hideMiddleSeam(m_bg, f);
    }
    int tag = 0;
    for (const AdditionalBg& a : d.additionalBgs) {
        CCSprite* sp = CCSprite::create();
        if (CCSpriteFrame* f = frame(a.filename)) {
            sp->setDisplayFrame(f);
            hideMiddleSeam(sp, f);
        }
        setPositionOnCanvas(sp, static_cast<float>(a.x), static_cast<float>(a.y));
        sp->setRotation(static_cast<float>(a.rotation));
        sp->setFlipX(a.flipX);
        sp->setFlipY(a.flipY);
        sp->setTag(tag++);
        m_bgExtras->addChild(sp);
    }
}




void SurgeryScreen::buildInjuryGraphics(InjuryState* inj) {
    // (0x77c4c) the injury's nodes (its class's own pictures)
    inj->buildGraphics();
}

void SurgeryScreen::refreshHud() {
    // (0x37850) the clock, the heart (its colour: the original's every
    // change and every frame of the death shield's grace)
    refreshHeartColour();
    int t = static_cast<int>(m_time);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d:%02d", t / 60, t % 60);
    ui.setText("combo_time", buf);
    float h = std::ceil(m_healthShown);
    std::snprintf(buf, sizeof buf, "%d", h > 0 ? static_cast<int>(h) : 0);
    ui.setText("combo_health", buf);
    ui.setVisible("ingame_comboheart", m_hearts >= 1);
    if (m_hearts >= 1) std::snprintf(buf, sizeof buf, "x%d", m_hearts);
    else buf[0] = 0;
    ui.setText("comboheart", buf);
    // the clamps still to place, beside the clamps' button (1.45 0x456e8:
    // "x<n>" in the label "clampnumber" its in_game_ui.json added; it counted
    // from 2, or 4 with the upgrade, so Crumpet's 4 showed as 2: the port
    // counts from the tool's own limit)
    {
        auto it = m_tools.find(kClamp);
        if (it != m_tools.end() && it->second) {
            const ClampsController* cc = static_cast<const ClampsController*>(it->second);
            std::snprintf(buf, sizeof buf, "x%d", cc->maxClamps - cc->count());
            if (m_clampCount != buf) {
                m_clampCount = buf;
                ui.setText("clampnumber", buf);
                placeKeyLabels();  // (the clamps' key beside it: SurgeryHud.cpp)
            }
        } else {
            ui.setText("clampnumber", "");  // (no clamps in this surgery: no button to label)
        }
    }
    // the screen's edges whiten under 15 (not in sudden death); the tag
    // partner's button (both after the combo in the original)
    if (!m_def.isSuddenDeath && m_effects.dyingFade) m_effects.dyingFade->setHealth(m_healthShown);
    refreshTagButton();
    if (!m_combo.dirty) return;
    // the combo: its multiplier, the score, the bars in the level's colour
    if (m_combo.bars == 0 && m_combo.multiplier() == 1) buf[0] = 0;
    else std::snprintf(buf, sizeof buf, "x%d", m_combo.multiplier());
    ui.setText("combometer", buf);
    ui.setText("combo_score", std::to_string(m_combo.score));
    static const char* const kColours[5] = {"bronze", "silver", "gold", "diamond", "rainbow"};
    int level = m_combo.levelOf();
    const char* colour = kColours[std::max(1, std::min(level, 5)) - 1];
    if (level == 5) {
        ui.setVisible("ingame_combometer_rainbowbar", true);
        ui.setItemImage(m_hudDef, "ingame_combometer_scorebox", "rainbow");
    } else {
        ui.setVisible("ingame_combometer_rainbowbar", false);
        for (int i = 1; i <= 9; ++i)
            ui.setItemImage(m_hudDef, "ingame_combometer_bar0" + std::to_string(i), m_combo.bars < i ? "empty" : colour);
        ui.setItemImage(m_hudDef, "ingame_combometer_scorebox",
                        level == 1 && m_combo.bars == 0 ? "empty" : colour);
    }
    m_combo.dirty = false;
}

void SurgeryScreen::selectTool(int tool) {
    // (0x3900c) not while the tools are frozen (event 17)
    if (m_toolsFrozen) return;
    auto it = m_tools.find(tool);
    if (it == m_tools.end()) return;
    ToolState* t = it->second;
    if (t == m_tool) return;
    if (m_tool) m_tool->stop();
    m_tool = t;
    log("tool %s selected", tool < kToolCount ? kToolNames[tool] : "?");  // (for test scripts)
    // its effect (0x3df88), and the cursor: the chainsaw's trail, or none
    // (0x39d08; the original's cursor slot also held what the tongs hold,
    // which it so dropped here: the port keeps the tongs' piece apart)
    m_effects.selectTool(tool, toolContext());
    if (m_trail) {
        removeChild(m_trail, true);
        m_trail->release();
        m_trail = nullptr;
    }
    if (tool == kChainsaw) {
        m_trail = ChainsawTrailEffect::create();
        m_trail->retain();
        m_trail->setVisible(false);
        addChild(m_trail, 225);
    }
}

void SurgeryScreen::onButton(CCObject* sender) {
    // (0x30d9c: the tool buttons; the pause button)
    std::string name = ui.nameOf(m_hudDef, sender);
    if (name.empty()) {
        logError("Clicked element not found on the items dictionary. This is probably a bug.");
        return;
    }
    const std::string prefix = "ingame_tool_";
    // the injection's two buttons and its menu (SurgeryTools.cpp)
    if (injectionButton(name, sender)) return;
    if (name.compare(0, prefix.size(), prefix) == 0) {
        int tool = toolByName(name.substr(prefix.size()));
        if (tool == kNoTool) return;
        // a tool turned off (a tutorial's, +0x2e68) is refused, its toggle
        // shown off again (the original: the next frame, +0x300c)
        bool enabled = m_enabledItems[name];
        if (enabled) selectTool(tool);
        // only the selected tool's toggle stays on; the ones shown off stay
        // off (0x39090)
        for (int i = 0; i < kToolCount; ++i)
            if (CCMenuItemToggle* tg = dynamic_cast<CCMenuItemToggle*>(ui.node(prefix + kToolNames[i]))) {
                if (i == tool) tg->setSelectedIndex(enabled ? 1 : 2);
                else if (tg->getSelectedIndex() != 2) tg->setSelectedIndex(0);
            }
        // (0x39090 goes through every item: the injection's out button too,
        // lit in the injection's place once the out mode was chosen)
        if (CCMenuItemToggle* tg = dynamic_cast<CCMenuItemToggle*>(ui.node("ingame_tool_Injection_out")))
            if (tg->getSelectedIndex() != 2) tg->setSelectedIndex(0);
        SoundMgr::sharedSoundMgr()->playSound(enabled ? "tool_selected" : "button_click_denied", false);
        // another tool chosen: the injection's menu shuts (0x30d9c)
        if (m_injectionMenu) hideInjectionMenu();
    } else if (name == "ingame_tagpartner") {
        // the tag partner's power, when charged (SurgeryPartners.cpp)
        if (m_enabledItems["ingame_tagpartner"] && !m_ending && !m_over) {
            SoundMgr::sharedSoundMgr()->playSound("button_click", false);
            tagPressed();
        } else {
            SoundMgr::sharedSoundMgr()->playSound("button_click_denied", false);
            if (m_tagButton) m_tagButton->setSelectedIndex(2);
        }
    } else if (name == "ingame_tutorial_ok") {
        tutorialOk();  // (SurgeryTutorial.cpp)
    } else if (name == "ingame_pause") {
        SoundMgr::sharedSoundMgr()->playSound("button_click", false);
        pause();
    } else {
        logError("Unknown button!: %s", name.c_str());
    }
}

// ---- touches (queued, run in the update: 0x30cdc) ----

bool SurgeryScreen::ccTouchBegan(CCTouch* touch, CCEvent*) {
    if (m_touches.size() < 10) m_touches.push_back({kBegan, touch->getLocation()});
    return true;
}

void SurgeryScreen::ccTouchMoved(CCTouch* touch, CCEvent*) {
    if (m_touches.size() < 10) m_touches.push_back({kMoved, touch->getLocation()});
}

void SurgeryScreen::ccTouchEnded(CCTouch* touch, CCEvent*) {
    if (m_touches.size() < 10) m_touches.push_back({kEnded, touch->getLocation()});
}

void SurgeryScreen::processTouches() {
    for (const QueuedTouch& t : m_touches) {
        if (t.kind == kMoved) {
            m_touch = ccpAdd(t.point, m_touchShift);
            touchMoved();
        } else if (t.kind == kEnded) {
            touchEnded();
        } else {
            m_touchShift = CCPointZero;
            m_touch = t.point;
            touchBegan();
            m_reachPending = tongsReachWanted();
        }
    }
    m_touches.clear();
    m_touchMoved = false;
}

void SurgeryScreen::touchBegan() {
    // (0x2fc38) the tag partner's cursor first (the Reverend's light); in
    // his power the touch may pick the tool itself (reverendPicksTool)
    m_touchDown = true;
    m_effects.partnerTouchBegan(m_touch);
    // (debug: cheats 5 and 6, a press wins or loses the surgery, 1.45
    // processOnPress 0x3d5c0; cheat 10 the Reverend's power for ever: Cheats.h)
    if (cheat(Cheats::kPressWins) && !m_ending) return win();
    if (cheat(Cheats::kPressLoses) && !m_ending) return lose();
    if ((m_reverendTimer > 0 || cheat(Cheats::kReverendForever)) && !m_ending && reverendPicksTool()) return;
    // (not while the tools are frozen, +0xfb0: the tool stays selected)
    if (!m_tool || m_ending || m_toolsFrozen) return;
    // the tool at the touch (0x34434), its grace, then pressed
    placeTool();
    m_toolGrace = m_tool->turnOnGrace;
    m_tool->begin(m_touch);
    m_showTrail = true;
    // the tongs on a transplant's new organ on the tray take it
    if (m_tool->type == kTongs && m_trayShown && m_trayPiece) takeTrayOrgan();
    if (m_tool->active) m_effects.toolTouchBegan(m_touch);
}

void SurgeryScreen::touchMoved() {
    // (0x30c58) the partner's cursor, then the tool's effect
    m_effects.partnerTouchMoved(m_touch);
    if (!m_tool || !m_tool->active) return;
    m_touchMoved = true;
    m_effects.toolTouchMoved(m_touch);
    // (the tool follows the touch in the frame: toolsFrame)
    m_tool->moved(m_touch);
}

void SurgeryScreen::touchEnded() {
    // (0x2ff18) the partner's cursor
    // first; the tool's effect last
    m_touchDown = false;
    m_effects.partnerTouchEnded(m_touch);
    if (!m_tool) return;
    bool holding = m_tool->type == kTongs && static_cast<Tongs*>(m_tool)->holdingPiece;
    // (the original took the held injury before letting go: stop() forgets it)
    InjuryState* held = m_tool->target;
    const bool wasActive = m_tool->active;
    m_tool->end(m_touch);
    // (for test scripts: the hit points the touch treated)
    for (InjuryState* i : run().injuries)
        if (!i->finished) i->traceIfWorn();
    // the injuries are told the tool is down (0x34cbc: their vf23), and its effect
    if (wasActive && !m_tool->active) {
        std::vector<InjuryState*> list = run().injuries;
        for (InjuryState* i : list)
            if (!i->finished && !i->hasDependencies) i->toolStopped(m_tool->type);
        m_effects.toolStopped();
    }
    if (m_trail) m_trail->setVisible(false);
    m_effects.toolTouchEnded(m_touch);
    if (holding) {
        releasePiece(held);
    } else if (m_tool->type == kTongs && !m_trayShown) {
        // (0x2ff18) an object let go of before it was taken: the sound; one
        // being pulled out slips back, a surface one goes back in place, and
        // the tongs hurt. (The port first slipped a pulled one back without
        // the sound or the hurt.)
        for (InjuryState* i : run().injuries) {
            if (i->def->base == kAngledExtraction && static_cast<AngledExtraction*>(i)->holding) {
                SoundMgr::sharedSoundMgr()->playSound("organ_placement", false);
                static_cast<AngledExtraction*>(i)->slipBack();
                misuse(TagEffect::kMiss);
            }
            if (i->def->base == kSurfaceExtraction && static_cast<SurfaceExtraction*>(i)->holding) {
                SurfaceExtraction* se = static_cast<SurfaceExtraction*>(i);
                SoundMgr::sharedSoundMgr()->playSound("organ_placement", false);
                if (!se->sprites.empty()) se->sprites[0]->setPosition(se->position);
                se->holding = false;
                se->perfect = false;
                misuse(TagEffect::kMiss);
            }
        }
    }
}

// ---- the update ----

void SurgeryScreen::tick(float dt) {
    // (0x3b2ec)
    hudFrame();  // (the port's: the HUD's layout follows the screen, SurgeryHud.cpp)
    // (the port's) the last stage's background faded out: gone
    if (m_bgGhost && (m_bgGhostLeft -= dt) <= 0) dropBackgroundGhost();
    endWobbles();
    if (m_over) return;
    // paused (the tag partner coming in, a tutorial's popup): only the high
    // five runs, its end going on (EffectList::updateHighFive)
    if (m_ending) {
        m_effects.updateHighFive(dt, this);
        return;
    }
    // the time spent on the stage, while it runs (1.45 0x49348: +0x1040, the
    // SurgeryState's +0x70c, for TimeElapsedOnStageTC)
    m_stageTime += dt;
    updatePartnerPowers(dt);  // (SurgeryPartners.cpp)
    // the death shield's grace; a patient alive at its end counts as revived
    // (the revive's, +0x2d68, went with the revive: removed)
    if (m_deathGrace > 0) {
        m_deathGrace -= dt;
        if (m_deathGrace <= 0 && m_health > 0)  // (1.45 runLogic 0x49348: stat 14)
            StatsMgr::sharedInstance()->incrementStat(StatTypes::kRevivePatient);
    }
    // the clock; it stands still while Bot's power runs. Passing 61, 31 and
    // 11 seconds left it says so (0x3a65c)
    // (debug: cheat 9 stops it too, 1.45 0x49348)
    if (m_botTimer == 0 && !cheat(Cheats::kInvincible)) {
        m_time -= dt;
        const float before = m_time + dt;
        if (m_time <= 61.0f && 61.0f < before) showClockWarning("60 Seconds Left");
        else if (m_time <= 31.0f && 31.0f < before) showClockWarning("30 Seconds Left");
        else if (m_time <= 11.0f && 11.0f < before) showClockWarning("10 Seconds Left");
    }
    // (traced for test scripts each second: the heart and clock the HUD shows)
    if (static_cast<int>(m_time) != static_cast<int>(m_time + dt)) log("heart %.1f time %.0f", m_health, m_time);
    // the shown heart catches up with the real one, 0.35 a frame (0x3b630;
    // the healing partners' sparkles stop with it)
    if (m_healthShown < m_health) {
        m_healthShown = std::min(m_health, m_healthShown + 0.35f);
        if (m_healthShown >= m_health) healthCaughtUp();
    }
    // the heart's blip under 15 (the shown heart), every 3 s at 15 down to
    // 0.15 s (0x3b634); above, the timer is kept full so the first comes at once
    if (m_healthShown < 15) {
        m_warningTimer += dt;
        float every = std::max(0.15f, m_warningEvery * (m_healthShown / 15.0f));
        if (every <= m_warningTimer) {
            m_warningTimer = 0;
            SoundMgr::sharedSoundMgr()->playSound("heartrate_blip", false);
            // the meter's warning flashes with it
            if (CCNode* w = ui.node("ingame_combometerfillWarning"))
                if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(w)) {
                    p->setOpacity(255);
                    w->runAction(CCEaseIn::create(CCFadeTo::create(0.45f, 0), 0.45f));
                }
        }
    } else {
        m_warningTimer = m_warningEvery * (m_healthShown / 15.0f);
    }
    // the clock's tick in the last 11 seconds (not while Bot holds it)
    if (m_time <= 11.0f && m_botTimer == 0) {
        if (1.0f <= m_tickTock) {
            m_tickTock = 0;
            SoundMgr::sharedSoundMgr()->playSound("tick_tock", false);
        } else {
            m_tickTock += dt;
        }
    } else {
        m_tickTock = 1;
    }
    // the touches (0x30cdc); one that paused the game ends the frame here
    processTouches();
    if (m_ending) return;
    updateInjuries(dt);
    // the tools frozen a while (event 17: 0x3a278)
    if (m_toolsFrozen) {
        m_frozenTimer -= dt;
        if (m_frozenTimer <= 0) unfreezeTools();
    }
    useTool(dt);
    // the cooldowns, the selected tool's frame (0x34470, SurgeryTools.cpp;
    // harmless in the Reverend's power)
    toolsFrame(dt);
    placeHeld();
    if (m_tool && !m_tool->events.empty()) runEvents(m_tool->events, true);
    sumInjuryDamage();
    refreshInjuries();
    drainInjuryEvents();
    removeTreated();
    effectsFrame(dt);  // (0x35c4c, SurgeryPartners.cpp)
    m_combo.update(dt);  // (0x9a974)
    runTutorial(dt);  // (0x3c238, SurgeryTutorial.cpp)
    refreshHud();
    checkStageDone();
    checkStageJump();
    checkEnd();
    // a heart back to 20 may be warned of again (+0x8f9)
    if (20.0f <= m_health) m_lifeWarningArmed = true;
    // (debug: cheat 8, the tag meter charged every frame, 1.45 0x49348;
    // the port's stops once it is full, so the trace isn't flooded)
    if (cheat(Cheats::kTagAlwaysFull) && !m_tagReady) chargeTag();
}

void SurgeryScreen::updateInjuries(float dt) {
    // (0x32e38)
    for (InjuryState* i : run().injuries)
        if (!i->finished) {
            i->update(dt);
            i->vf4(dt);
        }
}

void SurgeryScreen::useTool(float dt) {
    // (0x39624) the held tool works on what it is over; held over nothing it
    // can treat, once its grace is over, it hurts the patient (misuse)
    m_toolGrace -= dt;
    m_timer5b4 -= dt;
    // the Reverend's power: the tool follows what the finger is on
    if ((m_reverendTimer > 0 || cheat(Cheats::kReverendForever)) && m_touchDown) reverendPicksTool();
    if (!m_tool || !m_tool->active) return;
    // the scanner's scan, the clamps' own touch, the injection off the
    // injuries' areas (SurgeryTools.cpp)
    if (!toolBeforeHitTest()) return;
    HitResult r = hitInjuries(m_touch);
    // (the port's: the tongs' reach on a touch device, the touch's first frame)
    if (!r.hit && m_reachPending && tongsReachWanted()) r = tongsReach();
    m_reachPending = false;
    // the chainsaw's trail shows every frame but one it cuts what spurts (+0x1d)
    m_showTrail = !(r.hit && m_tool->type == kChainsaw && r.b1d);
    if (r.hit) {
        m_tool->useOn(r);
        if (m_tool->type == kTongs && r.piece) pickUp(r);
        else if (m_tool->type == kClamp || m_tool->type == kInjection) toolHit(r);
        // (only the tongs and the injection hold what they work on: 0x398e4)
        // (fixed: not while they carry a piece, as 0x39624 returns then; the
        // port's first version took the injury passed over as the one held,
        // so the piece let go of on the tray finished that one instead, and
        // the carried creature was left with its picture freed: a crash;
        // nor once the hit put them down (a set bone or vein: stop() cleared
        // it, and kept, it held the tongs to that injury for good)
        if (m_tool->type == kTongs && !r.piece && !static_cast<Tongs*>(m_tool)->holdingPiece && m_tool->active)
            m_tool->target = r.injury;
        if (m_tool->type == kInjection) m_tool->target = r.injury;
        return;
    }
    if (!m_tool->active) return;
    // (0x39624) an object the tongs pulled too fast and lost (+0x52c): "TOO
    // FAST!" (the port never read the flag)
    bool tooFast = false;
    if (m_tool->type == kTongs) {
        Tongs* tongs = static_cast<Tongs*>(m_tool);
        tooFast = tongs->dropped;
        tongs->dropped = false;
        // (the tongs carrying what they took do no harm away from it)
        if (tongs->holdingPiece) return;
    }
    if (m_toolGrace > 0) return;
    if (m_tool->type == kStapler && m_tool->uses != 0) return;
    // the same wrong injury tried again and again: kind 4 (the Reverend's
    // power spares the patient in misuse itself: 0x34d44)
    misuse(tooFast ? TagEffect::kTooFast : m_combo.repeatedWrong() ? TagEffect::kWrongTool : TagEffect::kMiss);
}

void SurgeryScreen::scream() {
    // (0x34e88, 1.45 playScream 0x42858) the patient's scream: one of
    // "<scream>_scream_1..4". Fixed: as many as the surgery's sounds list
    // (4 if none): the whale's files list whale_scream_1..5, the fifth of
    // which the original never played
    if (m_def.scream.empty()) return;
    const std::string prefix = m_def.scream + "_scream_";
    int variants = 0;
    for (const std::string& s : m_def.sounds)
        if (s.compare(0, prefix.size(), prefix) == 0) variants = std::max(variants, std::atoi(s.c_str() + prefix.size()));
    SoundMgr::sharedSoundMgr()->playRandomSound(m_def.scream + "_scream", variants > 0 ? variants : 4, false);
}

void SurgeryScreen::pickUp(const HitResult& r) {
    // (0x39624, the tongs) an object pulled all the way out comes away in
    // the tongs, its sprite over the screen following the finger; the tray
    // comes up for it
    Tongs* tongs = static_cast<Tongs*>(m_tool);
    if (tongs->holdingPiece) return;
    if (r.injury->def->base == kAngledExtraction) {
        AngledExtraction* ae = static_cast<AngledExtraction*>(r.injury);
        if (ae->holding) return;  // still being pulled
        ae->takenOut = true;
    } else if (r.injury->def->base == kSurfaceExtraction) {
        SurfaceExtraction* se = static_cast<SurfaceExtraction*>(r.injury);
        if (se->holding) return;  // not off yet
        se->takenOut = true;
    }
    if (!r.piece->sprite) return;
    tongs->target = r.injury;
    tongs->piece = r.piece;
    CCSprite* sp = r.piece->sprite;
    sp->retain();
    sp->removeFromParentAndCleanup(false);
    // (kept at the injury's turn, which its clipping node gave it; the
    // others' pictures have it already)
    if (r.injury->def->base == kAngledExtraction) sp->setRotation(r.injury->rotation + sp->getRotation());
    hold(sp);
    sp->release();
    m_heldOffset = r.piece->grabOffset;
    tongs->holdingPiece = true;
    placeHeld();
    showTray();
}

void SurgeryScreen::hold(CCSprite* sprite) {
    // (0x39d08) what the tongs hold, over the injuries
    dropHeld();
    if (!sprite) return;
    m_held = sprite;
    m_held->retain();
    addChild(m_held, 225);
}

void SurgeryScreen::dropHeld() {
    // (0x3dc94)
    if (!m_held) return;
    removeChild(m_held, false);
    m_held->setVisible(true);
    m_held->release();
    m_held = nullptr;
}

void SurgeryScreen::placeHeld() {
    // (0x34c40) at the touch, by the offset it was taken at; at the tool
    // while it moves on its own (the chainsaw slipping: followTool)
    if (!m_held) return;
    CCPoint p = m_tool && m_tool->followTool ? m_tool->position : m_touch;
    m_held->setPosition(ccp(p.x + m_heldOffset.x, p.y + m_heldOffset.y));
}

void SurgeryScreen::showTray() {
    // (0x39d3c, 0x3dd60) the tray rises at the bottom left, with the tag
    // partner's hand (Ophelia's when none)
    if (m_trayShown) return;
    m_trayShown = true;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasWidth - r->width / s) * 0.5f, my = (kCanvasHeight - r->height / s) * 0.5f;
    m_tray = CCSprite::create();
    if (CCSpriteFrame* f = frame("ingame_tray.png")) m_tray->setDisplayFrame(f);
    m_tray->setAnchorPoint(ccp(1, 1));
    m_tray->setScaleX(-1);
    addChild(m_tray, 200);
    // (the surgery's partner: the slot's is cleared once its power is used)
    std::string hand = partnerSheet(m_partnerId);
    if (hand.empty()) hand = "Ophelia";
    m_trayHand = CCSprite::create();
    if (CCSpriteFrame* f = frame("pausescreen_taghand-" + hand + ".png")) m_trayHand->setDisplayFrame(f);
    m_trayHand->setScaleX(-1);
    m_trayHand->setScaleY(-1);
    // Fixed: over the HUD (350; under the effects' 400). The original put it
    // at 201, under the HUD's left tool column, which hid most of every
    // hand (its arm comes from the bottom left, under the buttons): only
    // cut-off pieces showed, Ophelia's (no partner) a strip of thumb and a
    // patch under the tongs' button. The buttons it covers aren't needed
    // while the tray is up (the tongs are held, or are what takes the new
    // organ).
    addChild(m_trayHand, 351);
    setPositionOnCanvas(m_tray, mx + 25.6f, my);
    log("tray at %.0f,%.0f", 25.6f * s, 184.32f * s);
    m_tray->runAction(CCEaseOut::create(CCMoveTo::create(0.25f, ccp(25.6f * s, 184.32f * s)), 2));
    setPositionOnCanvas(m_trayHand, mx + 38.4f, my - 103.68f);
    m_trayHand->runAction(CCEaseOut::create(CCMoveTo::create(0.25f, ccp(38.4f * s, 80.64f * s)), 2));
}

void SurgeryScreen::hideTray() {
    // (0x33f24)
    if (!m_trayShown) return;
    m_trayShown = false;
    if (m_tray) removeChild(m_tray, true);
    if (m_trayHand) removeChild(m_trayHand, true);
    m_tray = m_trayHand = nullptr;
}

void SurgeryScreen::releasePiece(InjuryState* inj) {
    // (0x2ff18, the tongs let go of what they hold) a transplant's new organ
    // goes in wherever it is let go; anything else on the tray is out (a
    // transplant's old organ piece by piece: with the last, its stage is
    // done and the new organ comes up on the tray); anywhere else it goes
    // back where it was, and the tongs hurt
    Tongs* tongs = static_cast<Tongs*>(m_tool);
    PieceState* piece = tongs->piece;
    tongs->holdingPiece = false;
    tongs->target = nullptr;
    tongs->piece = nullptr;
    if (!inj || !piece) return;
    Transplant* tp = inj->def->base == kTransplant ? static_cast<Transplant*>(inj) : nullptr;
    const size_t index = tp ? static_cast<size_t>(piece - tp->pieces.data()) : 0;
    if (tp && piece->def && piece->def->isReplacement) {
        tp->placeReplacement(index);
        if (CCSprite* sp = piece->sprite) {
            sp->retain();
            dropHeld();
            sp->setPosition(inj->position);
            sp->setRotation(inj->rotation);
            inj->spritesNode->addChild(sp, piece->def->zOrder);
            sp->release();
        }
        m_trayPiece = nullptr;
        m_trayInjury = nullptr;
        hideTray();
        inj->finishStage();
        SoundMgr::sharedSoundMgr()->playSound("organ_placement", false);
        comboPopup();  // (0x2ff18 → 0x3dbbc: placing it scores)
        return;
    }
    bool onTray = m_tray && m_tray->boundingBox().containsPoint(m_touch);
    if (onTray) {
        SoundMgr::sharedSoundMgr()->playSound("tray_hit", false);
        piece->onTray = true;
        piece->held = false;
        dropHeld();
        hideTray();
        if (tp) {
            if (index < tp->flags.size()) {
                tp->flags[index].inBody = false;
                tp->flags[index].shown = false;
            }
            // (its picture went with the tongs: forgotten, not left dangling
            // as the original's)
            inj->sprites.erase(std::remove(inj->sprites.begin(), inj->sprites.end(), piece->sprite), inj->sprites.end());
            piece->sprite = nullptr;
            if (tp->allOnTray()) {
                inj->finishStage();
                if (!inj->finished) showTrayOrgan(inj);
            }
        } else {
            // (the same for an extraction's object or a creature's picture,
            // freed by dropHeld: no pointer to it kept)
            inj->sprites.erase(std::remove(inj->sprites.begin(), inj->sprites.end(), piece->sprite), inj->sprites.end());
            piece->sprite = nullptr;
            if (!inj->finished) inj->finishStage();
        }
        // every piece put on the tray scores (0x2ff18 → 0x3dbbc; the port's
        // gave nothing)
        comboPopup();
        return;
    }
    SoundMgr::sharedSoundMgr()->playSound("organ_placement", false);
    CCSprite* sp = piece->sprite;
    if (sp) {
        sp->retain();
        dropHeld();
        if (inj->def->base == kAngledExtraction) {
            AngledExtraction* ae = static_cast<AngledExtraction*>(inj);
            sp->setRotation(static_cast<float>(ae->angleOffset));
            if (ae->clip) ae->clip->addChild(sp);
            ae->slipBack();
            ae->takenOut = false;
        } else {
            // back at its place (kept within the play area), at its z order
            if (inj->def->base == kSurfaceExtraction) static_cast<SurfaceExtraction*>(inj)->takenOut = false;
            if (tp) sp->setRotation(inj->rotation);
            const CCRect area = playArea();
            CCPoint at = inj->position;
            at.x = std::max(area.getMinX(), std::min(at.x, area.getMaxX()));
            at.y = std::max(area.getMinY(), std::min(at.y, area.getMaxY()));
            inj->spritesNode->addChild(sp, piece->def ? piece->def->zOrder : 0);
            sp->setPosition(at);
        }
        sp->release();
    }
    hideTray();
    misuse(2);
}

void SurgeryScreen::showTrayOrgan(InjuryState* inj) {
    // the tray up again with the new organ on it, slid in (0.25 s)
    Transplant* tp = static_cast<Transplant*>(inj);
    const int index = tp->replacementIndex();
    if (index < 0) return;
    showTray();
    PieceState& piece = tp->pieces[static_cast<size_t>(index)];
    CCSprite* sp = CCSprite::create();
    piece.sprite = sp;
    if (CCSpriteFrame* f = frame(piece.def->sprite)) sp->setDisplayFrame(f);
    sp->setColor(piece.def->tint);
    addChild(sp, 202);
    // (1.45 0x3d8a8, processOnRelease: its place on the tray (170, 95)
    // canvas points; it starts at (170, 10) less its active rect's middle.
    // 1.02's 0x2ff18 worked the place out from the canvas' margins: about
    // (175, 96) on this screen, starting below the bottom edge)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    const CCPoint to = ccp(s * 170.0f, s * 95.0f);
    const CCPoint middle = ccpSub(inj->position, piece.grabOffset);
    sp->setPosition(ccp(to.x - middle.x, s * 10.0f - middle.y));
    sp->runAction(CCEaseOut::create(CCMoveTo::create(0.25f, to), 1.25f));
    m_trayPiece = &piece;
    m_trayInjury = inj;
    log("tray organ at %.0f,%.0f from %.0f,%.0f", to.x, to.y, sp->getPositionX(),
        sp->getPositionY());  // (traced for test scripts)
}

bool SurgeryScreen::takeTrayOrgan() {
    // (0x2fc38) the tongs pressed on the new organ (its picture's rect)
    PieceState* piece = m_trayPiece;
    if (!piece || !piece->sprite || !m_trayInjury) return false;
    Transplant* tp = static_cast<Transplant*>(m_trayInjury);
    const size_t index = static_cast<size_t>(piece - tp->pieces.data());
    if (index < tp->flags.size() && !tp->flags[index].inBody) return false;
    CCSprite* sp = piece->sprite;
    const CCSize sz = sp->getContentSize();
    const CCPoint c = sp->getPosition();
    if (!CCRectMake(c.x - sz.width * 0.5f, c.y - sz.height * 0.5f, sz.width, sz.height).containsPoint(m_touch))
        return false;
    Tongs* tongs = static_cast<Tongs*>(m_tool);
    tongs->piece = piece;
    tongs->target = m_trayInjury;
    sp->retain();
    sp->removeFromParentAndCleanup(false);
    hold(sp);
    sp->release();
    // held by the middle of its active rect, turned as the transplant
    m_heldOffset = piece->grabOffset;
    sp->setRotation(m_trayInjury->rotation);
    tongs->holdingPiece = true;
    placeHeld();
    // taking it scores (0x2fc38 → 0x3dbbc; the port's gave nothing)
    comboPopup();
    return true;
}

void SurgeryScreen::misuse(int kind) {
    // (0x34d44) the tool's own misuse (a burn, a damage effect), its damage,
    // and every 2 seconds at most the "wrong tool" popup (with the effects)
    m_tool->misuse(m_touch);
    // (no harm in the Reverend's power; the grace only after a harm: 0x34d44)
    // (debug: none either with cheat 7 or 10, 1.45 0x42710; Cheats.h)
    if (m_tool->wrongUsageDamage > 0 && m_reverendTimer <= 0 && !cheat(Cheats::kReverendForever) &&
        !cheat(Cheats::kNoMisuseDamage)) {
        damage(static_cast<float>(m_tool->wrongUsageDamage));
        if (m_timer5b4 <= 0) {
            // its popup ("MISS!", "TOO FAST!", "WRONG TOOL"), not making room
            log("bad popup %d at %.0f,%.0f", kind, m_touch.x, m_touch.y);  // (for test scripts)
            scream();
            TagEffect* t = TagEffect::create(kind, 0, false);
            t->setPosition(ccp(m_touch.x, m_touch.y + t->getContentSize().height * 0.5f));
            m_effects.add(t, false);
            m_timer5b4 = 2.0f;
            m_combo.breakCombo();
            // a sudden death challenge: the first mistake kills (0x34d44;
            // not with cheat 9)
            if (m_def.isSuddenDeath && !cheat(Cheats::kInvincible)) {
                m_health = 0;
                m_healthShown = 0;
            }
        }
        m_toolGrace = m_tool->timeBetweenWrongUsages;
    }
}

HitResult SurgeryScreen::hitInjuries(const CCPoint& touch) {
    // (0x398e4) the first injury the tool works on
    HitResult result;
    bool wrongNoted = false;
    for (InjuryState* i : run().injuries) {
        if (i->finished || i->hasDependencies) continue;
        // (0x398e4) the tongs on something only work on it
        if (m_tool->type == kTongs && m_tool->target && m_tool->target != i) continue;
        // the clamps' and the injection's own hit tests (vf10, vf9: the
        // injection where it is stuck in, by its mode and fill)
        HitResult r;
        if (m_tool->type == kClamp) {
            r = i->clampHitTest(touch, m_tool);
        } else if (m_tool->type == kInjection) {
            const Injection* inj = static_cast<const Injection*>(m_tool);
            r = i->injectionHitTest(inj->position, inj->mode, inj->fill);
        } else {
            r = i->hitTest(touch, m_tool);
        }
        // (0x398e4) the tongs keep to what they took hold of: from the first
        // hit (+0x510) no other injury is tried until they are put down.
        // (The port set it only for a hit without a piece, so an object being
        // pulled, a piece or a creature's picture left the tongs free: moved
        // over an earlier injury in the list they took that too, two objects
        // pulled or carried at once.)
        if (r.hit && m_tool->type == kTongs) m_tool->target = r.injury ? r.injury : i;
        // a completed action scores, fills the combo and the tag partner's
        // button, with its popup ("NAILED IT" after a run of wrong tries);
        // a bonus only shows the multiplier's points (0x398e4)
        if (r.comboAction) {
            int kind = m_combo.repeatedWrong() ? TagEffect::kCombo : TagEffect::kGood;
            int points = m_combo.action();
            if (m_def.tagAllowed) chargeTag();
            showTag(kind, points, touch, true);
        } else if (r.comboBonus) {
            int kind = m_combo.repeatedWrong() ? TagEffect::kCombo : TagEffect::kPartner;
            showTag(kind, m_combo.multiplier() * 10, touch, true);
        }
        // touched but not treated: a wrong try on it, once a frame, for a
        // tool that works on one injury at a time (0x398e4 → 0x9b11c; the
        // port never noted them, so "NAILED IT" and the repeated mistake's
        // "WRONG TOOL" never showed)
        if (!r.hit && r.touched && !wrongNoted && !m_tool->multipleInjuries && !m_probing) {
            m_combo.wrongTarget(r.injury);
            wrongNoted = true;
        }
        if (r.hit) {
            // (only a useful hit spares the patient the tool's harm: the
            // chainsaw cutting round a transplant still hurts)
            if (r.useful) m_tool->usefulThisFrame = true;
            if (r.deselectTool) m_tool->stop();
            m_toolGrace = m_tool->afterHealGrace;
            if (!m_tool->multipleInjuries) return r;
            result = r;
        }
    }
    return result;
}

void SurgeryScreen::sumInjuryDamage() {
    // (0x32ea4) the injuries' damage this frame, by the surgery's multiplier
    float d = 0;
    for (InjuryState* i : run().injuries) d += i->damageNow;
    if (d > 0) damage(d * m_injuryDamageMultiplier);
}

void SurgeryScreen::refreshInjuries() {
    // (0x333b4) an injury that changed stage gets its hit areas and picture again
    for (InjuryState* i : run().injuries) {
        if (i->stageFound && !i->finished) {
            // (its picture changes by its own event)
            i->rebuildHitAreas();
            if (i->markersNode) i->buildMarkers();
            i->traceStage();
        }
        i->stageFound = false;
    }
}

void SurgeryScreen::drainInjuryEvents() {
    // (0x33464) the injuries' events; an injury ending its stage may send the
    // surgery to another stage. The list as it grows: an injury made by an
    // event here has its own events (its injury_to_create_at_start) run in
    // the same pass, as 0x33464 read the vector's end again each time (the
    // port's went over a copy, so each injury made by another came a frame
    // later: the giant worm's head was gone a frame before its remains came,
    // a blink)
    std::vector<InjuryState*>& injuries = run().injuries;
    for (size_t k = 0; k < injuries.size(); ++k) {
        InjuryState* i = injuries[k];
        // its areas on the body (event 12: 0x3435c, which takes the injury)
        for (const InjuryEvent& e : i->events)
            if (e.type == InjuryEvent::kGraphics) addGraphicsRect(e, i);
        if (!i->events.empty()) runEvents(i->events, false);
        if (i->toSurgeryStage != 0 && i->stageEndToSwap == i->m_stage - 1) {
            m_jumpToStage = i->toSurgeryStage;
            i->toSurgeryStage = 0;
        }
        i->stageFound = false;
    }
}

void SurgeryScreen::removeTreated() {
    // (0x330fc) the finished injuries leave the stage's list
    StageRun& r = run();
    bool removed = false;
    for (size_t k = 0; k < r.injuries.size();) {
        InjuryState* i = r.injuries[k];
        if (i->finished) {
            log("injury #%d gone", i->soundChannel);  // (for test scripts: however it finished)
            r.treated.push_back(i);
            // a transplant done: injuries may be made again (+0xf90, 0x330fc)
            if (i->def->base == kTransplant) m_noNewInjuries = false;
            // (0x33188: its area and its clamps go, SurgeryTools.cpp)
            injuryRemoved(i);
            if (i->def->removeOnComplete && i->root) i->root->setVisible(false);
            r.injuries.erase(r.injuries.begin() + static_cast<long>(k));
            removed = true;
        } else {
            ++k;
        }
    }
    if (removed) checkDependencies();
}

void SurgeryScreen::checkDependencies() {
    // (0x33244) an injury hidden until others are finished shows; one that
    // waits for others to be treated first can be treated (a transplant only
    // once every other injury still to treat waits too). Only the injuries
    // with dependencies are looked at (the original's lists of them): an
    // injury that holds itself back (a bursting boil) is left alone.
    StageRun& r = run();
    for (InjuryState* i : r.injuries) {
        if (i->finished) continue;
        if (!i->visualDependencies.empty() && i->root && !i->root->isVisible() && allFinished(i->visualDependencies))
            i->root->setVisible(true);
        if (i->interactionDependencies.empty() || !i->hasDependencies || !allFinished(i->interactionDependencies))
            continue;
        bool othersWait = true;
        if (i->def->base == kTransplant)
            for (InjuryState* o : r.injuries)
                if (o != i && !o->hasDependencies) othersWait = false;
        if (!othersWait) continue;
        i->hasDependencies = false;
        i->traceStage();  // (for test scripts: it can be treated now)
        // a transplant to do: no injury is made meanwhile (+0xf90, 0x33244)
        if (i->def->base == kTransplant) m_noNewInjuries = true;
        if (i->hasStage() && i->stage().showHitAreas) i->buildMarkers();
    }
}

void SurgeryScreen::checkStageDone() {
    // (0x3c990) every injury treated: the next stage, or the end
    StageRun& r = run();
    if (r.done || !r.injuries.empty()) return;
    // not while a clamp is still on: taking the last off looks again (event
    // 13) (0x3c990)
    if (ClampsController* c = clamps())
        if (c->count() > 0) return;
    r.done = true;
    // As the original (1.02 0x3c990, 1.45 checkSurgeryStageCompleted
    // 0x4aa34): +0xf7c counts every time a stage is done, and the surgery is
    // won once it reaches the number of stages. A done stage gone back to
    // has nothing left (beginStage clears its done flag, 1.45
    // setSurgeryStateForStage 0x4042c), so it is done and counted again at
    // once and passes on to its own to_surgery_stage: the surgeries whose
    // route comes back through a done stage (karlpuccino, gossamerrag,
    // alanprobe's challenges, upgradejade) end once the patient is closed
    // up, without the stage left begun (the port once counted each stage
    // once and sent the player back there, which the original never shows).
    ++m_completedStages;
    m_weightDone += stageDef().weight;
    if (m_completedStages == static_cast<int>(m_def.stages.size())) {
        win();
        return;
    }
    int next = stageDef().toSurgeryStage;
    endStage(true);
    if (next < 1 || next > static_cast<int>(m_def.stages.size())) {
        logError("SurgeryScreen: stage %d goes to a stage it doesn't have (%d)", m_stage, next);
        win();
        return;
    }
    m_stage = next;
    beginStage(next);
    crossfadeStage();
    showStage();
}

namespace {
constexpr float kStageFade = 0.4f;  // seconds

// every picture under a node faded out
void fadeOut(CCNode* n) {
    if (dynamic_cast<CCRGBAProtocol*>(n)) n->runAction(CCFadeOut::create(kStageFade));
    if (CCArray* kids = n->getChildren())
        for (unsigned i = 0; i < kids->count(); ++i) fadeOut(static_cast<CCNode*>(kids->objectAtIndex(i)));
}
}  // namespace

void SurgeryScreen::crossfadeStage() {
    // (the port's) a stage change (opened up, deeper in, closed up): the
    // stage's background left (its picture and extra pictures) stays over
    // the new one and fades out; the injuries, the tools' trails, the
    // effects and the HUD change at once as in the original, which cut the
    // background at once too
    dropBackgroundGhost();
    m_bgGhost = m_bg;
    m_bgExtrasGhost = m_bgExtras;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    m_bg = CCSprite::create();
    m_bg->setContentSize(CCSizeMake(kCanvasWidth * s, kCanvasHeight * s));
    setPositionOnCanvas(m_bg, 320, 192);
    m_bgExtras = CCNode::create();
    addChild(m_bg, 0);
    addChild(m_bgExtras, 0);  // (over the picture: it came after)
    reorderChild(m_bgGhost, 1);
    reorderChild(m_bgExtrasGhost, 1);
    fadeOut(m_bgGhost);
    fadeOut(m_bgExtrasGhost);
    m_bgGhostLeft = kStageFade;
}

void SurgeryScreen::dropBackgroundGhost() {
    for (CCNode** g : {&m_bgGhost, &m_bgExtrasGhost})
        if (*g) {
            removeChild(*g, true);
            *g = nullptr;
        }
}

void SurgeryScreen::checkStageJump() {
    // (0x3a244)
    if (m_jumpToStage != 0 && m_jumpToStage <= static_cast<int>(m_def.stages.size())) {
        endStage(true);
        m_stage = m_jumpToStage;
        beginStage(m_stage);
        crossfadeStage();
        showStage();
    }
    m_jumpToStage = 0;
}

void SurgeryScreen::checkEnd() {
    // (0x35138) out of time, or dead (unless the death shield's grace)
    if (m_over) return;
    clampForTutorial();  // (the first surgery's tutorial: not lost)
    debugGodMode();      // (debug menu: the heart and clock kept up; SurgeryDebug.cpp)
    if (m_time <= 0) {
        lose();
        return;
    }
    if (m_health > 0) return;
    if (m_deathGrace > 0) return;
    // Lady Saturday's revive (SurgeryPartners.cpp)
    if (reviveWithSaturday()) return;
    if (!m_deathShieldUsed) {
        SavedGame& slot = SaveData::shared().currentSlot();
        if (slot.owns(SavedGame::kItemUpgrade, upgradeIndex("healing_death_shield_upgrade")) || slot.allUpgrades) {
            m_deathGrace = 5;
            m_deathShieldUsed = true;
            return;
        }
    }
    lose();
}

void SurgeryScreen::damage(float amount) {
    // (0x32ef0) none in Brutality's power (+0xfc4; nor, in the original,
    // just after a revive: removed)
    if (m_brutalityTimer > 0) return;
    if (cheat(Cheats::kInvincible)) return;  // (debug: cheat 9, 1.45 0x40830)
    m_health -= amount;
    m_healthShown -= amount;
    // under 10 in a main surgery before surgery_shanksalot_earnestpickle is
    // won: the heart's warning, once until it is back to 20 (0x33ce0). (Not
    // for a heart already stopped: the original's showed under the results.)
    // 1.45 (0x40830) leaves it out while the injection is the tool held (the
    // player is healing); its other new test, the surgery not being
    // "surgery_bunker_giblets" (a name no surgery has), always passes.
    const bool injecting = m_tool && m_tool->type == kInjection;
    if (m_health < 10 && m_health > 0 && m_lifeWarningArmed && m_params.mode == 0 && !m_over && !injecting) {
        const auto& stars = SaveData::shared().currentSlot().stars;
        auto it = stars.find("surgery_shanksalot_earnestpickle");
        if (it == stars.end() || it->second[0] == 0) showLifeWarning();
    }
    // (the original left the heart below 0 when it warned: lost all the same)
    if (m_health < 0) {
        m_health = 0;
        m_healthShown = 0;
    }
}

void SurgeryScreen::showLifeWarning() {
    // (0x33ce0) frozen, the popup over the pause box
    endStage(false);
    m_lifeWarningArmed = false;
    SceneMgr::sharedSceneMgr()->pushScene(
        LifeWarningScreen::create(this, callfunc_selector(SurgeryScreen::lifeWarningClosed)), false);
    showTutorialPause();
}

void SurgeryScreen::lifeWarningClosed() {
    // (0x33dec) the game on, the pause box down
    resumeGame();
    hideTutorialPause(false);
}

void SurgeryScreen::showClockWarning(const std::string& text) {
    // (0x3a65c) the box, its picture and the text rise from 80 canvas points
    // below (1 s, elastic), stay 3 s, and go
    m_clockWarningShown = true;
    const float off = static_cast<float>(static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * 80.0f));
    ui.setText("out_of_times", text);
    CCNode* item = ui.node("ingame_out_of_time");
    CCNode* label = ui.node("out_of_times");
    CCNode* box = ui.node("ingame_out_of_time_box");
    if (label) label->setVisible(true);
    if (item) {
        item->stopAllActions();
        item->setPosition(ccp(m_clockItemAt.x, m_clockItemAt.y - off));
        item->setVisible(true);
        item->runAction(CCSequence::create(
            CCEaseElasticOut::create(CCMoveTo::create(1.0f, m_clockItemAt), 0.6f), CCDelayTime::create(3.0f),
            CCCallFunc::create(this, callfunc_selector(SurgeryScreen::hideClockWarning)), nullptr));
    }
    if (label) {
        label->stopAllActions();
        label->setPosition(ccp(m_clockTextAt.x, m_clockTextAt.y - off));
        label->runAction(CCEaseElasticOut::create(CCMoveTo::create(1.0f, m_clockTextAt), 0.6f));
    }
    if (box) {
        box->stopAllActions();
        // (from under the tutorial's box: the same place)
        box->setPosition(ccp(m_tutorialBoxAt.x, m_clockBoxAt.y - off));
        box->setVisible(true);
        box->runAction(CCEaseElasticOut::create(CCMoveTo::create(1.0f, m_clockBoxAt), 0.6f));
    }
    // the tutorial's text, in the same place, hidden 0.1 s on (0x3b06c)
    runAction(CCSequence::create(CCDelayTime::create(0.1f),
                                 CCCallFunc::create(this, callfunc_selector(SurgeryScreen::hideTutorialText)), nullptr));
    log("clock warning: %s", text.c_str());  // (for test scripts)
}

void SurgeryScreen::hideClockWarning() {
    // (0x3ab9c) down 80 canvas points (0.35 s, elastic in), the text at once;
    // the tutorial's text back 0.35 s on (0x3b1ac)
    const float off = static_cast<float>(static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * 80.0f));
    if (CCNode* item = ui.node("ingame_out_of_time")) {
        item->setPosition(m_clockItemAt);
        item->runAction(CCEaseElasticIn::create(
            CCMoveTo::create(0.35f, ccp(m_clockItemAt.x, m_clockItemAt.y - off)), 1.0f));
    }
    if (CCNode* label = ui.node("out_of_times")) {
        label->setVisible(false);
        label->setPosition(m_clockTextAt);
    }
    if (CCNode* box = ui.node("ingame_out_of_time_box")) {
        box->setPosition(m_clockBoxAt);
        box->runAction(CCEaseElasticIn::create(
            CCMoveTo::create(0.35f, ccp(m_clockBoxAt.x, m_tutorialBoxAt.y - off)), 1.0f));
    }
    m_clockWarningShown = false;
    runAction(CCSequence::create(CCDelayTime::create(0.35f),
                                 CCCallFunc::create(this, callfunc_selector(SurgeryScreen::showTutorialText)), nullptr));
}

void SurgeryScreen::hideClockWarningAtOnce() {
    // (0x3ab9c(1), from 0x3f298) the box and its picture below their place
    // (hidden too in the port till the first warning), the text hidden
    const float off = static_cast<float>(static_cast<int>(ResolutionMgr::sharedResolutionMgr()->canvasScale * 80.0f));
    CCNode* item = ui.node("ingame_out_of_time");
    CCNode* label = ui.node("out_of_times");
    CCNode* box = ui.node("ingame_out_of_time_box");
    if (item) m_clockItemAt = item->getPosition();
    if (label) m_clockTextAt = label->getPosition();
    if (box) m_clockBoxAt = box->getPosition();
    if (item) item->setPosition(ccp(m_clockItemAt.x, m_clockItemAt.y - off));
    if (label) label->setPosition(ccp(m_clockTextAt.x, m_clockTextAt.y - off));
    if (box) box->setPosition(ccp(m_clockBoxAt.x, m_clockBoxAt.y - off));
    if (label) label->setVisible(false);
    m_clockWarningShown = false;
}

void SurgeryScreen::hideTutorialText() {
    if (CCNode* t = ui.node("tutorial")) t->setVisible(false);
}

void SurgeryScreen::showTutorialText() {
    if (CCNode* t = ui.node("tutorial")) t->setVisible(true);
}

void SurgeryScreen::heal(float amount) {
    // (0x3a188)
    m_health += amount;
    m_healthShown += amount;
}

void SurgeryScreen::runEvents(std::vector<InjuryEvent>& events, bool fromTool) {
    // (0x4c0c8 for an injury's queue, 0x4b294 for a tool's: the same events)
    std::vector<InjuryEvent> list;
    list.swap(events);
    for (const InjuryEvent& e : list) {
        switch (e.type) {
        case InjuryEvent::kAddTime:
            m_time += e.value;
            break;
        case InjuryEvent::kHeal:
            heal(e.value);
            break;
        case InjuryEvent::kCreateInjury: {
            InjuryState* inj = createInjury(e, fromTool);
            // the chainsaw's shake takes it in (0x64c24)
            if (inj && m_tool && m_tool->type == kChainsaw)
                if (auto* shake = dynamic_cast<ChainsawShakeEffect*>(m_effects.toolEffect())) shake->addGhosts(inj->root);
            break;
        }
        case InjuryEvent::kEffect:
            // (a tool's damage flash not in the Reverend's power: 0x4b294)
            // (or with cheat 10 on, 1.45 0x5cc9c: Cheats.h)
            if (e.effect == 0 && fromTool && (m_reverendTimer > 0 || cheat(Cheats::kReverendForever))) break;
            m_effects.eventEffect(e.effect, ccp(e.x, e.y), &ui);
            break;
        case InjuryEvent::kTagPopup:
            if (e.kind < 2) {
                comboPopup();
            } else if (e.kind == TagEffect::kPartner) {
                showTag(TagEffect::kPartner, m_combo.multiplier() * 10, m_touch, true);
            } else {
                // (0x4c0c8 case 11) a bad popup: the combo broken, a sudden
                // death challenge lost, the misuse popup held back 3 s
                // (0x652ac); not making room, never doubled
                m_combo.breakCombo();
                log("bad popup %d at %.0f,%.0f", e.kind, e.useTouch ? m_touch.x : e.x, e.useTouch ? m_touch.y : e.y);  // (for test scripts)
                if (m_def.isSuddenDeath && !cheat(Cheats::kInvincible)) {  // (not with cheat 9: 0x5cc9c)
                    m_health = 0;
                    m_healthShown = 0;
                }
                TagEffect* t = TagEffect::create(e.kind, 0, false);
                m_timer5b4 = 3.0f;
                const CCPoint at = e.useTouch ? m_touch : ccp(e.x, e.y);
                t->setPosition(ccp(at.x, at.y + t->getContentSize().height * 0.5f));
                m_effects.add(t, false);
            }
            break;
        case InjuryEvent::kSetFrame:
            if (e.sprite)
                if (CCSpriteFrame* f = frame(e.frame)) e.sprite->setDisplayFrame(f);
            // the chainsaw's shake's copy follows (0x64b74)
            if (e.sprite && m_tool && m_tool->type == kChainsaw)
                if (auto* shake = dynamic_cast<ChainsawShakeEffect*>(m_effects.toolEffect()))
                    shake->refreshGhost(e.sprite);
            break;
        case InjuryEvent::kHealTimes25:
            heal(e.value * 25);
            break;
        case InjuryEvent::kHealOne:
            heal(1);
            break;
        case InjuryEvent::kCombo:
            m_combo.breakCombo();
            break;
        case InjuryEvent::kEvent19:
            scream();
            break;
        case InjuryEvent::kEvent13:
            // (a clamp taken off: the stage looked at again, 0x3c990)
            checkStageDone();
            break;
        case InjuryEvent::kClamps:
            // every clamp off (an injury done with its clamps: 0x6a290)
            if (ClampsController* c = clamps()) c->releaseAll();
            break;
        case InjuryEvent::kEvent17:
            // the tools frozen for a while (0x33f68)
            freezeTools(e.f58);
            break;
        case InjuryEvent::kEvent8:
            // the tool let go of (0x33ea4: a syringe that drew all out)
            deselectTool();
            break;
        case InjuryEvent::kLiquid:
            // (0x4c0c8 case 15) the layers wobble for +0x58 seconds, or that
            // much longer. (The original gave the background 2 s whatever
            // the event asked: all three take the event's time.)
            wobble(e.f58, 5, 4, 4, true);
            break;
        case InjuryEvent::kEvent18:
            // (0x42a60) a short wobble
            wobble(0.1f, 6, 5, 5, false);
            break;
        case InjuryEvent::kDamage:
            // (a tool's harms nothing in the Reverend's power: 0x4b294)
            if (fromTool && (m_reverendTimer > 0 || cheat(Cheats::kReverendForever))) break;
            damage(static_cast<float>(e.damage));
            break;
        default:
            // (the effects, combo, tag popups, graphics queue, clamps, liquid:
            // being ported)
            break;
        }
    }
    (void)fromTool;
}

InjuryState* SurgeryScreen::createInjury(const InjuryEvent& e, bool fromTool) {
    // (event 2: 0x4c0c8 / 0x4b294) a new injury on the current stage. Not
    // while a transplant is being done (+0xf90). A PassiveParticle not past
    // 40 on the stage (unless it doesn't count): 1.45 (0x5db1c / 0x5cc9c)
    // kept the cap for those alone, 1.02 for every injury made
    StageRun& r = run();
    if ((e.name == "PassiveParticle" && r.injuries.size() > 40 && e.countsTowardCap) || m_noNewInjuries)
        return nullptr;
    // a tenth fire hurts instead
    if (e.name == "Fire") {
        int fires = 0;
        for (InjuryState* i : r.injuries)
            if (i->def->name == "Fire") ++fires;
        if (fires > 9) {
            damage(1);
            return nullptr;
        }
    }
    // a tool makes none in the Reverend's power
    if (fromTool && (m_reverendTimer > 0 || cheat(Cheats::kReverendForever))) return nullptr;
    // within the play area, its edges out
    const CCRect area = playArea();
    if (!(area.getMinX() < e.x && e.x < area.getMaxX() && area.getMinY() < e.y && e.y < area.getMaxY())) {
        log("SurgeryScreen::runActionEvents - Trying to create injury in a position out of playable area bounds. "
            "Ignoring it.");
        return nullptr;
    }
    // not over an injury's area unless allowed (0x343a0: event 12's rects)
    if (!e.allowedOverLock && inGraphicsRect(ccp(e.x, e.y), false)) {
        log("SurgeryScreen::runActionEvents - Trying to create injury in a position over locked area. Ignoring it.");
        return nullptr;
    }
    auto d = m_injuryDefs.find(e.name);
    if (d == m_injuryDefs.end()) d = m_injuryDefs.emplace(e.name, loadInjuryDef(e.name)).first;
    InjuryState* inj = InjuryState::create(&d->second);
    // (0x75ee0: +8 makes it send its graphics event)
    inj->setup(ccp(e.x, e.y), e.rotation, e.flag8);
    r.injuries.push_back(inj);
    // made by a tool (the lighter's burn): treating it is only a bonus,
    // not an action that scores (+0xfda4; the port marked every injury made
    // by an event so, and an incision's cut scored nothing)
    inj->createdLater = fromTool;
    inj->traceStage();
    // the picture it was asked to show (+0x18: a particle's landing); a
    // soakable drop what it makes and its time (+0x60, +0x3c: 0x19f8bc)
    inj->graphicFrame = e.frame;
    if (inj->def->base == kSoakableParticle) {
        SoakableParticle* sp = static_cast<SoakableParticle*>(inj);
        sp->makes = e.names;
        sp->setSoakTime(e.delay);
    }
    buildInjuryGraphics(inj);
    m_injuryLayer->addChild(inj->root, inj->zOrder);
    // (0x4c0c8 case 2, after its graphics) it waits for the injury that made
    // it (+0x70: an embedded object's wound, an incision's cut): no tool
    // works on it until that one is finished and gone (checkDependencies,
    // the stage's +0x9bc list); hidden till then (+0x74). (The port ignored
    // both: a wound could be closed with the object still in it.) Not for
    // one already finished, which nothing would free it from again.
    if (e.interactionDependency && !e.interactionDependency->finished) {
        inj->interactionDependencies.push_back(e.interactionDependency);
        inj->hasDependencies = true;
    }
    if (e.visualDependency && !e.visualDependency->finished) {
        inj->visualDependencies.push_back(e.visualDependency);
        inj->hasDependencies = true;
        inj->root->setVisible(false);
    }
    if (inj->hasDependencies) inj->traceStage();  // (for test scripts: it waits)
    // (Android 1.43, SurgeryScreen::runActionEvents<20> 0x7ac3f0, at
    // 0x7acfb4) a ghost split off another comes with its lives (+0x28),
    // hidden as the other is
    if (e.name == "Ghost" && e.value != 0)
        if (Ghost* g = dynamic_cast<Ghost*>(inj)) g->setLivesAndHide(e.value);
    return inj;
}

void SurgeryScreen::win() {
    // (0x36ec4) the success sound and effect, then the results 2.75 s later
    if (m_over) return;
    m_over = true;
    endStage(false);
    m_effects.removeType(kEffectToolsSheen);  // (the port's: Crumpet's boost ends, its end sound)
    SoundMgr::sharedSoundMgr()->playSound("Win_effect", false);
    m_effects.add(SuccessEffect::create());  // (0x3d5a8)
    runAction(CCSequence::create(CCDelayTime::create(2.75f), CCCallFunc::create(this, callfunc_selector(SurgeryScreen::finishWin)),
                                 nullptr));
}

void SurgeryScreen::lose() {
    // (0x35aa8) the flatline (or the time's up effect), then the results
    if (m_over) return;
    m_over = true;
    // (1.45 failedSurgery 0x43478: a patient lost, stat 10; not an edited level's)
    if (!editor::isEditedPlay(m_params)) StatsMgr::sharedInstance()->incrementStat(StatTypes::kKillPatient);
    endStage(false);
    m_effects.removeType(kEffectToolsSheen);  // (the port's: Crumpet's boost ends, its end sound)
    if (m_health <= 0) {
        SoundMgr::sharedSoundMgr()->playSound("heartrate_dead", false);
    } else {
        // out of time (0x3da74)
        SoundMgr::sharedSoundMgr()->playSound("OutofTime", false);
        m_effects.add(OutOfTimeEffect::create());
    }
    runAction(CCSequence::create(CCDelayTime::create(2.75f),
                                 CCCallFunc::create(this, callfunc_selector(SurgeryScreen::finishLose)), nullptr));
}

SurgeryResult SurgeryScreen::result(bool won) const {
    // (0x4f384)
    SurgeryResult r;
    r.surgery = m_surgery.id;
    r.area = m_surgery.area.areaId;
    r.timeLeft = static_cast<int>(m_time);
    r.score = m_combo.score;
    r.won = won;
    r.tutorial = m_tutorial && m_tutorial->started();
    r.partner = m_partnerId;  // (+0xf98's id: the selection is cleared when its power is used)
    r.stage = m_stage - 1;
    float total = 0;
    for (const SurgeryStageDef& st : m_def.stages) total += st.weight;
    r.progress = total > 0 ? static_cast<int>(std::ceil(m_weightDone / total * 100.0f)) : 0;
    r.partnerFlag = m_tagUsed;  // (+0xfd8)
    return r;
}

void SurgeryScreen::finishWin() {
    // (0x3cc64) the stars: the main surgery's by score and time against
    // its thresholds, a challenge's for beating its score; the coins (the
    // score's by the area's rate, 50 or 75 a new star); then the outro (the
    // main surgery) or the results (a challenge)
    SoundMgr::sharedSoundMgr()->stopAllSounds();
    log("surgery won %s score %d time %.0f", m_surgery.id.c_str(), m_combo.score, m_time);
    // (a level of the surgery editor's: nothing kept, its own results; EditorPlay.h)
    if (editor::isEditedPlay(m_params)) {
        SurgeryResult r = result(true);
        cleanUp();
        editor::playFinished(r, true);
        return;
    }
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    // (1.45 showCompletedScreen 0x4af40) after the first two surgeries (as
    // they stood before this win), the blood pack the start took comes back
    const bool packBack = GameStateUtils::BloodPacksInUse(slot);
    std::array<unsigned char, 3> stars = slot.stars[m_surgery.id];
    int gained = 0;
    int score = m_combo.score;
    if (m_params.mode == 0) {
        float used = static_cast<float>(m_def.stages.empty() ? 0 : m_def.stages[0].time) - m_time;
        int now = 1;
        if (score >= m_def.minScore[2] && used <= static_cast<float>(m_def.minTime[2])) now = 3;
        else if (score >= m_def.minScore[1] && used <= static_cast<float>(m_def.minTime[1])) now = 2;
        if (stars[0] < now) {
            gained = now - stars[0];
            // (1.45 showCompletedScreen 0x4af40: the first 3 stars, stat 0)
            if (now == 3) StatsMgr::sharedInstance()->incrementStat(StatTypes::kAchieve3StarsOnSurgery);
            stars[0] = static_cast<unsigned char>(now);
        }
        StatsMgr::sharedInstance()->incrementStat(StatTypes::kCollectYellowStar, gained);  // (0x4af40: stat 4)
        // the slot's best score for it (0x222490; its leaderboard: removed)
        unsigned int& best = slot.highScores[m_surgery.id];
        if (score > 0 && static_cast<unsigned int>(score) > best) best = static_cast<unsigned int>(score);
    } else {
        int i = m_params.mode;
        if (m_def.minScore[0] < 1 || m_def.minScore[0] < score) {
            if (stars[i] != 1) {
                gained = 1;
                stars[i] = 1;
                // (1.45 0x4af40: a challenge won the first time, stat 5 or
                // 6; the blue (stat 2) or red star (stat 3). Fixed: the
                // original asked is_challenge (+0x8c4) first, which every
                // challenge file sets, sudden death ones too (+0x8c5), so
                // stat 6 and "Unflappable" never counted: a sudden death
                // challenge counts as such, the others as bonus challenges)
                StatsMgr* stats = StatsMgr::sharedInstance();
                if (m_def.isSuddenDeath) stats->incrementStat(StatTypes::kCompleteSuddenDeathSurgery);
                else if (m_def.isChallenge) stats->incrementStat(StatTypes::kCompleteBonusChallenge);
                stats->incrementStat(i == 1 ? StatTypes::kCollectBlueStar : StatTypes::kCollectRedStar);
            }
        }
    }
    SurgeryResult r = result(true);
    r.firstTime = m_params.mode == 0 && gained > 0 && gained == stars[0];
    // the coins' rate: the area's; the bunker's last surgery takes the best of all
    float rate = m_surgery.area.pointsMultiplier;
    if (m_surgery.id == "surgery_bunker_hubris2")
        for (const AreaDef& a : AreasDefUtils::shared()->areas) rate = std::max(rate, a.pointsMultiplier);
    int shift = m_partner == 5 && m_tagUsed ? 1 : 0;  // Filthy Nowe's power doubles them
    // (1.45 0x4af40 doubled them again with the Coin Doubler owned, which
    // only its shop sold: removed)
    r.coinsFromScore = ComboMeter::coinsFor(score, rate, shift);
    r.coinsBonus = static_cast<int>(std::floor(static_cast<float>(gained * (m_params.mode ? 75 : 50)))) << shift;
    r.starsGained = gained;
    for (int i = 0; i < 3; ++i) r.stars[i] = stars[i];
    slot.stars[m_surgery.id] = stars;
    // two surgeries give a tag partner the first time: Lady Saturday, Reverend
    // (1.45 0x4af40: stats 15 and 16)
    if (m_surgery.id == "surgery_mumbojungle_deceasedkeith" && !slot.owns(SavedGame::kItemPartner, 1))
        slot.choosePartner(1, 0), r.b2c = true, StatsMgr::sharedInstance()->incrementStat(StatTypes::kUnlockLadySaturday);
    if (m_surgery.id == "surgery_robotopia_reverendsorgan" && !slot.owns(SavedGame::kItemPartner, 2))
        slot.choosePartner(2, 0), r.b2c = true, StatsMgr::sharedInstance()->incrementStat(StatTypes::kUnlockReverend);
    int coins = r.coinsFromScore + r.coinsBonus;
    if (coins) slot.addTransaction(coins, 0, 0, coins);
    slot.failures = 0;
    completeTutorial();  // (SurgeryTutorial.cpp)
    if (packBack) {
        GameStateUtils::AddBloodPacks(slot, 1);
        log("blood packs: one back for the win, %d", slot.numBloodPacks);
    }
    save.saveGame();
    cleanUp();
    if (m_params.mode == 0)
        SceneMgr::sharedSceneMgr()->replaceScene(DialogueScreen::createOutro(m_surgery, m_params, r));
    else
        SceneMgr::sharedSceneMgr()->replaceScene(SurgeryEndScreen::create(m_surgery, m_params, r));
}

void SurgeryScreen::finishLose() {
    // (0x3d6f0) the failure noted in the save, the results over the surgery
    SoundMgr::sharedSoundMgr()->stopAllSounds();
    log("surgery lost %s stage %d time %.0f heart %.0f", m_surgery.id.c_str(), m_stage, m_time, m_health);
    if (editor::isEditedPlay(m_params)) {  // (EditorPlay.h)
        SurgeryResult r = result(false);
        cleanUp();
        editor::playFinished(r, false);
        return;
    }
    SaveData& save = SaveData::shared();
    SavedGame& slot = save.currentSlot();
    ++slot.failures;
    slot.lastFailedSurgery = m_surgery.id;
    slot.lastFailedSurgeryMode = m_params.mode;
    slot.lastFailedSurgeryStage = m_stage;
    save.saveGame();
    SurgeryResult r = result(false);
    cleanUp();
    SceneMgr::sharedSceneMgr()->pushScene(SurgeryEndScreen::createLost(m_surgery, m_params, r, hooks()), false);
}

SurgeryHooks SurgeryScreen::hooks() {
    SurgeryHooks h;
    h.screen = this;
    h.cleanUp = callfunc_selector(SurgeryScreen::cleanUp);
    h.stage = m_stage;
    return h;
}

void SurgeryScreen::wobble(float duration, float grid, unsigned int waves, float amplitude, bool extend) {
    CCNode* layers[3] = {m_bg, m_bgExtras, m_injuryLayer};
    for (int tag = 0; tag < 3; ++tag) {
        CCNode* n = layers[tag];
        if (!n) continue;
        if (n->numberOfRunningActions() == 0) {
            CCLiquid* liquid = CCLiquid::create(duration, CCSizeMake(grid, grid), waves, amplitude);
            liquid->setTag(tag);
            n->runAction(liquid);
        } else if (extend) {
            if (tag == 0) m_effects.renewType(kEffectBlueScreen);
            if (CCActionInterval* a = dynamic_cast<CCActionInterval*>(n->getActionByTag(tag)))
                a->setDuration(a->getElapsed() + duration);
        }
    }
}

void SurgeryScreen::pauseInjuryAnimations(bool pause) {
    for (InjuryState* i : run().injuries)
        for (CCNode* n : i->animations) {
            if (!n) continue;
            Animation* a = dynamic_cast<Animation*>(n);
            if (pause) {
                n->pauseSchedulerAndActions();
                if (a) a->pause();
            } else {
                n->resumeSchedulerAndActions();
                if (a) a->resume();
            }
        }
}

void SurgeryScreen::buildHeartMeter() {
    // (0x3f298) the layout's circle replaced by one that takes a colour, in a
    // node of its own with the heart, its highlight and the warning over it
    CCNode* item = ui.node("ingame_combometerFillCirlce");
    if (!item || !item->getParent()) return;
    ui.setVisible("ingame_combometerFillCirlce", false);
    m_heartFill = CCSprite::createWithSpriteFrameName("ingame_combometerFillCirlce.png");
    if (!m_heartFill) return;
    // (at the circle's middle: the original added half its size to its
    // position, the layout's items being anchored at their corner)
    const CCRect box = item->boundingBox();
    m_heartFill->setAnchorPoint(ccp(0.5f, 0.5f));
    m_heartFill->setPosition(ccp(box.getMidX(), box.getMidY()));
    CCNode* meter = CCMenuItem::create();
    meter->setAnchorPoint(ccp(0.5f, 0.5f));
    meter->addChild(m_heartFill);
    item->getParent()->addChild(meter, item->getZOrder());
    for (const char* name : {"ingame_combometerFillHighlight", "ingame_combometerhealthheart", "ingame_combometerfillWarning"}) {
        CCNode* n = ui.node(name);
        if (!n) continue;
        n->retain();
        n->removeFromParentAndCleanup(false);
        meter->addChild(n);
        n->release();
    }
    if (CCRGBAProtocol* h = dynamic_cast<CCRGBAProtocol*>(ui.node("ingame_combometerFillHighlight"))) h->setOpacity(177);
    if (CCRGBAProtocol* w = dynamic_cast<CCRGBAProtocol*>(ui.node("ingame_combometerfillWarning"))) w->setOpacity(0);
    refreshHeartColour();
}

void SurgeryScreen::refreshHeartColour() {
    // (0x35034) green to red as the shown heart falls; red and yellow in
    // turn every 0.2 s of the death shield's grace
    if (!m_heartFill) return;
    const ccColor3B kRed = ccc3(199, 29, 3), kYellow = ccc3(227, 180, 36);
    ccColor3B c;
    if (m_deathGrace > 0) {
        const int steps = m_deathGrace >= 0.2f ? static_cast<int>(m_deathGrace / 0.2f) : 0;
        c = steps % 2 == 1 ? kRed : kYellow;
    } else if (m_healthShown >= 40) {
        c = ccc3(83, 192, 12);
    } else if (m_healthShown >= 30) {
        c = ccc3(157, 193, 30);
    } else if (m_healthShown >= 20) {
        c = kYellow;
    } else if (m_healthShown >= 10) {
        c = ccc3(227, 87, 33);
    } else {
        c = kRed;
    }
    m_heartFill->setColor(c);
}

void SurgeryScreen::endWobbles() {
    CCNode* layers[3] = {m_bg, m_bgExtras, m_injuryLayer};
    for (int tag = 0; tag < 3; ++tag)
        if (CCNode* n = layers[tag])
            if (CCGridBase* g = n->getGrid())
                if (g->isActive() && !n->getActionByTag(tag)) g->setActive(false);
}

void SurgeryScreen::pause() {
    // (0x3eb7c)
    if (m_over) return;
    endStage(false);
    SceneMgr::sharedSceneMgr()->pushScene(PauseScreen::create(m_surgery, hooks(), m_params), false);
}

void SurgeryScreen::onShown() {
    // (vf91 0x434c8; the ads: removed)
    Scene::onShown();
    // (debug: the load tests, cheats 21-23, go back to the pre-op screen
    // once loaded: Android onBecomeActive 0x7ab1c4; Cheats.h)
    if (cheat(Cheats::kLoadTestFull) || cheat(Cheats::kLoadTestNothing) || cheat(Cheats::kLoadTestSheets)) {
        log("load test: %s back to the pre-op", m_surgery.id.c_str());
        m_over = true;
        cleanUp();
        SceneMgr::sharedSceneMgr()->replaceScene(PreOpScreen::create(m_surgery, m_params));
        return;
    }
    // (1.45 onBecomeActive 0x51b7c) the first surgery after the first two:
    // the blood packs explained, once a slot (stat UsedBloodpack), the game
    // paused under the popup until it closes
    if (!m_over && !editor::isEditedPlay(m_params) &&
        GameStateUtils::BloodPacksInUse(SaveData::shared().currentSlot()) &&
        StatsMgr::sharedInstance()->getStatValue(StatTypes::kUsedBloodpack) < 1) {
        endStage(false);
        SceneMgr::sharedSceneMgr()->pushScene(GeneralTipPopupScreen::create("bloodpacks_first"), false);
        StatsMgr::sharedInstance()->setStatValue(StatTypes::kUsedBloodpack, 1);
        return;
    }
    if (m_effects.highFiveIndex == -1 && !m_over) resumeGame();
}

void SurgeryScreen::cleanUp() {
    // (0x3d574) no more updates or touches, no more sounds
    unschedule(schedule_selector(SurgeryScreen::tick));
    CCDirector::sharedDirector()->getTouchDispatcher()->removeDelegate(this);
    SoundMgr::sharedSoundMgr()->stopAllSounds();
}

}  // namespace mt
