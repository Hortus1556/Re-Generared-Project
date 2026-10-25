#include "as2/GameState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "as2/Connector.h"
#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/Library.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/SceneManager.h"
#include "as2/effects/Popup.h"
#include "as2/menus/Hud.h"
#include "as2/injuries/InjuryGraphic.h"
#include "as2/menus/Panel.h"
#include "as2/tools/ITool.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
Texture2D* texture(const std::string& name) {
    return SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(name);
}
int missionId() { return Connector::sharedConnector().current_mission_id; }

// A HUD or world sprite at a place, layer and colour.
Sprite* sprite(const std::string& image, int w, int h, float x, float y, int layer) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->pos = {x, y};
    s->currentShape()->layer = layer;
    return s;
}

void setColour(Shape* s, float r, float g, float b, float a) {
    s->color[0] = r;
    s->color[1] = g;
    s->color[2] = b;
    s->color[3] = a;
}

// -[NSObject intValue] of a plist value (a number or a string of one).
int intValue(const json::Value& v) {
    if (v.type == json::Value::kString) return std::atoi(v.str().c_str());
    if (v.type == json::Value::kBool) return v.boolean ? 1 : 0;
    return v.integer();
}

Panel* panel() { return dynamic_cast<Panel*>(MenuManager::sharedMenuManager().mCurrentViewController); }

// Where a new injury's box must lie (the globals minX..maxY, world points).
// The top is below the HUD's tool cards (Flash's, down to screen y 158:
// world 79), as Flash kept its injuries below its cards (minY 150 of its
// 460); the iPad's, with its tools at the sides, allowed 10. (Under a card,
// a press picks the tool: squid ink landed there could not be cleared.)
constexpr int kMinX = 60, kMaxX = 452, kMinY = 80, kMaxY = 370;

}  // namespace

// ------------------------------------------------------------------- init

GameState::GameState() {
    MyOpenAL::sharedMyOpenAL().playMusicWithKey("ingame");
    paused = true;
    gameState = this;
    // The tools: every one made; the HUD offers the mission's and those
    // the player has used before.
    for (ToolData* td : Library::sharedLibrary().getToolLibrary()) {
        ITool* t = ITool::create(td);
        if (!t) continue;
        possible_tools.addObject(t);
        bool mission = false;
        for (int id : game->mission_data->tools)
            if (id == td->mId) mission = true;
        if (mission || SaveDetails::GetToolUsedToolID(td->mId)) _tools.addObject(t);
        releaseRef(t);
    }
    updateToolPanel = true;
    // (The HUD is Flash's: menus/Hud.)
    joggerFrame = joggerFrameType = 1;
    soundTimer = 0;
    scale = 1;
    _xScroll = _yScroll = _xWorld = _yWorld = 0;
    spriteRedFlash = sprite("redFlash", 128, 128, 0, 0, 8);
    spriteRedFlash->currentShape()->scale = 4;
    // The battery's spark: centred on the screen's middle (doubled with
    // every shape's position, so on the iPad it was centred on the
    // screen's corner until showBatteryAnimAtX:Y: moves it).
    spriteBattery = sprite("battery1", 128, 128, gameController->ActualScreenSize.width * 0.5f,
                           gameController->ActualScreenSize.height * 0.5f, 8);
    spriteBattery->currentShape()->offset = {-64, -64};
    spriteBattery->currentShape()->scale = 4;
    batteryAnim = -1;
    std::string text;
    json::Value all;
    if (assets::readText("Tutorials.plist", text) && json::parse(text, all) && missionId() < static_cast<int>(all.size()))
        level_tutorial = all[static_cast<size_t>(missionId())];
    int best = SaveDetails::GetScoreForLevel(missionId() + 1);
    showTutorial = best < 1;
    sprLeftEye = sprite("pupil.png", 16, 16, 185, 233, 41);
    sprLeftEye->currentShape()->offset = {-8, -8};
    sprRightEye = sprite("pupil.png", 16, 16, 340, 237, 41);
    sprRightEye->currentShape()->offset = {-8, -8};
}

GameState::~GameState() {
    for (Sprite** s : {&sprLeftEye, &sprRightEye, &sprLeftEyeMask, &sprRightEyeMask, &spriteBg, &spriteBgOld, &spriteFg,
                       &spriteRedFlash, &spriteBattery}) {
        if (*s) scene().removeShape((*s)->currentShape());
        releaseRef(*s);
        *s = nullptr;
    }
    if (gameState == this) gameState = nullptr;
}

