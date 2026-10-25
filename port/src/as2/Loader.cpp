#include "as2/Loader.h"

#include <algorithm>
#include <string>

#include "as2/Connector.h"
#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/Library.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/menus/FlashStage.h"
#include "as2/Texture2D.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

// Work done per frame: until 10 ms have gone (at least one step). (The
// original's 40 ms held the loading screen to some 20 frames a second.)
constexpr double kFrameBudget = 0.01;
// The least time the loading screen's bar takes to fill (Flash's preloader
// filled as the game downloaded; here the work may take a fifth of a second).
constexpr double kLeastShown = 0.8;

const char* const (*const kLevelSounds[30])[3] = {
    GameData::levelSounds1,  GameData::levelSounds2,  GameData::levelSounds3,  GameData::levelSounds4,
    GameData::levelSounds5,  GameData::levelSounds6,  GameData::levelSounds7,  GameData::levelSounds8,
    GameData::levelSounds9,  GameData::levelSounds10, GameData::levelSounds11, GameData::levelSounds12,
    GameData::levelSounds13, GameData::levelSounds14, GameData::levelSounds15, GameData::levelSounds16,
    GameData::levelSounds17, GameData::levelSounds18, GameData::levelSounds19, nullptr,
    GameData::levelSounds21, GameData::levelSounds22, GameData::levelSounds23, GameData::levelSounds24,
    GameData::levelSounds25, GameData::levelSounds26, GameData::levelSounds27, GameData::levelSounds28,
    GameData::levelSounds29, GameData::levelSounds30,
};

const char* const (*genericSounds())[3] {
    return gameController->mLevelPack == 1 ? GameData::genericSounds2 : GameData::genericSounds;
}

}  // namespace

Loader& Loader::sharedLoader() {
    static Loader* instance = new Loader;
    return *instance;
}

Loader::Loader() {
    // loaderScreen: the iPhone's 480 x 320 strip, widened to the screen.
    const float base[12] = {0, 0, 0, 480, 0, 0, 0, 320, 0, 480, 320, 0};
    std::copy(base, base + 12, screen);
    float w = gameController->ActualScreenSize.width * gameController->InverseScaleFactor;
    float h = gameController->ActualScreenSize.height * gameController->InverseScaleFactor;
    screen[3] = screen[9] = w;
    screen[7] = screen[10] = h;
}

void Loader::restart() {
    menuSoundProgress = ingameProgress = 0;
    shown = 0;
    shownClock = -1;
}

bool Loader::shownLongEnough() {
    // (Moved on here, a step a pass, as the loading screen is drawn.)
    double now = platform::passTime();
    if (shownClock < 0) shownClock = now;
    shown = std::min({1.0f, GetLoadProgress(), shown + static_cast<float>((now - shownClock) / kLeastShown)});
    shownClock = now;
    return shown >= 1;
}

void Loader::startMenuLoad() {
    if (mLoadingState != 0) return;
    restart();
    mLoadingState = 1;
}

void Loader::startMission() {
    if (mLoadingState != 0) return;
    gameController->mEAGLView->setAnimationFrameInterval(2);
    restart();
    mLoadingState = 5;
}

void Loader::startMissionFailed() {
    if (mLoadingState != 0) return;
    MyOpenAL::sharedMyOpenAL().stopSoundWithKey("ingame");
    restart();
    mLoadingState = 6;
}

void Loader::startReplay() {
    if (mLoadingState != 0) return;
    MyOpenAL::sharedMyOpenAL().stopSoundWithKey("ingame");
    restart();
    mLoadingState = 4;
}

void Loader::startMissionComplete() {
    if (mLoadingState != 0) return;
    MyOpenAL::sharedMyOpenAL().stopSoundWithKey("ingame");
    restart();
    mLoadingState = 3;
}

void Loader::startQuitGame() {
    if (mLoadingState != 0) return;
    MyOpenAL::sharedMyOpenAL().stopSoundWithKey("ingame");
    restart();
    mLoadingState = 2;
}

