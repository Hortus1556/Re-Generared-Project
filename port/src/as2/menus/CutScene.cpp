#include "as2/menus/CutScene.h"

#include <algorithm>
#include <cstdio>

#include "as2/Connector.h"
#include "as2/GameController.h"
#include "as2/Loader.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"
#include "as2/menus/MainMenu.h"
#include "as2/menus/Results.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

// The ambient loops of the story's places.
const char* const kAmbient[] = {"bathroom", "chickens", "clock", "outsidetent", "InsideTent_loop",
                                "S5_6_music", "birds_loop", "S10_InsideTent_loop"};

VectorArt& cutArt() { return flashstage::art("flash-cutscene"); }
int backgrounds() {
    static const int bg = cutArt().innerSymbol(cutArt().symbol("cutSceneHolder"), 1, 0, "/bg");
    return bg;
}
constexpr unsigned long long kBgOwner = 0x4355544247000000ull;  // "CUTBG"
constexpr unsigned long long kCharOwner = 0x4355544348000000ull;  // "CUTCH"
int holder() {
    static const int h = cutArt().symbol("cutSceneHolder");
    return h;
}
// The characters' clip (each of char1..char4 is one), a frame labelled by
// character and look ("alan_thoughtful").
int characters() {
    static const int c = cutArt().innerSymbol(holder(), 1, 0, "/char1");
    return c;
}

// changeColorOfImage: (colorizeImage:color: with 50% grey): the image
// multiplied by grey, its shape kept: a character not speaking, darkened.
ui::Image* darkened(ui::Image* img) {
    std::vector<unsigned char> px = img->pixels();
    for (size_t i = 0; i < px.size(); i += 4)
        for (int c = 0; c < 3; ++c) px[i + c] = static_cast<unsigned char>((px[i + c] + 1) / 2);
    return ui::Image::withPixels(img->width(), img->height(), std::move(px));
}

}  // namespace

int cutscene::backgroundFrame(const std::string& place) {
    return backgrounds() >= 0 && place != "nothing" ? cutArt().frameOf(backgrounds(), place) : 0;
}

namespace {
// The stage covering the screen (600x460 is a little narrower than it).
VectorTransform covering() {
    const float W = gameController->ActualScreenSize.width, H = gameController->ActualScreenSize.height;
    VectorTransform t;
    t.scaleX = t.scaleY = std::max(W / 600, H / 460);
    t.x = (W - 600 * t.scaleX) / 2;
    t.y = (H - 460 * t.scaleY) / 2;
    return t;
}

// The characters' label for a story's character name: the same name, or
// the one label it begins (the iPad's stories shortened a few: its
// "aide_business" is Flash's "aide_businesslike"); "" if Flash has none.
std::string flashLabel(const std::string& name) {
    if (characters() < 0) return std::string();
    if (name == "nothing" || cutArt().frameOf(characters(), name) > 0) return name;
    return cutArt().labelStartingWith(characters(), name);
}

// Character slot k's clip (char<k+1>) where the holder places it on the
// stage covering the screen.
bool placed(int k, VectorTransform& t, int& clip) {
    if (!cutArt().placement(holder(), 1, "char" + std::to_string(k + 1), t, &clip)) return false;
    const VectorTransform screen = covering();
    t.x = screen.x;
    t.y = screen.y;
    t.scaleX = screen.scaleX;
    t.scaleY = screen.scaleY;
    return true;
}

json::Value story(int mission, bool end) {
    char name[64];
    std::snprintf(name, sizeof name, end ? "cutscene_end_%i.json" : "cutscene_start_%i.json", mission + 1);
    std::string text;
    json::Value v;
    if (assets::readText(name, text)) json::parse(text, v);
    return v;
}
}  // namespace

void cutscene::drawBackground(int frame, long age) {
    if (frame <= 0) return;
    cutArt().draw(backgrounds(), frame, age, age, covering(), kBgOwner);
}