Array<InjuryGraphic>& GameState::currentInjuries() { return game->mFindCorrectInjury; }

// ----------------------------------------------------------------- touches

void GameState::setMouseXY(float x, float y) {
    unscaledMouseX = x;
    unscaledMouseY = y;
    // Into the world: undo the scroll and the zoom around (240, 160).
    mouseX = static_cast<float>(static_cast<int>((x - (_xScroll + 240)) / scale + 240));
    mouseY = static_cast<float>(static_cast<int>((y - (_yScroll + 160)) / scale + 160));
}

Point GameState::worldToScreen(Point p) const {
    return {(2 * p.x - 240) * scale + 240 + _xScroll + _xWorld, (2 * p.y - 160) * scale + 160 + _yScroll + _yWorld};
}

void GameState::setShakeAsHigh(int shake) {
    if (currentShake != 1) {
        shakeTimer = 0;
        currentShake = shake;
    }
}

// -------------------------------------------------------------- the HUD

void GameState::updatePupils() {
    if (missionId() != 29) return;
    // The ringmaster's eyes follow the ticks (stage 1) or the finger.
    scene().removeShape(sprLeftEye->currentShape());
    scene().removeShape(sprRightEye->currentShape());
    // (Shown at their own layer: a new part of the body moves every shape
    // to the fading old layer, the pupils too, and they came back there,
    // faded out: no pupils once the skull was closed.)
    sprLeftEye->currentShape()->layer = sprRightEye->currentShape()->layer = 41;
    if (currentStage == 1) {
        InjuryGraphic* first = nullptr;
        InjuryGraphic* second = nullptr;
        for (InjuryGraphic* i : _injuries)
            if (i->isKindOf("Tick") && i->alive && i->mSpriteArray.count() > 0) {
                if (!first) first = i;
                else second = i;
            }
        if (!first) {
            eyeMovementForSprite(sprLeftEye, 185, 233, mouseX, mouseY);
            eyeMovementForSprite(sprRightEye, 340, 237, mouseX, mouseY);
        } else {
            Shape* a = first->mSpriteArray[0]->currentShape();
            eyeMovementForSprite(sprLeftEye, 185, 233, a->pos.x, a->pos.y);
            Shape* b = (second ? second : first)->mSpriteArray[0]->currentShape();
            eyeMovementForSprite(sprRightEye, 340, 237, b->pos.x, b->pos.y);
        }
        scene().addShape(sprLeftEye->currentShape());
        scene().addShape(sprRightEye->currentShape());
    }
    if (currentStage == 2 || currentStage == 5) {
        eyeMovementForSprite(sprLeftEye, 185, 233, mouseX, mouseY);
        eyeMovementForSprite(sprRightEye, 340, 237, mouseX, mouseY);
        scene().addShape(sprLeftEye->currentShape());
        scene().addShape(sprRightEye->currentShape());
    }
}

void GameState::eyeMovementForSprite(Sprite* eye, float xo, float yo, float xt, float yt) {
    float dx = xt - xo, dy = yt - yo;
    float d = std::sqrt(dx * dx + dy * dy) * 0.125f;
    if (d > 18) d = 18;
    double a = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    Shape* s = eye->currentShape();
    s->pos.x = static_cast<float>(xo + std::cos(a) * d * 1.1);
    double k = currentStage == 5 ? (eye == sprRightEye ? 0.4 : 0.6) : 0.8;
    s->pos.y = static_cast<float>(yo + d * std::sin(a) * k);
}

void GameState::ChangeBG(const std::string& bg) {
    if (!_bg.empty()) {
        if (spriteBg) scene().removeShape(spriteBg->currentShape());
        releaseRef(spriteBg);
        spriteBg = nullptr;
        SingletonTextureManager::sharedTextureManagerInstance().removeTextureByName(_bg);
    }
    _bg = bg;
    Texture2D* t = texture(_bg);
    if (t && static_cast<int>(t->contentSize().width) == 1024) {
        spriteBg = Sprite::initWithImageNamed(_bg, 512, 384);
        spriteBg->calculateTextureCoordsAtOffsetX(0, 0);
    } else {
        // The 960 x 640 art stretched over the iPad's 1024 x 768.
        spriteBg = Sprite::initWithImageNamed(_bg, 480, 320);
        spriteBg->calculateTextureCoordsAtOffsetX(0, 0);
        float* v = spriteBg->currentShape()->verts;
        v[5] = v[7] = 768;
        v[2] = v[6] = 1024;
    }
    spriteBg->currentShape()->pos = {0, 0};
    spriteBg->currentShape()->layer = 46;
    scene().addShape(spriteBg->currentShape());
}

