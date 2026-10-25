# The plan: one engine for every Amateur Surgeon

The approved plan for the workspace, and where it stands (updated 2026-10-22). The rules every step follows are in [rules.md](rules.md); the engine as built is in [engine.md](engine.md).

## Goal

One engine runs every Amateur Surgeon game, each from its own content:

| Game | From | State |
|---|---|---|
| Amateur Surgeon Christmas | its Flash original | Runs on the general engine as an extension of AS1: one Flash surgeon game, its operation by its own scripts since 2026-10-23 ([christmas.md](christmas.md)) |
| Amateur Surgeon 1 | its Flash original (the user's choice, 2026-10-21/22: "the better version") | Being rebuilt (step 5): the whole game, operation included, runs by its own translated scripts ([as1.md](as1.md)); patients being played through one by one, and the user's play-test reports fixed as they come. The iPad port is archived (`port/attic/as1-ipad/`); `dist/surgeon1` holds the Flash rebuild |
| Amateur Surgeon 2 | its IPA, with parts of its Flash original taken in on the user's request | Plays whole: all 30 missions won by real play; Flash parts taken in one by one ([as2.md](as2.md)) |
| Amateur Surgeon 3 | its IPA, the final build 1.45 (it has no Flash original) | Ported for Windows and Android, every area (The Hospital and Le Flairground Fantastique included): all 31 surgeries won by scripted real play in all three modes, at 16:9 and 4:3, with keys and in the phone layout (step 7) |

**Porting a game is extracting and indexing its content, plus native modules only for real game logic.** No Flash emulation: the original's scripts run as translated data, and the rest is native C++ (see [rules.md](rules.md) §1–2).

## Steps

Each step ends with Christmas unchanged and the play-test suite green.

1. **Extraction. Done.**
   - What was built: `extract_swf` (round trip exact), the SVG dialect, the JSON package (`port/games/<game>/`) and the loader.
   - Christmas's art pack and `assets-christmas/` are gone.
   - Dist ships the package.
2. **Script index. Done.**
   - `as2.py` (every script parses and prints back), `index_scripts.py` (`index.json`, `--report`, `--diff`), `data/modules.json`.
   - The operation's data is `data/gamedata.json`, generated from the index by `flash_data.py`.
3. **Runtime. Done, in a different shape than first outlined:**

   | Outlined | Built |
   |---|---|
   | a manifest, `ClipScreen` | the main timeline itself runs as the screen (`StageScreen`), set up by the game's `FlashSurgeonGame::Setup` (AS1's and Christmas's) |
   | an action runner, a variable store | the runner (`Script.*`) runs whole functions and loops too, not only a small action set; the variable store is the scene's clips |
   | data-driven sounds and save | Flash's own classes (`Sound`, `XML`, `SharedObject`) |
   | art quirks as data | they disappeared where the scripts run |

   Also added: blur, glow and drop-shadow filters.
4. **Christmas on the runtime. Done.**
   - The hand-made screens are deleted; the players' additions are modules (`StageExtras`). The operation was a native module until 2026-10-23, when Christmas became an extension of AS1 (step 5): it runs by its own scripts, set up by the shared `FlashSurgeonGame` (the native one archived in `port/attic/christmas-native-operation/`).
   - The suite has a scenario through the real menus (`story`).
5. **AS1 from Flash. Under way (2026-10-22): the user asked for the Flash version to be ported whole, working on until everything is.**
   - **Done:**
     - `port/games/as1/` holds its SWF extracted and indexed (every statement translated);
     - AS1's set-up (`As1Game`: save format, transition variable) and its data tool (`flash_data.py as1`);
     - the operation runs by its own scripts, no native module: the engine gained the general features it needed (inner clips made nodes, Mouse and Key listeners, AVM1's `this` for plain calls, `getBounds`; [engine.md](engine.md#running-the-scripts)). A native module derived from Christmas's was tried first and archived (`port/attic/as1-native-operation/`);
     - the debug probe writes Christmas's trace lines from the scripts' state, so `flash_playtest.py --game as1` plays AS1's patients.
     - the touch build (2026-10-24): `build_apk.py --game as1` → `dist/surgeon1-android/`, Christmas's with `--game christmas` → `dist/surgeonxmas-android/`; no cursor drawn, no hover look left by a finger, a press after the pointer moved there, a fingertip-sized hit; the display thread on Android too ([port.md](port.md#building-and-running));
     - the renderer reworked (2026-10-24): small art in shared texture pages, draws batched (a frame some 140 draws instead of 575), the matrices kept off GL ([engine.md](engine.md#the-loop-and-the-display-three-threads)). Not measured on a real phone yet.
     - the stutters (2026-10-24): the display has its own thread, a pass behind the logic, and textures are uploaded on a third; a screen's new art is made by all the raster workers side by side. AS1's and Christmas's ways through (title to the won scenes) now play at the display's rate with no hold ([engine.md](engine.md#the-loop-and-the-display-three-threads)).
   - **Next:**
     1. Play every patient (1–31) to "mission won" with the live driver, fixing what each needs (engine first, the driver where it aims wrong). 25 pass (2026-10-23); 11 and 13 are lost by the driver (the metal poison's spread; the brain's organ carried to the tray), the swarms (17, 21, 22, 28) and the car (30) pass only with god mode, the driver too slow for them ([as1.md](as1.md#status)). `dist/surgeon1` holds the Flash rebuild.
     2. Compare with Ruffle replays, stage by stage.
     3. The menus' flow (`story`, `story2`) and the end screens.
6. **AS2 from its IPA, then pieces of its Flash original. The IPA port is done; Flash parts are being taken in (2026-10-18 on).** AS1 and Christmas are set aside, and only AS2 is worked on and tested. Details: [as2.md](as2.md), analysis: [amateur-surgeon-2-ipad.md](amateur-surgeon-2-ipad.md).
   - **Done:**
     - the decompile (`re/as2/`: the method names come from the Objective-C metadata, the binary being stripped);
     - the package `port/games/as2/` from `extract_ipa.py` (nibs as JSON layouts; the engine's asset lookup knows the new types), its data from `as2_data.py`;
     - the engine features AS2 needed, made general: the UIKit layer, AAC music, text with system fonts, the pass length;
     - AS2's framework and loop, the menus and cut scenes, the game, every tool and every injury class its 30 missions use;
     - the play-tests: every mission won by real play (`surgeon2_playtest.py`), the flow through the real menus;
     - from the Flash AS2, on the user's request: the HUD and pointer, the injection, the pause and death clipboards, the loading screen and the bandages, the results and the upgrade, the stories' places and characters, the monsters (logic and art, standing where Flash's levels put them) and their remains.
   - **Next:** what the user picks: the Flash AS2's pace (its faster injury timers, poison and fire that spread) is researched and awaits a decision; the touch builds (no pointer, tilt and shake from the device: [rules.md](rules.md) §2, §7).
7. **AS3 from its IPA. Started (2026-10-20).** AS3 is cocos2d-x 2.1.1 (C++) by Mediatonic, with no Flash original ([amateur-surgeon-3.md](amateur-surgeon-3.md)). It runs on Bleed Engine as AS2 does:
   - its content goes into the package layout by `extract_ipa.py`;
   - what it needs that the engine lacks (sprite sheets, bitmap fonts, particle systems, node actions) is added to the engine generally, with cocos2d-x 2.1.1's source (`re/as3/cocos2d-x-2.1.1/`) as the reference for behaviour, not as a runtime;
   - its `mt` classes are mirrored in `port/src/as3`.
   - **Done:**
     - the analysis;
     - the decompile (`re/as3/`: virtual functions named from RTTI, cocos2d's slots from its headers);
     - the engine reads PVR/CCZ textures (`platform::decodeImage`);
     - the engine decodes CAF sounds natively (`platform::decodeCaf`: PCM and IMA4), and `extract_ipa.py` keeps them as they are and skips AS3's online and build leftovers;
     - the package `port/games/as3/`;
     - the free-to-play decisions (the energy, blood packs, kept but free: never sold, only waited for; coins earned only; partner cooldowns a twentieth);
     - the cocos layer (`port/src/engine/cocos/`: nodes, actions, scheduler, touches, director, scenes and layers, textures, sprite frames and sprites, bitmap-font labels, menus, CocosDenshion, particles, draw and clipping nodes);
     - the launch, the screen manager, the layouts, sounds, the save, the content; the title, intro, dialogues, save slots, world and area maps, case files, pre-op, upgrades, tag partners, credits, loading screen; `Animation`, `ParticleSystem`, the effects;
     - the surgery: stages, every tool, every injury class and creature, the tray, the combo meter and score, the effects, the tag partners and their powers, the tutorials, pause, the results (won, lost), winning and losing; the grid effects (`CCLiquid`) on the recorded drawing (`platform::beginWarp`); the first surgery is won by the play-test driver (`port/tools/surgeon3_playtest.py`);
     - the package target (`AmateurSurgeon3_dist` → `dist/surgeon3/`) and the Android APK (`build_apk.py --game as3` → `dist/surgeon3-android/`);
     - the rebase on 1.45 (The Hospital, Le Flairground Fantastique, the achievements, the surgery editor as a bonus); the screen taking the window's shape; the PC's HUD (the tool bar across the top, the split meter, keys, the Flash pointer) and the touch devices' (the phone's columns, the tongs' reach); the user's decisions (no shop, consumables or revive; blood packs free; the restored music);
     - every surgery won by the play-test driver in all three modes (1.02's 69 runs and the first surgery from a new game: 70/70 on 2026-10-20; 1.45's 31 surgeries, 93 runs plus 4:3 and keys: 155/155 on 2026-10-21), after audits of the menus and save, the surgery and tools, the injury and creature classes, and the engine layer (with an AddressSanitizer build), each fixing what it found.

## Verification (every step)

- **Round trip:** `extract_swf` reads its package back exactly.
- **Pixel identity:** screens and a symbol sweep, before and after, compared in RGB with seeded randomness.
- **Gameplay:** `flash_playtest.py` (all missions, the scheduled scenarios, `story`) green; for AS2, `surgeon2_playtest.py` on the missions a change touches; Ruffle comparisons of what changed.
- **Index:** 0 unported.
- **Performance:** the preloader under 2 s, and `AS_PROFILE` showing no new stalls.
