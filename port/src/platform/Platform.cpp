#include "platform/Platform.h"

#include <SDL3/SDL.h>
#if defined(__ANDROID__)
#include <android/choreographer.h>
#include <android/looper.h>
#endif
#if defined(__ANDROID__)
#include <EGL/egl.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "platform/DrawList.h"
#include "platform/GL.h"
#include "platform/WinShell.h"
#include "stb_image_write.h"

namespace platform {
namespace {

// ---- debug automation (see Platform.h)
struct Click {
    long frame;
    float x, y;
};
long gFrame = 0;
std::string gShotPrefix;
std::vector<long> gShotFrames;
float gShotT = 1;
// AS_FILM
std::string gFilmPrefix;
long gFilmFrom = 0, gFilmTo = -1;
int gFilmSub = 4;
bool gFilmImages = false;
std::vector<Click> gClicks;
// A press at the first point held along a path (straight segments, each
// over some frames), let go at its end.
struct Drag {
    long frame = 0, frames = 0;  // start, total frames
    bool hover = false;          // AS_HOVER: the pointer moved, no button
    std::vector<std::pair<float, float>> points;
    std::vector<long> segments;  // frames per segment
    std::pair<float, float> at(long k) const {
        for (size_t s = 0; s < segments.size(); ++s) {
            if (k <= segments[s] || s + 1 == segments.size()) {
                float t = std::min(1.0f, static_cast<float>(k) / segments[s]);
                auto [x1, y1] = points[s];
                auto [x2, y2] = points[s + 1];
                return {x1 + (x2 - x1) * t, y1 + (y2 - y1) * t};
            }
            k -= segments[s];
        }
        return points.back();
    }
};
std::vector<Drag> gDrags;
// Keys pressed at frames (AS_KEYS): "f1", "esc", a digit, "-" (the key
// after 0: numberKey 11), or another printable character (charKey).
struct Key {
    long frame;
    std::string key;
};
std::vector<Key> gKeys;
bool gShotsFinished = false;
std::string gMovieShotPrefix;
std::vector<double> gMovieShots;  // seconds into a movie
// AS_CONTROL: commands read from stdin while the game waits.
bool gControl = false;
long gControlUntil = 0;   // frames run without asking until this one
std::string gControlShot;  // a screenshot asked for, taken at the frame's end
// Pointer events asked for on this frame (down / move / up at x, y).
struct PointerEvent {
    char kind;  // 'd', 'm', 'u'
    float x, y;
};
std::vector<PointerEvent> gControlPointer;
std::string gControlText;  // typed this frame (the "text" command)
int gControlWindowW = 0, gControlWindowH = 0;  // a window size asked for (the "window" command)
float gControlX = 0, gControlY = 0;  // where the last one left the pointer

bool parseClick(const std::string& item) {
    Click c{};
    if (std::sscanf(item.c_str(), "%ld:%f:%f", &c.frame, &c.x, &c.y) != 3) return false;
    gClicks.push_back(c);
    return true;
}

bool parseKey(const std::string& item) {
    std::string::size_type colon = item.find(':');
    if (colon == std::string::npos) return false;
    gKeys.push_back({std::atol(item.c_str()), item.substr(colon + 1)});
    return true;
}

bool parseDrag(const std::string& item) {
    // frame:x1:y1:x2:y2:frames[:x3:y3:frames...]
    Drag d;
    std::istringstream fields(item);
    std::string f;
    std::vector<double> v;
    while (std::getline(fields, f, ':')) v.push_back(std::atof(f.c_str()));
    if (v.size() < 6 || (v.size() - 3) % 3 != 0) return false;
    d.frame = static_cast<long>(v[0]);
    d.points.push_back({static_cast<float>(v[1]), static_cast<float>(v[2])});
    for (size_t k = 3; k + 2 < v.size(); k += 3) {
        d.points.push_back({static_cast<float>(v[k]), static_cast<float>(v[k + 1])});
        long n = std::max(1L, static_cast<long>(v[k + 2]));
        d.segments.push_back(n);
        d.frames += n;
    }
    gDrags.push_back(d);
    return true;
}

// AS_CONTROL: at the frame the last `run` reached, says "@frame <n>" and
// reads commands until the next `run`. Frames in commands are absolute.
//   click <frame>:<x>:<y>   key <frame>:<key>   drag <AS_DRAG item>
//   down <x> <y>  move <x> <y>  up <x> <y>   (the pointer, on this frame)
//   hover <x> <y>           (the pointer moved, not pressed, on this frame)
//   text <chars>            (keys pressed, on this frame: charKey)
//   shot <file>             (this frame, once drawn)
//   window <w>x<h>          (the window resized: its shape changes)
//   run <frames>            quit
bool controlStep() {
    if (gFrame < gControlUntil) return true;
    std::printf("@frame %ld\n", gFrame);
    std::fflush(stdout);
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd, arg;
        in >> cmd;
        std::getline(in >> std::ws, arg);
        bool ok = true;
        if (cmd == "run") {
            gControlUntil = gFrame + std::max(1L, std::atol(arg.c_str()));
            return true;
        } else if (cmd == "quit") {
            return false;
        } else if (cmd == "click") {
            ok = parseClick(arg);
        } else if (cmd == "key") {
            ok = parseKey(arg);
        } else if (cmd == "drag") {
            ok = parseDrag(arg);
        } else if (cmd == "down" || cmd == "move" || cmd == "up" || cmd == "hover") {
            PointerEvent e{cmd[0], 0, 0};
            ok = std::sscanf(arg.c_str(), "%f %f", &e.x, &e.y) == 2;
            if (ok) gControlPointer.push_back(e);
        } else if (cmd == "text") {
            gControlText += arg;
        } else if (cmd == "shot") {
            gControlShot = arg;
        } else if (cmd == "window") {
            ok = std::sscanf(arg.c_str(), "%dx%d", &gControlWindowW, &gControlWindowH) == 2 && gControlWindowW > 0 &&
                 gControlWindowH > 0;
        } else {
            ok = false;
        }
        if (!ok) {
            std::printf("@bad %s\n", line.c_str());
            std::fflush(stdout);
        }
    }
    return false;  // stdin closed
}

void parseAutomation() {
    gControl = std::getenv("AS_CONTROL") != nullptr;
    if (const char* s = std::getenv("AS_SHOT")) {
        std::string v = s;
        std::string::size_type colon = v.rfind(':');
        // A drive letter ("C:\...") is not the frame separator.
        if (colon != std::string::npos && colon > 1) {
            gShotPrefix = v.substr(0, colon);
            std::istringstream frames(v.substr(colon + 1));
            std::string f;
            while (std::getline(frames, f, ',')) gShotFrames.push_back(std::atol(f.c_str()));
        } else {
            gShotPrefix = v;
            gShotFrames.push_back(60);
        }
    }
    if (const char* s = std::getenv("AS_SHOT_T")) gShotT = static_cast<float>(std::atof(s));
    if (const char* s = std::getenv("AS_MOVIE_SHOT")) {
        std::string v = s;
        std::string::size_type colon = v.rfind(':');
        if (colon != std::string::npos && colon > 1) {
            gMovieShotPrefix = v.substr(0, colon);
            std::istringstream times(v.substr(colon + 1));
            std::string t;
            while (std::getline(times, t, ',')) gMovieShots.push_back(std::atof(t.c_str()));
        }
    }
    if (const char* s = std::getenv("AS_FILM")) {
        std::string v = s;
        std::vector<std::string> parts;
        // The prefix may hold a drive letter's colon: the fields are the last ones.
        for (int k = 0; k < 4; ++k) {
            std::string::size_type c = v.rfind(':');
            if (c == std::string::npos || c <= 1) break;
            parts.insert(parts.begin(), v.substr(c + 1));
            v = v.substr(0, c);
        }
        if (parts.size() >= 3) {
            gFilmPrefix = v;
            gFilmImages = parts.size() == 4 && parts[3] == "img";
            if (parts.size() == 4 && !gFilmImages) {  // no :img, so the first field belongs to the prefix
                gFilmPrefix = v + ":" + parts[0];
                parts.erase(parts.begin());
            }
            gFilmFrom = std::atol(parts[0].c_str());
            gFilmTo = std::atol(parts[1].c_str());
            gFilmSub = std::max(1, std::atoi(parts[2].c_str()));
        }
    }
    auto each = [](const char* name, bool (*parse)(const std::string&)) {
        const char* s = std::getenv(name);
        if (!s) return;
        std::istringstream items(s);
        std::string item;
        while (std::getline(items, item, ',')) parse(item);
    };
    each("AS_CLICKS", parseClick);
    each("AS_KEYS", parseKey);
    each("AS_DRAG", parseDrag);
    each("AS_HOVER", [](const std::string& item) {
        if (!parseDrag(item)) return false;
        gDrags.back().hover = true;
        return true;
    });
}

}  // namespace