void GameState::displayArrowOnTool(int tool) { Hud::shared().showArrow(tool); }

void GameState::showBatteryAnimAtXY(int x, int y) {
    batteryAnim = 10;
    Shape* s = spriteBattery->currentShape();
    s->tex = texture("battery1");
    s->pos = {static_cast<float>(x), static_cast<float>(y)};
    scene().removeShape(s);
    scene().addShape(s);
}

void GameState::checkBleedFingerFix() {
    if (missionId() != 19 || currentStage != 2) return;
    for (InjuryGraphic* i : _injuries)
        if (i->isKindOf("Bleed")) i->checkThumbClick();
}

void GameState::flashRed() {
    flashRedDirection = 0.1f;
    SceneManager& sm = scene();
    sm.alphaEffect = sm.redEffect = sm.blueEffect = sm.greenEffect = 1;
    sm.removeShape(spriteRedFlash->currentShape());
    sm.addShape(spriteRedFlash->currentShape());
    flashRedTimer = 20;
}

void GameState::loseBrain() {
    if (brainOut()) return;
    brainOutLeft = kBrainOutTicks;
    brainOutFrom = std::max(health, 1.0f);
    platform::debugTrace("brain out %d", kBrainOutTicks);
}

bool GameState::heartCounts() const { return missionId() != 9 && missionId() != 19 && !gameController->godMode; }

void GameState::restoreBrain(float restartAmount) {
    if (heartStopped) return;
    brainOutLeft = -1;
    health = std::max(health, restartAmount);
    platform::debugTrace("brain in");
}

void GameState::createPopup(const std::string& imageIn, int x, int y, InjuryGraphic* injury) {
    std::string image = imageIn;
    bool good = image == "correct" || image == "healed" || image == "revived" || image == "yummy";
    bool playerCreated = false;
    if (!good) {
        // A miss: the combo is lost and the screen flashes red.
        combo = 0;
        flashRed();
        playerCreated = injury && injury->mPlayerCreated;
    } else if (!injury) {
        if (image == "correct" || image == "healed" || image == "yummy") {
            if (missionId() != 9) image = "healed";
            checkTutorial(10, 8);
        }
    } else {
        if (!injury->mPlayerCreated && missionId() != 19) {
            ++combo;
            score += combo * 20;
        }
        image = "awesome";
        checkTutorial(6, injury->mInjury->mID);
        playerCreated = injury->mPlayerCreated;
    }
    Popup* p = Popup::initWithXY(x, y, image, playerCreated, combo);
    mPopups.addObject(p);
    releaseRef(p);
    injuryTimer = 0;
}

void GameState::removeInjury(InjuryGraphic* injury) {
    retainRef(injury);
    for (int i = 0; i < injury->hitBoxCount; ++i)
        if (injury->hitboxes[i].sprite) scene().removeShape(injury->hitboxes[i].sprite->currentShape());
    // These keep showing once treated (kept with the stage's art): the rest
    // vanish.
    bool kept = false;
    for (const char* c : {"Tick", "Transplant", "AmpPlug", "BrainPill", "Transistor", "Insertion", "Lip", "Incision",
                          "ClownNose", "Eyelid", "Goo", "SimonSays"})
        kept = kept || injury->isKindOf(c);
    if (kept) {
        mTransplants.addObject(injury);
    } else {
        for (Sprite* s : injury->mSpriteArray) scene().removeShape(s->currentShape());
    }
    injury->trace("removed");
    currentInjuries().removeObject(injury);
    _display_list.removeObject(injury);
    _injuries.removeObject(injury);
    checkTutorial(7, 0);
    if (game) game->checkInjuries(7);
    releaseRef(injury);
}

// ---------------------------------------------------------------- tutorials

