#include "as3/EditorScreen.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>

#include "as3/EditorLevelsScreen.h"
#include "as3/InjuryState.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;
using namespace editor;

namespace {

const ResolutionMgr* res() { return ResolutionMgr::sharedResolutionMgr(); }
// the screen's height (points: 320 to 384 by its shape)
float screenH() { return static_cast<float>(res()->height); }
// The bars are laid out for 568 points (16:9) and shrunk to fit a narrower
// screen (512 at 4:3), their touches with them.
constexpr float kBarsWidth = 568;
float barScale() { return std::min(1.0f, static_cast<float>(res()->width) / kBarsWidth); }
float marginX() { return (kCanvasW - res()->width / res()->canvasScale) * 0.5f; }
float marginY() { return (kCanvasH - res()->height / res()->canvasScale) * 0.5f; }

// a canvas point (the files') on the screen, and back (loadSurgeryDef's sums)
CCPoint toScreen(float x, float y) {
    const float s = res()->canvasScale;
    return ccp((x - marginX()) * s, (y - marginY()) * s);
}
void toCanvas(const CCPoint& p, int& x, int& y) {
    const float s = res()->canvasScale;
    x = std::max(0, std::min(kCanvasW, static_cast<int>(std::lround(p.x / s + marginX()))));
    y = std::max(0, std::min(kCanvasH, static_cast<int>(std::lround(p.y / s + marginY()))));
}

// v turned by `deg` degrees counter-clockwise
CCPoint turned(const CCPoint& v, float deg) {
    float a = deg * static_cast<float>(M_PI) / 180.0f;
    return ccp(v.x * std::cos(a) - v.y * std::sin(a), v.x * std::sin(a) + v.y * std::cos(a));
}

int wrapDegrees(int d) {
    while (d > 180) d -= 360;
    while (d <= -180) d += 360;
    return d;
}

// "bg_mrgibletsChest.png" -> "mrgibletsChest"
std::string shortName(const std::string& frame) {
    std::string s = frame;
    if (s.size() > 4 && s.compare(s.size() - 4, 4, ".png") == 0) s.erase(s.size() - 4);
    if (s.compare(0, 3, "bg_") == 0) s.erase(0, 3);
    return s.substr(0, 26);
}

// the palette's groups, by the class's base
const char* const kGroups[5] = {"ALL", "WOUNDS", "CREATURES", "OBJECTS", "ORGANS"};
int groupOf(int base) {
    switch (base) {
    case kBat: case kMiniBat: case kFlea: case kLeech: case kSnake: case kIceAnt: case kMiniFrog: case kBlackFrog:
    case kPoisonFrog: case kFireMaggot: case kRobotScorpion: case kMiniWorm: case kGiantWorm: case kSpider:
    case kGhost:
        return 2;
    case kAngledExtraction: case kSurfaceExtraction: case kUnderSurfaceExtraction: case kSpikeBase:
        return 3;
    case kTransplant: case kBrokenBone: case kBrokenVein: case kArtery:
        return 4;
    default:
        return 1;
    }
}

// the tool buttons' pictures (surgery_common1_sheet)
const char* toolFrame(const std::string& tool) {
    static const std::map<std::string, const char*> frames = {
        {"Lighter", "ingame_lighter_button-normal.png"},   {"Cream", "ingame_gel_button-normal.png"},
        {"Stapler", "ingame_stapler_button-normal.png"},   {"Cutter", "ingame_pizzacutter_button-normal.png"},
        {"Tazer", "ingame_battery_button-normal.png"},     {"Tongs", "ingame_tongs_button-normal.png"},
        {"Chainsaw", "ingame_chainsaw_button-normal.png"}, {"Injection", "ingame_injection_button-normal.png"},
        {"Vacuum", "ingame_vacuum_button-normal.png"},     {"Clamp", "ingame_clamps_button-normal.png"},
        {"Scanner", "ingame_metaldetector_button-normal.png"}};
    auto it = frames.find(tool);
    return it == frames.end() ? "" : it->second;
}

int s_group = 0;  // the palette's group, kept between openings
int s_page[3] = {0, 0, 0};

}  // namespace

EditorScreen* EditorScreen::create() {
    EditorScreen* s = new EditorScreen;
    s->setName("EditorScreen");
    s->init();
    s->autorelease();
    return s;
}

EditorScreen::~EditorScreen() {
    clearPreviews();
    clearViews();
}

LevelStage& EditorScreen::stage() {
    Session& s = session();
    if (s.level.stages.empty()) s.level.stages.push_back(newLevel().stages[0]);
    s.stage = std::max(1, std::min(s.stage, static_cast<int>(s.level.stages.size())));
    return s.level.stages[static_cast<size_t>(s.stage - 1)];
}

void EditorScreen::load() {
    // (loadAssets 0x446f64, initLayers 0x4471a4, initUI 0x447400)
    PanelScene::load();
    SoundMgr::sharedSoundMgr()->stopChannel(SoundMgr::kAmbienceChannel, false);
    SoundMgr::sharedSoundMgr()->stopMusic(true);
    m_sheetNode = CCNode::create();
    addChild(m_sheetNode, -10);
    m_bgExtras = CCNode::create();
    m_injuryLayer = CCNode::create();
    m_overlay = CCDrawNode::create();
    addChild(m_bgExtras, 1);
    addChild(m_injuryLayer, 2);
    addChild(m_overlay, 300);
    needSheets(Catalog::shared().sheetsFor(session().level));
    buildBars();
    showStage();
    log("editor: open \"%s\" stage %d", session().level.title.c_str(), session().stage);
}

void EditorScreen::onRemoved() {
    PanelScene::onRemoved();
    clearPreviews();
    clearViews();
    std::vector<std::string> all;
    for (auto& kv : m_sheets) all.push_back(kv.first);
    for (auto& kv : m_sheets) kv.second->removeFromParentAndCleanup(true);
    m_sheets.clear();
    unloadSheets(all);
}

void EditorScreen::needSheets(const std::vector<std::string>& sheets) {
    for (const std::string& name : sheets) {
        if (m_sheets.count(name)) continue;
        CCNode* holder = CCNode::create();
        m_sheetNode->addChild(holder);
        loadSheets(holder, {name});
        m_sheets[name] = holder;
    }
}

void EditorScreen::trimSheets(const std::vector<std::string>& keep) {
    std::vector<std::string> drop;
    for (auto& kv : m_sheets)
        if (std::find(keep.begin(), keep.end(), kv.first) == keep.end()) drop.push_back(kv.first);
    for (const std::string& name : drop) {
        m_sheets[name]->removeFromParentAndCleanup(true);
        m_sheets.erase(name);
    }
    if (!drop.empty()) unloadSheets(drop);
}

const InjuryDef* EditorScreen::def(const std::string& className) {
    auto it = m_defs.find(className);
    if (it == m_defs.end()) it = m_defs.emplace(className, loadInjuryDef(className)).first;
    return &it->second;
}

void EditorScreen::changed() {
    session().dirty = true;
    refreshBars();
}

// ---- the bars -------------------------------------------------------------

void EditorScreen::buildBars() {
    m_top = Panel::create(false);
    addPanel(m_top, 400);
    // (at the screen's top, shrunk about it to fit; TEST at its right end)
    const float k = barScale(), right = static_cast<float>(res()->width) / k;
    m_top->setScale(k);
    m_top->setPosition(ccp(0, screenH() - 30 * k));
    const float y = 0;
    m_top->add(Button::create("MENU", 58, 26, Tone::Orange, [this] { openMenu(); }), 4, y);
    m_top->add(Button::create("<", 26, 26, Tone::Blue, [this] { stepStage(-1); }, 14), 66, y);
    m_stageButton = m_top->add(Button::create("STAGE 1/1", 96, 26, Tone::Blue, [this] { openStageProps(); }), 94, y);
    m_top->add(Button::create(">", 26, 26, Tone::Blue, [this] { stepStage(1); }, 14), 192, y);
    m_modeButton = m_top->add(Button::create("INJURIES", 92, 26, Tone::Purple, [this] { setMode(1 - session().mode); }),
                              222, y);
    m_top->add(Button::create("+ ADD", 70, 26, Tone::Green,
                              [this] { openPalette(session().mode == 0 ? kInjuries : kPictures, -1); }),
               318, y);
    m_top->add(Button::create("LIST", 54, 26, Tone::Blue, [this] { openInjuryList(0); }), 392, y);
    m_top->add(Button::create("TEST", 64, 26, Tone::Green, [this] { test(); }), right - 68, y);

    m_bottom = Panel::create(false);
    addPanel(m_bottom, 400);
    m_bottom->setScale(k);
    CCDrawNode* back = plate(246, 26, Tone::Dark, 7);
    back->setPosition(ccp(4, 4));
    m_bottom->addChild(back);
    m_info = text(" ", 11, 236);
    if (m_info) {
        m_info->setAnchorPoint(ccp(0, 0.5f));
        m_info->setPosition(ccp(11, 17));
        m_bottom->addChild(m_info, 2);
    }
    m_bottom->add(Button::create("<15", 40, 26, Tone::Blue, [this] { rotateSelected(-15); }), 254, 4);
    m_bottom->add(Button::create("<1", 32, 26, Tone::Blue, [this] { rotateSelected(-1); }), 296, 4);
    m_bottom->add(Button::create("1>", 32, 26, Tone::Blue, [this] { rotateSelected(1); }), 330, 4);
    m_bottom->add(Button::create("15>", 40, 26, Tone::Blue, [this] { rotateSelected(15); }), 364, 4);
    m_bottom->add(Button::create("COPY", 50, 26, Tone::Purple, [this] { duplicateSelected(); }), 406, 4);
    m_bottom->add(Button::create("EDIT", 50, 26, Tone::Orange,
                                 [this] { session().mode == 0 ? openInjuryProps() : openPictureProps(); }),
                  458, 4);
    m_bottom->add(Button::create("DEL", 50, 26, Tone::Red, [this] { deleteSelected(); }), 512, 4);
}

