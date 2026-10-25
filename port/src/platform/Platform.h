// Window, OpenGL context, frame presentation and input for the port.
//
// The game draws exactly as on its device, in the original screen's
// coordinates (GameInfo) with its own glOrtho, but its draws are recorded
// (DrawList.h) and replayed into the real window at native resolution,
// letterboxed to the original's shape, so window size never leaks into game
// code. Mouse input arrives as single touches in the same coordinates. A
// game may change its screen's size between frames (setScreenSize: AS3
// follows the window's shape, windowAspect).
#pragma once

#include <functional>
#include <string>

namespace platform {

// What differs between the ported games; given to init().
struct GameInfo {
    const char* title;     // window title, error boxes, the Saved Games folder
    int screenWidth;       // the original's screen in game coordinates
    int screenHeight;      //   (iPad 1024x768, iPhone 480x320)
    const char* assetsDir; // its assets folder in port/ ("assets"), found
                           //   from port/build; installed, it is "assets"
    const char* prefName;  // %APPDATA%\AmateurSurgeonPort\<prefName>: the
                           //   save when Saved Games is unavailable, and
                           //   where builds before Saved Games kept it
    float artScale = 1;    // game-screen units per pixel of the art: vector
                           //   art is rasterised for this times displayScale()
    double passSeconds = 0.05;  // one logic pass (the iOS games: 20 a second;
                                //   the Flash games: their movie's frame rate)
};
const GameInfo& game();
int screenWidth();
int screenHeight();
// The game's screen changed at run time (in game coordinates; GameInfo's
// size is the first): for a game laid out for the window's shape (AS3).
// The frames recorded from now on are at this size; each frame is shown,
// letterboxed, at the size it was recorded at, and touches map to the new
// one at once. Screenshots, the offscreen frame and displayScale() follow;
// the display scale listeners are called if it changed.
void setScreenSize(int w, int h);
// The window's shape: its width over its height in pixels (1 before init),
// for a game choosing its screen's size from it. Debug mode: AS_ASPECT=w:h
// (or setAspectOverride) answers that shape instead, whatever the window.
float windowAspect();
// Debug mode: the shape windowAspect() answers (w over h); 0 goes back to
// the window's (or AS_ASPECT's).
void setAspectOverride(float aspect);
// Calls f (main thread) whenever the window's shape changes (a resize, a
// foldable folded or unfolded, fullscreen, another display): a notice; the
// game decides when to follow it.
void onWindowShapeChange(std::function<void()> f);
// Window pixels per game-screen unit: the scale the frame is shown at.
float displayScale();
// Calls f (on the main thread, GL context current) whenever displayScale()
// changes, for art rasterised at the display's resolution.
void onDisplayScaleChange(std::function<void()> f);

// Editing keys forwarded while text input is on.
enum class EditKey { Backspace, Enter, Escape };

// Receives touches in game coordinates (origin top-left).
class TouchSink {
public:
    virtual ~TouchSink() = default;
    virtual void touchBegan(float x, float y) = 0;
    virtual void touchMoved(float x, float y, float prevX, float prevY) = 0;
    virtual void touchEnded(float x, float y, float prevX, float prevY) = 0;
    // The mouse moved with no button down (game coordinates): rollovers and
    // cursors that follow the mouse (the Flash games).
    virtual void mouseMoved(float /*x*/, float /*y*/) {}
    // Typed text (UTF-8) and editing keys, while startTextInput() is on.
    virtual void textInput(const char* /*utf8*/) {}
    virtual void textEditKey(EditKey /*key*/) {}
    // Escape outside text input (skips a movie; in an operation, the pause
    // button).
    virtual void escapeKey() {}
    // Number keys outside text input: 1..9, 10 for 0, and 11 for the key
    // after 0 on the number row ('-' on a US layout; it also gives its
    // charKey) (an operation's tools). PC port only.
    virtual void numberKey(int /*n*/) {}
    // A printable key pressed outside text input (not a digit: those pick
    // tools), as the keyboard's layout and shift make it. PC port only.
    virtual void charKey(char /*c*/) {}
    // An arrow key pressed or let go outside text input: (dx, dy) its way
    // (-1/1 across, -1/1 down). PC port only: a tilt held by hand.
    virtual void arrowKey(int /*dx*/, int /*dy*/, bool /*down*/) {}
    // F1: opens or closes the debug menu. (F2 is handled here: it saves a
    // snapshot of the game screen as snap_<n>.png in prefPath().)
    virtual void debugMenuKey() {}
};

// Creates the window and GL context for `game`. Shows an error box and
// returns false on failure.
bool init(const GameInfo& game);

// Debug mode (the -d switch): the F1 debug menu, F2 snapshots and the AS_*
// automation below. Off, the game ignores all of them. Set before init().
void setDebugMode(bool on);
bool debugMode();
// Whether AS_TRACE's detail is on: the env var, or -d by itself (so a
// plain "-d" playtest leaves a trace too, see debugTrace below). The
// game's own modules gate their own trace prints on this, not on
// std::getenv("AS_TRACE") directly: -d cannot reach that env var from
// inside the process (SDL3 is a separate module with its own CRT
// environment on Windows, so setting it there does not reach
// std::getenv() here) — an in-process flag is the only way that reaches
// every module of the same build.
bool traceEnabled();
// Debug mode: a crash traces its exception and stack (CrashLog.cpp).
void installCrashLog();
void shutdown();

// Ends the frame the game recorded (see DrawList.h): counts it for the debug
// automation and saves any screenshot due.
void endFrame();
// Draws the last frame into the window at its native resolution (the
// largest area of the original screen's shape, letterboxed), interpolated by `t` from the frame before
// (see renderFrame), and swaps buffers in step with the display. With a
// display thread (displayThread) that thread shows the frames by itself:
// this puts the latest on show at once and waits until it is shown.
void show(float t);
// Whether a display thread shows the frames (the desktop): each frame on its
// own clock (setFrameDue), once per display refresh, whatever the logic
// does meanwhile. Without one (Android), the main loop shows them between
// passes (show).
bool displayThread();
// endFrame() then show(1): for loops outside the main one (movies, loading).
void present();

// Smooth motion: the main loop shows each frame several times per logic
// pass, interpolated; off, once per pass as recorded (the original's look).
bool smoothMotion();
void setSmoothMotion(bool on);
// The mouse pointer: a hand (over a button, as Flash showed it) or the arrow.
void setHandCursor(bool hand);
// The system's pointer shown or hidden (a game drawing its own).
void setSystemCursorVisible(bool visible);
// Whether the device is played by touch (a phone, a tablet): there is no
// pointer then, the finger being it, so a game draws no cursor and nothing
// that only follows a hovering pointer (docs/rules.md §7).
bool touchDevice();
// The screen's shorter side in density-independent points (Android's dp,
// iOS's points: about 160 a inch), of the display the window is on now; 0
// if the system does not say.
float screenShortSideDp();
// Whether the screen is large enough for the layouts made for big screens
// (AS3's tool bar across the top): a PC's, whatever its window, or a touch
// device's whose shorter side is at least 600 dp (Android's sw600dp: the
// tablets, a foldable unfolded); a phone's is not. Asked again whenever a
// layout is chosen, so a foldable folded or unfolded, or a window moved to
// another display, is followed.
bool largeScreen();
// Where the pointer is right now, in game coordinates: the mouse as the
// system has it (fresher than the last pass's events), or, under automation
// (AS_CONTROL, recorded clicks and drags), the last position given to the game.
void livePointer(float& x, float& y);

// Pumps OS events, forwarding mouse input to `sink`. Returns false when the
// user asked to quit.
bool pollEvents(TouchSink& sink);

// The device's accelerometer (a phone's or tablet's): its tilt in the
// screen's terms, as the screen is shown however the device is turned:
// x > 0 the right edge lower, y > 0 the bottom edge lower, each gravity's
// share along that axis (the sine of the tilt, -1..1). False when there is
// no accelerometer (a PC): the games go by the pointer instead.
bool deviceTilt(float& x, float& y);
// Its whole reading in gravities along the device's own axes (x right, y
// up, z out of the screen; about 1 straight up at rest): for shakes, which
// go by how fast it changes. False without one.
bool deviceAcceleration(float& x, float& y, float& z);

// Turns the keyboard's text input on or off (the player-name field).
void startTextInput();
void stopTextInput();

// The system clipboard's text (UTF-8, "" when it holds none) and setting it
// (false on failure). Debug mode: AS_CLIPBOARD_FILE=<file> keeps the
// clipboard in that file instead, so automated runs never touch the user's.
std::string clipboardText();
bool setClipboardText(const std::string& text);

// The file the last F2 snapshot was saved to (empty: none yet) and when
// (now()), for an on-screen note.
const std::string& lastSnapshot(double& when);
// AS_MOVIE_SHOT=<prefix>:<s1>,<s2>,... (debug mode): saves the screen as
// <prefix>_<s>.png once a movie has played s seconds (the frame shown last),
// then quits like AS_SHOT. Movie loops call it with the movie's time.
void movieShots(double seconds);

// Seconds since init, monotonic.
double now();
// The machine's memory in megabytes (for sizing caches).
int systemMemoryMB();
void sleepSeconds(double s);
// The clock game logic animates by (wall-clock animations, the hourglass):
// the time the current logic pass was due, so it advances by exactly one
// pass (50 ms) per pass and what it moves interpolates evenly.
double passTime();
void setPassTime(double t);
// The length of a logic pass (GameInfo::passSeconds at start); a game may
// change it between passes.
double passSeconds();
void setPassSeconds(double s);

// Writable per-user folder for save data, with a trailing separator.
const std::string& prefPath();

// The file of the system's font of a kind ("sans", "sans-bold", "hand",
// "hand-wide", "mono", "mono-bold", "serif", "serif-bold"): the nearest the
// platform ships (Windows: Arial, Segoe Print, Courier New, Georgia;
// Android: Roboto, a handwritten face, a monospace, Noto Serif). Empty if
// it has none.
std::string systemFont(const std::string& kind);

// Texels: rasterised art (plutovg) is premultiplied BGRA in memory. Desktop
// GL takes it as GL_BGRA; OpenGL ES 1.1 (Android) only as GL_RGBA, so there
// it is turned around in place first. Upload with kTexelFormat after
// toTexelOrder.
void toTexelOrder(unsigned char* bgra, size_t pixels);

// Shows a fatal error to the user.
void fatal(const std::string& message);

// Debug automation, driven by environment variables (only in debug mode):
//   AS_SHOT=<prefix>:<f1>,<f2>,...  save the game screen (at the original's size) as
//       <prefix>_<frame>.png on those frames (logic passes), then quit after
//       the last one.
//   AS_SHOT_T=<t>                   take those shots as smooth motion shows
//       the frame t (0..1) of the way through the pass (default 1: as
//       recorded).
//   AS_FILM=<prefix>:<from>:<to>:<steps>[:img]  for frames from..to, log
//       where every draw lands at each of <steps> display steps between
//       passes to <prefix>.csv (and with :img save each step as a PNG), then
//       quit: the raw material for checking motion (tools/motion_check.py).
//   AS_CLICKS=<frame>:<x>:<y>,...   synthesize a tap at game coordinates
//       (touch down on that frame, up two frames later).
//   AS_DRAG=<frame>:<x1>:<y1>:<x2>:<y2>:<n>,...  touch down at (x1, y1) on
//       that frame, move to (x2, y2) over n frames, release the frame after.
//   AS_HOVER=<as AS_DRAG>,...       the same path with no button down: the
//       pointer moving over the game in real time (rollovers, a cursor).
//   AS_CONTROL=1                    driven live from stdin: at each frame a
//       `run <n>` reached, prints "@frame <n>" and reads commands (click,
//       key, drag in the forms above with absolute frames; down/move/up
//       <x> <y> for the pointer now; shot <file>; run <n>; quit). Scripts
//       react to AS_TRACE (tools/flash_playtest.py).
//   AS_HIDDEN=1                     no window on the desktop (screenshots
//       still render).
//   AS_WINDOW=<w>x<h>               the window's size (screen pixels).
//   AS_ASPECT=<w>:<h>               the shape windowAspect() answers (a
//       game laid out for the window's shape takes it); without AS_WINDOW
//       the window starts at that shape too.
//   (AS_CONTROL's "window <w>x<h>" resizes the window while it runs.)
//   AS_SAVE_DIR=<folder>            keep save data there instead of the
//       user's profile (so test runs do not touch the real save).
// Whether debug automation drives this run (any of the AS_* above that
// script or capture it, AS_EVENT or AS_HIDDEN): the engine's splash is then
// left out, so frame numbers count from the game's first.
bool automationActive();
//   AS_EVENT=<n>                    start on screen event n instead of the
//       splash (e.g. 10 = Operation01; see GameCanvas::Event); 110..139
//       start that patient through its story intro.
int debugStartEvent(int fallback);
//   AS_AUTOWIN=<n>                  an operation counts as won after n
//       logic passes (20 per second), as with the original's cheat flag.
int debugAutoWinPasses();
// -d by itself turns the trace below on (traceEnabled()): a manual
// playtest needs no env var set, and everything printed also goes to
// debug.log next to the save data (prefPath()) — the game is windowed,
// not console, so stdout alone is not reachable from a normal play
// session.
void debugTrace(const char* fmt, ...);

}  // namespace platform