std::string GameState::getTutorialText() const {
    std::string text = current_tutorial ? (*current_tutorial)["TextInfo"].str() : std::string();
    // The iPad's words for what the platform does: on a device with an
    // accelerometer it is "the device"; without one (a PC) the torch follows
    // the pointer.
    const std::string ipad = "iPad";
    std::string::size_type at = text.find(ipad);
    if (at == std::string::npos) return text;
    float x, y;
    if (!platform::deviceTilt(x, y) && text.rfind("Tilt the iPad to move the Flashlight", 0) == 0)
        return "Move the pointer: the Flashlight follows it around the screen";
    for (; at != std::string::npos; at = text.find(ipad, at)) text.replace(at, ipad.size(), "device");
    return text;
}

void GameState::clickedTutorial() { checkTutorial(9, 0); }

void GameState::showHint(const std::string& text) {
    if (!showTutorial || current_tutorial) return;
    json::parse("{\"Popup\": true, \"clicked\": true, \"actions\": [], \"TextInfo\": \"" + text + "\"}", mHint);
    current_tutorial = &mHint;
    setupTutorial();
}

void GameState::setupTutorial() {
    bool popup = (*current_tutorial)["Popup"].flag();
    Panel* p = panel();
    platform::debugTrace("tutorial %s: %s", popup ? "popup" : "panel", getTutorialText().c_str());
    if (!popup) {
        if (p) {
            if (p->mImgViewTutPanel) p->mImgViewTutPanel->hidden = false;
            if (p->mTextViewTutorial) {
                p->mTextViewTutorial->hidden = false;
                p->mTextViewTutorial->text = getTutorialText();
            }
        }
        tutUp = false;
    } else if (p) {
        p->addPopupView("TutorialPopup");
        tutUp = true;
        paused = true;
    }
}

void GameState::checkTutorial(int event, int stage) {
    if (!showTutorial) return;
    if (!current_tutorial) {
        // The next unused tutorial this event (and stage) opens.
        for (size_t n = 0; n < level_tutorial.size(); ++n) {
            const json::Value* t = &level_tutorial[n];
            if (std::find(mUsedTutorials.begin(), mUsedTutorials.end(), t) != mUsedTutorials.end()) continue;
            int ev = intValue((*t)["event"]);
            bool open = false;
            if (event == 1 || event == 5) open = ev == event && intValue((*t)["stage"]) == stage;
            else if (event == 8) open = ev == 8 && static_cast<int>(n) == stage;
            // The original also compared updatedInjuryCurrentStage (never
            // set: 0) with the open tutorial's value (none open: 0), so
            // only the injury counts.
            else if (event == 6)
                open = ev == 6 && intValue((*t)["stage"]) == stage;
            if (open) {
                current_tutorial = t;
                setupTutorial();
                return;
            }
        }
        return;
    }
    const json::Value& cur = *current_tutorial;
    // Does this event close the open one?
    if (event == 9) {
        if (!cur.has("clicked")) return;
        paused = false;
    } else if (event == 6) {
        const json::Value& sel = cur["selecttoolarray"];
        if (sel.isNull() || intValue(sel["action"]) != 6) return;
        int id = intValue(sel["injuryID"]);
        if (id == 0) return;
        for (InjuryGraphic* i : _injuries)
            if (i->mInjury->mID == id && i->currentStage < intValue(sel["value"])) return;
    } else if (event == 7) {
        const json::Value& sel = cur["selecttoolarray"];
        if (sel.isNull() || intValue(sel["action"]) != 7) return;
        int id = intValue(sel["injuryID"]);
        if (id != 0)
            for (InjuryGraphic* i : _injuries)
                if (i->mInjury->mID == id) return;
    } else if (event == 10) {
        const json::Value& sel = cur["selecttoolarray"];
        if (sel.isNull() || intValue(sel["action"]) != 10 || intValue(sel["injuryID"]) != stage) return;
    } else {
        return;
    }
    if (Panel* p = panel()) {
        p->removePopupView();
        if (p->mImgViewTutPanel) p->mImgViewTutPanel->hidden = true;
        if (p->mTextViewTutorial) p->mTextViewTutorial->hidden = true;
        paused = false;
    }
    mUsedTutorials.push_back(current_tutorial);
    const json::Value& actions = cur["actions"];
    int trigger = -1;
    for (const json::Value& a : actions.items) {
        const std::string& action = a["action"].str();
        int element = intValue(a["element"]);
        if (action == "unlock") {
            for (ITool* t : possible_tools)
                if (t->toolData->mId == element) {
                    _tools.addObject(t);
                    updateToolPanel = true;
                    break;
                }
        } else if (action == "arrow") {
            displayArrowOnTool(element);
        } else if (action == "TUTORIAL_TRIGGER") {
            trigger = element;
        }
    }
    current_tutorial = nullptr;
    if (trigger != -1) checkTutorial(8, trigger);
}