void EditorScreen::refreshBars() {
    Session& s = session();
    char buf[64];
    std::snprintf(buf, sizeof buf, "STAGE %d/%d", s.stage, static_cast<int>(s.level.stages.size()));
    if (m_stageButton) m_stageButton->setLabel(buf);
    if (m_modeButton) m_modeButton->setLabel(s.mode == 0 ? "INJURIES" : "PICTURES");
    bool any = false;
    std::string info = s.level.title + (s.dirty ? " *" : "");
    if (s.mode == 0 && m_selected >= 0 && m_selected < static_cast<int>(stage().injuries.size())) {
        const LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
        std::snprintf(buf, sizeof buf, "  %d,%d  %d deg", i.x, i.y, i.rotation);
        info = i.name + buf;
        any = true;
    } else if (s.mode == 1 && m_selected >= 0 && m_selected < static_cast<int>(stage().bgs.size())) {
        const LevelBg& b = stage().bgs[static_cast<size_t>(m_selected)];
        std::snprintf(buf, sizeof buf, "  %d,%d  %d deg", b.x, b.y, b.rotation);
        info = shortName(b.filename) + buf;
        any = true;
    }
    if (m_info) m_info->setString(info.c_str());
    for (Button* b : m_bottom->buttons) b->setVisible(any);
}

// ---- the stage ------------------------------------------------------------

void EditorScreen::clearViews() {
    for (InjuryState* v : m_views) {
        if (!v) continue;
        if (v->root) v->root->removeFromParentAndCleanup(true);
        delete v;
    }
    m_views.clear();
}

void EditorScreen::buildView(size_t index) {
    // (setupInjurySpritesForSurgeryState 0x756e18: each injury's own pictures)
    if (index >= stage().injuries.size()) return;
    if (m_views.size() < stage().injuries.size()) m_views.resize(stage().injuries.size(), nullptr);
    if (InjuryState* old = m_views[index]) {
        if (old->root) old->root->removeFromParentAndCleanup(true);
        delete old;
        m_views[index] = nullptr;
    }
    const LevelInjury& i = stage().injuries[index];
    needSheets(Catalog::shared().sheetsForClass(i.className));
    InjuryState* v = InjuryState::create(def(i.className));
    v->setup(toScreen(static_cast<float>(i.x), static_cast<float>(i.y)), static_cast<float>(i.rotation), false);
    v->name = i.name;
    v->buildGraphics();
    v->events.clear();  // (refreshScene 0x44bebc clears what they asked for)
    if (v->root) m_injuryLayer->addChild(v->root, v->zOrder);
    m_views[index] = v;
}

void EditorScreen::showBackground() {
    // (setupBackgroundForSurgeryState 0x756948; as SurgeryScreen::showBackground)
    const LevelStage& d = stage();
    const float s = res()->canvasScale;
    // (a new sprite each time: a colour after a picture shows no picture)
    if (m_bg) m_bg->removeFromParentAndCleanup(true);
    m_bg = CCSprite::create();
    m_bg->setContentSize(CCSizeMake(kCanvasW * s, kCanvasH * s));
    addChild(m_bg, 0);
    m_bgExtras->removeAllChildrenWithCleanup(true);
    needSheets(Catalog::shared().sheetsForBg(d.bg));
    if (d.bg.find(".png") == std::string::npos) {
        int rgb[3] = {0, 0, 0}, k = 0;
        for (size_t p = 0; p < d.bg.size() && k < 3;) {
            while (p < d.bg.size() && !std::isdigit(static_cast<unsigned char>(d.bg[p]))) ++p;
            if (p >= d.bg.size()) break;
            rgb[k++] = std::atoi(d.bg.c_str() + p);
            while (p < d.bg.size() && std::isdigit(static_cast<unsigned char>(d.bg[p]))) ++p;
        }
        CCLayerColor* c = CCLayerColor::create(ccc4(static_cast<GLubyte>(rgb[0]), static_cast<GLubyte>(rgb[1]),
                                                    static_cast<GLubyte>(rgb[2]), 255),
                                               kCanvasW * s, kCanvasH * s);
        m_bg->addChild(c);
    } else if (CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(d.bg.c_str())) {
        m_bg->setDisplayFrame(f);
        m_bg->setFlipX(d.flipX);
        m_bg->setFlipY(d.flipY);
    }
    setPositionOnCanvas(m_bg, 320, 192);
    int tag = 0;
    for (const LevelBg& a : d.bgs) {
        needSheets(Catalog::shared().sheetsForBg(a.filename));
        CCSprite* sp = CCSprite::createWithSpriteFrameName(a.filename.c_str());
        if (!sp) sp = CCSprite::create();
        setPositionOnCanvas(sp, static_cast<float>(a.x), static_cast<float>(a.y));
        sp->setRotation(static_cast<float>(a.rotation));
        sp->setFlipX(a.flipX);
        sp->setFlipY(a.flipY);
        sp->setTag(tag++);
        m_bgExtras->addChild(sp);
    }
}

void EditorScreen::showStage() {
    // (refreshScene 0x44bebc)
    clearViews();
    m_injuryLayer->removeAllChildrenWithCleanup(true);
    showBackground();
    m_views.assign(stage().injuries.size(), nullptr);
    for (size_t i = 0; i < stage().injuries.size(); ++i) buildView(i);
    // (the creatures' loops started as they were made: the editor is quiet)
    SoundMgr::sharedSoundMgr()->stopAllSounds();
    int count = session().mode == 0 ? static_cast<int>(stage().injuries.size()) : static_cast<int>(stage().bgs.size());
    if (m_selected >= count) m_selected = -1;
    refreshBars();
    refreshOverlay();
    for (const LevelInjury& i : stage().injuries)
        log("editor: injury %s %s at %d,%d rot %d", i.className.c_str(), i.name.c_str(), i.x, i.y, i.rotation);
}