void cutscene::prefetchStory(int mission, bool end) {
    json::Value v = story(mission, end);
    const json::Value& screens = v["dialog"]["screen"];
    std::vector<int> done;
    for (size_t i = 0; i < screens.size(); ++i) {
        int frame = backgroundFrame(screens[i]["bg"].str());
        if (!frame || std::find(done.begin(), done.end(), frame) != done.end()) continue;
        done.push_back(frame);
        cutArt().prefetch(backgrounds(), frame, 0, covering(), kBgOwner, nullptr, 40 * 60);
    }
    // The first pack's characters, each where its slot puts it.
    if (mission >= 20 || characters() < 0) return;
    std::vector<std::pair<int, int>> shown;  // (slot, frame)
    for (size_t i = 0; i < screens.size(); ++i)
        for (int k = 0; k < 4; ++k) {
            const std::string label = flashLabel(screens[i]["pos" + std::to_string(k + 1)].str());
            int frame = label.empty() ? 0 : cutArt().frameOf(characters(), label);
            if (!frame || std::find(shown.begin(), shown.end(), std::make_pair(k, frame)) != shown.end()) continue;
            shown.emplace_back(k, frame);
            VectorTransform t;
            int clip = -1;
            if (!placed(k, t, clip)) continue;
            cutArt().prefetch(clip, frame, 0, t, kCharOwner + static_cast<unsigned long long>(k), nullptr, 40 * 60);
        }
}

CutScene::~CutScene() {
    if (animationTimer) {
        animationTimer->invalidate();
        releaseRef(animationTimer);
    }
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    for (const char* k : kAmbient) al.stopSoundWithKey(k);
    for (const char* k : {"CUT_SCENE_FIRE", "tricks_strong", "tricks_weak", "feedback"}) al.stopSoundWithKey(k);
}

void CutScene::connectOutlet(const std::string& name, ui::View* v) {
    int n = 0;
    if (std::sscanf(name.c_str(), "mImgViewPos%d", &n) == 1 && n >= 1 && n <= 4)
        mImgViewPos[n - 1] = dynamic_cast<ui::ImageView*>(v);
    else if (name == "mTextView") mTextView = dynamic_cast<ui::TextView*>(v);
    else if (name == "mTextView2") mTextView2 = dynamic_cast<ui::TextView*>(v);
}

void CutScene::viewDidLoad() {
    isActive = true;
    playingTrack = 0;
    int id = Connector::sharedConnector().current_mission_id;
    mCutSceneDictionary = story(id, gameController->isEndScene);
    cutscene::prefetchStory(id, gameController->isEndScene);  // (its later places)
    if (gameController->isEndScene && gameController->mLevelPack != 1) MissionWin::prefetch();
    for (int k = 0; k < 4; ++k)
        if (mImgViewPos[k]) mSlot[k] = mImgViewPos[k]->frame();
    // The Flash background, under everything the nib shows.
    ui::GLView* g = new ui::GLView;
    g->bounds = view()->bounds;
    g->center = {view()->bounds.width / 2, view()->bounds.height / 2};
    g->userInteractionEnabled = false;
    g->drawHook = [this] { drawBackground(); };
    view()->insertSubviewAtIndex(g, 0);
    releaseRef(g);
    // The characters on the right face left.
    for (ui::ImageView* v : {mImgViewPos[1], mImgViewPos[3]})
        if (v) v->transform = v->transform.concat(ui::Affine::scale(-1, 1));
    // The ringmaster's second visit, once played: straight to its end.
    dialogPosition = id == 20 && SaveDetails::GetScoreForLevel(21) != 0 && !gameController->isEndScene ? 17 : 0;
    setupScene();
}

void CutScene::playAmbient(const char* key, int track) {
    if (playingTrack != track) {
        MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
        for (const char* k : kAmbient)
            if (std::string(k) != key) al.stopSoundWithKey(k);
        al.playMusicWithKey(key);
    }
    playingTrack = track;
}