namespace {
bool gDebug = false;
const char* debugEnv(const char* name) { return gDebug ? std::getenv(name) : nullptr; }
}  // namespace

void setDebugMode(bool on) {
    gDebug = on;
    if (on) installCrashLog();
}
bool debugMode() { return gDebug; }
bool traceEnabled() { return gDebug; }

bool automationActive() {
    for (const char* name : {"AS_CONTROL", "AS_SHOT", "AS_MOVIE_SHOT", "AS_FILM", "AS_CLICKS", "AS_DRAG", "AS_HOVER", "AS_KEYS",
                             "AS_EVENT", "AS_HIDDEN"})
        if (debugEnv(name)) return true;
    return false;
}

int debugStartEvent(int fallback) {
    const char* s = debugEnv("AS_EVENT");
    return s ? std::atoi(s) : fallback;
}

namespace {
// The trace a play session leaves behind, in the save folder next to its
// save data (the one place a player already knows how to find). One file
// per run (overwritten on the next): a playtest is about what just
// happened, not a history.
FILE* debugLogFile() {
    static FILE* f = [] {
        FILE* fp = std::fopen((prefPath() + "debug.log").c_str(), "w");
        return fp;
    }();
    return f;
}
}  // namespace

void debugTrace(const char* fmt, ...) {
    if (!traceEnabled()) return;
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    static std::mutex mutex;  // (the display thread traces too)
    std::lock_guard<std::mutex> lock(mutex);
    std::puts(buf);
    std::fflush(stdout);
    if (FILE* f = debugLogFile()) {
        std::fputs(buf, f);
        std::fputc('\n', f);
        std::fflush(f);
    }
}

int debugAutoWinPasses() {
    const char* s = debugEnv("AS_AUTOWIN");
    return s ? std::atoi(s) : 0;
}

namespace {

void writeScreen(const std::string& file, float t);

void saveShot(long frame) { writeScreen(gShotPrefix + "_" + std::to_string(frame) + ".png", gShotT); }

SDL_Window* gWindow = nullptr;
GameInfo gGame{};  // set by init()
SDL_GLContext gContext = nullptr;
GLuint gFbo = 0;
GLuint gFboTexture = 0;
bool gMouseDown = false;
float gLastX = 0, gLastY = 0;
// The last pointer position given to the game (read by the display thread).
std::atomic<float> gPointerX{0}, gPointerY{0};
bool gShowFromPump = false;           // show() called by the event watch (a live resize)
void* gHwnd = nullptr;                // (Windows: the window, for the pointer from any thread)

// The display thread (presenter): its own context, sharing the textures of
// the game's, shows the frames on their own clock (DrawList's dueFrames)
// once per display refresh, so no logic pass holds the display. The game's
// thread keeps the window's events, the logic and the uploads.
SDL_GLContext gPresentContext = nullptr;
std::thread gPresenter;
// The texture thread: its own context too, uploading what the game's thread
// queues (DrawList's textureWorkLoop).
SDL_GLContext gTextureContext = nullptr;
std::thread gTextureThread;
std::atomic<bool> gPresenterStop{false};
// The app going to the background (Android): the display thread lets the
// window's surface go (gPresenterHold) and says so (gPresenterHolding), as
// the surface goes with the app; it takes the new one back on its return.
std::atomic<bool> gPresenterHold{false};
bool gPresenterHolding = false;  // (guarded by gPresentMutex)
std::mutex gPresentMutex;
std::condition_variable gPresentedCv;
unsigned long long gPresented = 0;  // display frames shown (guarded by gPresentMutex)
void startPresenter();
bool makeCurrentOffscreen(SDL_GLContext context);

PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers;
PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D;
PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus;
PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers;
PFNGLGENRENDERBUFFERSPROC pglGenRenderbuffers;
PFNGLBINDRENDERBUFFERPROC pglBindRenderbuffer;
PFNGLRENDERBUFFERSTORAGEPROC pglRenderbufferStorage;
PFNGLFRAMEBUFFERRENDERBUFFERPROC pglFramebufferRenderbuffer;
PFNGLDELETERENDERBUFFERSPROC pglDeleteRenderbuffers;
GLuint gFboStencil = 0;
bool gFboSupported = false;  // (Android: if the driver has framebuffers)

template <class F>
bool load(F& fn, const char* name) {
    fn = reinterpret_cast<F>(SDL_GL_GetProcAddress(name));
    return fn != nullptr;
}

// Largest rectangle of a game screen's shape (sw x sh) centred in the
// window, in window pixels. A screen within one of its units of the
// window's shape fills it (a game sizing its screen to the window in whole
// units: no band of a pixel or two; the two axes then differ by less than a
// unit's share, which does not show).
SDL_Rect letterbox(int sw, int sh) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(gWindow, &w, &h);
    SDL_Rect r;
    if (sw <= 0 || sh <= 0 || w <= 0 || h <= 0) return SDL_Rect{0, 0, std::max(w, 1), std::max(h, 1)};
    if (static_cast<long long>(w) * sh > static_cast<long long>(h) * sw) {
        r.h = h;
        r.w = static_cast<int>(static_cast<long long>(h) * sw / sh);
        if (w - r.w <= r.w / sw) r.w = w;
    } else {
        r.w = w;
        r.h = static_cast<int>(static_cast<long long>(w) * sh / sw);
        if (h - r.h <= r.h / sh) r.h = h;
    }
    r.x = (w - r.w) / 2;
    r.y = (h - r.h) / 2;
    return r;
}
// The game's current screen, as touches map to it.
SDL_Rect letterbox() { return letterbox(gGame.screenWidth, gGame.screenHeight); }
// The frame on show, at the size it was recorded at.
SDL_Rect shownLetterbox() {
    int sw = 0, sh = 0;
    shownFrameSize(sw, sh);
    return letterbox(sw, sh);
}

// Window (logical) mouse coordinates -> game coordinates.
void toGame(float wx, float wy, float& gx, float& gy) {
    float scale = SDL_GetWindowPixelDensity(gWindow);
    SDL_Rect r = letterbox();
    gx = (wx * scale - r.x) * gGame.screenWidth / r.w;
    gy = (wy * scale - r.y) * gGame.screenHeight / r.h;
}

// The offscreen frame (debug screenshots, AS_FILM) at a game screen's size,
// made again when that size changes. False if it cannot be made. It has
// kShotScale times the pixels each way, averaged down when read (the
// window's multisampling smooths the edges of what the GPU draws as
// triangles; this does for the shots).
constexpr int kShotScale = 4;
int gFboWidth = 0, gFboHeight = 0;
bool makeFbo(int w, int h);
bool useFbo(int w, int h) {
    if (w == gFboWidth && h == gFboHeight && gFbo) return true;
    return makeFbo(w, h);
}

}  // namespace

const GameInfo& game() { return gGame; }

namespace {
std::vector<std::function<void()>> gScaleListeners;
float gNotifiedScale = 0;
}  // namespace

float displayScale() {
    if (!gWindow) return 1;
    return static_cast<float>(letterbox().w) / static_cast<float>(gGame.screenWidth);
}

void onDisplayScaleChange(std::function<void()> f) { gScaleListeners.push_back(std::move(f)); }
int screenWidth() { return gGame.screenWidth; }
int screenHeight() { return gGame.screenHeight; }

