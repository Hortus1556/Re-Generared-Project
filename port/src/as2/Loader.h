// Loader: loads and unloads the sounds and textures of the menus and of a
// mission a little each frame (40 ms of work), showing a progress screen
// (the Flash AS2's preloader: its orange stage, the blood blobs pulsing,
// the bomb's bar filling), then moves on to the next screen.
#pragma once

namespace as2 {

class Loader {
public:
    static Loader& sharedLoader();

    // What loading is under way (0: none): 1 the menus' sounds; 2 quitting
    // a mission; 3 a mission won; 4 a failed mission replayed (to its start
    // screen); 5 starting a mission; 6 a CFO mission failed (to its own
    // summary, the iPad's way).
    int mLoadingState = 0;

    void startMenuLoad();
    void startMission();
    void startReplay();
    void startMissionFailed();
    void startMissionComplete();
    void startQuitGame();
    // One frame of work, and the next screen when it is done.
    void Load();
    void render();
    // The loading screen's art made ahead, kept 40 seconds (by the screens
    // that start a loading: its first frame rasterised on the spot stalled).
    void prefetchArt();

    bool LoadMenuSounds();
    bool unLoadMenuSounds();
    bool LoadInitGame();
    bool unloadInitGame();
    float GetLoadProgress() const;

private:
    Loader();
    // The current mission's own sounds {key, file, extension}, and how many.
    const char* const (*getSoundsToLoad() const)[3];
    int getSoundCountToLoad() const;
    int menuSoundProgress = 0;
    int ingameProgress = 0;
    // Whether the screen after it may come: the loading screen shown long
    // enough for its bar to fill (the work is often done in a few frames).
    bool shownLongEnough();
    void restart();
    float screen[12];  // loaderScreen: the whole screen, as a strip
    float shown = 0;   // the bar's fill shown (0-1)
    double shownClock = -1;
    // The preloader's blobs (mc1, mc2: 25 frames each, a frame a Flash
    // frame).
    int blobFrame = 1;
    double blobClock = -1;
};

}  // namespace as2