// ------------------------------------------------------------------ stages

void GameState::fadeComplete() {
    // (The trace's "fade done": the tools work again, for the play-tests.)
    platform::debugTrace("fade done");
    if (spriteBgOld) {
        scene().removeShape(spriteBgOld->currentShape());
        SingletonTextureManager::sharedTextureManagerInstance().removeTextureByName(_bgOld);
        releaseRef(spriteBgOld);
        spriteBgOld = nullptr;
    }
    _fade_list.removeAllObjects();
    toRemoveFromClearList.clear();
    for (const std::string& t : mTexturesToClear)
        if (std::find(mTexturesToKeep.begin(), mTexturesToKeep.end(), t) == mTexturesToKeep.end()) {
            SingletonTextureManager::sharedTextureManagerInstance().removeTextureByName(t);
            toRemoveFromClearList.push_back(t);
        }
    for (const std::string& t : toRemoveFromClearList)
        mTexturesToClear.erase(std::remove(mTexturesToClear.begin(), mTexturesToClear.end(), t), mTexturesToClear.end());
}

void GameState::setupStage(int stage) {
    platform::debugTrace("stage %d", stage);
    Hud::shared().clearArrows();
    Connector& c = Connector::sharedConnector();
    if (c.current_mission_stage) {
        releaseRef(c.current_mission_stage);
        c.current_mission_stage = nullptr;
    }
    MissionStage* ms = MissionStage::initFromDictionary(game->mission_data->data[static_cast<size_t>(stage - 1)]);
    c.current_mission_stage = ms;
    if (_bg != ms->bg) {
        if (game->needToFade()) {
            for (InjuryGraphic* i : _blood) _fade_list.addObject(i);
            for (InjuryGraphic* i : mTransplants) _fade_list.addObject(i);
        }
        spriteBgOld = spriteBg;
        if (!_bg.empty()) _bgOld = _bg;
        _bg.clear();
        if (ms->bg != "nothing") {
            _bg = ms->bg;
            Texture2D* t = texture(_bg);
            if (t && static_cast<int>(t->contentSize().width) == 1024) {
                spriteBg = Sprite::initWithImageNamed(_bg, 512, 384);
                spriteBg->calculateTextureCoordsAtOffsetX(0, 0);
            } else {
                spriteBg = Sprite::initWithImageNamed(_bg, 480, 320);
                spriteBg->calculateTextureCoordsAtOffsetX(0, 0);
                float* v = spriteBg->currentShape()->verts;
                v[5] = v[7] = 768;
                v[2] = v[6] = 1024;
            }
            spriteBg->currentShape()->pos = {0, 0};
            spriteBg->currentShape()->layer = 46;
            scene().addShape(spriteBg->currentShape());
            if (missionId() == 28 && !spriteFg) {
                // The brain's glow over the scene. The original computed the
                // background's texture coordinates again here instead of the
                // glow's, so its quad stayed empty: it never showed.
                spriteFg = Sprite::initWithImageNamed("S9_bg1", 480, 320);
                spriteBg->calculateTextureCoordsAtOffsetX(0, 0);
                float* v = spriteBg->currentShape()->verts;
                v[5] = v[7] = 768;
                v[2] = v[6] = 1024;
                Shape* fg = spriteFg->currentShape();
                fg->pos = {0, 0};
                fg->layer = 31;
                setColour(fg, 0.071875f, 0.03125f, 0.071875f, 0.1f);
                scene().addShape(fg);
            }
        } else {
            spriteBg = nullptr;
        }
        _blood.removeAllObjects();
        mTransplants.removeAllObjects();
        platform::debugTrace("blood cleared");
    }
    game->mFindCorrectInjury.removeAllObjects();
    game->mFindHiddenInjury.removeAllObjects();
    _injuries.removeAllObjects();
    _display_list.removeAllObjects();
    mTexturesToKeep.clear();
    for (Injury* i : ms->injuries) {
        // The iPad's stretched art: the injuries 32 right and 42 down.
        if (gameController->isIPad) {
            i->x += 32;
            i->y += 42;
        }
        if (i->mID == 94) addBlood(i, -1);  // dirt
        else addInjury(i, false, true);
    }
    if (ms->time != -1) time = ms->time;
    if (ms->health != -1) health = static_cast<float>(ms->health);
    SceneManager& sm = scene();
    if (!(sm.mOldLayerFade > 0 && sm.mOldLayerFadeSpeed > 0)) fadeComplete();
    flashRedTimer = 0;
    sm.removeShape(spriteRedFlash->currentShape());
    sm.alphaEffect = sm.blueEffect = sm.greenEffect = sm.redEffect = 1;
    paused = false;
    checkTutorial(1, stage);
}