void EditorScreen::refreshOverlay() {
    m_overlay->clear();
    // the play area (the original drew it with Shaders/ScreenAreas.fsh,
    // initUI 0x447400: the ResolutionMgr's rect, where the tools reach)
    const CCRect& r = res()->rect;
    CCPoint a = toScreen(r.origin.x, r.origin.y), b = toScreen(r.origin.x + r.size.width, r.origin.y + r.size.height);
    ccColor4F line = ccc4f(0.35f, 0.35f, 0.35f, 0.35f);
    CCPoint corners[4] = {a, ccp(b.x, a.y), b, ccp(a.x, b.y)};
    for (int k = 0; k < 4; ++k) {
        CCPoint p0 = corners[k], p1 = corners[(k + 1) % 4];
        float len = ccpDistance(p0, p1);
        for (float t = 0; t < len; t += 12) {
            CCPoint q0 = ccpLerp(p0, p1, t / len), q1 = ccpLerp(p0, p1, std::min(len, t + 6) / len);
            m_overlay->drawSegment(q0, q1, 0.6f, line);
        }
    }
    if (m_selected < 0) return;
    ccColor4F yellow = ccc4f(1, 0.85f, 0.1f, 1), dark = ccc4f(0.11f, 0.09f, 0.08f, 1);
    CCPoint at;
    float w = 40, h = 40, rot = 0;
    if (session().mode == 0 && m_selected < static_cast<int>(stage().injuries.size())) {
        const LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
        const InjuryDef* d = def(i.className);
        at = m_drag == kMove ? m_dragFrom : toScreen(static_cast<float>(i.x), static_cast<float>(i.y));
        w = std::max(24.0f, d->size.width);
        h = std::max(24.0f, d->size.height);
        rot = static_cast<float>(m_drag == kRotate ? m_rotation : i.rotation);
    } else if (session().mode == 1 && m_selected < static_cast<int>(stage().bgs.size())) {
        CCNode* sp = m_bgExtras->getChildByTag(m_selected);
        if (!sp) return;
        at = sp->getPosition();
        w = std::max(24.0f, sp->getContentSize().width);
        h = std::max(24.0f, sp->getContentSize().height);
        rot = sp->getRotation();
    } else {
        return;
    }
    // its rect, turned as it is (cocos turns clockwise)
    CCPoint c[4];
    const CCPoint local[4] = {ccp(-w / 2, -h / 2), ccp(w / 2, -h / 2), ccp(w / 2, h / 2), ccp(-w / 2, h / 2)};
    for (int k = 0; k < 4; ++k) c[k] = ccpAdd(at, turned(local[k], -rot));
    for (int k = 0; k < 4; ++k) {
        m_overlay->drawSegment(c[k], c[(k + 1) % 4], 1.6f, dark);
        m_overlay->drawSegment(c[k], c[(k + 1) % 4], 0.8f, yellow);
    }
    if (session().mode != 0) return;
    // the handle that turns it
    CCPoint top = ccpAdd(at, turned(ccp(0, h / 2), -rot)), handle = ccpAdd(at, turned(ccp(0, h / 2 + 20), -rot));
    m_overlay->drawSegment(top, handle, 1.2f, yellow);
    m_overlay->drawDot(handle, 8, dark);
    m_overlay->drawDot(handle, 6, yellow);
}

void EditorScreen::select(int index) {
    m_selected = index;
    refreshBars();
    refreshOverlay();
    // (traced for test scripts: where its turning handle is, screen points y down)
    if (session().mode == 0 && index >= 0 && index < static_cast<int>(stage().injuries.size())) {
        const LevelInjury& i = stage().injuries[static_cast<size_t>(index)];
        float h = std::max(24.0f, def(i.className)->size.height);
        CCPoint at = toScreen(static_cast<float>(i.x), static_cast<float>(i.y));
        CCPoint handle = ccpAdd(at, turned(ccp(0, h / 2 + 20), -static_cast<float>(i.rotation)));
        log("editor: selected %s at %.0f,%.0f handle %.0f,%.0f", i.name.c_str(), at.x, screenH() - at.y, handle.x, screenH() - handle.y);
    }
}

void EditorScreen::changeStage(int n) {
    // (changeStage 0x44be6c)
    session().stage = n;
    m_selected = -1;
    m_drag = kNoDrag;
    showStage();
}

void EditorScreen::stepStage(int by) {
    // (clickedStageButton 0x449c9c: past the last stage a new one, if this
    // one has injuries)
    Session& s = session();
    int n = s.stage + by;
    if (n < 1) return;
    if (n > static_cast<int>(s.level.stages.size())) {
        if (stage().injuries.empty()) {
            message("NEW STAGE", "Put an injury on this stage first.");
            return;
        }
        if (static_cast<int>(s.level.stages.size()) >= kMaxStages) {
            message("NEW STAGE", "A surgery has 10 stages at most.");
            return;
        }
        confirm("NEW STAGE", "Add stage " + std::to_string(s.level.stages.size() + 1) + "?", "ADD", [this] {
            createNewStage();
            changeStage(static_cast<int>(session().level.stages.size()));
        });
        return;
    }
    changeStage(n);
}

void EditorScreen::createNewStage() {
    // (createNewStage 0x44ce5c) its picture and music the stage's shown,
    // its heart and clock left as they are (-1), going on to itself; the
    // last stage, if it went on to itself, now goes on to the new one (the port's)
    Level& l = session().level;
    LevelStage n;
    n.bg = stage().bg;
    n.bgm = stage().bgm;
    n.flipX = stage().flipX;
    n.flipY = stage().flipY;
    n.bodyLevel = stage().bodyLevel;
    n.health = n.time = -1;
    const int number = static_cast<int>(l.stages.size()) + 1;
    n.toSurgeryStage = number;
    if (l.stages.back().toSurgeryStage == number - 1 || l.stages.back().toSurgeryStage == 0)
        l.stages.back().toSurgeryStage = number;
    l.stages.push_back(n);
    changed();
}

void EditorScreen::deleteStage() {
    Level& l = session().level;
    if (l.stages.size() <= 1) return;
    const int gone = session().stage;
    l.stages.erase(l.stages.begin() + (gone - 1));
    // the jumps to it go nowhere, the ones past it one lower
    auto fix = [gone](int& to) {
        if (to == gone) to = 0;
        else if (to > gone) --to;
    };
    for (LevelStage& st : l.stages) {
        fix(st.toSurgeryStage);
        for (LevelInjury& i : st.injuries) fix(i.toSurgeryStage);
    }
    if (l.stages[0].health < 1) l.stages[0].health = 100;
    if (l.stages[0].time < 10) l.stages[0].time = 300;
    session().stage = std::max(1, gone - 1);
    changed();
    changeStage(session().stage);
}

void EditorScreen::setMode(int mode) {
    // (switchToEditorMode 0x450840)
    session().mode = mode;
    m_selected = -1;
    m_drag = kNoDrag;
    refreshBars();
    refreshOverlay();
}

std::string EditorScreen::newName(const std::string& className) {
    // (generateNameForNewInjury 0x44a0ec) the class in lower case, then one
    // more than the highest number after it in any stage
    std::string base;
    for (char ch : className) base += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    base = base.substr(0, kMaxName - 4);
    int most = 0;
    for (const LevelStage& st : session().level.stages)
        for (const LevelInjury& i : st.injuries) {
            std::string n;
            for (char ch : i.name) n += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (n.compare(0, base.size(), base) != 0 || n.size() <= base.size() + 1 || n[base.size()] != '_') continue;
            most = std::max(most, std::atoi(n.c_str() + base.size() + 1));
        }
    return base + "_" + std::to_string(most + 1);
}

void EditorScreen::addInjury(const std::string& className, const CCPoint& at) {
    // (clickedToolboxButton 0x449e44: made at the screen's centre, named,
    // put last in the stage, selected)
    if (stage().injuries.size() >= static_cast<size_t>(kMaxInjuriesPerStage)) {
        message("TOO MANY", "A stage has 50 injuries at most.");
        return;
    }
    LevelInjury i;
    i.className = className;
    i.name = newName(className);
    toCanvas(at, i.x, i.y);
    stage().injuries.push_back(i);
    m_views.resize(stage().injuries.size(), nullptr);
    buildView(stage().injuries.size() - 1);
    SoundMgr::sharedSoundMgr()->stopAllSounds();
    log("editor: added %s %s at %d,%d", className.c_str(), i.name.c_str(), i.x, i.y);
    changed();
    select(static_cast<int>(stage().injuries.size()) - 1);
}

void EditorScreen::addPicture(const std::string& frame, const CCPoint& at) {
    if (stage().bgs.size() >= static_cast<size_t>(kMaxExtraBgs)) {
        message("TOO MANY", "A stage has 8 extra pictures at most.");
        return;
    }
    LevelBg b;
    b.filename = frame;
    toCanvas(at, b.x, b.y);
    stage().bgs.push_back(b);
    showBackground();
    changed();
    select(static_cast<int>(stage().bgs.size()) - 1);
}

void EditorScreen::rotateSelected(int by) {
    // (clickedRotateLeftButton 0x44a4f0: a degree a press, here 1 or 15)
    if (m_selected < 0) return;
    if (session().mode == 0) {
        LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
        i.rotation = wrapDegrees(i.rotation + by);
        buildView(static_cast<size_t>(m_selected));
    } else {
        LevelBg& b = stage().bgs[static_cast<size_t>(m_selected)];
        b.rotation = wrapDegrees(b.rotation + by);
        showBackground();
    }
    changed();
    refreshOverlay();
}

