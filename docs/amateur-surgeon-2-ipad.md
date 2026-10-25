# Amateur Surgeon 2 HD (iPad) 1.2 — analysis

Source: `IPAs/Amateur Surgeon 2 HD v1.2 iPad os32most_uniQue.ipa` → `Payload/AS2.app`. The port built from it is described in [as2.md](as2.md).

## Identity

- Bundle id `com.turner.AmateurSurgeon2HD`, display name "Surgeon 2!", v1.2 (Adult Swim / Turner).
- iPad only (`UIDeviceFamily = [2]`), iOS 3.2 minimum, built with GCC 4.2 against the iOS 4.3 SDK.
- Landscape left, status bar hidden.

## Not the A3I engine

AS2 is a different code base from AS1 and Christmas (Mediatonic's A3I). Its Xcode project began as Apple's OpenGL ES template ("Created by Simon Taylor on 15/06/2010", `Shader.vsh`/`.fsh` left from it, unused), and its game logic follows the Flash game's (injury "linkage" names, tool data with an `enterFrame` flag, Flash-sized coordinates in `levels.txt`).
- **Rendering:** OpenGL ES 1.1 in an `EAGLView` (`ES1Renderer`), driven by a `CADisplayLink` at frame interval 1: one update and one draw **per display frame, 60 a second**. Game logic counts frames (the splash lasts 140 of them).
- **Sprites:** `Texture2D` (Apple's sample class), `SingletonTextureManager` (textures by name, kept or cleared per level), `Sprite` (sprite sheets: tiles, sequences, flips, effects), `SceneManager` (47 layers of `Shape`s: position, offset, rotation, scale, texture, quad, colour; layers above 8 are the world, scrolled and zoomed around (240, 160); layers 9–27 are the "old layer" that fades out when a stage changes).
- **Menus are UIKit:** each screen is a `UIViewController` loaded from its nib (`MenuManager ReplaceViewController:transition:direction:`, swapping views with a `CATransition` of 0.5 s for a fade, 0.25 s otherwise). In an operation the HUD (`Panel`: pause, the ten tool buttons, the tutorial panel) is UIKit over the GL view.
- **Screen:** the GL view is the iPad's 1024×768 (`glOrthof` on the portrait bounds turned by 270°). `GfxScaleFactor` is 2 on the iPad (1 on an iPhone, its screen scale on an iPhone 4): the art is the iPhone 4's 960×640 ("HD").
- **Audio:** OpenAL (`MyOpenAL`: sounds by key, `playSFXWithKey:`, `playMusicWithKey:`, music and effects switches).
- **Video:** `MPMoviePlayerController` for the intro (`MovieHi.m4v`) and the ending (`EndMovieHi.m4v`), 960×640 H.264 with AAC.
- **Data:** `levels.txt` (the 30 missions: per stage its injuries with id, position, rotation and scale, body level, background, time and health; the tools), parsed by the game's own lenient JSON reader `SPJSON` (it accepts `00`); `cutscene_start_N.json` / `cutscene_end_N.json` (the story scenes: background, the four character positions, who is highlighted, the line); `Tutorials.plist` (tutorial pop-ups and the actions they trigger).
- **Saves:** `SaveDetails` (scores, times, tool levels and upgrades, sound switches, resume flag).

## Executable

- `AS2`: 1.6 MB, single-arch **armv7** Mach-O, **decrypted** (`cryptid 0`).
- **Stripped of method symbols** (4,422 symbols, mostly imports), but its Objective-C metadata is whole: 195 classes, 2,283 methods with their addresses. `re/scripts/objc_symbols.py` turns `objc_dump.py`'s dump into the `symbols.txt` the other tools read, and `re/scripts/NameFunctions.java` names the functions in Ghidra before `DumpDecomp.java` runs.
- Third-party code, removed in the port: OpenFeint (`AS2OFDelegate`, `AS2OFLaunch`), Game Center (`GameCenterManager`, `GKAchievement*`), Bango analytics (`BangoAnalyticsManager`, Core Data model `Bango.momd`), ASIHTTPRequest (`PCG_ASI*`), Reachability, SBJson, the "More Games" button.

## Game structure (class names)

- **Flow:** `GameController` (the loop: splash, menus, the game), `MenuManager`, `Loader` (loading steps), `StartScene`, `MainMenu`, `FileSelect` (episode: the main game or "CFO"), `LevelSelect`, `CutScene`, `OperationDetails`, `PauseMenu`, `MissionWin`, `MissionFailed`, `UpgradeTool`, `Credits`, `HelpScreen`, `TutorialPopup`, `Popup`.
- **Episodes:** `GameController mLevelPack` 0 is the main game (missions 1–20), 1 the "CFO" pack (missions 21–30), with its own nibs (`CFO*.nib`) and art.
- **Game:** `Game` (the mission: stages, injuries, effects, the torch), `GameState` (score, combo, the HUD sprites, background, health, time, touches, scroll and zoom, shake, red flash), `Mission`/`MissionStage`/`Connector` (missions from `levels.txt`), `Library`/`InjuryData`/`ToolData` (the injury and tool catalogue).
- **Tools** (`ITool`): `Cutter`, `Tongs`, `Lighter`, `Cream`, `Stitch`, `Vacuum`, `Chainsaw`, `Injection`, `Battery`, `Scanner`; with three upgrade levels each.
- **Injuries** (`InjuryGraphic`): cuts (`SmallCut`, `LargeCut`, `Incision`, `SkullCut`, ...), extractions (`SurfaceExtraction`, `UnderSurfaceExtraction*`, `ExplosiveExtraction*`, `PeanutExtraction`, ...), some 30 transplants (`KneeTransplant`, `BrainTransplant`, `LeftLungTransplant`, ...), creatures (`Squid`, `BigCentipede`, `Centipede`, `Bat`, `Crab`, `Dove`, `Tick`, `BigTick`, `FireFly`, `Snake`, `InsectBoss`, `Bunny`, ...), puzzles (`TypeAndTell`, `SimonSays`, `Amp`/`AmpPlug`, `Transistor`, `Canister`), burns, fires and splatters.

## Assets

| Kind | Count | Notes |
|---|---|---|
| PNG | 1,326 | Apple CgBI "crushed", as AS1 |
| MP3 / WAV / CAF / M4A | 80 / 15 / 3 / 13 | CAF is 16-bit PCM; M4A is AAC (music) |
| M4V | 4 | Intro and ending, at 480×320 and 960×640 (`…Hi`) |
| JSON | 60 | Cut scenes |
| TXT | 1 | `levels.txt` |
| PLIST | 1 used | `Tutorials.plist` |
| NIB | 23 | The menus (NIBArchive format) |

`port/tools/extract_ipa.py` writes them as the package `port/games/as2/` (see [as2.md](as2.md)).