namespace {
void notifyDisplayScale() {
    if (displayScale() == gNotifiedScale) return;
    gNotifiedScale = displayScale();
    for (auto& f : gScaleListeners) f();
}
std::vector<std::function<void()>> gShapeListeners;
float gNotifiedAspect = 0;
float gAspectOverride = 0;  // (debug: setAspectOverride)
float gEnvAspect = -1;      // (debug: AS_ASPECT, read once; 0 none)
}  // namespace

void setScreenSize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == gGame.screenWidth && h == gGame.screenHeight)) return;
    gGame.screenWidth = w;
    gGame.screenHeight = h;
    debugTrace("screen size %dx%d", w, h);
    if (gWindow) notifyDisplayScale();
}

float windowAspect() {
    if (gDebug) {
        if (gAspectOverride > 0) return gAspectOverride;
        if (gEnvAspect < 0) {
            gEnvAspect = 0;
            float aw = 0, ah = 0;
            if (const char* s = std::getenv("AS_ASPECT"))
                if (std::sscanf(s, "%f:%f", &aw, &ah) == 2 && aw > 0 && ah > 0) gEnvAspect = aw / ah;
        }
        if (gEnvAspect > 0) return gEnvAspect;
    }
    int w = 0, h = 0;
    if (gWindow) SDL_GetWindowSizeInPixels(gWindow, &w, &h);
    return w > 0 && h > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
}

void setAspectOverride(float aspect) {
    if (!gDebug) return;
    gAspectOverride = aspect > 0 ? aspect : 0;
    debugTrace("window shape override %.4f", gAspectOverride);
    gNotifiedAspect = windowAspect();
    for (auto& f : gShapeListeners) f();
}

void onWindowShapeChange(std::function<void()> f) { gShapeListeners.push_back(std::move(f)); }

void fatal(const std::string& message) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, gGame.title, message.c_str(), gWindow);
}

bool init(const GameInfo& game) {
    gGame = game;
    installCrashLog();  // (in every run: a player's crash leaves crash.log)
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_SENSOR)) {
        fatal(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }
#if defined(__ANDROID__)
    // Debug on a device (there is no command line): a file debug_env.txt in
    // the save folder, NAME=VALUE lines ("{save}" in a value is that folder;
    // written with `adb shell run-as` on a --test build), turns debug mode
    // on with those variables; stdout and stderr go to stdout.txt and
    // stderr.txt beside it.
    {
        const std::string dir = prefPath();
        size_t n = 0;
        if (void* raw = SDL_LoadFile((dir + "debug_env.txt").c_str(), &n)) {
            std::istringstream lines(std::string(static_cast<const char*>(raw), n));
            SDL_free(raw);
            std::string line;
            while (std::getline(lines, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                const size_t eq = line.find('=');
                if (eq == std::string::npos || eq == 0) continue;
                std::string value = line.substr(eq + 1);
                for (size_t at; (at = value.find("{save}")) != std::string::npos;) value.replace(at, 6, dir);
                setenv(line.substr(0, eq).c_str(), value.c_str(), 1);
            }
            std::freopen((dir + "stdout.txt").c_str(), "w", stdout);
            std::freopen((dir + "stderr.txt").c_str(), "w", stderr);
            setDebugMode(true);
        }
    }
    // SDL indexes the APK's assets on first use, and a second thread asking
    // meanwhile finds the index empty (the packages are read on worker
    // threads): the index is made here, on this thread, first.
    {
        SDL_PathInfo info;
        SDL_GetPathInfo("game", &info);
    }
    // OpenGL ES 1.1: the original's own fixed-function pipeline (GL.h).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);  // masks (the Flash games)
    // Multisampling: the smooth edges of vector art the GPU draws as
    // triangles (flash/VectorMesh). Without it where none is had.
    // (debug AS_MSAA=<samples>, 0: none)
    int samples = 4;
    if (const char* n = gDebug ? std::getenv("AS_MSAA") : nullptr) samples = std::atoi(n);
    auto multisample = [](int n) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, n > 0 ? 1 : 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, n > 0 ? n : 0);
    };
    multisample(samples);

    // Start as large as fits comfortably on the primary display: whole
    // multiples of a small original screen (iPhone), else shrunk until it fits.
    int w = gGame.screenWidth, h = gGame.screenHeight;
    if (const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
        for (int k = 2; gGame.screenWidth * k * 1.25f <= dm->w && gGame.screenHeight * k * 1.25f <= dm->h; ++k) {
            w = gGame.screenWidth * k;
            h = gGame.screenHeight * k;
        }
        while (w * 1.25f > dm->w || h * 1.25f > dm->h) {
            w = w * 3 / 4;
            h = h * 3 / 4;
        }
    }
    // Debug automation (AS_HIDDEN=1): no window on the desktop; frames and
    // AS_SHOT screenshots still render. AS_WINDOW=<w>x<h>: the window's size.
    bool hidden = gDebug && std::getenv("AS_HIDDEN");
    // (AS_ASPECT=<w>:<h> without AS_WINDOW: the window starts at that shape,
    // as high as it would be)
    if (const char* shape = gDebug ? std::getenv("AS_ASPECT") : nullptr) {
        float aw = 0, ah = 0;
        if (std::sscanf(shape, "%f:%f", &aw, &ah) == 2 && aw > 0 && ah > 0) w = static_cast<int>(h * aw / ah + 0.5f);
    }
    if (const char* size = gDebug ? std::getenv("AS_WINDOW") : nullptr) std::sscanf(size, "%dx%d", &w, &h);
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                            (hidden ? SDL_WINDOW_HIDDEN : 0);
#if defined(__ANDROID__)
    // A phone: the whole screen, turned to landscape (the game's 4:3 screen
    // letterboxed in it), the back button as Escape.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    gWindow = SDL_CreateWindow(gGame.title, w, h, flags);
    if (!gWindow && samples > 0) {
        multisample(0);
        gWindow = SDL_CreateWindow(gGame.title, w, h, flags);
    }
    if (!gWindow) {
        fatal(std::string("Could not create the window: ") + SDL_GetError());
        return false;
    }
#if defined(_WIN32)
    if (void* hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(gWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)) {
        useExeIcon(hwnd);
        gHwnd = hwnd;
    }