void GameState::addBlood(Injury* blood, int velocity) {
    // In the graveyard (mission 20, stage "grave") only the dirt.
    if (missionId() == 19 && _bg == "grave" && blood->mID != 94) return;
    InjuryData* data = Library::sharedLibrary().getInjuryByID(blood->mID);
    InjuryGraphic* g = InjuryGraphic::create(data, blood, false, false, blood->mID == 94 ? -1 : velocity);
    if (!g) return;
    _blood.addObject(g);
    _display_list.addObject(g);
    releaseRef(g);
}

InjuryGraphic* GameState::addInjury(Injury* injury, bool playerCreated, bool force) {
    // While an incision is open nothing else is added.
    if (_injuries.count() > 0 && _injuries[0]->isKindOf("Incision")) return nullptr;
    int id = injury->mID;
    if (!force) {
        // Kept inside the area injuries may be in (minX..maxX, minY..maxY).
        Rect b = getBoundingBoxForInjury(injury);
        if (static_cast<float>(kMinX) <= b.origin.x) {
            float right = b.origin.x + b.size.width;
            if (right > static_cast<float>(kMaxX)) injury->x -= static_cast<int>(right - kMaxX);
        } else {
            injury->x += static_cast<int>(kMinX - b.origin.x);
        }
        if (static_cast<float>(kMinY) <= b.origin.y) {
            float bottom = b.origin.y + b.size.height;
            if (bottom > static_cast<float>(kMaxY)) injury->y -= static_cast<int>(bottom - kMaxY);
        } else {
            injury->y += static_cast<int>(kMinY - b.origin.y);
        }
    }
    InjuryGraphic* g =
        InjuryGraphic::create(Library::sharedLibrary().getInjuryByID(id), injury, playerCreated, force);
    if (!g) return nullptr;
    _injuries.addObject(g);
    _display_list.addObject(g);
    g->trace("created");
    releaseRef(g);
    return g;
}

Rect GameState::getBoundingBoxForInjury(Injury* injury) {
    const int* box = Library::sharedLibrary().getInjuryBoundsForID(injury->mID);
    if (box[0] == 0 && box[1] == 0 && box[2] == 1 && box[3] == 1)
        return {{static_cast<float>(injury->x), static_cast<float>(injury->y)},
                {static_cast<float>(box[2]), static_cast<float>(box[3])}};
    // The injury's box {x, y, w, h} turned by its rotation; the injury moved
    // by the turned box's centre; its bounds.
    float r = static_cast<float>(injury->rotation * 3.141592653589793 / 180.0);
    float cs = std::cos(r), sn = std::sin(-r);
    float x0 = static_cast<float>(box[0]), y0 = static_cast<float>(box[1]);
    float w = static_cast<float>(box[2]), h = static_cast<float>(box[3]);
    float cx = x0 + w * 0.5f, cy = y0 + h * 0.5f;
    float rcx = cs * cx + sn * cy, rcy = cs * cy - sn * cx;
    float pts[4][2] = {{x0, y0}, {x0 + w, y0}, {x0, y0 + h}, {x0 + w, y0 + h}};
    float minX = 0, maxX = 0, minY = 0, maxY = 0;
    for (int i = 0; i < 4; ++i) {
        float px = cs * pts[i][0] + sn * pts[i][1], py = -sn * pts[i][0] + cs * pts[i][1];
        if (i == 0) {
            minX = maxX = px;
            minY = maxY = py;
        } else {
            minX = std::min(minX, px);
            maxX = std::max(maxX, px);
            minY = std::min(minY, py);
            maxY = std::max(maxY, py);
        }
    }
    injury->x = static_cast<int>(static_cast<float>(injury->x) - static_cast<float>(static_cast<int>(rcx)));
    injury->y = static_cast<int>(static_cast<float>(injury->y) - rcy);
    return {{static_cast<float>(static_cast<int>(minX + injury->x)), static_cast<float>(static_cast<int>(minY + injury->y))},
            {static_cast<float>(static_cast<int>(maxX - minX)), static_cast<float>(static_cast<int>(maxY - minY))}};
}