void EditorScreen::deleteSelected() {
    // (clickedDeleteButton 0x44a718) and the others' dependencies on it
    if (m_selected < 0) return;
    if (session().mode == 0) {
        std::string name = stage().injuries[static_cast<size_t>(m_selected)].name;
        stage().injuries.erase(stage().injuries.begin() + m_selected);
        for (LevelInjury& o : stage().injuries)
            for (std::vector<std::string>* deps : {&o.visualDeps, &o.interactionDeps})
                deps->erase(std::remove(deps->begin(), deps->end(), name), deps->end());
        log("editor: deleted %s", name.c_str());
    } else {
        stage().bgs.erase(stage().bgs.begin() + m_selected);
    }
    m_selected = -1;
    changed();
    showStage();
}

void EditorScreen::duplicateSelected() {
    if (m_selected < 0) return;
    if (session().mode == 0) {
        LevelInjury i = stage().injuries[static_cast<size_t>(m_selected)];
        addInjury(i.className, toScreen(static_cast<float>(i.x + 20), static_cast<float>(i.y - 20)));
        LevelInjury& n = stage().injuries.back();
        n.rotation = i.rotation;
        n.toSurgeryStage = i.toSurgeryStage;
        n.stageEndToSwap = i.stageEndToSwap;
        n.allowNonEssential = i.allowNonEssential;
        buildView(stage().injuries.size() - 1);
        refreshOverlay();
    } else {
        LevelBg b = stage().bgs[static_cast<size_t>(m_selected)];
        addPicture(b.filename, toScreen(static_cast<float>(b.x + 20), static_cast<float>(b.y - 20)));
        stage().bgs.back().rotation = b.rotation;
        showBackground();
    }
}

// ---- touches on the patient -------------------------------------------------

bool EditorScreen::canvasBegan(const CCPoint& p) {
    // (processTouchBeganEvent 0x44c584 / update 0x4498f0: the topmost
    // injury under the touch, picked and held)
    m_drag = kNoDrag;
    m_dragged = false;
    m_dragFrom = p;
    if (session().mode == 0) {
        if (m_selected >= 0 && m_selected < static_cast<int>(stage().injuries.size())) {
            const LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
            const InjuryDef* d = def(i.className);
            float h = std::max(24.0f, d->size.height);
            CCPoint at = toScreen(static_cast<float>(i.x), static_cast<float>(i.y));
            CCPoint handle = ccpAdd(at, turned(ccp(0, h / 2 + 20), -static_cast<float>(i.rotation)));
            if (ccpDistance(handle, p) < 14) {
                m_drag = kRotate;
                m_rotation = i.rotation;
                return true;
            }
        }
        for (int k = static_cast<int>(stage().injuries.size()) - 1; k >= 0; --k) {
            const LevelInjury& i = stage().injuries[static_cast<size_t>(k)];
            const InjuryDef* d = def(i.className);
            CCPoint at = toScreen(static_cast<float>(i.x), static_cast<float>(i.y));
            CCPoint local = turned(ccpSub(p, at), static_cast<float>(i.rotation));
            float w = std::max(24.0f, d->size.width), h = std::max(24.0f, d->size.height);
            if (std::fabs(local.x) <= w / 2 && std::fabs(local.y) <= h / 2) {
                select(k);
                m_drag = kMove;
                m_grab = ccpSub(at, p);
                m_dragFrom = at;
                return true;
            }
        }
    } else {
        for (int k = static_cast<int>(stage().bgs.size()) - 1; k >= 0; --k) {
            CCNode* sp = m_bgExtras->getChildByTag(k);
            if (sp && sp->boundingBox().containsPoint(p)) {
                select(k);
                m_drag = kMove;
                m_grab = ccpSub(sp->getPosition(), p);
                m_dragFrom = sp->getPosition();
                return true;
            }
        }
    }
    select(-1);
    return false;
}

void EditorScreen::canvasMoved(const CCPoint& p) {
    if (m_selected < 0) return;
    if (m_drag == kMove) {
        CCPoint at = ccpAdd(p, m_grab);
        int x, y;
        toCanvas(at, x, y);
        at = toScreen(static_cast<float>(x), static_cast<float>(y));
        if (!m_dragged && ccpDistance(at, m_dragFrom) < 2) return;
        m_dragged = true;
        if (session().mode == 0) {
            const LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
            CCPoint was = toScreen(static_cast<float>(i.x), static_cast<float>(i.y));
            if (InjuryState* v = m_views[static_cast<size_t>(m_selected)])
                if (v->root) v->root->setPosition(ccpSub(at, was));
        } else if (CCNode* sp = m_bgExtras->getChildByTag(m_selected)) {
            sp->setPosition(at);
        }
        m_dragFrom = at;
        refreshOverlay();
    } else if (m_drag == kRotate) {
        const LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
        CCPoint d = ccpSub(p, toScreen(static_cast<float>(i.x), static_cast<float>(i.y)));
        if (d.x * d.x + d.y * d.y < 25) return;
        m_rotation = wrapDegrees(static_cast<int>(std::lround(90.0 - std::atan2(d.y, d.x) * 180.0 / M_PI)));
        m_dragged = true;
        if (InjuryState* v = m_views[static_cast<size_t>(m_selected)]) v->setRotation(static_cast<float>(m_rotation));
        refreshOverlay();
    }
}

void EditorScreen::canvasEnded(const CCPoint&) {
    Drag drag = m_drag;
    m_drag = kNoDrag;
    if (!m_dragged || m_selected < 0) {
        refreshOverlay();
        return;
    }
    if (session().mode == 0) {
        LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
        if (drag == kMove) toCanvas(m_dragFrom, i.x, i.y);
        else if (drag == kRotate) i.rotation = m_rotation;
        buildView(static_cast<size_t>(m_selected));
        SoundMgr::sharedSoundMgr()->stopAllSounds();
        log("editor: placed %s at %d,%d rot %d", i.name.c_str(), i.x, i.y, i.rotation);
    } else {
        LevelBg& b = stage().bgs[static_cast<size_t>(m_selected)];
        toCanvas(m_dragFrom, b.x, b.y);
        showBackground();
    }
    changed();
    refreshOverlay();
}

// ---- the palette --------------------------------------------------------------

void EditorScreen::clearPreviews() {
    for (InjuryState* v : m_previews) {
        if (v->root) v->root->removeFromParentAndCleanup(true);
        delete v;
    }
    m_previews.clear();
}