void Loader::Load() {
    MenuManager& mm = MenuManager::sharedMenuManager();
    const bool full = shownLongEnough();
    switch (mLoadingState) {
        case 1:
            if (LoadMenuSounds()) mLoadingState = 0;
            return;
        case 2:
            if (!unloadInitGame() || !LoadMenuSounds() || !full) return;
            mLoadingState = 0;
            gameController->mEAGLView->setAnimationFrameInterval(30);
            mm.setupNewViewController("MainMenu", "MainMenu");
            return;
        case 3:
            if (!unloadInitGame() || !LoadMenuSounds() || !full) return;
            mLoadingState = 0;
            gameController->mEAGLView->setAnimationFrameInterval(30);
            gameController->isEndScene = true;
            mm.setupNewViewController(gameController->mLevelPack == 1 ? "CFOCutScene" : "CutScene", "CutScene");
            return;
        case 4:
            if (!unloadInitGame() || !LoadMenuSounds() || !full) return;
            mLoadingState = 0;
            gameController->mEAGLView->setAnimationFrameInterval(30);
            mm.setupNewViewController(gameController->mLevelPack == 1 ? "CFOStartScene" : "StartScene", "StartScene");
            return;
        case 6:
            if (!unloadInitGame() || !LoadMenuSounds() || !full) return;
            mLoadingState = 0;
            gameController->mEAGLView->setAnimationFrameInterval(30);
            mm.ReplaceViewController("CFOMissionFailed", "CFOMissionFailed", "fade", "fade");
            return;
        case 5:
            if (!unLoadMenuSounds() || !LoadInitGame() || !full) return;
            mLoadingState = 0;
            gameController->StartGameOnLevel();
            if (game) game->StartGame();
            return;
        default:
            return;
    }
}

float Loader::GetLoadProgress() const {
    if (mLoadingState == 1) return menuSoundProgress * 0.25f;
    return static_cast<float>(menuSoundProgress + ingameProgress) / (getSoundCountToLoad() + 265u);
}

const char* const (*Loader::getSoundsToLoad() const)[3] {
    int id = Connector::sharedConnector().current_mission_id;
    return id >= 0 && id < 30 ? kLevelSounds[id] : nullptr;
}

int Loader::getSoundCountToLoad() const {
    switch (Connector::sharedConnector().current_mission_id) {
        case 0: case 1: case 2: case 3: case 7: case 10: case 14: case 16: case 17: case 20: case 27: return 4;
        case 4: return 3;
        case 5: case 28: return 6;
        case 6: return 10;
        case 8: return 8;
        case 9: case 29: return 9;
        case 11: case 13: case 21: case 22: case 25: case 26: return 5;
        case 12: case 24: return 12;
        case 15: case 23: return 7;
        case 18: return 11;
        default: return 0;
    }
}

bool Loader::unLoadMenuSounds() {
    double start = platform::now();
    bool did = false;
    while (platform::now() - start < kFrameBudget || !did) {
        if (menuSoundProgress > 13) return true;
        MyOpenAL::sharedMyOpenAL().unloadSound(GameData::menuMusicToLoad[menuSoundProgress][0]);
        ++menuSoundProgress;
        did = true;
    }
    return false;
}

bool Loader::LoadMenuSounds() {
    double start = platform::now();
    bool did = false;
    for (;;) {
        if (!(platform::now() - start < kFrameBudget) && did) return false;
        if (menuSoundProgress > 13) {
            MyOpenAL::sharedMyOpenAL().musicOn = SaveDetails::GetMusicOn();
            MyOpenAL::sharedMyOpenAL().soundFXOn = SaveDetails::GetSFXOn();
            return true;
        }
        const char* const* e = GameData::menuMusicToLoad[menuSoundProgress];
        std::string key = e[0], ext = e[2];
        bool loop = ext == "m4a" || key == "CUT_SCENE_FIRE" || key == "tricks_strong";
        MyOpenAL::sharedMyOpenAL().loadSoundWithKey(key, e[1], ext, loop);
        did = true;
        ++menuSoundProgress;
    }
}

