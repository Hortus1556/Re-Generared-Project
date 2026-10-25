// The main loop every game on Bleed Engine runs: platform start-up, the
// engine's splash (Splash.h), then one logic pass of the game's canvas per
// 50 ms, each shown once per display refresh.
// A game's main() is runGame<ItsCanvas>(argc, argv, itsInfo).
#pragma once

#include <algorithm>
#include <cstdlib>
#include <string>
#include <type_traits>

#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"
#include "platform/Splash.h"

namespace platform {

// The original game thread slept 0.05 s after every pass and all game logic
// counts passes, so the port runs one pass per 50 ms. Each pass records one
// frame; until the next pass is due the frame is shown once per display
// refresh, interpolated from the one before (or just once, smoothing off).
constexpr double kPassSeconds = 0.05;  // the default; GameInfo::passSeconds

// Whether a canvas has a static preload(): what it reads at start, begun in
// the background before the splash.
template <class C, class = void>
struct HasPreload : std::false_type {};
template <class C>
struct HasPreload<C, std::void_t<decltype(C::preload())>> : std::true_type {};
// And a static preloaded(): whether that is done (the splash waits for it).
template <class C, class = void>
struct HasPreloaded : std::false_type {};
template <class C>
struct HasPreloaded<C, std::void_t<decltype(C::preloaded())>> : std::true_type {};

// Canvas: a TouchSink with cycle(), one logic pass (the original GameCanvas).
template <class Canvas>
int runGame(int argc, char** argv, const GameInfo& info) {
    // -d: debug mode (F1 menu, F2 snapshots, test automation).
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "-d") setDebugMode(true);
    if (!init(info)) return 1;
    if (assets::root().empty()) {
        fatal(std::string("Game data not found.\n\nCreate the assets folder next to the game with:\n"
                          "python port/tools/extract_ipa.py <") +
              info.title + " .ipa> assets");
        shutdown();
        return 1;
    }
    // The game's content read on worker threads meanwhile (a new build's
    // thousands of files, scanned on first read, held a black screen after
    // the splash for seconds).
    if constexpr (HasPreload<Canvas>::value) Canvas::preload();
    // "Powered by Bleed Engine", before the game's own first screen (held
    // until that content is read: the window never stops answering).
    std::function<bool()> ready;
    if constexpr (HasPreloaded<Canvas>::value) ready = [] { return Canvas::preloaded(); };
    if (!showEngineSplash(ready)) {
        shutdown();
        return 0;
    }
    {
        Canvas canvas;
        setPassSeconds(info.passSeconds > 0 ? info.passSeconds : kPassSeconds);
        double next = now();
        // AS_FAST=1 with AS_HIDDEN (debug mode): the passes run as fast as
        // they compute, on the pass clock (not the wall clock), the window
        // shown once a second: for long scripted play-throughs.
        static const bool fast = debugMode() && std::getenv("AS_FAST") && std::getenv("AS_HIDDEN");
        long passes = 0;
        // With a display thread, the frame a pass records is shown from
        // `lead` after the pass was due: as long as passes take, so the
        // display never waits on one, and no longer (a whole pass of it was
        // input lag). It follows the longest pass of the last second, a
        // millisecond a pass at most (so the display's clock never jumps).
        double lead = 0.005, longest[25] = {};
        while (pollEvents(canvas)) {
            setPassTime(next);
            if (!fast && displayThread()) setFrameDue(next + lead);
            canvas.cycle();
            // A game may change its pass length as it runs (AS2's display
            // link: 60 updates a second, 30 in a mission).
            const double pass = passSeconds();
            if (fast) {
                next += pass;
                if (++passes % 60 == 0) show(1);
                continue;
            }
            double done = now();
            longest[passes++ % 25] = done - next;
            double want = 0;
            for (double t : longest) want = std::max(want, t);
            want = std::clamp(want + 0.003, 0.004, pass);
            lead += std::clamp(want - lead, -0.001, 0.001);
            next += pass;
            if (next < done - 0.5) next = done;  // do not try to catch up after a stall
            if (displayThread()) {
                sleepSeconds(next - now());
            } else if (smoothMotion()) {
                // The display runs one pass behind on the pass clock itself
                // (not from whenever this pass finished computing), so every
                // pass takes the same time on screen.
                double from = next - pass;
                do {
                    show(static_cast<float>((now() - from) / pass));
                } while (now() < next);
            } else {
                show(1);
                sleepSeconds(next - now());
            }
        }
    }
    shutdown();
    return 0;
}

}  // namespace platform
