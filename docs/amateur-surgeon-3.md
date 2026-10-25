# Amateur Surgeon 3 (iOS) 1.02 — analysis

Source: `IPAs/Amateur Surgeon 3.ipa` → `Payload/Surgeon3.app`. AS3 has no Flash original: the IPA is the only source ([plan.md](plan.md), step 7). The port is to run on Bleed Engine like the others: its content converted into the engine's package layout and every capability it needs added to the engine generally ([rules.md](rules.md) §1). cocos2d-x is a reference for how the original behaved, not a runtime to bring along.

## Identity

- Bundle id `com.turner.amateursurgeon3`, display name "Surgeon 3", version 1.02 (build 5.9; `Data/build_date`: 1 August 2013). Adult Swim / Turner, developed by Mediatonic.
- Universal (iPhone and iPad, `UIDeviceFamily` [1, 2]), iOS 6.0 minimum, built with Xcode 4.6 against the iOS 6.1 SDK.
- Landscape (left and right), status bar hidden; it requires an accelerometer.

## The engine: cocos2d-x 2.1.1, in C++

AS3 is a third code base, after AS1/Christmas (A3I, Objective-C) and AS2 (OpenGL ES + UIKit, Objective-C). It is **cocos2d-x 2.1.1** (C++), with Mediatonic's game in the namespace `mt`.
- **Telling the version.** Its classes include `CCClippingNode`, `CCDrawNode` and `CCHttpClient` (2.1 on), and `CCObject` lacks `acceptVisitor` (added in 2.1.2). The virtual tables of 82 of its 86 cocos2d classes match 2.1.1's headers slot for slot. Of the other four, `CCParticleSystem` and `CCParticleSystemQuad` only seemed to have 8 more virtual functions: their 180 slots are 2.1.1's once its virtual property accessors are counted (the header parser misses some; the game's own calls agree: slot 93 `setDuration`, 160 `setEmissionRate`, 173 `getEmitterMode`). `CCApplication` and `CCEGLView` were changed by Mediatonic.
- **Libraries.** CocosDenshion (`SimpleAudioEngine`, sounds), JsonCpp (`Json::`), kazmath, and the extension `CCScrollView`.
- **The iOS side** is Objective-C: `AppController`, `EAGLView`, `iOSPaymentsWrapper` (StoreKit), the movie player, Game Center.

## Executable

- **Binary.** `Surgeon3` is 8.6 MB, a fat binary with **armv7** and **armv7s** slices, **decrypted** (`cryptid 0`). The armv7 slice is `re/as3/Surgeon3`.
- **Stripped**, but C++ RTTI survives: 278 type infos (160 of them in `mt`) and 368 vtables.
- **Naming the functions.** `re/scripts/cxx_rtti.py` names every virtual function (`symbols.txt`, `classes.txt`: each class's bases and vtable slots, and the class whose override each slot holds). `re/scripts/cxx_slots.py` takes the cocos2d slots' names from the 2.1.1 headers (`slots.txt`), so a game class's `onEnter`, `update` or `ccTouchBegan` is named. The game's own virtual functions stay `vfN` until read.
- **Reference source.** The cocos2d-x 2.1.1 source the binary was built from is kept in `re/as3/cocos2d-x-2.1.1/` (MIT): the behaviour reference for what the engine has to do.
- **Decompile.** `re/as3/src/<Class>.c` comes from Ghidra (6153 functions):

  ```
  python re/scripts/cxx_rtti.py re/as3/Surgeon3 re/as3/classes.txt re/as3/symbols.txt re/as3/slots.txt
  tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat re/ghidra AS3 -import re/as3/Surgeon3 -overwrite -scriptPath re/scripts -preScript NoReturnGuessOff.java -postScript Rebase.java 10000000 -postScript NameFunctions.java re/as3/symbols.txt -postScript DumpDecomp.java re/as3/src
  ```

  - `NoReturnGuessOff.java` keeps Ghidra from deciding that `std::string`'s constructor never returns, which cut 669 functions short.
  - `Rebase.java` moves the image up by 0x10000000 after the analysis, so that small struct offsets are not read as pointers into the image. The printed addresses are still the binary's own.
- **Third-party code, all removed in the port:**
  - PlayHaven (ads, `AdMgr`) and Kontagent;
  - Bango, Push IO, and BGO / gamefuel analytics (`UserTrackingMgr`, `StatsMgr`, `analytics.gamefuel.info`);
  - the content server (`content.gamefuel.info`, `assets.gamefuel.info`: newer data files; the bundle's own are the defaults);
  - the Adult Swim news feed (`ASNewsFeed*`) and Game Center (`GamingSocialNetworkMgr`: leaderboards, achievements);
  - StoreKit in-app purchases (`PaymentsMgr`, `iOSPaymentsWrapper`), cloud saves (`CloudMgr`), Facebook, mail.

## Game structure (`mt` class names)

- **Screens (`mt::Scene`, a `CCScene`):**
  - `MediatonicSplashScreen`, `IntroScreen`, `WelcomeScreen`, `SaveSlotScreen`, `WorldScreen` (the world map) and `AreaScreen` (an area's map);
  - `NewAreaScreen`, `AreaLockedScreen`, `BunkerPreviewScreen`, `InfoScreen`, `TagInfoScreen`, `DialogueScreen` (story scenes);
  - `PreOpScreen`, `SurgeryScreen`, `PauseScreen`, `SurgeryEndScreen`, `LifeWarningScreen`, `UpgradeScreen` (the tech tree);
  - the shop: `ShopCurrencyScreen`, `SurgeryCurrencyScreen`;
  - the tag partners: `PartnerSelectScreen`, `PartnerInfoScreen`, `PartnerCooldownInfoScreen`, `CooldownInfoScreen`;
  - `GeneralTipPopupScreen`, `CreditsScreen`, `LoadingScreen`.

  `SceneMgr` switches between them, and `ContentManager` holds the data.
- **Surgery:**
  - `InjuryState` is the injury base class, and `ToolState` the tool base (`Cream`, `Lighter`, `Stapler`, `Cutter`, `Tazer`, `Tongs`, `Chainsaw`, `Injection`, `Vacuum`, `Scanner`, the clamps).
  - The tutorials are `TutorialMgr`, with their actions (`…TA`) and conditions (`…TC`).
  - The effects (`Effect`) are about 40 of them: tag powers, screen flashes, particles.
- **Injury classes:** cuts (`SmallCut`, `Incision`, `LargeCut`, `SnakeCut`), extractions (`SurfaceExtraction`, `UnderSurfaceExtraction`, `AngledExtraction`), `Transplant` (39 kinds), `BrokenBone`/`BrokenVein`, `Artery`, `Burn`, `Fire`, `FrostBite`, `Boil`, `CementDrop`, poison (`PoisonArea`, `SuperPoisonArea`, `SuperPoisonScab`), and the creatures (`Bat`, `MiniBat`, `BlackFrog`, `PoisonFrog`, `MiniFrog`, `Flea`, `Leech`, `IceAnt`, `GiantWorm`, `MiniWorm`, `FireMaggot`, `Snake`, `RobotScorpion`, `SpikeBase`).

## Content: data-driven JSON

| Folder | Holds |
|---|---|
| `Data/areas.json` | 6 areas (Bunker, Shanks-a-lot, Mumbo Jungle, Lunacy Falls, Robotopia, Exile Clinic): their surgeries, boss, map, music, point and damage multipliers |
| `Data/Surgeries/` | 69 surgeries: 23 patients, most with two challenge variants. Each has stages (background, music, health, time, injuries with class, place and rotation), its tools, star thresholds by score and time, sheets and sounds; `surgeries_info.json` holds the case files' texts and bonus objectives |
| `Data/Injuries/` | 120 injury definitions on about 35 base classes: stages with state, tools, damage, timers, particles, animations, hit-area polygons |
| `Data/Tutorials/`, `Data/Dialogues/` | 10 tutorials (steps of conditions and actions), 51 story scenes (background, four character slots, the line) |
| `Data/tools.json` | the 11 tools' wrong-use damage, cooldowns, healing, grace periods |
| `Data/default_data.json` | the economy and progression the content server could override (below) |
| `common/` | Mediatonic's own formats: UI layouts (`ui_*.json`: items, sprites, groups, positions), frame animations (`anim_*_anim.json`: clips, props, timelines), transplants (pieces, replacements, dependencies), maps |
| `1.0x/`, `2.0x/`, `4.0x/` | the art by content scale: TexturePacker sheets (`.plist` + `.pvr.ccz`, PVRTC 4 bpp, up to 4096²), particle systems (cocos2d `.plist`), BMFont fonts (`.fnt` + `.png`), loose PNGs |
| `Sounds/` | 263 CAF sounds, IMA4 ADPCM |
| `Videos/` | the intro movie (`intro.m4v`, `intro.mp4`) |
| `Shaders/` | GLSL ES shaders (`Custom`, `ScreenAreas`), cocos2d 2's programmable pipeline |

- **Scales.** The game picks its scale folder from a table of 19 screen sizes. Its design screen is 480×320 points.
- **Why 4.0x is enough.** The 4.0x set is the iPad retina's and the largest. 2.0x has two files 4.0x lacks, and both are unused:
  - `surgery_ernestpickles_sheet4`'s frames are all in the 4.0x sheets, and no surgery loads it;
  - `dialogue_char_Reverand copy` is a stray copy.
- **Sizes.** The 4.0x `.pvr.ccz` sheets are 120 MB. Written out as PNGs they would be about 520 MB, so the package keeps them as they are and the engine reads them (`platform::decodeImage`, [engine.md](engine.md)).

## The package: `port/games/as3/`

The package is now made from the final build, 1.45 ([as3.md](as3.md#how-it-is-built)): `python port/tools/extract_ipa.py "IPAs/Amateur Surgeon 3 v1.45 (iOS).ipa" port/games/as3` (about 1 600 files, 243 MB with the port's own additions below; 1.02's package was 1156 files, 181 MB). The bundle's folders are kept as they are:
- `4.0x/`: the sheets as `.pvr.ccz` with their property lists as JSON, the fonts (`.fnt` + `.png`), the particle systems as JSON, the loose PNGs;
- `Sounds/`: the CAF sounds as they are (IMA4, a quarter of their size as WAV), decoded by the engine (`platform::decodeCaf`, [engine.md](engine.md));
- `Data/`, `common/`, `Videos/`, `Shaders/` as they are.

Left out:
- `1.0x/` and `2.0x/`;
- the news feed's nibs and pictures;
- the libraries' build leftovers (kazmath, marmalade, JsonCpp, Push IO, a sound conversion script);
- launch images and icons.

`extract_ipa.py` mends the shipped data's own mistakes as it writes the files (`--correct <dir>` for a package extracted before), on the text, so the package is otherwise byte for byte the IPA's: CORRECTIONS sets a top-level string key only where the file still holds the shipped value (the patients' screams); TEXT_CORRECTIONS replaces a piece of text only while its correction isn't there yet (the mini worm's animation naming its whole art, the whale's old rib tinted sick: [as3.md](as3.md)). It also writes the port's tool art (`toolart/`: AS3's tools without their cards, made from the HUD's buttons by `as3_toolart.py`; [as3.md](as3.md#the-tool-art-the-ports)). The user's restored music (`music restored/as3/`) goes into `Sounds/` in place of the shipped tracks it replaces (`REPLACES`; `--restore-music <dir>`; [as3.md](as3.md#restored-music)).

The game asks for files by their bundle names (`4.0x/anim_bat_sheet.plist`), which the engine's lookup maps to the converted files.

## The free-to-play economy

`default_data.json` describes a freemium game, which the port has to settle (the online parts go; what stays is a design decision):
- **Coins.**
  - Earned in surgeries.
  - Bought in five in-app packs (1 000 to 33 750 coins).
  - Spent on upgrades, partners and shop items.
- **Blood packs (energy).**
  - At most 3; one comes back every 30 minutes (`bloodpack_recharge_secs` 1800).
  - A surgery started after the first two costs one; a win gives it back.
- **Stars** (1 to 3 a surgery, by score and time) unlock the areas (2, 15, 30, 55, 75 stars) and the tech tree's rows (5 to 55 stars).
- **Upgrades:** 30 in a tech tree, bought with coins (300 to 7 500).
- **Tag partners:** 8 helpers bought with coins (up to 8 500), used for a power in surgery, then cooling down for a while.
- **Shop** (1.02; 1.45 dropped `shop_items`): Atomic Toolbox (every upgrade for one surgery, 1 500), Coin Doubler (10 000, once), Super Serum (clears a surgery at once, 2 500).

**The user's decisions for the port (2026-10-20):**
- **Blood packs are free** (changed 2026-10-21; first removed): they work as the original's (at most 3, one back every 30 minutes, a surgery started after the first two costs one, a win gives it back), but are never sold: out of packs, the player waits ([as3.md](as3.md#blood-packs)).
- **Coins are earned only**, at the original's rates; the coin packs sold for money are gone.
- **The partners' cooldowns stay, 20 times shorter:** each partner's `cooldown_duration` is divided by 20 (`kPartnerCooldownDivisor`, `port/src/as3/PartnerInfoUtils.h`). Mr Giblets 25 min → 75 s; Filthy Nowe 6 h → 18 min; Crumpet 8 h → 24 min; Flowmaster 12 h → 36 min; Procrastibot, Officer Brutality, Lady Saturday and the Reverend 23 h 20 min → 70 min. The price to end one at once (`end_cooldown_cost`, in earned coins) is unchanged.
- **No shop, no consumables, no revive** (2026-10-21; the shop's items had first stayed, bought with earned coins): [as3.md](as3.md#shop-consumables-and-revives-removed).