#endif
    gContext = SDL_GL_CreateContext(gWindow);
    if (!gContext && samples > 0) {
        multisample(0);
        gContext = SDL_GL_CreateContext(gWindow);
    }
    if (!gContext) {
        fatal(std::string("Could not create an OpenGL context: ") + SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(1);
    if (gDebug) parseAutomation();

#if defined(__ANDROID__)
    // The offscreen frame is for debug screenshots only: OpenGL ES 1.1 has
    // it as an extension (its names end in OES); without it, no shots.
    const bool fboOk =
        load(pglGenFramebuffers, "glGenFramebuffersOES") && load(pglBindFramebuffer, "glBindFramebufferOES") &&
        load(pglFramebufferTexture2D, "glFramebufferTexture2DOES") &&
        load(pglCheckFramebufferStatus, "glCheckFramebufferStatusOES") &&
        load(pglDeleteFramebuffers, "glDeleteFramebuffersOES") && load(pglGenRenderbuffers, "glGenRenderbuffersOES") &&
        load(pglBindRenderbuffer, "glBindRenderbufferOES") && load(pglRenderbufferStorage, "glRenderbufferStorageOES") &&
        load(pglFramebufferRenderbuffer, "glFramebufferRenderbufferOES") &&
        load(pglDeleteRenderbuffers, "glDeleteRenderbuffersOES");
    gFboSupported = fboOk;
    if (fboOk) makeFbo(gGame.screenWidth, gGame.screenHeight);
#else
    if (!load(pglGenFramebuffers, "glGenFramebuffers") || !load(pglBindFramebuffer, "glBindFramebuffer") ||
        !load(pglFramebufferTexture2D, "glFramebufferTexture2D") ||
        !load(pglCheckFramebufferStatus, "glCheckFramebufferStatus") ||
        !load(pglDeleteFramebuffers, "glDeleteFramebuffers") || !load(pglGenRenderbuffers, "glGenRenderbuffers") ||
        !load(pglBindRenderbuffer, "glBindRenderbuffer") || !load(pglRenderbufferStorage, "glRenderbufferStorage") ||
        !load(pglFramebufferRenderbuffer, "glFramebufferRenderbuffer") ||
        !load(pglDeleteRenderbuffers, "glDeleteRenderbuffers")) {
        fatal("This graphics driver does not support OpenGL framebuffer objects (OpenGL 3.0).");
        return false;
    }
    gFboSupported = true;
    if (!makeFbo(gGame.screenWidth, gGame.screenHeight)) {
        fatal("Could not create the offscreen framebuffer.");
        return false;
    }
#endif
    gNotifiedAspect = windowAspect();
    // (AS_ONE_THREAD=1, debug: the game's thread shows its frames itself, as
    // where the display thread cannot be had, to compare.)
    if (!debugEnv("AS_ONE_THREAD")) startPresenter();
    // While the window's edge is dragged, Windows keeps the event loop in its
    // own: the game cannot run a pass, but the frame on screen is shown again
    // at the new size, so the picture follows the window.
    SDL_AddEventWatch(
        [](void*, SDL_Event* e) {
            // The app to the background (Android: called on the game's thread
            // as SDL lets the window's surface go, before it puts the game's
            // context aside): the display thread lets go of the surface first.
            // Back (SDL has made the game's context current with the new
            // surface): the game's context without one again, and the display
            // thread takes the surface.
            if (e->type == SDL_EVENT_WILL_ENTER_BACKGROUND && gPresenter.joinable()) {
                gPresenterHold = true;
                std::unique_lock<std::mutex> lock(gPresentMutex);
                gPresentedCv.wait_for(lock, std::chrono::seconds(1), [] { return gPresenterHolding; });
            } else if (e->type == SDL_EVENT_DID_ENTER_FOREGROUND && gPresenter.joinable()) {
#if defined(__ANDROID__)
                SDL_GL_MakeCurrent(gWindow, nullptr);
#endif
                makeCurrentOffscreen(gContext);
                gPresenterHold = false;
            }
            if (e->type == SDL_EVENT_WINDOW_EXPOSED && e->window.data1 == 1) {
                gShowFromPump = true;  // (inside SDL's pump: no pump again)
                show(1);
                gShowFromPump = false;
            }
            return true;
        },
        nullptr);
    // The accelerometer, if the device has one, opened now (the trace says
    // which).
    float tiltX, tiltY;
    deviceTilt(tiltX, tiltY);
    return true;
}

namespace {

// The first accelerometer the system has, opened on first use (none: null).
SDL_Sensor* accelerometer() {
    static SDL_Sensor* sensor = [] {
        SDL_Sensor* found = nullptr;
        int count = 0;
        SDL_SensorID* ids = SDL_GetSensors(&count);
        for (int i = 0; ids && i < count && !found; ++i)
            if (SDL_GetSensorTypeForID(ids[i]) == SDL_SENSOR_ACCEL) found = SDL_OpenSensor(ids[i]);
        SDL_free(ids);
        debugTrace("accelerometer %s", found ? SDL_GetSensorName(found) : "none");
        return found;
    }();
    return sensor;
}

// An orientation as quarter turns counter-clockwise from portrait.
int quarterTurns(SDL_DisplayOrientation o) {
    switch (o) {
        case SDL_ORIENTATION_LANDSCAPE: return 1;  // portrait's right side up
        case SDL_ORIENTATION_PORTRAIT_FLIPPED: return 2;
        case SDL_ORIENTATION_LANDSCAPE_FLIPPED: return 3;
        default: return 0;
    }
}

}  // namespace

bool deviceAcceleration(float& x, float& y, float& z) {
    SDL_Sensor* sensor = accelerometer();
    float a[3];
    if (!sensor || !SDL_GetSensorData(sensor, a, 3)) return false;
    x = a[0] / SDL_STANDARD_GRAVITY;
    y = a[1] / SDL_STANDARD_GRAVITY;
    z = a[2] / SDL_STANDARD_GRAVITY;
    return true;
}

bool deviceTilt(float& x, float& y) {
    SDL_Sensor* sensor = accelerometer();
    float a[3];
    if (!sensor || !SDL_GetSensorData(sensor, a, 3)) return false;
    // SDL gives the device's own axes as in its natural orientation (x
    // right, y up), in m/s^2, pointing away from the ground at rest. The
    // screen is shown turned from that by the display's orientation: the
    // vector turned the same way is in the screen's axes.
    SDL_DisplayID display = gWindow ? SDL_GetDisplayForWindow(gWindow) : 0;
    int turns = display ? (quarterTurns(SDL_GetCurrentDisplayOrientation(display)) -
                           quarterTurns(SDL_GetNaturalDisplayOrientation(display)) + 4) % 4
                        : 0;
    float sx = a[0], sy = a[1];
    for (int t = 0; t < turns; ++t) {
        float r = -sy;
        sy = sx;
        sx = r;
    }
    // "Up" leaning left means the right edge is lower; leaning to the top,
    // the bottom edge.
    x = -sx / SDL_STANDARD_GRAVITY;
    y = sy / SDL_STANDARD_GRAVITY;
    return true;
}

namespace {

bool makeFbo(int w, int h) {
    if (!gFboSupported || w <= 0 || h <= 0) return false;
    if (gFbo) pglDeleteFramebuffers(1, &gFbo);
    if (gFboStencil) pglDeleteRenderbuffers(1, &gFboStencil);
    if (gFboTexture) glDeleteTextures(1, &gFboTexture);
    gFbo = gFboStencil = gFboTexture = 0;
    gFboWidth = w;
    gFboHeight = h;
    w *= kShotScale;
    h *= kShotScale;
    glGenTextures(1, &gFboTexture);
    glBindTexture(GL_TEXTURE_2D, gFboTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if defined(__ANDROID__)
    // (OpenGL ES 1.1: the framebuffer names end in OES)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    pglGenFramebuffers(1, &gFbo);
    pglBindFramebuffer(GL_FRAMEBUFFER_OES, gFbo);
    pglFramebufferTexture2D(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, gFboTexture, 0);
    pglGenRenderbuffers(1, &gFboStencil);
    pglBindRenderbuffer(GL_RENDERBUFFER_OES, gFboStencil);
    pglRenderbufferStorage(GL_RENDERBUFFER_OES, GL_DEPTH24_STENCIL8_OES, w, h);
    pglFramebufferRenderbuffer(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, gFboStencil);
    pglFramebufferRenderbuffer(GL_FRAMEBUFFER_OES, GL_STENCIL_ATTACHMENT_OES, GL_RENDERBUFFER_OES, gFboStencil);
    pglBindRenderbuffer(GL_RENDERBUFFER_OES, 0);
    const bool ok = pglCheckFramebufferStatus(GL_FRAMEBUFFER_OES) == GL_FRAMEBUFFER_COMPLETE_OES;
    pglBindFramebuffer(GL_FRAMEBUFFER_OES, 0);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    pglGenFramebuffers(1, &gFbo);
    pglBindFramebuffer(GL_FRAMEBUFFER, gFbo);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gFboTexture, 0);
    pglGenRenderbuffers(1, &gFboStencil);
    pglBindRenderbuffer(GL_RENDERBUFFER, gFboStencil);
    pglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gFboStencil);
    pglBindRenderbuffer(GL_RENDERBUFFER, 0);
    const bool ok = pglCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!ok) {
        pglDeleteFramebuffers(1, &gFbo);
        gFbo = 0;
    }
    return ok;
}

}  // namespace

void shutdown() {
    if (gPresenter.joinable()) {
        gPresenterStop = true;
        gPresenter.join();
        gTextureThread.join();
        setDisplayThread(false);
    }
    if (gPresentContext) SDL_GL_DestroyContext(gPresentContext);
    if (gTextureContext) SDL_GL_DestroyContext(gTextureContext);
    if (gFbo) pglDeleteFramebuffers(1, &gFbo);
    if (gFboStencil) pglDeleteRenderbuffers(1, &gFboStencil);
    if (gFboTexture) glDeleteTextures(1, &gFboTexture);
    if (gContext) SDL_GL_DestroyContext(gContext);
    if (gWindow) SDL_DestroyWindow(gWindow);
    SDL_Quit();
}

