# Amateur Surgeon Christmas Edition — analysis

Source: `IPAs/com.turner.AmateurSurgeonXMas-iOS3.0-(Clutch-2.0.4).ipa` → `Payload/AmateurSurgeonXMas.app`

## Identity

- Bundle id `com.turner.AmateurSurgeonXMas`, display name "SurgeonXmas", v1.0.4 (Adult Swim / Turner; Mediatonic).
- **iPhone only** (`UIDeviceFamily = [1]`), iOS 3.0 minimum, built with GCC 4.2 against the iOS 4.2 SDK.
- Landscape, **480×320** (`GamePanel`: `glViewport(0,0,480,320)`, `glOrthof(0,480,320,0,-1,1)`), status bar hidden. No Retina (`@2x`) art.

## Same engine as Amateur Surgeon 1: confirmed

Christmas is a branch of the **iPhone** Amateur Surgeon 1 source tree, the same code the iPad build (`A.Surgeon` 1.0.3, ported in [port.md](port.md)) came from:

- 129 of its 155 Objective-C classes also exist in AS1: the whole A3I engine (`AdvancedImage`, `Spritex`, `FrameData`, `A3IButton`, `A3ITextDrawer`, `ResourceManager`, `Morph`/POD), the game framework (`GameCanvas`, `Operation`, `Patient`, `Layer`, `Wound`, `Toolbar`, `TimeGlass`, `LifeMeter`, `ScoreTimer`, `StoryBoard`, `DialogPlayer`) and every wound, creature and organ class.
- 1,487 of its 1,861 methods have the same name as an AS1 method. Comparing each pair's sequence of selectors, classes, ivars and strings: 535 are identical, 108 at least 90 % alike and 565 at least 60 % alike. The rest differ mostly because the AS1 iPad methods branch on `DeviceConfiguration` (iPad or iPhone), which Christmas does not have, and because of the Christmas additions below.
- The shared data files are byte-identical in format (`Karl.txt`, `boneSpot.txt`, `PNGpackinglist.txt`, the `*Spot*.txt` path tables, the `a`/`b`/`c` animation tracks, `dialogue.xml` + `dialogue_seq.xml`). The engine's existing parsers read them; only the coordinates are iPhone-sized.
- Assets have no `ipad_` prefix (the iPhone names).

Differences from AS1 iPad in the shared code:

- Online code is gone: no OpenFeint, Facebook, Twitter or HS high-score server, and no `ReportBoard`, two-player mode or `DeviceConfiguration`.
- Many AS1 tuning constants that were ivars (`ANT_MAX_SPEEDX`, `SMALLCUT_WIDTH`, …) are compile-time constants here.
- Audio is the older OpenAL + AudioQueue engine (`OpenALObject`, `OpenALThread`, `SoundEngineEffect`, `BackgroundTrackMgr`) behind the same `MediaPlayer` interface.
- Additions to shared classes: wounds can have a parent and be suspended and resumed (`Wound parentWound`, `suspendWound`), `OrganTransplant`/`RemovedObject` hold bound sub-wounds (gift boxes, ice, fire, scorpions), `Layer` can flip-animate its background, ants can be iced, `FireWound` throws random explosions, and `Operation` adds background vibration and scream types.
- Still compiled in but unused: `Operation01`–`Operation30` (and their routing, events 10–39). The bundle has no art for them.

## Christmas-only code

