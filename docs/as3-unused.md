# Amateur Surgeon 3 — unused and cut content in the originals

A survey of what the original games ship but never use: code that can't be reached, data and art no code or data names, features built halfway. It covers both originals: the iOS 1.02 app (`IPAs/Amateur Surgeon 3.ipa`, decompiled in `re/as3`, packaged in `port/games/as3`) and the Android 1.43 build (`IPAs/Amateur Surgeon 3 v1.43 (Android).xapk`, decompiled with its real symbol names in `re/as3android`). Nothing here is implemented yet. The list is for choosing what to bring into the port ([as3.md](as3.md)).

Each item has the evidence that it is unused, what it would do, and an effort rating: **small** (data, or a few functions in existing classes), **medium** (a new class or screen, or changes across several), **large** (a whole new system or new art). The groups are:

- **(a)** finished features that only need wiring;
- **(b)** partial features, with enough code or data to rebuild them;
- **(c)** bare leftovers: an image, a sound or a file on its own;
- **(d)** things that only made sense online or with money. These are out of scope, because the port is offline and free to play.

## How it was found

- **Names.** Every name in both packages was checked against every string in both binaries (iOS `Surgeon3`, Android `libgame.so`) and every data file in both packages. Those names cover sheet frames, sheets, animations and their animation names, particles, sounds, layouts, injuries, surgeries, tutorials, dialogues and JSON keys. The search excluded each file's own listing, and the sound-scene lists (below), which no code reads.
- **Names the code builds.** These were allowed for, from the port's code (which mirrors the original's) and the decompiles:
  - layout `spritePrefix` + `-normal/-selected/-disabled/-locked/-grey`;
  - an item's prefix + `-` + a suffix set by code (`setItemImage`);
  - animation clips `<clip>_<n>.png`;
  - injury pictures `injury-<class>-<state>.png`;
  - random sound families `<name>_1..N`, the screams `<scream>_scream_1..4`, the patient photos `surgeryinfo_photo-<photo_suffix>`.
- **Reachability.** Injury definitions, surgeries, tutorials and dialogues were followed from `Data/areas.json`: through the surgeries' stages, `extra_injuries`, `injury_to_create_*` and the transplant data files, then checked against code that makes injuries by name.
- **Dead code.** In Android's `libgame.so` (built at -O0, so nothing is inlined away), a function counts as dead when nothing references it. That means no direct `bl`/`b`/`blx`, no pointer to it in the GOT, the vtables or the data. Functions referenced only from dead ones count as dead too. Of 5029 `mt::`/`AppDelegate::` functions, 117 are dead. Most of them are online parts. The iOS build is optimised, so its dead code was mostly stripped. It was checked by its strings, and by the always-false flag function at 0xaa11c.
- Not surveyed in depth: virtual slots that are overridden but never called. Android's virtual calls go through offsets, so they can't be told apart without type analysis.

## Best candidates