namespace {

// The shown frame at the original screen's size, read back as a PNG (debug
// screenshots, so they are the same size on every display).
void writeScreen(const std::string& file, float t) {
    finishTextureWork();  // (drawn by this thread's context: every texture there)
#if !defined(__ANDROID__)
    // (AS_SHOT_NATIVE=1: at the window's own pixels instead, letterboxed as
    // the window shows it: how a big window looks)
    static const bool native = debugEnv("AS_SHOT_NATIVE") != nullptr;
    if (native && gWindow) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(gWindow, &w, &h);
        if (w > 0 && h > 0) {
            GLuint tex = 0, depth = 0, fbo = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);
            pglGenFramebuffers(1, &fbo);
            pglBindFramebuffer(GL_FRAMEBUFFER, fbo);
            pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            pglGenRenderbuffers(1, &depth);
            pglBindRenderbuffer(GL_RENDERBUFFER, depth);
            pglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
            pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
            pglBindRenderbuffer(GL_RENDERBUFFER, 0);
            SDL_Rect r = shownLetterbox();
            glViewport(0, 0, w, h);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glViewport(r.x, h - (r.y + r.h), r.w, r.h);
            glScissor(r.x, h - (r.y + r.h), r.w, r.h);
            glEnable(GL_SCISSOR_TEST);
            renderFrame(t);
            glDisable(GL_SCISSOR_TEST);
            std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            pglBindFramebuffer(GL_FRAMEBUFFER, 0);
            pglDeleteFramebuffers(1, &fbo);
            pglDeleteRenderbuffers(1, &depth);
            glDeleteTextures(1, &tex);
            stbi_flip_vertically_on_write(1);
            stbi_write_png(file.c_str(), w, h, 4, px.data(), w * 4);
            return;
        }
    }
#endif
    // (at the shown frame's own size)
    int w = 0, h = 0;
    shownFrameSize(w, h);
    if (!useFbo(w, h)) return;
    pglBindFramebuffer(GL_FRAMEBUFFER, gFbo);
    const int bw = w * kShotScale, bh = h * kShotScale;
    glViewport(0, 0, bw, bh);
    renderFrame(t);
    std::vector<unsigned char> big(static_cast<size_t>(bw) * bh * 4);
    glReadPixels(0, 0, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, big.data());
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int k = 0; k < 4; ++k) {
                int sum = 0;
                for (int j = 0; j < kShotScale; ++j)
                    for (int i = 0; i < kShotScale; ++i)
                        sum += big[((static_cast<size_t>(y) * kShotScale + j) * bw + x * kShotScale + i) * 4 + k];
                px[(static_cast<size_t>(y) * w + x) * 4 + k] =
                    static_cast<unsigned char>(sum / (kShotScale * kShotScale));
            }
    // The replay's NDC y points up, so row 0 read back is the screen's bottom.
    stbi_flip_vertically_on_write(1);
    stbi_write_png(file.c_str(), w, h, 4, px.data(), w * 4);
}

// AS_FILM: every display step of the pass just ended, its draws logged to
// <prefix>.csv (frame, step, context, owner, group, texture, x, y) and, with
// :img, saved as <prefix>_<frame>_<step>.png.
void filmFrame(long frame) {
    finishTextureWork();
    static FILE* csv = nullptr;
    if (!csv) csv = std::fopen((gFilmPrefix + ".csv").c_str(), "w");
    std::vector<TracedDraw> draws;
    for (int k = 0; k < gFilmSub; ++k) {
        float t = static_cast<float>(k) / gFilmSub;
        draws.clear();
        traceDraws(&draws);
        if (gFilmImages) {
            writeScreen(gFilmPrefix + "_" + std::to_string(frame) + "_" + std::to_string(k) + ".png", t);
        } else {
            int w = 0, h = 0;
            shownFrameSize(w, h);
            if (useFbo(w, h)) {
                pglBindFramebuffer(GL_FRAMEBUFFER, gFbo);
                glViewport(0, 0, w * kShotScale, h * kShotScale);
                renderFrame(t);
                pglBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }
        traceDraws(nullptr);
        for (const TracedDraw& d : draws)
            std::fprintf(csv, "%ld,%d,%llu,%llu,%llu,%u,%.2f,%.2f\n", frame, k, d.context, d.owner, d.group,
                         d.texture, d.x, d.y);
    }
    std::fflush(csv);
}

// F2: the next frame is saved as snap_<n>.png in the save folder (the first
// unused n), for reporting bugs.
bool gSnapNow = false;
std::string gLastSnapshot;
double gLastSnapshotTime = 0;

void saveSnapshot() {
    std::string file;
    for (int n = 1;; ++n) {
        file = prefPath() + "snap_" + std::to_string(n) + ".png";
        if (!SDL_GetPathInfo(file.c_str(), nullptr)) break;
    }
    writeScreen(file, 1);
    gLastSnapshot = file;
    gLastSnapshotTime = now();
    std::printf("snapshot %s\n", file.c_str());
    std::fflush(stdout);
}

std::atomic<bool> gSmoothMotion{true};
double gLastSwap = 0;

}  // namespace

bool smoothMotion() { return gSmoothMotion; }

const std::string& lastSnapshot(double& when) {
    when = gLastSnapshotTime;
    return gLastSnapshot;
}
void setSmoothMotion(bool on) { gSmoothMotion = on; }

void setHandCursor(bool hand) {
    if (touchDevice()) return;  // (no pointer to show)
    static bool shown = false;
    static SDL_Cursor* handCursor = nullptr;
    static SDL_Cursor* arrow = nullptr;
    if (hand == shown) return;
    if (!handCursor) handCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    if (!arrow) arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    if (SDL_Cursor* c = hand ? handCursor : arrow) {
        SDL_SetCursor(c);
        shown = hand;
    }
}

void livePointer(float& x, float& y) {
    x = gPointerX;
    y = gPointerY;
    if (gControl || !gClicks.empty() || !gDrags.empty() || !gWindow) return;
    // Windows: from the system now (the display thread asks between the
    // game's event pumps).
    float cx, cy, cw, ch;
    if (gHwnd && cursorInClient(gHwnd, cx, cy, cw, ch)) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(gWindow, &w, &h);
        SDL_Rect r = letterbox();
        if (r.w > 0 && r.h > 0) {
            x = (cx * w / cw - r.x) * gGame.screenWidth / r.w;
            y = (cy * h / ch - r.y) * gGame.screenHeight / r.h;
        }
        return;
    }
    // (Fresh: show() pumps the events before each display frame.)
    float wx, wy;
    SDL_GetMouseState(&wx, &wy);
    toGame(wx, wy, x, y);
}

bool touchDevice() {
#if defined(__ANDROID__) || defined(SDL_PLATFORM_IOS)
    return true;
#else
    return false;
#endif
}

float screenShortSideDp() {
    SDL_DisplayID display = gWindow ? SDL_GetDisplayForWindow(gWindow) : SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = display ? SDL_GetCurrentDisplayMode(display) : nullptr;
    if (!mode || mode->w <= 0 || mode->h <= 0) return 0;
    const float side = static_cast<float>(std::min(mode->w, mode->h));
#if defined(SDL_PLATFORM_IOS)
    return side;  // (the mode is in UIKit's points already)
#else
    // (the mode's pixels over the system's content scale: Android's
    // density, 1 at 160 dpi; a PC's display scaling, 1 at 96 dpi)
    const float pixels = side * (mode->pixel_density > 0 ? mode->pixel_density : 1);
    const float scale = SDL_GetDisplayContentScale(display);
    return pixels / (scale > 0 ? scale : 1);
#endif
}

bool largeScreen() {
    if (!touchDevice()) return true;  // (a PC: any window counts as large)
    return screenShortSideDp() >= 600;
}