void EditorScreen::openPalette(Palette kind, int page) {
    // (the toolbox of injury classes, refreshToolbox 0x44b114: five names a
    // page with up and down; here eight drawn as they look, in groups)
    if (popupOpen()) popPopup();
    clearPreviews();
    const Catalog& cat = Catalog::shared();
    std::vector<std::string> items;
    if (kind == kInjuries) {
        for (const Catalog::Class& c : cat.classes)
            if (s_group == 0 || groupOf(c.base) == s_group) items.push_back(c.name);
    } else {
        for (const Catalog::Bg& b : kind == kPictures ? cat.extraBgs : cat.bgs) items.push_back(b.frame);
    }
    const int perPage = 8, pages = std::max(1, (static_cast<int>(items.size()) + perPage - 1) / perPage);
    if (page < 0) page = s_page[kind];
    page = std::max(0, std::min(page, pages - 1));
    s_page[kind] = page;
    // the sheets the page draws with (the others let go first)
    std::vector<std::string> keep = cat.sheetsFor(session().level), more;
    for (int k = page * perPage; k < std::min(static_cast<int>(items.size()), (page + 1) * perPage); ++k) {
        std::vector<std::string> s = kind == kInjuries ? cat.sheetsForClass(items[static_cast<size_t>(k)])
                                                        : cat.sheetsForBg(items[static_cast<size_t>(k)]);
        for (const std::string& n : s)
            if (std::find(keep.begin(), keep.end(), n) == keep.end()) keep.push_back(n);
    }
    trimSheets(keep);
    needSheets(keep);

    const char* titles[3] = {"ADD AN INJURY", "ADD A PICTURE", "CHOOSE THE BACKGROUND"};
    Popup* p = Popup::create(titles[kind], 548, 304, kind == kInjuries ? Tone::Green : Tone::Purple);
    float y0 = 236;
    if (kind == kInjuries) {
        for (int g = 0; g < 5; ++g) {
            Button* b = p->put(Button::create(kGroups[g], 96, 24, g == s_group ? Tone::Orange : Tone::Grey, [this, g] {
                                   s_group = g;
                                   s_page[kInjuries] = 0;
                                   openPalette(kInjuries, 0);
                               }, 11),
                               14 + 104 * g, 240);
            (void)b;
        }
        y0 = 222;
    }
    const float cw = 126, ch = 86;
    for (int k = 0; k < perPage; ++k) {
        int idx = page * perPage + k;
        if (idx >= static_cast<int>(items.size())) break;
        const std::string item = items[static_cast<size_t>(idx)];
        float x = 14 + (cw + 4) * static_cast<float>(k % 4), y = y0 - (ch + 4) * static_cast<float>(k / 4 + 1);
        Button* cell = Button::create("", cw, ch, Tone::Dark, nullptr);
        CCNode* icon = CCNode::create();
        if (kind == kInjuries) {
            InjuryState* v = InjuryState::create(def(item));
            v->setup(CCPointZero, 0, false);
            v->buildGraphics();
            v->events.clear();
            m_previews.push_back(v);
            if (v->root) icon->addChild(v->root);
            const InjuryDef* d = def(item);
            float w = std::max(20.0f, d->size.width), h = std::max(20.0f, d->size.height);
            icon->setScale(std::min(1.6f, std::min((cw - 14) / w, (ch - 24) / h)));
            icon->setPosition(ccp(0, 6));
        } else if (item.find(".png") != std::string::npos) {
            if (CCSprite* sp = CCSprite::createWithSpriteFrameName(item.c_str())) {
                CCSize sz = sp->getContentSize();
                sp->setScale(std::min(1.0f, std::min((cw - 10) / std::max(1.0f, sz.width), (ch - 22) / std::max(1.0f, sz.height))));
                sp->setPosition(ccp(0, 6));
                icon->addChild(sp);
            }
        } else {
            CCLayerColor* c = CCLayerColor::create(ccc4(128, 128, 128, 255), cw - 20, ch - 28);
            int rgb[3] = {0, 0, 0}, n = 0;
            for (size_t q = 0; q < item.size() && n < 3;) {
                while (q < item.size() && !std::isdigit(static_cast<unsigned char>(item[q]))) ++q;
                if (q >= item.size()) break;
                rgb[n++] = std::atoi(item.c_str() + q);
                while (q < item.size() && std::isdigit(static_cast<unsigned char>(item[q]))) ++q;
            }
            c->setColor(ccc3(static_cast<GLubyte>(rgb[0]), static_cast<GLubyte>(rgb[1]), static_cast<GLubyte>(rgb[2])));
            c->setPosition(ccp(-(cw - 20) / 2, -(ch - 28) / 2 + 6));
            icon->addChild(c);
        }
        cell->addIcon(icon);
        if (CCLabelBMFont* l = text(kind == kInjuries ? item : shortName(item), 9, cw - 8)) {
            l->setPosition(ccp(cw / 2, 9));
            cell->addChild(l, 3);
        }
        const CCPoint centre = toScreen(320, 192);
        if (kind == kInjuries) {
            cell->onTap = [this, item, centre] {
                popPopup();
                clearPreviews();
                addInjury(item, centre);
            };
            cell->onDragOut = [this, item](const CCPoint& at) {
                // dragged out of the palette: it goes where the finger goes
                popPopup();
                clearPreviews();
                touchToCanvas();
                addInjury(item, at);
                m_drag = kMove;
                m_dragged = true;
                m_grab = CCPointZero;
                const LevelInjury& i = stage().injuries.back();
                m_dragFrom = toScreen(static_cast<float>(i.x), static_cast<float>(i.y));
            };
        } else if (kind == kPictures) {
            cell->onTap = [this, item, centre] {
                popPopup();
                addPicture(item, centre);
            };
        } else {
            cell->onTap = [this, item] {
                popPopup();
                stage().bg = item;
                changed();
                showBackground();
                openStageProps();
            };
        }
        p->put(cell, x, y);
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d / %d", page + 1, pages);
    p->put(Button::create("<", 40, 28, Tone::Blue, [this, kind, page] { openPalette(kind, page - 1); }, 14), 14, 12);
    p->putText(buf, 100, 26, 12, 60, true);
    p->put(Button::create(">", 40, 28, Tone::Blue, [this, kind, page] { openPalette(kind, page + 1); }, 14), 146, 12);
    if (kind == kInjuries) p->putText("Tap one, or drag it onto the patient", 340, 26, 11, 250, true);
    p->put(Button::create("CLOSE", 80, 28, Tone::Red, [this, kind] {
               popPopup();
               clearPreviews();
               trimSheets(Catalog::shared().sheetsFor(session().level));
               if (kind == kBackgrounds) openStageProps();
           }), 454, 12);
    pushPopup(p);
    SoundMgr::sharedSoundMgr()->stopAllSounds();
}

// ---- the property popups ------------------------------------------------------

namespace {

// a row of a property popup: its name, its value between - and + (or < >)
void row(Popup* p, float y, const std::string& name, const std::string& value, std::function<void()> less,
         std::function<void()> more, const char* l = "-", const char* m = "+") {
    p->putText(name, 20, y + 13, 12, 150, false);
    if (less) p->put(Button::create(l, 32, 26, Tone::Blue, less, 14), 176, y);
    p->putText(value, 276, y + 13, 12, 130, true);
    if (more) p->put(Button::create(m, 32, 26, Tone::Blue, more, 14), 344, y);
}

std::string keepOr(int v, bool time) {
    if (v < 0) return "keep";
    if (!time) return std::to_string(v);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d", v / 60, v % 60);
    return buf;
}

// the next value of a list, by `by` (clamped)
template <class T>
T step(const std::vector<T>& list, const T& now, int by) {
    auto it = std::find(list.begin(), list.end(), now);
    int i = it == list.end() ? 0 : static_cast<int>(it - list.begin());
    i = std::max(0, std::min(static_cast<int>(list.size()) - 1, i + by));
    return list[static_cast<size_t>(i)];
}

}  // namespace

void EditorScreen::openStageProps() {
    // (clickedStagePropsButton 0x44e7ec: name, bg, bg_flip_x, bg_flip_y,
    // bgm, body_level, to_surgery_stage, health, time)
    if (popupOpen()) popPopup();
    Session& s = session();
    LevelStage& st = stage();
    const bool first = s.stage == 1;
    const int n = static_cast<int>(s.level.stages.size());
    Popup* p = Popup::create("STAGE " + std::to_string(s.stage), 420, 300, Tone::Blue);
    auto reopen = [this] {
        changed();
        showBackground();
        openStageProps();
    };
    p->putText("Background", 20, 245, 12, 150, false);
    p->put(Button::create(shortName(st.bg), 150, 26, Tone::Purple, [this] { openPalette(kBackgrounds, -1); }, 10), 120, 232);
    p->put(Button::create(st.flipX ? "FLIP X: ON" : "FLIP X", 60, 26, st.flipX ? Tone::Orange : Tone::Grey, [&st, reopen] {
               st.flipX = !st.flipX;
               reopen();
           }, 10), 276, 232);
    p->put(Button::create(st.flipY ? "FLIP Y: ON" : "FLIP Y", 60, 26, st.flipY ? Tone::Orange : Tone::Grey, [&st, reopen] {
               st.flipY = !st.flipY;
               reopen();
           }, 10), 342, 232);
    const std::vector<std::string>& music = Catalog::shared().music;
    std::string tune = st.bgm.empty() ? "(keep playing)" : st.bgm.substr(0, st.bgm.find('.'));
    row(p, 200, "Music", tune, [&st, &music, reopen] { st.bgm = step(music, st.bgm, -1); reopen(); },
        [&st, &music, reopen] { st.bgm = step(music, st.bgm, 1); reopen(); }, "<", ">");
    std::vector<int> hearts, clocks;
    if (!first) hearts.push_back(-1), clocks.push_back(-1);
    for (int v = 10; v <= 200; v += 5) hearts.push_back(v);
    for (int v = 30; v <= 900; v += 15) clocks.push_back(v);
    row(p, 168, "Heart (health)", keepOr(st.health, false), [&st, hearts, reopen] { st.health = step(hearts, st.health, -1); reopen(); },
        [&st, hearts, reopen] { st.health = step(hearts, st.health, 1); reopen(); });
    row(p, 136, "Clock (time)", keepOr(st.time, true), [&st, clocks, reopen] { st.time = step(clocks, st.time, -1); reopen(); },
        [&st, clocks, reopen] { st.time = step(clocks, st.time, 1); reopen(); });
    row(p, 104, "Body level", std::to_string(st.bodyLevel), [&st, reopen] { st.bodyLevel = std::max(1, st.bodyLevel - 1); reopen(); },
        [&st, reopen] { st.bodyLevel = std::min(2, st.bodyLevel + 1); reopen(); }, "<", ">");
    std::string then = st.toSurgeryStage == 0 ? "first left" : "stage " + std::to_string(st.toSurgeryStage);
    row(p, 72, "When done, go to", then, [&st, reopen] { st.toSurgeryStage = std::max(0, st.toSurgeryStage - 1); reopen(); },
        [&st, n, reopen] { st.toSurgeryStage = std::min(n, st.toSurgeryStage + 1); reopen(); }, "<", ">");
    Button* del = p->put(Button::create("DELETE STAGE", 130, 30, Tone::Red, [this] {
                             confirm("DELETE STAGE", "Delete this stage and its injuries?", "DELETE", [this] {
                                 popPopup();
                                 deleteStage();
                             });
                         }, 11), 20, 14);
    del->setEnabled(n > 1);
    p->put(Button::create("DONE", 110, 30, Tone::Green, [this] { popPopup(); }), 290, 14);
    pushPopup(p);
}

void EditorScreen::openInjuryProps() {
    // (the property toolbox, refreshPropertyToolbox 0x44cfc0: name, x, y,
    // rotation, to_surgery_stage; and the dependencies' buttons)
    if (m_selected < 0 || session().mode != 0) return;
    if (popupOpen()) popPopup();
    LevelInjury& i = stage().injuries[static_cast<size_t>(m_selected)];
    const int n = static_cast<int>(session().level.stages.size());
    const Catalog::Class* cls = Catalog::shared().findClass(i.className);
    const int stages = cls ? cls->stages : 1;
    Popup* p = Popup::create(i.name, 420, 300, Tone::Orange);
    auto reopen = [this] {
        changed();
        openInjuryProps();
    };
    p->putText("Injury: " + i.className, 20, 245, 12, 250, false);
    p->put(Button::create("RENAME", 90, 26, Tone::Purple, [this] {
               std::string old = stage().injuries[static_cast<size_t>(m_selected)].name;
               askText("NAME (LETTERS, DIGITS, _)", old, kMaxName, [this, old](const std::string& t) {
                   bool good = !t.empty();
                   for (unsigned char ch : t)
                       if (!std::isalnum(ch) && ch != '_' && ch != '-') good = false;
                   for (const LevelInjury& o : stage().injuries)
                       if (o.name == t && t != old) good = false;
                   if (!good) {
                       message("RENAME", "Use letters, digits and _ only, and a name no other injury of the stage has.");
                       return;
                   }
                   for (LevelInjury& o : stage().injuries)
                       for (std::vector<std::string>* deps : {&o.visualDeps, &o.interactionDeps})
                           std::replace(deps->begin(), deps->end(), old, t);
                   stage().injuries[static_cast<size_t>(m_selected)].name = t;
                   changed();
                   buildView(static_cast<size_t>(m_selected));
                   openInjuryProps();
               });
           }, 11), 310, 232);
    row(p, 196, "Opens stage", i.toSurgeryStage == 0 ? "none" : std::to_string(i.toSurgeryStage),
        [&i, reopen] { i.toSurgeryStage = std::max(0, i.toSurgeryStage - 1); reopen(); },
        [&i, n, reopen] { i.toSurgeryStage = std::min(n, i.toSurgeryStage + 1); reopen(); }, "<", ">");
    row(p, 164, "...when its step ends", i.stageEndToSwap == 0 ? "its last" : std::to_string(i.stageEndToSwap),
        [&i, reopen] { i.stageEndToSwap = std::max(0, i.stageEndToSwap - 1); reopen(); },
        [&i, stages, reopen] { i.stageEndToSwap = std::min(stages, i.stageEndToSwap + 1); reopen(); }, "<", ">");
    row(p, 132, "Makes extra injuries", i.allowNonEssential ? "yes" : "no",
        [&i, reopen] { i.allowNonEssential = !i.allowNonEssential; reopen(); },
        [&i, reopen] { i.allowNonEssential = !i.allowNonEssential; reopen(); }, "<", ">");
    p->put(Button::create("WAITS FOR... (" + std::to_string(i.interactionDeps.size()) + ")", 180, 28, Tone::Blue,
                          [this] { openDeps(false, 0); }, 11),
           20, 88);
    p->put(Button::create("HIDDEN UNTIL... (" + std::to_string(i.visualDeps.size()) + ")", 180, 28, Tone::Blue,
                          [this] { openDeps(true, 0); }, 11),
           220, 88);
    p->putText("Waits for: can't be treated until those are. Hidden until: shows when those are done.", 210, 66, 9, 380,
               true);
    p->put(Button::create("DONE", 110, 30, Tone::Green, [this] { popPopup(); }), 290, 14);
    pushPopup(p);
}

void EditorScreen::openPictureProps() {
    // (applyChangesToBGObject 0x44dbb0: filename, x, y, rotation, flip_x, flip_y)
    if (m_selected < 0 || session().mode != 1) return;
    if (popupOpen()) popPopup();
    LevelBg& b = stage().bgs[static_cast<size_t>(m_selected)];
    Popup* p = Popup::create(shortName(b.filename), 360, 150, Tone::Purple);
    auto reopen = [this] {
        changed();
        showBackground();
        refreshOverlay();
        openPictureProps();
    };
    p->put(Button::create(b.flipX ? "FLIP X: ON" : "FLIP X", 150, 30, b.flipX ? Tone::Orange : Tone::Grey, [&b, reopen] {
               b.flipX = !b.flipX;
               reopen();
           }), 20, 66);
    p->put(Button::create(b.flipY ? "FLIP Y: ON" : "FLIP Y", 150, 30, b.flipY ? Tone::Orange : Tone::Grey, [&b, reopen] {
               b.flipY = !b.flipY;
               reopen();
           }), 190, 66);
    p->put(Button::create("DONE", 110, 30, Tone::Green, [this] { popPopup(); }), 230, 14);
    pushPopup(p);
}

void EditorScreen::openDeps(bool visual, int page) {
    // (clickedVisualDepsButton 0x44a9e4 / clickedInteractionDepsButton
    // 0x44ad7c: the stage's other injuries, ticked; BooleanPropertiesPopup)
    if (m_selected < 0) return;
    if (popupOpen()) popPopup();
    LevelInjury& me = stage().injuries[static_cast<size_t>(m_selected)];
    std::vector<std::string>& deps = visual ? me.visualDeps : me.interactionDeps;
    std::vector<std::string> others;
    for (const LevelInjury& o : stage().injuries)
        if (o.name != me.name) others.push_back(o.name);
    const int perPage = 10, pages = std::max(1, (static_cast<int>(others.size()) + perPage - 1) / perPage);
    page = std::max(0, std::min(page, pages - 1));
    Popup* p = Popup::create(std::string(visual ? "HIDDEN UNTIL" : "WAITS FOR") + " (" + me.name + ")", 440, 290,
                             Tone::Blue);
    if (others.empty()) p->putText("No other injury on this stage.", 220, 150, 12, 300, true);
    for (int k = 0; k < perPage; ++k) {
        int idx = page * perPage + k;
        if (idx >= static_cast<int>(others.size())) break;
        const std::string name = others[static_cast<size_t>(idx)];
        bool on = std::find(deps.begin(), deps.end(), name) != deps.end();
        p->put(Button::create((on ? "[X] " : "[  ] ") + name, 196, 30, on ? Tone::Green : Tone::Grey,
                              [this, visual, page, name] {
                                  LevelInjury& m = stage().injuries[static_cast<size_t>(m_selected)];
                                  std::vector<std::string>& d = visual ? m.visualDeps : m.interactionDeps;
                                  auto it = std::find(d.begin(), d.end(), name);
                                  if (it == d.end()) d.push_back(name);
                                  else d.erase(it);
                                  changed();
                                  openDeps(visual, page);
                              }, 11),
               18 + 206 * static_cast<float>(k % 2), 216 - 36 * static_cast<float>(k / 2));
    }
    p->put(Button::create("<", 36, 28, Tone::Blue, [this, visual, page] { openDeps(visual, page - 1); }, 14), 18, 14);
    p->putText(std::to_string(page + 1) + " / " + std::to_string(pages), 90, 28, 12, 60, true);
    p->put(Button::create(">", 36, 28, Tone::Blue, [this, visual, page] { openDeps(visual, page + 1); }, 14), 126, 14);
    p->put(Button::create("DONE", 110, 30, Tone::Green, [this] {
               popPopup();
               openInjuryProps();
           }), 312, 14);
    pushPopup(p);
}

void EditorScreen::openInjuryList(int page) {
    // (InjuryListPopup: the stage's injuries in their order, moved up and
    // down, deleted, picked; refresh 0x55053c, clickedUp/DownButton
    // 0x550a5c/0x550bf0, clickedDeleteButton 0x550e20, clickedInjuryNameButton 0x550da0)
    if (popupOpen()) popPopup();
    if (session().mode != 0) setMode(0);
    std::vector<LevelInjury>& list = stage().injuries;
    const int perPage = 6, pages = std::max(1, (static_cast<int>(list.size()) + perPage - 1) / perPage);
    page = std::max(0, std::min(page, pages - 1));
    Popup* p = Popup::create("INJURIES OF STAGE " + std::to_string(session().stage), 470, 300, Tone::Blue);
    if (list.empty()) p->putText("No injuries yet: + ADD puts one on.", 235, 150, 12, 300, true);
    auto swapAt = [this, page](int a, int b) {
        std::vector<LevelInjury>& l = stage().injuries;
        if (a < 0 || b < 0 || a >= static_cast<int>(l.size()) || b >= static_cast<int>(l.size())) return;
        std::swap(l[static_cast<size_t>(a)], l[static_cast<size_t>(b)]);
        m_selected = b;
        changed();
        showStage();
        openInjuryList(b / 6);
        (void)page;
    };
    for (int k = 0; k < perPage; ++k) {
        int idx = page * perPage + k;
        if (idx >= static_cast<int>(list.size())) break;
        float y = 222 - 34 * static_cast<float>(k);
        const LevelInjury& i = list[static_cast<size_t>(idx)];
        p->put(Button::create(std::to_string(idx + 1) + ". " + i.name, 250, 28, idx == m_selected ? Tone::Orange : Tone::Grey,
                              [this, idx] {
                                  popPopup();
                                  select(idx);
                              }, 11),
               16, y);
        p->put(Button::create("UP", 52, 28, Tone::Blue, [swapAt, idx] { swapAt(idx, idx - 1); }, 11), 272, y);
        p->put(Button::create("DOWN", 60, 28, Tone::Blue, [swapAt, idx] { swapAt(idx, idx + 1); }, 11), 328, y);
        p->put(Button::create("DEL", 52, 28, Tone::Red, [this, idx] {
                   m_selected = idx;
                   deleteSelected();
                   openInjuryList(idx / 6);
               }, 11),
               392, y);
    }
    p->put(Button::create("<", 36, 28, Tone::Blue, [this, page] { openInjuryList(page - 1); }, 14), 16, 14);
    p->putText(std::to_string(page + 1) + " / " + std::to_string(pages), 88, 28, 12, 60, true);
    p->put(Button::create(">", 36, 28, Tone::Blue, [this, page] { openInjuryList(page + 1); }, 14), 124, 14);
    p->put(Button::create("CLOSE", 100, 30, Tone::Green, [this] { popPopup(); }), 354, 14);
    pushPopup(p);
}

void EditorScreen::openSurgeryProps() {
    // (openSurgeryPropertiesPopup 0x44def4: is_challenge, is_sudden_death,
    // min_score_to_1..3_star, min_time_to_1..3_star; the port's: the title,
    // the scream, the injuries' damage)
    if (popupOpen()) popPopup();
    Level& l = session().level;
    Popup* p = Popup::create("SURGERY SETTINGS", 440, 304, Tone::Orange);
    auto reopen = [this] {
        changed();
        openSurgeryProps();
    };
    p->putText("Title: " + l.title, 20, 252, 12, 270, false);
    p->put(Button::create("RENAME", 90, 26, Tone::Purple, [this] {
               askText("SURGERY TITLE", session().level.title, kMaxTitle, [this](const std::string& t) {
                   std::string clean = t;
                   while (!clean.empty() && clean.back() == ' ') clean.pop_back();
                   if (!clean.empty()) session().level.title = clean;
                   changed();
                   openSurgeryProps();
               });
           }, 11), 330, 239);
    const std::vector<std::string>& screams = Catalog::shared().screams;
    row(p, 206, "Patient's scream", l.scream.empty() ? "none" : l.scream,
        [&l, &screams, reopen] { l.scream = step(screams, l.scream, -1); reopen(); },
        [&l, &screams, reopen] { l.scream = step(screams, l.scream, 1); reopen(); }, "<", ">");
    std::vector<float> damage = {-1, 0.5f, 0.75f, 1, 1.25f, 1.5f, 2, 2.5f, 3};
    char buf[32];
    std::snprintf(buf, sizeof buf, "x%.2f", l.damageMultiplier);
    row(p, 174, "Injuries' damage", l.damageMultiplier < 0 ? "the area's" : buf,
        [&l, damage, reopen] { l.damageMultiplier = step(damage, l.damageMultiplier, -1); reopen(); },
        [&l, damage, reopen] { l.damageMultiplier = step(damage, l.damageMultiplier, 1); reopen(); }, "<", ">");
    p->put(Button::create(l.isChallenge ? "CHALLENGE: ON" : "CHALLENGE: OFF", 190, 28,
                          l.isChallenge ? Tone::Orange : Tone::Grey, [&l, reopen] {
                              l.isChallenge = !l.isChallenge;
                              reopen();
                          }, 11),
           20, 136);
    p->put(Button::create(l.isSuddenDeath ? "SUDDEN DEATH: ON" : "SUDDEN DEATH: OFF", 190, 28,
                          l.isSuddenDeath ? Tone::Red : Tone::Grey, [&l, reopen] {
                              l.isSuddenDeath = !l.isSuddenDeath;
                              reopen();
                          }, 11),
           226, 136);
    for (int s = 1; s <= 2; ++s) {
        float y = 98 - 34 * static_cast<float>(s - 1);
        std::string star = std::to_string(s + 1) + " stars:";
        p->putText(star, 20, y + 13, 12, 70, false);
        p->put(Button::create("-", 26, 26, Tone::Blue, [&l, s, reopen] { l.minScore[s] = std::max(0, l.minScore[s] - 50); reopen(); }, 14), 92, y);
        p->putText("score " + std::to_string(l.minScore[s]), 163, y + 13, 11, 80, true);
        p->put(Button::create("+", 26, 26, Tone::Blue, [&l, s, reopen] { l.minScore[s] = std::min(1000000, l.minScore[s] + 50); reopen(); }, 14), 208, y);
        p->put(Button::create("-", 26, 26, Tone::Blue, [&l, s, reopen] { l.minTime[s] = std::max(0, l.minTime[s] - 10); reopen(); }, 14), 244, y);
        std::snprintf(buf, sizeof buf, "in %d:%02d", l.minTime[s] / 60, l.minTime[s] % 60);
        p->putText(buf, 314, y + 13, 11, 80, true);
        p->put(Button::create("+", 26, 26, Tone::Blue, [&l, s, reopen] { l.minTime[s] = std::min(36000, l.minTime[s] + 10); reopen(); }, 14), 358, y);
    }
    p->put(Button::create("TOOLS...", 110, 30, Tone::Blue, [this] { openTools(); }), 20, 14);
    p->put(Button::create("DONE", 110, 30, Tone::Green, [this] { popPopup(); }), 310, 14);
    pushPopup(p);
}

void EditorScreen::openTools() {
    // (openAvailableToolsPopup 0x44e1a0 / closedAvailableToolsPopup 0x44e480:
    // each tool ticked or not)
    if (popupOpen()) popPopup();
    Level& l = session().level;
    Popup* p = Popup::create("THE SURGERY'S TOOLS", 440, 290, Tone::Blue);
    for (int t = 0; t < kToolCount; ++t) {
        const std::string name = kToolNames[t];
        bool on = std::find(l.tools.begin(), l.tools.end(), name) != l.tools.end();
        float x = 18 + 104 * static_cast<float>(t % 4), y = 192 - 70 * static_cast<float>(t / 4);
        Button* b = Button::create("", 96, 64, on ? Tone::Green : Tone::Grey, [this, name] {
            std::vector<std::string>& tools = session().level.tools;
            auto it = std::find(tools.begin(), tools.end(), name);
            if (it == tools.end()) {
                // in the game's order
                tools.push_back(name);
                std::vector<std::string> sorted;
                for (int k = 0; k < kToolCount; ++k)
                    if (std::find(tools.begin(), tools.end(), kToolNames[k]) != tools.end()) sorted.push_back(kToolNames[k]);
                tools = sorted;
            } else {
                tools.erase(it);
            }
            changed();
            openTools();
        });
        if (CCSprite* icon = CCSprite::createWithSpriteFrameName(toolFrame(name))) {
            CCSize sz = icon->getContentSize();
            icon->setScale(std::min(1.0f, 40.0f / std::max(sz.width, sz.height)));
            CCNode* holder = CCNode::create();
            icon->setPosition(ccp(0, 6));
            if (!on) icon->setOpacity(110);
            holder->addChild(icon);
            b->addIcon(holder);
        }
        if (CCLabelBMFont* lb = text(name, 9, 90)) {
            lb->setPosition(ccp(48, 8));
            b->addChild(lb, 3);
        }
        p->put(b, x, y);
    }
    p->put(Button::create("DONE", 110, 30, Tone::Green, [this] {
               popPopup();
               openSurgeryProps();
           }), 312, 14);
    pushPopup(p);
}

// ---- the menu ---------------------------------------------------------------

void EditorScreen::openMenu() {
    // (EditorMenu::init 0x43aa2c: "< Switch to version... >", "< Email this
    // version... >", "< Save (Overwrite) >", "< Save (New) >", "< Test >",
    // "< Delete this version >", "< Discard changes >", "< Edit surgery
    // metadata... >", "< Edit surgery tools... >", "< Close >")
    if (popupOpen()) popPopup();
    Session& s = session();
    std::string title = s.level.title;
    if (!s.file.empty()) {
        std::string id;
        int v = 0;
        size_t at = s.file.rfind("_v");
        if (at != std::string::npos) v = std::atoi(s.file.c_str() + at + 2);
        title += " (v" + std::to_string(v) + ")";
    }
    if (s.dirty) title += " *";
    Popup* p = Popup::create(title, 400, 284, Tone::Orange);
    struct Item {
        const char* label;
        Tone tone;
        std::function<void()> f;
        bool on;
    };
    const bool saved = !s.file.empty();
    Item items[12] = {
        {"SAVE", Tone::Green, [this] { save(false); }, true},
        {"SAVE AS NEW VERSION", Tone::Green, [this] { save(true); }, true},
        {"TEST", Tone::Green, [this] { test(); }, true},
        {"COPY SHARE CODE", Tone::Purple, [this] { shareCode(); }, true},
        {"SURGERY SETTINGS", Tone::Blue, [this] { openSurgeryProps(); }, true},
        {"TOOLS", Tone::Blue, [this] { openTools(); }, true},
        {"EXPORT FILE", Tone::Purple, [this] { exportFile(); }, true},
        {"VERSIONS...", Tone::Blue, [this] { openVersions(); }, saved},
        {"DISCARD CHANGES", Tone::Red, [this] { discard(); }, saved && s.dirty},
        {"DELETE VERSION", Tone::Red, [this] { deleteVersion(); }, saved},
        {"CLOSE EDITOR", Tone::Orange, [this] { close(); }, true},
        {"BACK TO EDITING", Tone::Grey, [this] { popPopup(); }, true},
    };
    for (int k = 0; k < 12; ++k) {
        Button* b = p->put(Button::create(items[k].label, 176, 30, items[k].tone, items[k].f, 11),
                           16 + 192 * static_cast<float>(k % 2), 208 - 38 * static_cast<float>(k / 2));
        b->setEnabled(items[k].on);
    }
    pushPopup(p);
}

void EditorScreen::save(bool newVersion) {
    // (saveSurgeryOverwrite 0x450114 / saveSurgeryNew 0x44f958)
    Session& s = session();
    std::string error;
    if (!saveLevel(s.level, s.id, s.file, newVersion, error)) {
        message("NOT SAVED", error);
        return;
    }
    s.dirty = false;
    refreshBars();
    if (popupOpen()) popPopup();
    message("SAVED", "Saved as " + s.file);
}

void EditorScreen::test() {
    // (testCurrentSurgery 0x4504ac: the surgery screen on the level as it is)
    std::string why;
    if (!playable(session().level, why)) {
        message("CAN'T TEST YET", why);
        return;
    }
    closePopups();
    SceneMgr::sharedSceneMgr()->replaceScene(playLevel(session().level, Back::Editor));
}

void EditorScreen::shareCode() {
    // (the original's sendCurrentSurgeryByEmail 0x45036c mailed the file;
    // the port's code is copied for the player to paste anywhere)
    std::string why;
    if (!playable(session().level, why)) {
        message("CAN'T SHARE YET", why);
        return;
    }
    std::string code = encodeCode(session().level);
    Level check;
    if (code.empty() || !decodeCode(code, check, why)) {
        message("CAN'T SHARE", why.empty() ? "The code can't be made." : why);
        return;
    }
    bool copied = platform::setClipboardText(code);
    log("editor: code %s", code.c_str());
    if (popupOpen()) popPopup();
    message("SHARE CODE", copied ? "The code (" + std::to_string(code.size()) +
                                       " letters) is copied: paste it anywhere to share this surgery."
                                 : "The code couldn't be copied.");
}

void EditorScreen::exportFile() {
    std::string path, error;
    Session& s = session();
    if (!exportLevel(s.level, s.id, path, error)) {
        message("NOT EXPORTED", error);
        return;
    }
    if (popupOpen()) popPopup();
    message("EXPORTED", "Written to\n" + path);
}

void EditorScreen::openVersions() {
    // (VersionSelectorPopup: refresh 0x861928 lists editedLevels/'s files
    // of this surgery, "_v"; clickedVersionBtn 0x861604 loads one)
    if (popupOpen()) popPopup();
    Session& s = session();
    std::vector<LevelFile> files;
    for (const LevelFile& f : listLevels())
        if (f.id == s.id) files.push_back(f);
    std::sort(files.begin(), files.end(), [](const LevelFile& a, const LevelFile& b) { return a.version > b.version; });
    Popup* p = Popup::create("VERSIONS", 420, 290, Tone::Blue);
    for (size_t k = 0; k < files.size() && k < 6; ++k) {
        const LevelFile f = files[k];
        std::string label = "v" + std::to_string(f.version) + "  " + (f.error.empty() ? f.title : "(damaged)");
        Button* b = p->put(Button::create(label, 380, 28, f.file == s.file ? Tone::Orange : Tone::Grey, [this, f] {
                               auto load = [this, f] {
                                   Session& ss = session();
                                   std::string error;
                                   Level l;
                                   if (!loadLevel(f.file, l, error)) {
                                       message("CAN'T OPEN", error);
                                       return;
                                   }
                                   ss.level = l;
                                   ss.file = f.file;
                                   ss.dirty = false;
                                   ss.stage = 1;
                                   closePopups();
                                   m_selected = -1;
                                   needSheets(Catalog::shared().sheetsFor(ss.level));
                                   showStage();
                               };
                               if (session().dirty) confirm("UNSAVED CHANGES", "Open this version and lose your changes?", "OPEN", load);
                               else load();
                           }, 11),
                           20, 214 - 34 * static_cast<float>(k));
        b->setEnabled(f.error.empty());
    }
    p->put(Button::create("CLOSE", 100, 30, Tone::Green, [this] { popPopup(); }), 300, 14);
    pushPopup(p);
}

void EditorScreen::discard() {
    // (discardUnsavedChanges 0x4507d4: the file loaded again)
    Session& s = session();
    std::string error;
    Level l;
    if (s.file.empty() || !loadLevel(s.file, l, error)) return;
    s.level = l;
    s.dirty = false;
    s.stage = std::min(s.stage, static_cast<int>(l.stages.size()));
    closePopups();
    m_selected = -1;
    showStage();
}

void EditorScreen::deleteVersion() {
    // (deleteCurrentVersion 0x4505a8: the file deleted, the surgery loaded
    // again; here the newest version left, else the list)
    confirm("DELETE VERSION", "Delete " + session().file + "?", "DELETE", [this] {
        Session& s = session();
        deleteLevel(s.file);
        s.file.clear();
        std::vector<LevelFile> files;
        for (const LevelFile& f : listLevels())
            if (f.id == s.id && f.error.empty()) files.push_back(f);
        closePopups();
        if (files.empty()) {
            SceneMgr::sharedSceneMgr()->replaceScene(EditorLevelsScreen::create());
            return;
        }
        std::sort(files.begin(), files.end(), [](const LevelFile& a, const LevelFile& b) { return a.version > b.version; });
        std::string error;
        Level l;
        if (loadLevel(files[0].file, l, error)) {
            s.level = l;
            s.file = files[0].file;
            s.dirty = false;
            s.stage = 1;
            m_selected = -1;
            showStage();
        }
    });
}

void EditorScreen::close() {
    auto leave = [] { SceneMgr::sharedSceneMgr()->replaceScene(EditorLevelsScreen::create()); };
    if (session().dirty) confirm("UNSAVED CHANGES", "Close the editor and lose your changes?", "CLOSE", leave);
    else leave();
}

void EditorScreen::back() { openMenu(); }

void EditorScreen::keyBackClicked() {
    PanelScene::keyBackClicked();
    // (the palette closed by Escape: its drawings and sheets let go)
    if (!popupOpen() && !m_previews.empty()) {
        clearPreviews();
        trimSheets(Catalog::shared().sheetsFor(session().level));
    }
}

}  // namespace mt
