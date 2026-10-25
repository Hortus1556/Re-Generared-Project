#include "as2/menus/InGame.h"

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/Bandages.h"
#include "as2/menus/Panel.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

// The help's tool buttons (help.holder.buttons.<name>): each opens the page
// of that label.
const char* const kHelpButtons[8] = {"smallCut", "largeCut", "burn", "fire", "surfaceExtraction",
                                     "underSurfaceExtraction", "poison", "transplant"};
const std::string kButtons = "help.holder.buttons.";

// The clips of the pause screen its scripts keep on frame 1 (stop() on
// their first frame, or on every frame with nothing moving them).
const char* const kStoppedClips[7] = {"shell_fla.blob1_92",     "shell_fla.holderbullet_95",
                                      "shell_fla.lholder_65",   "shell_fla.pageholdercopy_55",
                                      "shell_fla.ringocopy_62", "shell_fla.shodercopy_57",
                                      "shell_fla.shotholdercopy_91"};

}  // namespace

// ------------------------------------------------------------------ PauseMenu

void PauseMenu::prefetch() {
    VectorLook on;
    on.frames["holder.sholder"] = on.frames["holder.mholder"] = 2;
    FlashScreen::prefetch("flash-shell", "pauseScreen", {39, 6});
    FlashScreen::prefetch("flash-shell", "pauseScreen", {39, 6}, &on);
}

PauseMenu::PauseMenu(const std::string&) : FlashScreen("flash-shell", "pauseScreen") {
    placeAt = {39, 6};
    main.stops = {21};
    for (const char* c : kStoppedClips) art.setFrameOverride(art.symbol(c), 1);
    help = &inner("help");
    help->stops = {1, 11};
    help->gotoAndStop(1);
    bandages::prefetch();
}

void PauseMenu::onFrame() {
    if (age % (20 * 30) == 0) bandages::prefetch();
    // checkResume: out at the clip's last frame, and the mission goes on.
    if (!resuming || main.frame != main.frames) return;
    resuming = false;
    main.playing = false;
    // (Before the next draw: this screen goes, not while it draws.)
    ui::Timer::scheduled(0, false, [](ui::Timer*) {
        MenuManager& mm = MenuManager::sharedMenuManager();
        mm.setupNewViewController("Panel", "Panel");
        if (Panel* p = dynamic_cast<Panel*>(mm.mCurrentViewController)) p->setupButtons();
        if (gameState) gameState->paused = false;
    });
}

void PauseMenu::dress(VectorLook& look) {
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    look.frames["holder.sholder"] = al.soundFXOn ? 2 : 1;
    look.frames["holder.mholder"] = al.musicOn ? 2 : 1;
    if (!asking) {
        look.hidden.push_back("holder.quit.yes");
        look.hidden.push_back("holder.quit.no");
    }
    if (page.empty()) {
        look.hidden.push_back("help.holder.pages");
    } else {
        look.hidden.push_back("help.holder.buttons");
        int pages = art.innerSymbol(symbol, main.frame, age, "/help.holder.pages");
        if (int f = pages >= 0 ? art.frameOf(pages, page) : 0) look.frames["help.holder.pages"] = f;
    }
}

std::vector<std::string> PauseMenu::clickable() const {
    if (resuming) return {};
    // The open filofax takes every click on it (none while it moves).
    if (help->frame != 1) {
        if (help->playing) return {};
        if (!page.empty()) return {"help.holder.pages.back", "help.holder.back"};
        std::vector<std::string> v;
        for (const char* b : kHelpButtons) v.push_back(kButtons + b);
        v.push_back("help.holder.back");
        return v;
    }
    std::vector<std::string> v;
    if (asking) v = {"holder.quit.yes", "holder.quit.no"};
    else v = {"holder.quit"};
    for (const char* p : {"holder.resume", "holder.help", "holder.sholder", "holder.mholder"}) v.push_back(p);
    return v;
}

void PauseMenu::onPress(const std::string& part) {
    // pressQuit (on the press): are you sure?
    if (part != "holder.quit") return;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    asking = true;
}