bool Loader::unloadInitGame() {
    double start = platform::now();
    bool did = false;
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    for (;;) {
        if (!(platform::now() - start < kFrameBudget) && did) return false;
        if (ingameProgress == 0) {
            Connector::sharedConnector();
            Library::sharedLibrary();
        }
        int p = ingameProgress, n = getSoundCountToLoad();
        bool beyond = false;
        if (p < 24) {
            al.unloadSound(genericSounds()[p][0]);
        } else if (p < n + 24) {
            al.unloadSound(getSoundsToLoad()[p - 24][0]);
        } else {
            beyond = true;
        }
        if (n + 24 <= ingameProgress) {
            SingletonTextureManager::sharedTextureManagerInstance().removeTextures();
            ingameProgress += 227;
        }
        if (beyond) return true;
        did = true;
        ++ingameProgress;
    }
}

bool Loader::LoadInitGame() {
    double start = platform::now();
    bool did = false;
    MyOpenAL& al = MyOpenAL::sharedMyOpenAL();
    for (;;) {
        if (!(platform::now() - start < kFrameBudget) && did) return false;
        if (ingameProgress == 0) {
            Connector::sharedConnector();
            Library::sharedLibrary();
        }
        int p = ingameProgress, n = getSoundCountToLoad();
        if (p < 24) {
            const char* const* e = genericSounds()[p];
            std::string key = e[0], ext = e[2];
            al.loadSoundWithKey(key, e[1], ext, ext == "m4a" || key == "EXPLOSION_FIRE");
        } else if (p < n + 24) {
            const char* const* e = getSoundsToLoad()[p - 24];
            std::string ext = e[2];
            al.loadSoundWithKey(e[0], e[1], ext, ext == "m4a");
        } else {
            int t = p - n - 24;
            if (t > 226) {
                al.musicOn = SaveDetails::GetMusicOn();
                al.soundFXOn = SaveDetails::GetSFXOn();
                return true;
            }
            SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(GameData::injuryTextures[t], true);
        }
        did = true;
        ++ingameProgress;
    }
}

namespace {
constexpr unsigned long long kPreloaderOwner = 0x4c4f414445520002ull;

// The loading screen's look: the bar at `percent`, the splashes on `blob`.
VectorLook preloaderLook(int percent, int blob) {
    VectorLook look;
    look.frames["preloader.inner"] = std::max(percent, 1);
    look.frames["preloader.inner.mc1"] = blob;
    look.frames["preloader.inner.mc2"] = blob;
    return look;
}
}  // namespace

void Loader::prefetchArt() {
    VectorArt& art = flashstage::art("flash-preloader");
    int clip = art.symbol("preloader_mc");
    if (clip < 0) return;
    for (int p = 1; p <= 100; ++p) {
        VectorLook look = preloaderLook(p, (p - 1) % 25 + 1);
        art.prefetch(clip, 1, 0, flashstage::at(), kPreloaderOwner, &look, 40 * 60);
    }
}

void Loader::render() {
    // The screen beyond the Flash stage in its colours (loaderColours:
    // yellow along the top, orange along the bottom).
    static const float colours[16] = {1, 0.79685f, 0, 1, 1, 0.79685f, 0, 1, 1, 0.4f, 0, 1, 1, 0.4f, 0, 1};
    static const unsigned short strip[6] = {0, 1, 2, 1, 3, 2};
    platform::drawMesh(0x4c4f414445520001ull, screen, 3, 4, colours, strip, 6);
    glColor(1, 1, 1, 1);
    // PreloaderView.update: the bar (preloader.inner) at the percent loaded,
    // its blobs a frame on each time.
    VectorArt& art = flashstage::art("flash-preloader");
    static const int clip = art.symbol("preloader_mc");
    if (clip >= 0) {
        double now = platform::passTime();
        if (blobClock < 0) blobClock = now;
        while (now - blobClock >= 1 / flashstage::kFps) {
            blobClock += 1 / flashstage::kFps;
            if (++blobFrame > 25) blobFrame = 1;
        }
        VectorLook look = preloaderLook(static_cast<int>(shown * 100), blobFrame);
        art.draw(clip, 1, 0, flashstage::at(), kPreloaderOwner, &look);
    }
    if (mLoadingState == 5) MenuManager::sharedMenuManager().killViewController();
}

}  // namespace as2