| # | What | Group | Effort |
|---|---|---|---|
| 1 | Three finished tutorials no surgery names (Batshift's bats, the Reverend's robo-scorpions, Useful Brother's ice ants), plus the tutorial condition they need | (a) | small |
| 2 | The developer cheats (`Cheats::IsCheatEnabled`, iOS 0xaa11c): about 20 switches the code still checks, all off. Several make good debug-menu or "assist" options (hit anywhere, permanent Reverend power, endless tag power) | (a) | small |
| 3 | Achievements: 22 defined, and every one of their stats is counted by the game. Only Game Center ever showed them. A local achievements list | (a)+(b) | medium |
| 4 | Using the Atomic Toolbox and Super Serum mid-surgery: the pause tray's "supertools" panel and `SurgeryScreen::useConsumable`. Without it, the Atomic Toolbox can be bought but never used | (b) | medium |
| 5 | The in-game surgery editor (Android): a complete, unreachable level editor | (b) | large |
| 6 | Cut injuries with art: LighterShiv, OrganShard1–3, MonitorShard1, Toothbrush_Green, Earnest Pickle's old right knee, the giant worm's chest wrap | (b) | small each |
| 7 | Cut tag partner "Beetle" / "Bugdeuce" (a bug-eyed Mr Giblets) | (c) | large |
| 8 | Unused sounds and animations that fit existing moments: the tutorial success jingle, the Reverend's auto-tool twinkle loop, the tool-boost start/end, the bat's wing opening, the frog's idle and jump, the scorpion's swoosh | (c) | small |
| 9 | Android-only extras the port lacks: the "free partner of the day" and three more tutorials (Sweet Meats Pete, Tammy Gracefuls, Earnest Pickle) | (a) | small |
| 10 | Le Flairground Fantastique (Pocket God): four case files, text only | (c) | large |

## (a) Finished features that only need wiring

### Three tutorials no surgery names (Android 1.43)

- **What.** Android's `Data/Tutorials/` has three finished tutorials that no surgery's `tutorial_filename` names, in Android or anywhere else:
  - `tutorial_lunacyfalls_batshift`, 3 steps: "Bats!? What was he THINKING!? First I'd better BURN them to WAKE them up, ONE at a time." / "If I SHOCK them three times, that should sort them out." / "But quickly! If I leave him on the stage, BABY BATS will fly out! Remove him with the TONGS!";
  - `tutorial_robotopia_revsorgan`, 2 steps: "Robo-scorpions! I need to SHOCK them to fuse their internal machinery." / "I'd better CLAMP him down to stop him escaping. Then I can CHAINSAW him.";
  - `tutorial_exile_usefulbro1`, 1 step: "SHOCK THE ANTS! AS FAST AS POSSIBLE! Don't touch FROST, either. Not without a LIGHTER."
- **Evidence.** None of the 69 surgeries (iOS or Android) names them. The iOS package doesn't ship them.
- **What wiring needs.**
  - Each starts with `TimeElapsedOnStage {time}`, a condition iOS doesn't have (no `TimeElapsedOnStageTC` in the iOS binary) and the port doesn't have. Android has it: `mt::TimeElapsedOnStageTC`, ctor 0x82b79c reads `time`; `check` 0x82b844 is `time <= SurgeryState+0xd24` (the time spent on the current stage).
  - Then each surgery's `tutorial_filename` has to be set.
  - The injury names must match the iOS surgery files. `revsorgan` (stage 3, `robotscorpion_1`) and `usefulbro1` (stage 2, `iceant_2`) match. Batshift's stage 3 is iOS's third stage, the one with the bats, but its three bats are all named `bat` on iOS, where the tutorial wants `bat_1..3`.
- **Effort.** Small: one condition class in `TutorialMgr`, three file copies, small data edits.

### Developer cheats (`mt::Cheats`)

- **What.** `Cheats::IsCheatEnabled(code, string*)` always returns 0. Its body is compiled out: Android 0x3c61c0; iOS 0xaa11c (`movs r0,#0; bx lr`, called `mt::debugFlag` in `re/as3/names.txt`). `EnableCheat`, `DisableCheat` and `Init` are empty and never called (Android 0x3c6184, 0x3c61a4, 0x3c6170). The checks are still there: 55 call sites in Android, 46 in iOS.
- **The codes.** Read from Android's call sites (`re/as3android/ann`):

  | Code | Where | What it does when on |
  |---|---|---|
  | 0 | `InjuryState::runInteractionsWithHitGroup` | A tool wears every hit area of the group, wherever it touches ("hit anywhere") |
  | 5, 6 | `SurgeryScreen::processOnPress` | Skip the press's tool/cursor placement (touch tests; unclear) |
  | 7 | `runToolLogic`, `onToolWrongUsage` | No damage from misusing a tool |
  | 8 | `SurgeryScreen::runLogic` | Fill the tag meter every frame |
  | 9 | `dealDamage`, `onToolWrongUsage`, `runActionEvents` | No damage at all; a sudden-death mistake doesn't kill |
  | 10 | `runToolLogic`, `checkToolUsage`, `runLogic`, `processOnPress`, `runActionEvents` | The Reverend's power for ever: automatic tool choice, no misuse damage, its timer never runs out, injury events do no damage |
  | 0xb | `WelcomeScreen`, `SaveSlotScreen`, `WorldScreen`, `AreaScreen`, `PreOpScreen`, `SurgeryScreen::showCompletedScreen`, `FacebookPopupScreen` | Unlock everything: every area and boss, skip the first-surgery rules |
  | 0xc / 0xd | `SurgeryScreen::loadAssets` | Skip tutorials / replay completed ones |
  | 0xe | `AdMgr::preloadAd`, `showAd` | No ads |
  | 0xf | `IntroScreen::onBecomeActive` | Skip the intro movie |
  | 0x10 | `UpgradeScreen` (3 sites) | Every tech-tree row open, whatever the stars |
  | 0x11 | `PartnerSelectScreen`, `PartnerInfoScreen` | Every partner unlocked, no cooldowns shown |
  | 0x12 | `GameStateUtils::GetStarsBalance*` | 250 stars |
  | 0x13 | `SurgeryUtils::getSurgeryToLoadFullPath`, `TutorialMgr::loadTutorial`, editor | Load surgeries and tutorials from the bundle's `Data/`, not `Downloaded/` (development) |
  | 0x14 | `WorldScreen::loadAssets` | Called, result ignored |
  | 0x15, 0x16, 0x17 | `SurgeryScreen::onBecomeActive`, `loadAssets` | Load-only tests: load the sheets, then go back to the pre-op screen |
  | 0x19 | `AppDelegate::applicationDidFinishLaunching` | The Facebook bonus ×10 (d) |

- **Port.** The debug menu already has god mode (≈9), unlock all (≈0xb) and tag power full (≈8). Still worth adding: 0 (hit anywhere), 10 (permanent Reverend), 7 (no misuse damage), 0x12 (stars), 0x10/0x11 (tech tree and partners). They could also become player assist options.
- **Effort.** Small.

### The free partner of the day (Android 1.43 only)

- **What.** Android's `default_data.json` has `free_daily_partner_list` (written to `Downloaded/free_daily_partner_list.json`): an order of nine partners. `SettingsInfoUtils::LoadFreePartnerList` (0x6daa9c), `getNumFreePartners`, `getFreePartnerFromID` and `PartnerInfoUtils::LoadPartners` (0x5fb1e8) make one partner free each day (the day number modulo the list).
- **Status.** Live on Android, not in the port (which follows iOS data). It is not online.
- **Effort.** Small (`PartnerInfoUtils`).

### Tutorials Android added to three surgeries

- **What.** Android names `tutorial_mumbojungle_sweetmeatspete` (veins: clamp both ends, then the tongs), `tutorial_mumbojungle_tammygracefuls` (zap the snakes, then the chainsaw; cut the poison, then vacuum it) and `tutorial_shanksalot_earnestpickle` (cut the ribs along the yellow lines, then replace them).
- **Status.** iOS 1.02 has no tutorial for these surgeries.
- **Effort.** Small: the files and `tutorial_filename`. The injury names need checking against the iOS surgery files.

### Dialogue lines per tag partner

- **What.** Every dialogue line has `tag_partner_required`. `DialogueScreen` shows a line only when the chosen partner matches, and the port's `DialogueScreen` filters by it too.
- **Evidence.** All 879 lines in the 61 dialogue files leave it empty, so partner-specific banter was planned but never written.
- **Effort.** Small in code (none). Needs writing.

### Android's "coming soon" world-map pages

- **What.** Android's `WorldScreen::loadAssets` adds a page before and after the areas, each a `worldmap_mainbutton_comingsoon-locked.png` sign (Alan on a TV, "COMING SOON!", tags 1000/1001).
- **Status.** The port leaves them out on purpose ([as3.md](as3.md#the-hospital-android-143)).
- **Effort.** Small, if ever wanted.

## (b) Partial features with enough to rebuild

### Achievements, kept locally

- **Data.** `Data/achievements.json` lists 22 Game Center achievements, each `{achievementId, statType, countToUnlock}`:

  | Id | Stat | Count |
  |---|---|---|
  | as3ach_high5 | UseTagPower | 1 |
  | as3ach_tagyourit | UseTagPower | 25 |
  | as3ach_ladysaturday | UnlockLadySaturday | 1 |
  | as3ach_reverend | UnlockReverend | 1 |
  | as3ach_kleptomaniac | ExtractItem | 5 |
  | as3ach_10bluestars | CollectBlueStar | 10 |
  | as3ach_10redstars | CollectRedStar | 10 |
  | as3ach_contender | CompleteBonusChallenge | 1 |
  | as3ach_holybatjuice | ExtractBatMutagen | 1 |
  | as3ach_holdstillmaggot | ClampMaggot | 1 |
  | as3ach_clampetyclampclamp | Use4ClampsAtOnce | 1 |
  | as3ach_3stars | Achieve3StarsOnSurgery | 1 |
  | as3ach_fleacircus | ZapFlea | 50 |
  | as3ach_freshmeat | ReplaceOrgan | 100 |
  | as3ach_allthestars | CollectYellowStar | 69 |
  | as3ach_technicalwizard | UpgradeTech | 30 |
  | as3ach_necromancer | RevivePatient | 5 |
  | as3ach_acleanbreak | RemoveLeechNotLeavingTeeth | 1 |
  | as3ach_unflappable | CompleteSuddenDeathSurgery | 5 |
  | as3ach_expertvirologist | CureSuperPoisonPatch | 5 |
  | as3ach_lifeandlimb | ReplaceLimb | 10 |
  | as3ach_massmurderer | KillPatient | 50 |

- **The stats.**
  - `StatsMgr` keeps them in the slot's `statProgress` and reported them to Game Center (`syncAchievementsWithSocialNetwork`). Its name table is iOS 0x440ad4 / Android `mt::StatsMgr`. In order:
    - 0 Achieve3StarsOnSurgery, 1 ClampMaggot, 2 CollectBlueStar, 3 CollectRedStar, 4 CollectYellowStar, 5 CompleteBonusChallenge;
    - 6 CompleteSuddenDeathSurgery, 7 CureSuperPoisonPatch, 8 ExtractBatMutagen, 9 ExtractItem, 10 KillPatient;
    - 11 RemoveLeechNotLeavingTeeth, 12 ReplaceLimb, 13 ReplaceOrgan, 14 RevivePatient, 15 UnlockLadySaturday;
    - 16 UnlockReverend, 17 UpgradeTech, 18 Use4ClampsAtOnce, 19 UseTagPower, 20 ZapFlea;
    - 21–25 TaggedGiblets/Flow/Reverand/Bot/Saturday, **26 TaggedBeetle**, 27 UsedBloodpack, 28–30 TaggedNowe/Crumpet/Brutality.
  - Android's `StatsMgr::incrementStat` call sites count every one of them:
    - `AngledExtraction` 9; `Bat` 8; `ClampsController` 18; `FireMaggot` 1; `Flea` 20; `Leech` 11; `SuperPoisonArea` 7; `Transplant` 12/13; `UpgradeScreen` 17;
    - `SurgeryScreen`: 0, 2–6, 10, 14, 15, 16, 19, the Tagged* ones, and 27.
- **Missing.** Titles, descriptions and pictures: they lived on Game Center's servers. The titles can be guessed from the ids ("Kleptomaniac", "Holy Bat Juice", "Hold Still Maggot", "Clampety Clamp Clamp", "Flea Circus", "Fresh Meat", "Necromancer", "A Clean Break", "Unflappable", "Expert Virologist", "Life and Limb", "Mass Murderer", "Technical Wizard", "Tag You're It"...).
- **Port.** `SaveData` already loads and saves `statProgress`, but only the Tagged* stats are counted (`SurgeryPartners.cpp`).
- **Effort.**
  - Counting: small, about 20 one-line increments at the places listed.
  - A list screen and an unlock popup: medium. The popup can reuse `GeneralTipPopupScreen`; there is no art for it.

### Using the Atomic Toolbox / Super Serum in surgery

- **The code.** `SurgeryScreen::useConsumable(std::string)` (Android 0x796b04) is dead: nothing calls it, and iOS doesn't have it at all. What it does:
  - `"superserum"`: pause, `SuperSerumEffect`;
  - `"atomictoolbox"`: `ToolState::upgradeTool` on every tool, a `ToolsSheenEffect` over the tool buttons, and `GameState+0xc4 = 1` (a "toolbox used" flag, saved).
- **The layout.** `common/ui_pause_screen.json` has the panel it was meant for: `supertools_panel`, `supertools_item01/02` (`supertools_icon01/02`), `supertools_button_use01/02` (`supertools_traybutton-use`), and the count labels `ui_supertools_itemnumber01/02` (font ConsumableItemCount01). No sheet has a `supertools_*` frame, and the port's `PauseScreen` blanks the labels.
- **The item.** `default_data.json`'s `shop_items` sells the Atomic Toolbox ("fully upgrade all of your tools and abilities for a single surgery", 1500 coins), but nothing on iOS ever uses one. Crumpet's tag power is the same effect, and the port has it (`SurgeryPartners.cpp`, case 6).
- **Art.** Usable art exists in the unused `shop_screen_sheet` (below): `shop_screen_icons-02` (a "SUPER+" serum jar) and `-03` (a red toolbox).
- **Effort.** Medium: the pause-tray panel, the buy path for the toolbox, and the use (Crumpet's code).

### The surgery editor (Android)

- **What.** Android keeps a complete in-game level editor for surgeries, written in the game's own classes:
  - `EditorScreen` (0x446874…, 74 functions), `EditorMenu` (0x43a798…);
  - the popups `InjuryListPopup`, `PropertiesPopup`, `BooleanPropertiesPopup`, `VersionSelectorPopup`, `InjuryEntryUIComp`.
- **What it does.**
  - Load a surgery and its injury definitions, switch stages, and pick the stage background.
  - Drop injuries from a toolbox of definitions, with generated names.
  - Drag, rotate (left/right buttons) and delete injuries.
  - Edit an injury's or a stage's properties.
  - Set visual and interaction dependencies.
  - Edit the surgery's properties and its available tools.
  - Save as a new version or overwrite (`editedLevels/`), discard, delete a version, switch versions.
  - Email the file (`sendCurrentSurgeryByEmail`), and test-play it (`testCurrentSurgery`).
- **Related code.** `SurgeryInfoState`+4 holds an edited version's path, which `SurgeryUtils::getSurgeryToLoadFullPath` (0x75af90) loads instead of the normal file. Cheat 0x13 loads from `Data/`. The surgery loader's warning "…Please, clean the surgery file by saving it again using the editor" is in both binaries.
- **Evidence.** `EditorScreen::scene` (0x446e2c) has no caller, and nothing else makes one. iOS strips it all: only the "editedLevels/" string is left.
- **Port.** A debug-mode surgery editor on the engine's debug UI could follow it: the data format is the port's own `SurgeryData`.
- **Effort.** Large.

### The giant worm's chest wrap and other ways to remove it

Definitions that no surgery or injury reaches:

- **`GiantWormWrapChest`** (Transplant: chainsaw and tongs, `transplant_wormwrapChest`). An older copy of the `GiantWormChestWrap` that hubris2 uses. The state `GiantWormWrapChest` (Android 0x57, port 80) is never set by any code.
- **`GiantWormTransplant`** (Transplant, chainsaw and tongs; layout `common/ToxicWormDead.json`, sprite `ToxicWormFront.png`, which no sheet has). The dead worm cut free and carried off, a version the shipped `GiantWormIncision` replaced. `common/GiantWormDead.json` (sprites `injury-GiantWormFront-Extracting.png`, `GiantWormFront.png`) is a second unused layout of it.
- **`GiantWormHead`** (SurfaceExtraction pulled with the tongs, leaving a `WormStuck`).
- **`MiniWormBite`** (a LargeCut copy; the mini worm leaves a `SmallCut` instead).
- **Effort.** Small to medium for `GiantWormHead` and the chest wrap (data plus a stage). `GiantWormTransplant` needs art.

### Injuries with art but no use

| Injury | What there is | Evidence | Effort |
|---|---|---|---|
| `LighterShiv` | Frame `injury-LighterShiv-Extracting.png` in `surgery_dalebreak_sheet1`: a blue lighter on two prongs (a prison shank, like Dale Break's toothbrushes and razors) | No definition, no class name, no reference | Small: an AngledExtraction definition copied from `Toothbrush_*` |
| `OrganShard1–3` | Frames `injury-OrganShard1/2/3-Extracting.png` in `surgery_organ1_sheet` (brown, grey, circuit-board green shards, the Reverend's Organ) | No definition anywhere. The shipped surgery uses `MonitorShard2/3` | Small |
| `MonitorShard1` | Definition and frame (`surgery_mrGiblets1_sheet`) | No surgery places it (2 and 3 are used) | Small: data only |
| `Toothbrush_Green` | Definition and frame (`surgery_dalebreak_sheet1`) | No surgery places it (yellow, orange, pink are used) | Small: data only |
| `kneeRTransplant` | Definition, `common/transplant_kneeR.json`, art in the unloaded `surgery_ernestpickles_sheet1` (`kneeR_piece1`, `kneeR_replaced`, `kneeR_tube1`) and `_sheet3` | Earnest Pickle uses `kneeR2Transplant` | Small: load the sheet's frames, place it |
| `UnderSurfaceSpike`, `UnderSurfaceSpikeRoot` | Definitions (hidden objects that become `Spike`/`SpikeRoot` once cut open) | No surgery places them; spikes are placed directly | Small: data only |
| `bugHand`, `bugHandR` | Definitions on `transplant_hand(R)` | The Hospital's gossamerrag uses `hand`/`handR`: same data, older names | None (duplicates) |
| `PoisonDart` "Embedded" | Frame `injury-PoisonDart-Embedded.png` | The definition's state has another name: an older state | (c) |

### A third cutter upgrade, `cutting_cut_timer_boost`

- **What.** `ToolState`'s factory asks whether `cutting_cut_timer_boost` is owned and then sets the cutter's upgrade level to 3. It is in Android `ToolState::createToolStateForEnum` 0x7e2b04, and in iOS's strings.
- **Evidence.** `upgrades.json` has no such id. It is the only id the code names that the data lacks. Through `std::map::operator[]` it resolves to upgrade 0, so the check really asks about the tech tree's first upgrade (the port's `ToolState.cpp` notes it).
- **Intent.** By its name, a cutter counterpart of `cutting_chainsaw_timer_boost` (+3 s as a stage moves on to Free, `InjuryState::advanceStage` 0x561ccc), meant for the tech tree's "cutting" column.
- **Effort.** Small to medium: an upgrade entry and art slot, plus the boost's rule.

### `actions_to_use` on five partners

- **What.** `tag_partners` give Giblets, Saturday, the Reverend, Flow and Bot `actions_to_use: 30`, alongside `actions_to_unlock`.
- **Evidence.** Neither binary has the string. Only the five original partners have it.
- **Intent.** Probably a separate charge needed for each use.
- **Effort.** Small, if wanted.

### A sprite-tint shader

- **What.** `Shaders/Custom.fsh` turns every opaque texel blue (`r*0.25, g*0.25, 1`), with an unused `u_time` wobble.
- **Evidence.** The functions that would apply it to injuries and animations have no callers (Android): `Animation::setShaderForClips` 0x2b134c, `SurgeryUtils::setInjurySpritesShader` 0x756f4c, `SurgeryScreen::setAllToDefaultShaders` 0x79c068. Neither binary names `Shaders/Custom`.
- **Intent.** A frozen or "scanned" look.
- **Effort.** Small to medium (a tint on the engine's sprites).

### Le Flairground Fantastique (Pocket God)

- **What.** Android's `surgeries_info.json` has four case files for `area_pocketgods`:
  - `surgery_pocketgods_boils_triplet1`, "Cobalt Flair", Plague of Boils;
  - `surgery_pocketgods_fire_triplet2`, "Crimson Flair", Plague of Fire;
  - `surgery_pocketgods_locusts_triplet3`, "Chartreuse Flair", Plague of Locusts;
  - `surgery_pocketgods_whale`, "Baby AWOOOOGAH", Jellyfish & Pygmy Infestation.
- **Each case file** has its full text: description, observations, symptoms, actions. It has its two bonus objectives, with descriptions and the new `bonus1/2_overview` keys no code reads. It names its photo, pre-op background, sheets, dialogue files and ambience.
- **Missing.** Everything else: no surgery files, no injuries (frogs, locusts, jellyfish, pygmies, barnacles, whale teeth), no dialogues, no art (`surgeryinfo_part3`, `preop_pocketgods1/2`, `dialogue_char_cobalt/crimson/chartreuse/whale`), no area in `areas.json`.
- **Effort.** Large: the case texts are the only part that exists. That is the November 2013 update, which is in neither build ([as3.md](as3.md)).

## (c) Bare leftovers

### Sheets never loaded

- **`shop_screen_sheet`** (iOS and Android, 34 frames): an older shop. Neither binary names it; the live coin shop is `shop_screen_buymore_sheet`.
  - Its background, buy buttons and price tags.
  - `shop_screen_icons-01..04`: three gold stars, a "SUPER+" serum jar, a red toolbox, a coin purse "x2" (the coin doubler).
  - Partner icons, including `ui_tagpartner-Bugdeuce`.
- **Older PNG copies of surgery sheets** that no surgery lists:
  - `surgery_ernestpickles_sheet1` (Earnest Pickle's leg and knees, with `kneeR_tube1`), `surgery_realBumpsGrinder3_sheet` (leg backgrounds), `surgery_sweeMeatsPete_sheet` (misspelt; Pete's leg with `peteLegR_tube_1`), `surgery_usefulBrother2_sheet`.
  - Most of their frames are also in the loaded sheets. The exceptions are below.
  - ([amateur-surgeon-3.md](amateur-surgeon-3.md) also notes the 2.0x-only `surgery_ernestpickles_sheet4` and `dialogue_char_Reverand copy`.)
- **`bloodpack_popup_sheet`**: (d).

### Frames nobody names

Beyond the ones above:

- **Partner "Bugdeuce":** `ui_tagpartner-Bugdeuce.png` in `world_map_sheet1` (iOS 4.0x and Android) and `shop_screen_sheet`. It is Mr Giblets' portrait redrawn with two big bulging eyes. Together with the stat `TaggedBeetle` (26, among the first five partners' stats), it is the trace of a cut ninth tag partner. There is no power, no `partners.json` entry, no body, arm, hand or tutorial-box art, and no dialogue sheet.
- **Patients' stage backgrounds:**
  - `bg_usefulBrotherNeck.png`, `bg_usefulBrotherInnerNeck.png` (in `surgery_usefulBrother1-2_sheet` and `_2_sheet`): a neck stage for Useful Brother that no surgery has.
  - `dragHealthy-bg.png` (`surgery_dragnabbit1_sheet`): Drag Nammit cured.
  - `peteLegR_tube_1.png`: a tube to cut on Pete's leg.
- **Tutorial box portraits:** `ingame_tutorial_partner-proxy4.png` (the skeleton test dummy, `surgery_common1_sheet`) and `ingame_tutorial_partner-saturday.png` (`surgery_Saturday_sheet`). The tutorial box uses `ingame_tutorial_partnerbox_pic-<partner>` instead.
- **Particle sprites:**
  - `particleSprite_blood.png` (iOS `surgery_common1_sheet`): blood drops.
  - `particleSprite_coffee.png` (the Hospital's supplement sheet): coffee drops for the coffee artery.
  - Only `_poison`, `_darkpoison`, `_frogpoison`, `_darkfrogpoison` and `_black` are named.
- **`tool_injection-fluid.png`:** the syringe's fluid strip (the port draws the fluid otherwise).
- **Patient photos:** `surgeryinfo_photo-brother.png` is unused, while Useful Brother 2's case file asks for `photo_suffix` "brother2", which no sheet has. This is likely a naming slip: the case file shows no photo, and "brother" is probably the intended one. Worth fixing in the port.
- **Dialogue poses no dialogue line uses:**
  - Bot `explaining`, `talking`; Flow `shouting`, `staring`; Mr Giblets `thinkingopen`; Animal5 `OpenMouth`; Drag `curedthinking`; Flexxx `pose1_Heart`, `pose2_Sad`;
  - Alissa `panicking`; Hisser `idle`; Hubris `TalkingMasked`; Horrace `dark`, `semidark`; Ursinator `cured`; Real Bumps Grinder `pained`; Tammy `basic`, `curedbasic`; Useful Brother 2 `shirtclosed`;
  - the Hospital's Alan `Smug`, `Thinking`; Chastity `GhostAppear`, `Listen`, `TalkingSad`; Emo `TalkingHappy`; Karl `JunkieWithCoffee1/2`, `Regular`, `RegularWithCoffee2`.
  - They are free poses for new lines (small).

### Animations never played

- **Files no code or data names:**
  - `anim_creature_green_flea_anim` (a green flea: death, jump, tazer; its frames exist in no sheet);
  - `anim_creature_poisonfrog_anim` (no frames: the poison frog plays `anim_creature_frog_anim`);
  - `anim_creature_mini_frog_anim` (the mini frog plays `anim_creature_frog_spawn_anim`; dart and look frames missing);
  - `anim_creature_mini_worm2_anim` (a copy);
  - `anim_injury_bone1/2`, `robobone1/2`, `vein1/2`, `robovein1/2`, `legInner` (the bones are drawn from single frames);
  - `anim_ui_tagbutton_anim` (a tag button "start"/"loop" animation; its frames exist in no sheet);
  - `anim_loading_screen_anim`, `anim_tag_giblets_anim`, `anim_tag_brutality_anim`, `anim_tool_super_serum_anim` (one-frame wrappers; the code uses the frames directly);
  - `common/explosion_anim.json`.
- **Animation names in used files that no class plays** (checked against every `Animation::play/append/showFrame` in Android's decompile and the port):

  | Creature | Unused | Frames |
  |---|---|---|
  | Bat | `opening` (16 frames), `open` | Probably the wings opening before take-off |
  | Poison frog (`anim_creature_frog_anim`) | `idle`, `jump` | |
  | Robot scorpion | `swoosh` (5), `clamped_released` (16), `death` (1) | |
  | Snake | `dead` | |

  Effort: small each, where a moment fits (the bat's take-off, the scorpion freed from a clamp).

### Particle systems never created

| System | Likely meant for |
|---|---|
| `particle_robobloodFX` | Robot patients' blood |
| `particle_scorpionFireFX` | The robot scorpion firing |
| `particle_spikeClumpFX` | The spike clump transplant |
| `particle_superpoisonareaFX` | Super poison bubbles |
| `particle_timerFX` | The clock or a time bonus |
| `particle_veinFX` | Vein spurts |
| `particle_tagStarburstFX`, `particle_taggibletsFX` | Tag powers (the latter superseded by `taggiblets2FX`) |
| `particle_dark_darkcloud_poisonareaFX` (Hospital) | |

Effort: small each, where a moment fits.

### Sounds never played

- **Dynamic names allowed for.** The patient screams pick `_1..4`; the Brutality samples `_1..4`; random families by their counts.
- **`Data/Sounds/ScenesDefinition_ios.json` / `_android.json`:** per-scene preload lists. Their loader, `SoundUtils::loadSounds` (Android 0x6fa60c), has no caller. Sounds that only these lists name are counted as unused.
- **Unused sounds:**

  | Sound | Note |
  |---|---|
  | `tutorial_success` | Could play when a tutorial step or tutorial is done |
  | `auto_tool_twinkle_loop` | Fits the Reverend's automatic tool choice |
  | `tool_boost_start`, `tool_boost_end` | Only `_charge`/`_loop` are used, by Crumpet's power |
  | `health_rising_loop` | Healing |
  | `pre_op_loop` | |
  | `map_scroll` | |
  | `Collapse_Impact` | The Bunker preview uses `Crumbling_wall` |
  | `chainsaw_loop_low` | Only `_mid` is used |
  | `extraction_2`, `extraction_3` | Preloaded by Mutabear's sound list, never played; only `extraction_1` is played |
  | `pizza_cutter_metal` | The cutter on metal (Robotopia) |
  | `tool_denied` | `button_click_denied` is used |
  | `Heart_beat` | `heartrate_blip` is used |
  | `Bot_Start`, `Bot_Loop`, `Bot_End` | Procrastibot's power uses `bot_time_*` |
  | `Brutality_sample1_no_voice`, `_2__`, `__4`, `___3` | Voiceless takes |
  | `male_scream_5/6`, `female_scream_5`, `hisser_scream_5`, `organ_scream_5`, `robot_female_scream_5`, `robot_female_scream_-1`, `Robot-Lady-Scream-2/3/4` | Fifth and sixth screams, and older takes |
  | `Mumbo-Jungle-Map-Ambience` | An older `mapambience_mumbojungle` |
  | `out_of_time` | iOS only |

  Effort: small each.

### Layouts and data files nobody loads

- **Layouts:**
  - `in_game_tutorial_popups.json` (in-surgery "shop", "upgrades" and "locked" boxes) and `in_game_tutorial_endpopups.json`: older versions of the end-of-surgery tips.
  - `tablet.json`: an older tech-tree icon layout. `ui_techtree_background.json`: the upgrade screen draws its background directly.
  - `neat_popup.json`, `exileclinic_map_bg.json`, `shanksalot_map_bg.json`: empty.
  - `ursinator_tube1/2.json`: tube cuts for the Ursinator's tank, never used. With `kneeR_tube1` and `peteLegR_tube_1`, a cut "tube" step of limb and tank replacements.
- **`Data/Dialogues/dialogue_area_hospital_complete.json`** (Android, in the port's package): a placeholder, identical to Mumbo Jungle's ending (Lady Saturday, Dr. Bleed's diary). Android's `areas.json` gives the Hospital the Bunker's ending file, and its boss returns to the world map, so neither is shown.
- **`Data/Tutorials/tutorial_lunacyfalls_dragnammit.json`** (both builds; listed in `default_data.json`'s tutorials): a one-step test with "Lorem ipsum…" text. It is the only file using `ShowInjuryHitAreas`.
- **Unused data keys:**
  - stages' `"tutorial_data": "tbc"` (Batshift challenge 2, Mutabear challenge 1);
  - the Pocket God `bonus1/2_overview`;
  - the partners' `actions_to_use`.
  - The tutorial action `PlaySound` (`PlaySoundTA`, in iOS and the port) is implemented, but no tutorial uses it.
- **Enum entries with nothing behind them:**
  - base classes 28–31 `LargeCutRobot`, `SmallCutRobot`, `IncisionRobot`, `Rust` (port numbering): their definitions use other bases;
  - states `OpenWithClamp`, `PoisonFrogIdle`, `RobotScorpionSubmerged`, `GiantWormAlive`, `GiantWormWrapChest`: no data names them, no code sets them.

### Small dead functions (Android, no callers)

- **Tests and tools:**
  - `AnimationSampleScreen` (0x2d75fc: an animation and attach-point browser) and `ParticleSampleScreen` (0x5e63a4: particles that follow the finger). The port's `AS_ANIM` and `AS_PARTICLES` viewers already do this.
  - `Utils::PrintMemoryUsageMB`.
- **Unused helpers:**
  - `HorizontalScrollLayer::moveToNextPage`/`moveToPreviousPage` (page arrows); `CreditsScreen::updateSelected`;
  - `InjuryState::setHitGroupActive`/`setHitGroupInactive`/`setPositionInWorldCoords`; `PoisonFrog::getNewPositionToFire` (a random point in the play area);
  - `SurgeryScreen::getToolDefForEnum`/`removeEffect`/`resetEffectsOfType`/`getScaledPoint`; `BluePoisonCloudEffect::resetSystems`/`stopSystems`;
  - `SceneMgr::overlayLoadingScene`/`removeLoadingScene`; `TextUtils::CreateTrueTypeText`;
  - `PathUtils::LoadPath` (a stub returning an empty `PathInfo`).
- None is worth porting on its own.

## (d) Online or money: out of scope

- **Game Center:** leaderboards (`leaderboardId` per area in `areas.json`; the area totals of `highScores`) and achievement reporting. Local achievements are in (b) above.
- **Purchases:**
  - coin packs (`settings` `surgeon3coinpack1..5`) and `ShopCurrencyScreen`'s "buy more";
  - Android's `ConsumablePurchaseScreen` and `purchase_popup` sheet / `ui_purchase_popup.json` (blood-pack and revive packs, `purchase_surgeon_points_display.png`);
  - Android's Coin Doubler IAP (`surgeon3coindoubler`, `currency_coindoubler_button`, `currency_doubler_icon`; Android's consumable type 4 read by `SurgeryCurrencyScreen`).
  - The Coin Doubler as a coin-bought shop item (`coin_doubler`, 10 000) could stay in-game; whether iOS 1.02 ever applies it was not settled here.
- **Blood packs:** restored, free ([as3.md](as3.md#blood-packs)): only their purchases are out (`surgeon3bloodpack1..3`, the buy buttons). Still unused: `bloodpack_popup_sheet` (its frames are also in `end_of_surgery_popups_sheet`, which the popups use), the pause screen's `ui_global_totalbludpax` (`ui_global_pauseblud`, a frame no sheet has).
- **Facebook:** `FacebookMgr`, `FacebookPopupScreen`, `facebook_popup_sheet`, `ui_start_dialogue_facebook.json`, `in_game_facebookconfirm.json`, `GameStateUtils::AddFacebookBonusToAllSaveGameStates` (and cheat 0x19).
- **Ads and analytics:** `AdMgr`/PlayHaven, `UserTrackingMgr`, Kontagent, Bango, Push IO (`pushio_config.json`).
- **Other online parts:**
  - the content server, and Android's OBB downloading (`ExpansionDownloadScene`, `downloading_popup_sheet`, `in_game_tutorial_downloads.json`);
  - iCloud saves (`CloudMgr`);
  - email (`EmailUtils`);
  - the Android title screen (`welcome_screen_sheet_android`, `ui_startscreen_android.json`);
  - the title screen's news, mail, more-games and Game Center buttons;
  - the rate-us prompt ("Shanks-a-lot for playing! Want to rate us?");
  - the three "come back" local notifications ("Another day, another freaky accident or illness to treat!…", "Think you've seen it all?…", "WHERE HAVE YOU BEEN??…").

## Brought back

- The surgery editor (Android 1.43's `EditorScreen`): ported as the port's bonus, unlocked by winning Hubris 2, with share codes ([as3.md](as3.md#surgery-editor)).

- (The consumables, the cut coin shop for earned coins with the pause tray's Super Serum and Atomic Toolbox, were brought back for a while, then removed again by the user's decision: [as3.md](as3.md#shop-consumables-and-revives-removed).)

- Achievements: a local list and banner, counted at every original counting site ([as3.md](as3.md#achievements-and-the-directors-cut)).
- The developer cheats: the F1 debug menu's Cheats page (cheats 5/6 turn out to be "a press wins/loses").
- The Director's cut: LighterShiv, Toothbrush_Green, OrganShard1–3, MonitorShard1, kneeRTransplant (Earnest Pickle uses kneeLTransplant; kneeR2 is Real Bumps Grinder's; kneeR's art is in his loaded sheets 2/3), GiantWormWrapChest, the three tutorials (1.45 ships them, with TimeElapsedOnStage, but still names none), the whale's BubbleEffect. Left out for lack of art: GiantWormTransplant, GiantWormHead, MiniWormBite.
- The sounds tutorial_success, auto_tool_twinkle_loop, tool_boost_start/end; the animations bat "opening", poison frog "idle"/"jump", scorpion "swoosh"/"clamped_released"; the Useful Brother 2 photo fix.
- The Reverend's Organ's organ_scream_1..4: played now, its scream corrected to "organ".

## Removed from the port

- `score_timeslow_upgrade` ("Bleed Buddies", 7500 coins): dead in every version (its name is only copied into the upgrade-name tables at startup, never read; its index is never checked). The port drops it from the Upgrades screen.