void PauseMenu::onClick(const std::string& part) {
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    if (part == "holder.quit") return;  // (its press did it)
    al.playSFXWithKey("SELECT");
    platform::debugTrace("pause %s", part.c_str());
    if (part == "holder.resume") {
        resuming = true;
        main.gotoAndPlay(main.frame + 1);
    } else if (part == "holder.sholder") {
        al.soundFXOn = !al.soundFXOn;
        SaveDetails::SaveSFXOn(al.soundFXOn);
    } else if (part == "holder.mholder") {
        al.musicOn = !al.musicOn;
        SaveDetails::SaveMusicOn(al.musicOn);
        if (al.musicOn) al.playMusicWithKey("ingame");
        else al.stopSoundWithKey("ingame");
    } else if (part == "holder.quit.yes") {
        asking = false;
        bandages::play([] {
            if (game) game->missionQuit();
        });
    } else if (part == "holder.quit.no") {
        asking = false;
    } else if (part == "holder.help") {
        page.clear();
        help->gotoAndPlay(2);
    } else if (part == "help.holder.back") {
        help->gotoAndPlay(11);
    } else if (part == "help.holder.pages.back") {
        page.clear();
    } else if (part.compare(0, kButtons.size(), kButtons) == 0) {
        page = part.substr(kButtons.size());
    }
}

// --------------------------------------------------------------- CFOPauseMenu

namespace {
void setButtonImage(ui::Button* b, const std::string& png) {
    if (!b) return;
    ui::Image* img = ui::Image::named(png + ".png");
    b->setImage(img, ui::Button::kNormal);
    releaseRef(img);
}
}  // namespace

void CFOPauseMenu::connectOutlet(const std::string& name, ui::View* v) {
    if (name == "mMusic") mMusic = dynamic_cast<ui::Button*>(v);
    else if (name == "mSoundFX") mSoundFX = dynamic_cast<ui::Button*>(v);
    else if (name == "mQuitYes") mQuitYes = dynamic_cast<ui::Button*>(v);
    else if (name == "mQuitNo") mQuitNo = dynamic_cast<ui::Button*>(v);
    else if (name == "mImgViewQuit") mImgViewQuit = dynamic_cast<ui::ImageView*>(v);
}

void CFOPauseMenu::viewDidLoad() {
    setMusicButtonImage();
    setSoundFXButtonImage();
}

void CFOPauseMenu::setMusicButtonImage() {
    setButtonImage(mMusic, MyOpenAL::sharedMyOpenAL().musicOn ? "CFOMusicONButton" : "CFOMusicOFFButton");
}

void CFOPauseMenu::setSoundFXButtonImage() {
    setButtonImage(mSoundFX, MyOpenAL::sharedMyOpenAL().soundFXOn ? "CFOSoundONButton" : "CFOSoundOFFButton");
}

void CFOPauseMenu::action(const std::string&, ui::View* sender) {
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    al.playSFXWithKey("SELECT");
    MenuManager& mm = MenuManager::sharedMenuManager();
    auto quitShown = [&](bool shown) {
        for (ui::View* v : {static_cast<ui::View*>(mImgViewQuit), static_cast<ui::View*>(mQuitYes),
                            static_cast<ui::View*>(mQuitNo)})
            if (v) v->hidden = !shown;
    };
    switch (sender->tag) {
        case 1:  // quit: are you sure?
            quitShown(true);
            break;
        case 2: {  // resume
            mm.killViewController();
            mm.setupNewViewController("Panel", "Panel");
            if (Panel* p = dynamic_cast<Panel*>(mm.mCurrentViewController)) p->setupButtons();
            gameState->paused = false;
            break;
        }
        case 3:
            mm.killViewController();
            mm.setupNewViewController("HelpScreen", "HelpScreen");
            break;
        case 4:
            al.soundFXOn = !al.soundFXOn;
            SaveDetails::SaveSFXOn(al.soundFXOn);
            setSoundFXButtonImage();
            break;
        case 5:
            al.musicOn = !al.musicOn;
            SaveDetails::SaveMusicOn(al.musicOn);
            if (al.musicOn) al.playMusicWithKey("ingame");
            else al.stopSoundWithKey("ingame");
            setMusicButtonImage();
            break;
        case 10:  // quit: yes
            mm.killViewController();
            game->missionQuit();
            break;
        case 11:  // quit: no
            quitShown(false);
            break;
        default:
            // 100 was the leaderboard (OpenFeint): removed.
            break;
    }
}

// -------------------------------------------------------------- TutorialPopup

void TutorialPopup::connectOutlet(const std::string& name, ui::View* v) {
    if (name == "mText") mText = dynamic_cast<ui::TextView*>(v);
}

void TutorialPopup::viewDidLoad() {
    if (!mText) return;
    mText->fontName = "MarkerFelt-Thin";
    mText->fontSize = 32;
    mText->text = gameState->getTutorialText();
}

void TutorialPopup::action(const std::string&, ui::View*) { gameState->clickedTutorial(); }

}  // namespace as2
