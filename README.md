# Re Generated Project

Native Windows/Linux/Android ports of Adult Swim/Mediatonic's **Amateur
Surgeon** series — Amateur Surgeon, its Christmas Edition, Amateur Surgeon
2 and Amateur Surgeon 3 — on one shared engine, **Bleed Engine**: the main
loop and threaded display pipeline, GPU vector-art rendering of the Flash
originals, a decompiled-ActionScript translator and runner, a cocos2d-x
subset (for AS3) and a UIKit subset (for AS2), built from each game's own
original (the Flash games from their SWFs, AS2 from a hybrid of its IPA
and Flash original, AS3 from its IPA alone), with the online services
removed.

This is the full buildable source: the engine, every game's own C++ layer
on top of it, the build system, and the tooling that fetches sources and
drives a build (`port/tools/build_all.py`, `build_gui.py`,
`BuildAllGames.exe`). The engine alone is also published on its own at
[Bleed-Engine](https://github.com/Hortus1556/Bleed-Engine), for anyone who
just wants the generic, game-agnostic part; this repository is the live
source those `BuildAllGames.exe` and `build_all.py` pull from and build.

## What's *not* here, on purpose

No game content, ever: the original Flash SWFs, the iOS/iPad IPAs, the
packages extracted from them (vector art, audio, decompiled scripts), or
the `re/` reverse-engineering workspace. Those are Adult Swim/Mediatonic's
copyrighted content (or derived from it), not this project's to
redistribute. `port/tools/fetch_sources.py` fetches what it can from
public preservation sources (archive.org), verified by hash, and reports
plainly what it can't — see `docs/port.md`.

## Building

```
python port/tools/build_all.py --game as1|xmas|as2|as3|all [--platform windows|linux|android]
```

or `port/tools/build_gui.py` (a small GUI for the same thing), or build it
yourself frozen into an exe with PyInstaller — see `docs/port.md` for the
exact command, prerequisites (CMake, a C++17 toolchain, SDL3; the Android
SDK/NDK and a JDK for Android; WSL for Linux) and what's auto-fetchable
versus what you need to supply yourself (two of the IPAs and two of the
Flash SWFs have no public source at all and must already exist locally).

`docs/README.md` is the documentation index: `docs/engine.md` for the
engine's architecture, `docs/rules.md` for the rules every port follows,
one page per game (`docs/as1.md`, `docs/christmas.md`, `docs/as2.md`,
`docs/as3.md`), and `docs/port.md` for the Windows/Linux/Android build
itself.

## License

This project's own code is MIT (`LICENSE`). Vendored third-party code
keeps its own license; see `THIRD_PARTY_LICENSES.md`. The four Windows
icon placeholders in `port/res/` are plain generated shapes, not the
original games' App Store artwork — replace them with your own if you
build a real distributable.