void CutScene::showCharacter(int slot, const std::string& name, const std::vector<std::string>& lit, float baseX,
                             float baseY) {
    ui::ImageView* v = mImgViewPos[slot];
    if (!v) return;
    bool speaking = std::find(lit.begin(), lit.end(), name) != lit.end();
    FlashCharacter& fc = flashCharacters[slot];
    const std::string label = flashLabel(name);
    if (gameController->mLevelPack != 1 && !label.empty()) {
        // The Flash AS2's (gotoAndStop on its label: what plays in it starts
        // over only when the label changes).
        if (fc.label != label) {
            fc.label = label;
            fc.since = frames;
        }
        fc.lit = speaking;
        v->setImage(nullptr);
        return;
    }
    fc.label.clear();
    // "nothing": no one there (the bundle has no such picture).
    ui::Image* img = name == "nothing" ? nullptr : ui::Image::named(name + ".png");
    ui::Image* shown = img && !speaking ? darkened(img) : retainRef(img);
    v->setImage(shown);
    releaseRef(shown);
    v->sizeToFit();
    if (img && gameController->mLevelPack == 1) {
        // correctImgViewPos:BaseX:BaseY:BaseSize: (the CFO pack's art):
        // standing on the base point.
        Rect f = v->frame();
        v->setFrame({{baseX - img->width() * 0.5f, baseY - img->height()}, f.size});
    } else if (img) {
        // Standing on its slot's bottom corner, as the nib's content mode
        // (bottom left, bottom right) meant: the original's sizeToFit kept
        // the slot's top instead, so a short character (the pig) hung in
        // the air and a tall one ran off the bottom.
        const Rect& s = mSlot[slot];
        Rect f = v->frame();
        float x = s.origin.x;
        if (v->contentMode == ui::kBottomRight) x = s.origin.x + s.size.width - f.size.width;
        else if (v->contentMode == ui::kBottom) x = s.origin.x + (s.size.width - f.size.width) * 0.5f;
        v->setFrame({{x, s.origin.y + s.size.height - f.size.height}, f.size});
    }
    releaseRef(img);
}

void CutScene::scheduleNext() {
    if (animationTimer) {
        animationTimer->invalidate();
        releaseRef(animationTimer);
    }
    animationTimer = retainRef(ui::Timer::scheduled(10.0, false, [this](ui::Timer*) { setupScene(); }));
}

void CutScene::setupScene() {
    if (animationTimer) {
        animationTimer->invalidate();
        releaseRef(animationTimer);
        animationTimer = nullptr;
    }
    const json::Value& screens = mCutSceneDictionary["dialog"]["screen"];
    MenuManager& mm = MenuManager::sharedMenuManager();
    bool cfo = gameController->mLevelPack == 1;
    if (dialogPosition >= static_cast<int>(screens.size())) {
        // Over (by its timer or a tap): a late tap must not move on again
        // (a second scene stacked on the first).
        isActive = false;
        if (!gameController->isEndScene) {
            mm.killViewController();
            mm.setupNewViewController(cfo ? "CFOStartScene" : "StartScene", "StartScene");
        } else if (!gameController->winUpgradeTool || cfo) {
            mm.killViewController();
            mm.setupNewViewController(cfo ? "CFOMissionWin" : "MissionWin", cfo ? "CFOMissionWin" : "MissionWin");
        } else {
            mm.setupNewViewController("UpgradeTool", "UpgradeTool");
        }
        return;
    }
    const json::Value& s = screens[static_cast<size_t>(dialogPosition)];
    const std::string& bg = s["bg"].str();
    int id = Connector::sharedConnector().current_mission_id;
    if (id == 27 || id == 28) playAmbient("birds_loop", 7);
    else if (bg == "office_dark" || bg == "office_light") playAmbient("clock", 3);
    else if (bg == "outsideTent" || bg == "toBeContinuedBG") playAmbient("outsidetent", 4);
    else if (id == 29) playAmbient("S10_InsideTent_loop", 8);
    else if (bg == "insideTentBg") playAmbient("InsideTent_loop", 5);
    else if (id == 24 || id == 25 || id == 26) playAmbient("S5_6_music", 6);
    else if (id < 10 || id == 19 || id == 20) playAmbient("bathroom", 1);
    else if (id < 20) playAmbient("chickens", 2);

    std::vector<std::string> lit;
    const std::string& h = s["highlight"].str();
    for (size_t p = 0; p <= h.size();) {
        size_t q = h.find(", ", p);
        lit.push_back(h.substr(p, q == std::string::npos ? std::string::npos : q - p));
        if (q == std::string::npos) break;
        p = q + 2;
    }
    int frame = cutscene::backgroundFrame(bg);
    if (frame != flashBg) bgSince = frames;
    flashBg = frame;
    setBackground(view(), flashBg ? std::string() : bg + ".png");
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    if (lit.size() == 1 && lit[0] == "beardyBurpy") al.playSFXWithKey("CUT_SCENE_FIRE");
    else if (lit.size() == 1 && lit[0] == "aide_sick") al.playSFXWithKey("tricks_weak");
    else if (lit.size() == 1 && lit[0] == "bij_5") al.playSFXWithKey("feedback");
    else if (lit.size() == 1 && lit[0] == "tricks_magic") al.playSFXWithKey("tricks_strong");
    else
        for (const char* k : {"CUT_SCENE_FIRE", "tricks_strong", "tricks_weak", "feedback"}) al.stopSoundWithKey(k);

    const float W = gameController->ActualScreenSize.width, H = gameController->ActualScreenSize.height;
    showCharacter(0, s["pos1"].str(), lit, 270, H);
    showCharacter(3, s["pos4"].str(), lit, W - 130, H - 60);
    showCharacter(1, s["pos2"].str(), lit, W - 270, H);
    showCharacter(2, s["pos3"].str(), lit, 130, H - 60);
    for (ui::TextView* t : {mTextView, mTextView2})
        if (t) {
            t->fontName = "MarkerFelt-Thin";
            t->fontSize = 40;
        }
    const std::string& line = s["text"].str();
    if (line != "TXT_MESSAGE") {
        if (mTextView) mTextView->text = line;
        if (mTextView2) mTextView2->text = line;
        ++dialogPosition;
        scheduleNext();
        isActive = true;
    } else {
        if (mTextView) mTextView->text.clear();
        if (mTextView2) mTextView2->text.clear();
        createAlertView1();
    }
}