// ------------------------------------------------------------------ render

void GameState::render() {
    if (gameState->paused) return;
    if (updateToolPanel) {
        if (Panel* p = panel()) {
            p->setupButtons();
            updateToolPanel = false;
        }
    }
    // The shake: five frames of random world offsets.
    if (shakeTimer < 5) {
        ++shakeTimer;
        if (shakeTimer < 5) {
            _xWorld = static_cast<float>(std::rand() % 100) / 100.0f * currentShake * 10.0f - currentShake;
            _yWorld = static_cast<float>(std::rand() % 100) / 100.0f * currentShake * 10.0f - currentShake;
        } else {
            currentShake = 0;
            _xWorld = _yWorld = 0;
        }
    }
    // The red flash: green and blue pulse between 0.5 and 1.
    SceneManager& sm = scene();
    if (flashRedTimer > 0) {
        if (--flashRedTimer == 0) {
            sm.alphaEffect = sm.blueEffect = sm.greenEffect = sm.redEffect = 1;
            sm.removeShape(spriteRedFlash->currentShape());
        } else {
            sm.greenEffect = sm.greenEffect + flashRedDirection;
            if (!(sm.greenEffect < 1.0f) || sm.greenEffect < 0.5f) {
                flashRedDirection = -flashRedDirection;
                sm.greenEffect = sm.greenEffect + flashRedDirection;
            }
            sm.blueEffect = sm.greenEffect;
            sm.redEffect = 1;
            sm.alphaEffect = 1;
        }
    }
    if (--batteryAnim < 0) {
        if (batteryAnim == -1) sm.removeShape(spriteBattery->currentShape());
        batteryAnim = -1;
    } else {
        spriteBattery->currentShape()->tex = texture("battery" + std::to_string(5 - (batteryAnim >> 1)));
    }
    bool fire = false;
    std::vector<InjuryGraphic*> list(_injuries.begin(), _injuries.end());
    for (InjuryGraphic* i : list) retainRef(i);
    for (InjuryGraphic* i : list) {
        if (!fire && i->isKindOf("Fire")) {
            fire = true;
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXPLOSION_FIRE");
        }
        if (_injuries.containsObject(i)) i->render();
    }
    for (InjuryGraphic* i : list) releaseRef(i);
    if (!fire) MyOpenAL::sharedMyOpenAL().stopSoundWithKey("EXPLOSION_FIRE");
    std::vector<InjuryGraphic*> blood(_blood.begin(), _blood.end());
    for (InjuryGraphic* i : blood) retainRef(i);
    for (InjuryGraphic* i : blood) i->render();
    for (InjuryGraphic* i : blood) releaseRef(i);
    for (int i = 0; i < mPopups.count();) {
        if (!mPopups[i]->update()) mPopups.removeObjectAtIndex(i);
        else ++i;
    }
    updatePupils();
    if (missionId() == 28) {
        // The brain jar's bubbles.
        if (currentStage == 7) {
            if (++bubbleTimer % 20 == 0)
                game->addEffect("BubbleEffect", (std::rand() % 14) * 20 + 100, 300, 1, 1, 1);
        } else if (currentStage == 1 || currentStage == 3 || currentStage == 4 || currentStage == 6) {
            ++bubbleTimer;
            auto bubble = [](int x, int y) { game->addEffect("BubbleTopEffect", x, y, 1, 1, 1); };
            if (bubbleTimer % 40 == 0) bubble(std::rand() % 30 + 60, std::rand() % 30 + 20);
            if (bubbleTimer % 50 == 40) bubble(std::rand() % 30 + 370, std::rand() % 30);
            if (bubbleTimer % 40 == 20) bubble(std::rand() % 30 + 60, std::rand() % 30 + 280);
            if (bubbleTimer % 50 == 15) bubble(std::rand() % 30 + 400, std::rand() % 30 + 280);
        }
    }
    // The brain out: the heart runs down with the clock, 0 at its end.
    if (brainOut()) {
        if (brainOutLeft > 0) --brainOutLeft;
        health = brainOutFrom * static_cast<float>(brainOutLeft) / static_cast<float>(kBrainOutTicks);
    }
    // A heart attack (driven up to 200), or the heart down to 0: stopped
    // for good.
    if (!heartStopped && heartCounts() && (health >= kHeartAttack || health <= 0)) {
        heartStopped = true;
        if (health > 0) {
            flashRed();
            platform::debugTrace("heart attack");
        }
    }
    if (heartStopped) health = 0;
    // The heart rate: the jogger dial's look and the beep by health.
    int type;
    bool danger;
    if (missionId() == 9) {
        type = 5;
        danger = false;
    } else if (health > 0) {
        if (health > 15) {
            danger = false;
            if (health > 25) {
                if (health > 45) {
                    type = 1;
                    soundTimer += 1;
                } else {
                    type = 2;
                    soundTimer += 3;
                }
            } else {
                type = 3;
                soundTimer += 5;
            }
        } else {
            if (health < 10) soundTimer += 10;
            type = 4;
            soundTimer += 8;
            danger = true;
        }
    } else {
        soundTimer += 20;
        health = 0;
        if (gameController->godMode) {
        } else if (joggerFrame == 33 && missionId() != 19) {
            game->missionFailed(false);
        }
        type = 5;
        danger = true;
    }
    if (soundTimer > 179) {
        if (!inShock) MyOpenAL::sharedMyOpenAL().playSFXWithKey(danger ? "BLIP2" : "BLIP");
        soundTimer = 180 - soundTimer;
    }
    if (inShock) {
        // Shocked: the jogger jolted, then its heart crossed out until the
        // battery restarts it. (The original had this animation and its
        // case below, but nothing chose it: a shock froze the death one.)
        type = 6;
    }
    if (brainOut() && health > 0) {
        // The brain out: crossed out on the dial while the heart runs down.
        type = 7;
    }
    if (joggerFrameType != type) {
        joggerFrameType = type;
        joggerFrame = 0;
    }
    char name[32] = {};
    float jogScale = 1;
    switch (joggerFrameType) {
        case 1:
            if (++joggerFrame > 13) joggerFrame = 1;
            std::snprintf(name, sizeof name, "highbpm%04d", joggerFrame);
            break;
        case 2:
            if (++joggerFrame > 18) joggerFrame = 1;
            std::snprintf(name, sizeof name, "medbpm%04d", joggerFrame);
            break;
        case 3:
            if (++joggerFrame > 16) joggerFrame = 1;
            std::snprintf(name, sizeof name, "lowbpm%04d", joggerFrame);
            break;
        case 4:
            if (++joggerFrame > 26) joggerFrame = 1;
            std::snprintf(name, sizeof name, "verylowbpm%04d", joggerFrame);
            break;
        case 5:
            if (++joggerFrame >= 34) joggerFrame = 33;
            std::snprintf(name, sizeof name, "death%04d", joggerFrame);
            break;
        case 6:
            // (Played once, held on its stopped heart: the original's
            // never-reached case looped it.)
            if (++joggerFrame > 41) joggerFrame = 41;
            std::snprintf(name, sizeof name, "shocked%04d", joggerFrame);
            // Its heart fills the whole picture, where the jogger takes about
            // half: shrunk as the jogger turns into it (frames 29-36), to fit
            // the dial's face.
            jogScale = 1.0f - 0.45f * static_cast<float>(std::clamp(joggerFrame - 29, 0, 7)) / 7.0f;
            break;
        case 7:
            // The port's own (tools/brain_dial.py), after the shocked one's
            // end: the brain swells out of the jogger, then is crossed out,
            // held; at the heart's size, so shrunk the same way.
            if (++joggerFrame > 16) joggerFrame = 16;
            std::snprintf(name, sizeof name, "brainout%04d", joggerFrame);
            jogScale = 1.0f - 0.45f * static_cast<float>(std::clamp(joggerFrame - 1, 0, 6)) / 6.0f;
            break;
        default:
            break;
    }
    joggerPicture = name;
    joggerScale = jogScale;
    int heart = inShock ? 0 : static_cast<int>(health);
    // The trace's "heart <n>", each 5 of change and as the shock starts or
    // ends ("heart 0 shock": revive with the battery; a plain 0 is the
    // heart stopped otherwise). "brain out <ticks>" / "brain in" mark a
    // brain transplant's countdown.
    if (heart / 5 != tracedHeart / 5 || inShock != tracedShock) {
        tracedHeart = heart;
        tracedShock = inShock;
        platform::debugTrace("heart %d%s", heart, inShock ? " shock" : "");
    }
}

}  // namespace as2