| Classes | What |
|---|---|
| `OperationXmas01`–`05` | The five patients; screen events 40–44 in `GameCanvas DispatchEventIntoView:` |
| `ReinDeer`, `DeerRibCagePuzzle`, `PuzzleWound`, `PresentPaper`, `BombGiftBox`, `ScopionGiftBox`, `TestTubeGiftBox`, `TestTube`, `Ice`, `IcedHeart`, `IcedKidney`, `Intestine`, `Stomach`, `StapleVessel`, `WireCover`, `Dirt`, `ElectricExtraction` | New wounds and organs |
| `M2DMotionWelder`, `M2DImage`, `M2DOpenGLImage`, `M2DFrameData`, `M2DResource*`, `M2DImageListDrawer` | MotionWelder animation player (the `.anu` files): main menu, credits, help menu |
| `MainMenu`, `PatientSelection`, `StoryBoard`, `HelpMenu` | Reworked for the Christmas look (Santa's sleigh, snow, reindeer nose and eyes, a phone-shaped help screen) |

## Executable

- `AmateurSurgeonXMas`: fat Mach-O, **armv6 + armv7**. The armv7 slice is the one analysed (`re/xmas/AmateurSurgeonXMas`).
- **Decrypted** (Clutch; `cryptid = 0`) and **not stripped** (21,541 symbols, full Objective-C names).
- 690 KB of `__text`, about 620 KB of it game and engine code; no third-party SDKs.
- Built **position-independent**: every selector, class, ivar and string is loaded as `literal + pc`. Ghidra prints that as `(DAT_x + 0x1234)`, which `re/scripts/annotate.py` resolves (see [port.md](port.md#reverse-engineering-workspace-re)).

## Assets

| Kind | Count | Notes |
|---|---|---|
| PNG | 506 | All Apple CgBI "crushed", as in AS1 |
| JPG | 7 | Backgrounds |
| TXT | 97 | Animation, path and atlas data (same formats as AS1), plus developer leftovers (`Readme.txt`, `changeLog.txt`, `setup.txt`) |
| WAV / MP3 | 43 / 4 | Effects and music |
| M4V | 2 | `splash.m4v`, `storyOpening.m4v` |
| XML | 2 | `dialogue.xml`, `dialogue_seq.xml` |
| POD | 5 | Poison bottle models (as AS1) |
| ANU | 2 | MotionWelder animations (`as_credit.anu`, `helpPageMenu.anu`); new format |
| `lang/` | — | `en/` and `bala/` string tables, as AS1 |

## Porting approach

**This describes the iOS-based port, now archived** (`port/attic/xmas-ios/`, [christmas-ios-port.md](christmas-ios-port.md)). The game that builds today is rebuilt from the Flash original instead: see [christmas.md](christmas.md).

The AS1 port's platform and engine layers apply as they are (the game now runs on AS1's 1024×768 screen, each Christmas panel fitting its iPhone layout into it), with two new pieces: the MotionWelder (`.anu`) player and the `.anu` asset type. The game layer follows the Christmas decompile (`re/xmas/ann/`), with the AS1 port's classes as the worked translation of the shared logic: same class structure, iPhone coordinates and the Christmas additions.

The art is 480×320. Drawn at native resolution it is still the low-resolution art scaled up, as on the original device. The port has since been restyled after AS1 at the user's request: AS1's 4:3 screen and HUD layout, and AS1's higher-resolution art wherever it is the same picture ([christmas-ios-port.md](christmas-ios-port.md#christmas-on-as1s-screen)).

## The five patients

| Patient | Operation | What it asks for |
|---|---|---|
| 1 Generic the elf | `OperationXmas01` | Debris (ornaments, a sweet, a stick) to pull out and treat; under his chest, four glowing lights (`ElectricExtraction`): cut each, pull its bulb, heal the cut; wire covers (`WireCover`) to saw free and lift; more debris |
| 2 Mincer the reindeer | `OperationXmas02` | Rebuilt part by part (`ReinDeer`, a 52-step script: eye, horn, body, legs, nails, a tool for a tail jump-started with the tazer, a light-bulb nose), opened twice: the rib-cage puzzle (`DeerRibCagePuzzle`) and green half intestines (`Intestine`) |
| 3 The officer | `OperationXmas03` | Sawn out of a block of ice (`Ice`), the frozen kidney melted and replaced (`IcedKidney`), ice shards; the frozen heart (`IcedHeart`) bound to three ants frozen in ice cubes; an ant nest found by the scan |
| 4 Bjorn the bipolar bear | `OperationXmas04` | Swallowed presents: a test tube of poison (`TestTubeGiftBox`, `TestTube`), a bomb that starts fires, a box of scorpions that run off; scorpions on his kidney; a shoe and a hand found by the scan |
| 5 Santa | `OperationXmas05` | A car wheel to pull out; twice his chest is rebuilt (ligament puzzle `PuzzleWound`, severed vessels `StapleVessel`, present paper `PresentPaper`); dirt (`Dirt`) and stones on his lungs; his stomach fed five cupcakes (`Stomach`) |

Most of the patients' steps are `OrganTransplant` state scripts: the Christmas engine adds states for melting ice (10), fires (11), sub-wounds that must be treated (12, reported through `callbackInSubWound:state:`), suspending until the layer comes back (13), the tazer (14), leaving a part on the layer (15), scorpions running off (16), feeding the stomach (17) and gift boxes fading or blowing up (18). See `port/attic/xmas-ios/src-xmas/wounds/RemovedObject.h`.

## Notes from porting

- `font_xmas.png` (canvas font 3, used by the help phone, the score texts and the patient files) is `FontHeight` 14 and `SpaceWidth` 4, not the 60/12 of the other fonts; with 60 each glyph also shows the row of glyphs below it.
- The organ classes pass their part areas to `FrameData initWithOffsetX:…` in float registers; `re/scripts/emucalls.py` shows them (soft-float ABI: they travel in core registers). `--dump` prints the object's ivars after an init, `--all` also shows `initWithFormat:` strings.
- `re/scripts/cfstrs.py <macho> <start> <end>` prints the strings behind a table of NSString pointers (the help texts, the light and bulb names).
- Static tables are mutated in place by the original (`correctBone`); the port keeps such state per wound.

## Port changes from play-testing

These depart from the original on purpose, after play-test reports:

- **Corkscrew (drill + pixie dust):** the drag must follow the red wedge of `CorkScrew`. The port widens its hit rect by 25 px (`kHitMargin`, `Effects.cpp`) and lets the finger stray for 6 drag samples (`kGracePasses`) before the turn restarts; the original reset on the first sample outside the exact wedge.
- **Mincer's rib-cage puzzle:** the original snapped a bone in when its edge touched the next bone's contact area, which locked bones in from the wrong place. The port snaps a bone only when it is within 30 px (`kSnapDistance`, `DeerRibCagePuzzle.cpp`) of its own place and the bone it rests on is already in.
- **Santa's cupcakes:** the stomach's target (`OrganTransplant` state 17) is enlarged by 45 px each side (`kFeedMargin`).
- **Look and layout:** AS1's 4:3 screen, its HUD layout (one row of tools along the top), no zoom buttons, AS1's HD art where it matches; see [christmas-ios-port.md](christmas-ios-port.md#christmas-on-as1s-screen).