void CutScene::drawBackground() {
    // Its frame held (gotoAndStop), what plays in it at Flash's 30 frames a
    // second.
    double now = platform::passTime();
    if (clock < 0) clock = now;
    while (now - clock >= 1 / flashstage::kFps) {
        clock += 1 / flashstage::kFps;
        ++frames;
    }
    if (flashBg) cutscene::drawBackground(flashBg, frames - bgSince);
    drawCharacters();
}

void CutScene::drawCharacters() {
    // In the holder's order (char4 lowest, char1 on top), each where the
    // holder places it on the stage covering the screen.
    for (int k = 3; k >= 0; --k) {
        const FlashCharacter& fc = flashCharacters[k];
        if (fc.label.empty() || fc.label == "nothing") continue;
        VectorTransform t;
        int clip = -1;
        if (!placed(k, t, clip)) continue;
        if (!fc.lit) {
            // ColourUtils.tint(0, 0.6): 60% black.
            t.color = flash::ColorTransform();
            for (int c = 0; c < 3; ++c) t.color.mult[c] = 0.4f;
        }
        const long age = frames - fc.since;
        cutArt().draw(clip, cutArt().frameOf(clip, fc.label), age, age, t, kCharOwner + static_cast<unsigned long long>(k));
    }
}

void CutScene::createAlertView1() {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("txt_msg");
    retainRef(this);
    ui::AlertView::show("Dr. Bleed", "H", {"CLOSE", "REPLY"}, [this](int i) {
        alertDismissed(i);
        releaseRef(this);
    });
}

void CutScene::createAlertView2() {
    retainRef(this);
    ui::AlertView::show("Critical error", "Unknown recipient.", {"CLOSE"}, [this](int i) {
        alertDismissed(i);
        releaseRef(this);
    });
}

void CutScene::alertDismissed(int index) {
    if (index == 1) {  // REPLY
        createAlertView2();
        return;
    }
    ++dialogPosition;
    isActive = true;
    setupScene();
}

void CutScene::action(const std::string&, ui::View* sender) {
    if (!isActive) return;
    isActive = false;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    if (sender->tag == 1) {  // SKIP
        dialogPosition = 10000;
        setupScene();
    } else if (sender->tag == 2) {  // NEXT
        setupScene();
    }
}

// ----------------------------------------------------------------- StartScene

void StartScene::viewDidLoad() {
    Loader::sharedLoader().prefetchArt();  // (the mission's loading next)
    setBackground(view(), "startscreen" + std::to_string(Connector::sharedConnector().current_mission_id + 1) + ".png");
}

void StartScene::action(const std::string&, ui::View*) {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    Loader::sharedLoader().startMission();
}

}  // namespace as2