std::string systemFont(const std::string& kind) {
#if defined(__ANDROID__)
    // Android's own faces (AOSP's /system/fonts; vendors keep them): Roboto
    // for Helvetica, a handwritten one for Marker Felt, a monospace, a serif.
    static const std::vector<std::pair<std::string, std::vector<const char*>>> kinds = {
        {"sans", {"Roboto-Regular.ttf", "RobotoStatic-Regular.ttf", "DroidSans.ttf"}},
        {"sans-bold", {"Roboto-Bold.ttf", "RobotoStatic-Bold.ttf", "DroidSans-Bold.ttf", "Roboto-Regular.ttf"}},
        {"hand", {"ComingSoon.ttf", "DancingScript-Regular.ttf", "Roboto-Regular.ttf"}},
        {"hand-wide", {"ComingSoon.ttf", "DancingScript-Bold.ttf", "Roboto-Regular.ttf"}},
        {"mono", {"CutiveMono.ttf", "DroidSansMono.ttf", "Roboto-Regular.ttf"}},
        {"mono-bold", {"CutiveMono.ttf", "DroidSansMono.ttf", "Roboto-Bold.ttf"}},
        {"serif", {"NotoSerif-Regular.ttf", "DroidSerif-Regular.ttf", "Roboto-Regular.ttf"}},
        {"serif-bold", {"NotoSerif-Bold.ttf", "DroidSerif-Bold.ttf", "Roboto-Bold.ttf"}},
    };
    const std::string dir = "/system/fonts/";
#else
    // Windows: Arial for Helvetica (its metrics), Segoe Print for Marker
    // Felt (the nearest handwritten face it ships).
    static const std::vector<std::pair<std::string, std::vector<const char*>>> kinds = {
        {"sans", {"arial.ttf"}},     {"sans-bold", {"arialbd.ttf"}}, {"hand", {"segoepr.ttf"}},
        {"hand-wide", {"segoeprb.ttf"}}, {"mono", {"cour.ttf"}},       {"mono-bold", {"courbd.ttf"}},
        {"serif", {"georgia.ttf"}},  {"serif-bold", {"georgiab.ttf"}},
    };
    const char* windir = std::getenv("WINDIR");
    const std::string dir = std::string(windir ? windir : "C:\\Windows") + "\\Fonts\\";
#endif
    for (const auto& [name, files] : kinds) {
        if (name != kind) continue;
        for (const char* f : files) {
            SDL_PathInfo info;
            if (SDL_GetPathInfo((dir + f).c_str(), &info) && info.type == SDL_PATHTYPE_FILE) return dir + f;
        }
    }
    return std::string();
}

void toTexelOrder(unsigned char* bgra, size_t pixels) {
#if defined(__ANDROID__)
    for (size_t i = 0; i < pixels; ++i, bgra += 4) std::swap(bgra[0], bgra[2]);
#else
    (void)bgra;
    (void)pixels;
#endif
}

void setSystemCursorVisible(bool visible) {
    if (touchDevice()) return;  // (no pointer to show)
    static bool shown = true;
    if (visible == shown) return;
    if (visible) SDL_ShowCursor();
    else SDL_HideCursor();
    shown = visible;
}

void endFrame() {
    commitFrame();
    ++gFrame;
    if (gSnapNow) {
        gSnapNow = false;
        saveSnapshot();
    }
    if (!gControlShot.empty()) {
        writeScreen(gControlShot, gShotT);
        gControlShot.clear();
    }
    for (long f : gShotFrames)
        if (f == gFrame) {
            saveShot(gFrame);
            if (f == gShotFrames.back()) gShotsFinished = true;
        }
    if (gFrame >= gFilmFrom && gFrame <= gFilmTo) {
        filmFrame(gFrame);
        if (gFrame == gFilmTo) gShotsFinished = true;
    }
}

namespace {

// The window cleared and the frame drawn into it, letterboxed at its own
// size, `t` of the way from the one before.
void drawWindow(const DueFrames* frames, float t) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(gWindow, &w, &h);
    SDL_Rect r = frames ? letterbox(frames->width, frames->height) : shownLetterbox();
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    // GL's window origin is bottom-left.
    glViewport(r.x, h - (r.y + r.h), r.w, r.h);
    glScissor(r.x, h - (r.y + r.h), r.w, r.h);
    glEnable(GL_SCISSOR_TEST);
    if (frames) renderFrames(*frames, t);
    else renderFrame(t);
    glDisable(GL_SCISSOR_TEST);
}

#if defined(__ANDROID__)
// Android's vsync (its Choreographer) for the display thread.
struct Vsync {
    bool posted = false, fired = false;
};
void onVsync(long, void* data) { static_cast<Vsync*>(data)->fired = true; }
#endif

void presentLoop() {
    // (It waits for the display almost all the time: on time whatever else
    // keeps the processor busy, the raster workers' bursts or other programs.)
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    SDL_GL_MakeCurrent(gWindow, gPresentContext);
#if defined(__ANDROID__)
    // On Android a frame is drawn once a refresh, just after the vsync, and
    // its swap never waits: drawn as fast as the swap let it, Android's
    // buffer queue stayed full, each frame chosen, then waiting for a free
    // buffer, then for its turn on screen, two or three refreshes after.
    ALooper_prepare(0);
    AChoreographer* choreographer = AChoreographer_getInstance();
    Vsync vsync;
    const int interval = choreographer ? 0 : 1;
#else
    const int interval = 1;
#endif
    SDL_GL_SetSwapInterval(interval);
    double last = 0;
    // AS_DISPLAY_LOG=<file> (debug): each display frame's time, the frame
    // shown and how far it is eased in, for checking the display's pace.
    FILE* log = nullptr;
    if (const char* file = debugEnv("AS_DISPLAY_LOG")) log = std::fopen(file, "w");
    DueFrames held;  // (what was shown, held while nothing new is ready)
    while (!gPresenterStop) {
        if (gPresenterHold) {
            SDL_GL_MakeCurrent(gWindow, nullptr);
            {
                std::lock_guard<std::mutex> lock(gPresentMutex);
                gPresenterHolding = true;
            }
            gPresentedCv.notify_all();
            while (gPresenterHold && !gPresenterStop) sleepSeconds(0.01);
            {
                std::lock_guard<std::mutex> lock(gPresentMutex);
                gPresenterHolding = false;
            }
            SDL_GL_MakeCurrent(gWindow, gPresentContext);
            SDL_GL_SetSwapInterval(interval);
            continue;
        }
#if defined(__ANDROID__)
        if (choreographer) {
            if (!vsync.posted) {
                vsync.posted = true;
                vsync.fired = false;
                AChoreographer_postFrameCallback(choreographer, onVsync, &vsync);
            }
            while (!vsync.fired && !gPresenterStop && !gPresenterHold) ALooper_pollOnce(50, nullptr, nullptr, nullptr);
            if (!vsync.fired) continue;
            vsync.posted = false;
        }
#endif
        const double start = now();
        DueFrames frames;
        bool any = dueFrames(start, frames);
        if (any) {
            held = frames;
        } else if (held.shown) {
            frames = held;
            frames.t = 1;
            any = true;
        }
        const double picked = now();
        if (any) {
            DrawTurn turn;
            drawWindow(&frames, gSmoothMotion ? frames.t : 1.0f);
        } else {
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        const double drawn = now();
        SDL_GL_SwapWindow(gWindow);
        if (log && any) {
            int vertices = 0;
            const int glDraws = takeGlDraws(&vertices);
            std::fprintf(log, "%.6f %llu %.4f %.2f %.2f %zu %d %d\n", start, frames.seq, frames.t, (drawn - picked) * 1000,
                         (now() - drawn) * 1000, frames.draws, glDraws, vertices);
        }
        // Debug mode: a display frame long after the last (a visible hitch).
        const double done = now();
        if (traceEnabled() && last > 0 && done - last > 0.05)
            debugTrace("slow display frame (%.0f ms since the last): replay and swap %.1f ms", (done - last) * 1000,
                       (done - start) * 1000);
        // Without vsync (a hidden or minimised window) the swap returns at
        // once: not faster than 240 frames per second.
        if (done - last < 1.0 / 240) sleepSeconds(1.0 / 240 - (done - last));
        last = now();
        {
            std::lock_guard<std::mutex> lock(gPresentMutex);
            ++gPresented;
        }
        gPresentedCv.notify_all();
    }
    if (log) std::fclose(log);
    SDL_GL_MakeCurrent(gWindow, nullptr);
}

// The game's and the texture thread's contexts made current on their thread:
// on Android with no surface (an EGL surface is current in one context at a
// time, the display thread's; theirs draw nowhere), by EGL itself, as SDL's
// Android backend makes no context current at all when given no window;
// elsewhere with the window (WGL needs one; they never draw to it).
bool makeCurrentOffscreen(SDL_GLContext context) {
#if defined(__ANDROID__)
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (!extensions || !std::strstr(extensions, "EGL_KHR_surfaceless_context")) return false;
    return eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, static_cast<EGLContext>(context)) == EGL_TRUE;
#else
    return SDL_GL_MakeCurrent(gWindow, context);
#endif
}

void releaseOffscreen() {
#if defined(__ANDROID__)
    eglMakeCurrent(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
#else
    SDL_GL_MakeCurrent(gWindow, nullptr);
#endif
}

void startPresenter() {
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    gPresentContext = SDL_GL_CreateContext(gWindow);
    SDL_GL_MakeCurrent(gWindow, gContext);
    gTextureContext = gPresentContext ? SDL_GL_CreateContext(gWindow) : nullptr;
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    // (Android: the game's context without a surface, if the driver can; SDL
    // first told the window is no longer this thread's)
#if defined(__ANDROID__)
    SDL_GL_MakeCurrent(gWindow, nullptr);
#endif
    const bool ok = gPresentContext && gTextureContext && makeCurrentOffscreen(gContext);
    if (!ok) {
        debugTrace("display thread: no shared context (%s); frames shown by the game's thread", SDL_GetError());
        SDL_GL_MakeCurrent(gWindow, gContext);
        if (gTextureContext) SDL_GL_DestroyContext(gTextureContext);
        if (gPresentContext) SDL_GL_DestroyContext(gPresentContext);
        gTextureContext = gPresentContext = nullptr;
        return;
    }
    setDisplayThread(true);
    gTextureThread = std::thread([] {
        makeCurrentOffscreen(gTextureContext);
        textureWorkLoop(gPresenterStop);
        releaseOffscreen();
    });
    gPresenter = std::thread(presentLoop);
}

}  // namespace

bool displayThread() { return gPresenter.joinable(); }

void show(float t) {
    if (gPresenter.joinable()) {
        // The display thread shows the frames by itself (during a live
        // resize too): here the latest is put on show now, and this waits
        // until it is (a display frame begun after this one).
        if (gShowFromPump) return;
        showLatestNow();
        std::unique_lock<std::mutex> lock(gPresentMutex);
        const unsigned long long target = gPresented + 2;
        gPresentedCv.wait_for(lock, std::chrono::milliseconds(250), [&] { return gPresented >= target; });
        return;
    }
    // (Not again from inside itself: the pump below can deliver the resize
    // that the event watch shows a frame for.)
    static bool showing = false;
    if (showing) return;
    showing = true;
    // The mouse state is as of the last event pump, which the game does once
    // a pass: pumped now, the pointer drawn where it is (livePointer) is
    // where the mouse is at this display frame. The events stay queued for
    // the next pass.
    if (!gControl && gClicks.empty() && gDrags.empty() && !gShowFromPump) SDL_PumpEvents();
    double renderStart = now();
    drawWindow(nullptr, t);
    const double drawn = now();
    SDL_GL_SwapWindow(gWindow);
    // (AS_DISPLAY_LOG, as the display thread's: the frame shown is the pass's)
    static FILE* log = debugEnv("AS_DISPLAY_LOG") ? std::fopen(debugEnv("AS_DISPLAY_LOG"), "w") : nullptr;
    if (log)
        std::fprintf(log, "%.6f %ld %.4f %.2f %.2f 0 %d\n", renderStart, gFrame, std::clamp(t, 0.0f, 1.0f),
                     (drawn - renderStart) * 1000, (now() - drawn) * 1000, takeGlDraws());
    // Debug mode: a display frame long after the last (a visible hitch) is
    // in debug.log, with how long drawing and presenting it took.
    if (traceEnabled() && gLastSwap > 0 && now() - gLastSwap > 0.05)
        debugTrace("slow display frame (%.0f ms since the last): replay and swap %.1f ms", (now() - gLastSwap) * 1000,
                   (now() - renderStart) * 1000);
    // Without vsync (or while minimised) the swap returns at once: do not
    // spin faster than 240 frames per second.
    double since = now() - gLastSwap;
    if (since < 1.0 / 240) sleepSeconds(1.0 / 240 - since);
    gLastSwap = now();
    showing = false;
}

void movieShots(double seconds) {
    if (!debugMode() || gMovieShots.empty()) return;
    while (!gMovieShots.empty() && gMovieShots.front() <= seconds) {
        char name[32];
        std::snprintf(name, sizeof name, "_%.2f.png", gMovieShots.front());
        writeScreen(gMovieShotPrefix + name, 1);
        gMovieShots.erase(gMovieShots.begin());
        if (gMovieShots.empty()) gShotsFinished = true;
    }
}

void present() {
    endFrame();
    show(1);
}

// An arrow key to the sink: its way across or down.
static void arrow(TouchSink& sink, SDL_Keycode key, bool down) {
    if (key == SDLK_LEFT) sink.arrowKey(-1, 0, down);
    else if (key == SDLK_RIGHT) sink.arrowKey(1, 0, down);
    else if (key == SDLK_UP) sink.arrowKey(0, -1, down);
    else if (key == SDLK_DOWN) sink.arrowKey(0, 1, down);
}

bool pollEvents(TouchSink& sink) {
    // Once the window was asked to close, every later poll says so too (a
    // movie's own loop may have seen the request first).
    static bool quit = false;
    if (gShotsFinished || quit) return false;
    if (gControl && !controlStep()) {
        quit = true;
        return false;
    }
    for (const PointerEvent& e : gControlPointer) {
        gPointerX = e.x;
        gPointerY = e.y;
        if (e.kind == 'd') sink.touchBegan(e.x, e.y);
        else if (e.kind == 'm') sink.touchMoved(e.x, e.y, gControlX, gControlY);
        else if (e.kind == 'h') sink.mouseMoved(e.x, e.y);
        else sink.touchEnded(e.x, e.y, gControlX, gControlY);
        gControlX = e.x;
        gControlY = e.y;
    }
    gControlPointer.clear();
    if (gControlWindowW > 0 && gControlWindowH > 0) {
        // (resized now; its events come with this pump)
        SDL_SetWindowSize(gWindow, gControlWindowW, gControlWindowH);
        SDL_SyncWindow(gWindow);
        gControlWindowW = gControlWindowH = 0;
    }
    for (char c : gControlText) sink.charKey(c);
    gControlText.clear();
    for (const Click& c : gClicks) {
        if (c.frame == gFrame || c.frame + 2 == gFrame) {
            gPointerX = c.x;
            gPointerY = c.y;
        }
        if (c.frame == gFrame) sink.touchBegan(c.x, c.y);
        if (c.frame + 2 == gFrame) sink.touchEnded(c.x, c.y, c.x, c.y);
    }
    for (const Key& k : gKeys) {
        // An arrow: held this frame, let go the next.
        int dx = k.key == "left" ? -1 : k.key == "right" ? 1 : 0, dy = k.key == "up" ? -1 : k.key == "down" ? 1 : 0;
        if ((dx || dy) && (k.frame == gFrame || k.frame + 1 == gFrame)) sink.arrowKey(dx, dy, k.frame == gFrame);
        if (k.frame != gFrame) continue;
        if (k.key == "f1") sink.debugMenuKey();
        else if (k.key == "esc") sink.escapeKey();
        else if (k.key.size() == 1 && k.key[0] >= '0' && k.key[0] <= '9') sink.numberKey(k.key[0] == '0' ? 10 : k.key[0] - '0');
        else if (k.key == "-") sink.numberKey(11);  // (the key after 0)
        else if (k.key.size() == 1 && k.key[0] > ' ' && k.key[0] < 0x7f) sink.charKey(k.key[0]);
    }
    for (const Drag& d : gDrags) {
        long i = gFrame - d.frame;
        if (i < 0 || i > d.frames + 1) continue;
        {
            auto [px, py] = i <= d.frames ? d.at(i) : d.points.back();
            gPointerX = px;
            gPointerY = py;
        }
        if (d.hover) {
            if (i <= d.frames) sink.mouseMoved(gPointerX, gPointerY);
            continue;
        }
        if (i == 0) {
            sink.touchBegan(d.points[0].first, d.points[0].second);
        } else if (i <= d.frames) {
            auto [x, y] = d.at(i);
            auto [px, py] = d.at(i - 1);
            sink.touchMoved(x, y, px, py);
        } else {
            auto [x, y] = d.points.back();
            sink.touchEnded(x, y, x, y);
        }
    }
    // Pointer motion is sampled once per pass (as the Flash movie read
    // _xmouse/_ymouse once per frame): several SDL motion events can queue
    // up within one ~40 ms pass during a fast swipe, and running each one
    // through the game's own touchMove (button rollovers, the cursor's
    // placement...) separately made that logic see, and react to, motion
    // the original never sampled. Runs of motion events are collapsed to
    // their last position; a button down/up still lands exactly where it
    // happened, relative to the motion around it.
    bool pendingMove = false;
    float moveX = 0, moveY = 0;
    auto flushMove = [&]() {
        if (!pendingMove) return;
        pendingMove = false;
        gPointerX = moveX;
        gPointerY = moveY;
        if (gMouseDown) {
            sink.touchMoved(moveX, moveY, gLastX, gLastY);
            gLastX = moveX;
            gLastY = moveY;
        } else {
            sink.mouseMoved(moveX, moveY);
        }
    };
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_MOUSE_MOTION) {
            toGame(e.motion.x, e.motion.y, moveX, moveY);
            pendingMove = true;
            continue;
        }
        flushMove();
        switch (e.type) {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            notifyDisplayScale();
            if (windowAspect() != gNotifiedAspect) {
                gNotifiedAspect = windowAspect();
                int w = 0, h = 0;
                SDL_GetWindowSizeInPixels(gWindow, &w, &h);
                debugTrace("window shape %dx%d (%.4f)", w, h, gNotifiedAspect);
                for (auto& f : gShapeListeners) f();
            }
            break;
        case SDL_EVENT_QUIT:
            quit = true;
            return false;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.button == SDL_BUTTON_LEFT && !gMouseDown) {
                gMouseDown = true;
                toGame(e.button.x, e.button.y, gLastX, gLastY);
                gPointerX = gLastX;
                gPointerY = gLastY;
                sink.touchBegan(gLastX, gLastY);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button == SDL_BUTTON_LEFT && gMouseDown) {
                gMouseDown = false;
                float x, y;
                toGame(e.button.x, e.button.y, x, y);
                sink.touchEnded(x, y, gLastX, gLastY);
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_F11 || (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT))) {
                bool full = SDL_GetWindowFlags(gWindow) & SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(gWindow, !full);
            } else if (SDL_TextInputActive(gWindow) &&
                       (e.key.key == SDLK_BACKSPACE || e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER ||
                        e.key.key == SDLK_ESCAPE)) {
                sink.textEditKey(e.key.key == SDLK_BACKSPACE ? EditKey::Backspace
                                 : e.key.key == SDLK_ESCAPE  ? EditKey::Escape
                                                             : EditKey::Enter);
            } else if ((e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK) && !e.key.repeat) {
                // (A phone's back button presses Escape: the pause.)
                sink.escapeKey();
            } else if (e.key.key == SDLK_F2 && !e.key.repeat && gDebug) {
                gSnapNow = true;
            } else if (e.key.key == SDLK_F1 && !e.key.repeat && gDebug) {
                sink.debugMenuKey();
            } else if (e.key.key >= SDLK_0 && e.key.key <= SDLK_9 && !e.key.repeat && !SDL_TextInputActive(gWindow)) {
                sink.numberKey(e.key.key == SDLK_0 ? 10 : static_cast<int>(e.key.key - SDLK_0));
            } else if (e.key.scancode == SDL_SCANCODE_MINUS && !e.key.repeat && !SDL_TextInputActive(gWindow)) {
                // The key after 0 on the number row ('-' on a US layout,
                // whatever it types on others): the eleventh (AS3's tools).
                sink.numberKey(11);
                SDL_Keycode k = SDL_GetKeyFromScancode(e.key.scancode, e.key.mod, true);
                if (k > ' ' && k < 0x7f && !(k >= '0' && k <= '9')) sink.charKey(static_cast<char>(k));
            } else if (!e.key.repeat && !SDL_TextInputActive(gWindow)) {
                arrow(sink, e.key.key, true);
                SDL_Keycode k = SDL_GetKeyFromScancode(e.key.scancode, e.key.mod, true);
                if (k > ' ' && k < 0x7f && !(k >= '0' && k <= '9')) sink.charKey(static_cast<char>(k));
            }
            break;
        case SDL_EVENT_KEY_UP:
            if (!SDL_TextInputActive(gWindow)) arrow(sink, e.key.key, false);
            break;
        case SDL_EVENT_TEXT_INPUT:
            sink.textInput(e.text.text);
            break;
        default:
            break;
        }
    }
    flushMove();
    setFramePointer(gPointerX, gPointerY);
    return true;
}

void startTextInput() { SDL_StartTextInput(gWindow); }
void stopTextInput() { SDL_StopTextInput(gWindow); }

std::string clipboardText() {
    if (const char* file = debugEnv("AS_CLIPBOARD_FILE")) {
        size_t n = 0;
        void* raw = SDL_LoadFile(file, &n);
        if (!raw) return "";
        std::string text(static_cast<const char*>(raw), n);
        SDL_free(raw);
        return text;
    }
    char* raw = SDL_GetClipboardText();  // "" (never null) when empty
    std::string text = raw ? raw : "";
    SDL_free(raw);
    return text;
}

bool setClipboardText(const std::string& text) {
    if (const char* file = debugEnv("AS_CLIPBOARD_FILE")) return SDL_SaveFile(file, text.data(), text.size());
    return SDL_SetClipboardText(text.c_str());
}

double now() { return SDL_GetTicksNS() / 1e9; }

int systemMemoryMB() { return SDL_GetSystemRAM(); }

namespace {
double gPassTime = 0;
}  // namespace

double passTime() { return gPassTime; }
void setPassTime(double t) { gPassTime = t; }

namespace {
double gPassSeconds = 0.05;
}
double passSeconds() { return gPassSeconds; }
void setPassSeconds(double s) {
    if (s > 0) gPassSeconds = s;
}

void sleepSeconds(double s) {
    if (s > 0) SDL_DelayPrecise(static_cast<Uint64>(s * 1e9));
}

const std::string& prefPath() {
    static std::string path = [] {
        if (const char* dir = debugEnv("AS_SAVE_DIR")) {  // debug automation: keep tests off the real save
            std::string d = dir;
            if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
            SDL_CreateDirectory(d.c_str());
            return d;
        }
#if defined(__ANDROID__)
        // The app's own storage.
        if (char* p = SDL_GetPrefPath("BleedEngine", gGame.title)) {
            std::string d = p;
            SDL_free(p);
            return d;
        }
        return std::string("./");
#endif
        // The user's Saved Games folder. Earlier builds kept the save in
        // %APPDATA%\AmateurSurgeonPort\<prefName>: it is copied over once
        // (and left where it was).
        std::string d = savedGamesFolder(gGame.title);
        const char* appData = std::getenv("APPDATA");
        std::string old = appData ? std::string(appData) + "\\AmateurSurgeonPort\\" + gGame.prefName + "\\" : "";
        if (d.empty()) {
            if (old.empty()) return std::string("./");
            SDL_CreateDirectory(old.c_str());
            return old;
        }
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path from = fs::u8path(old), to = fs::u8path(d);
        if (!old.empty() && !fs::exists(to / "RMS.txt", ec) && fs::exists(from / "RMS.txt", ec))
            for (const fs::directory_entry& e : fs::directory_iterator(from, ec))
                if (e.is_regular_file(ec)) fs::copy_file(e.path(), to / e.path().filename(), fs::copy_options::skip_existing, ec);
        return d;
    }();
    return path;
}

}  // namespace platform
