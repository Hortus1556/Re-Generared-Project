// mt::Cheats: the developers' switches (Cheats::IsCheatEnabled, iOS 1.02
// 0xaa11c, 1.45 0xdc080, Android 0x3c61c0). In every shipped build its body
// is compiled out (`return 0`) and EnableCheat/DisableCheat/Init (Android
// 0x3c6184, 0x3c61a4, 0x3c6170) are empty and never called, but the game
// still asks it at about 50 places. The port answers from switches the F1
// debug menu's "Cheats" page sets (DebugMenu.cpp), or AS_CHEATS=<code>,...
// (decimal or 0x hex) at start; all are off by default and every one reads
// off without -d (debug mode), so a player's game is never touched.
//
// The codes, by what the call sites do with them (iOS 1.45 addresses; the
// same codes in 1.02 and Android 1.43, docs/as3-unused.md):
//    0 kHitAnywhere      a tool wears every hit area of a group wherever it
//                        touches (InjuryState::runInteractionsWithHitGroup 0xac26c)
//    5 kPressWins        a press on the surgery wins it (processOnPress 0x3d5c0)
//    6 kPressLoses       a press on the surgery loses it (0x3d5c0)
//    7 kNoMisuseDamage   misusing a tool does no harm (runToolLogic 0x41e34,
//                        onToolWrongUsage 0x42710)
//    8 kTagAlwaysFull    the tag meter charged every frame (runLogic 0x49348)
//    9 kInvincible       no damage at all, the clock stands still, a sudden
//                        death mistake doesn't kill (dealDamage 0x40830,
//                        0x42710, runLogic, runActionEvents 0x5cc9c/0x5db1c)
//   10 kReverendForever  the Reverend's power for ever: the tool picked for
//                        what is touched, no tool does harm, his timer never
//                        runs out (0x3d5c0, 0x41e34, 0x42710, checkToolUsage
//                        0x4766c, runLogic, runActionEvents)
//   11 kUnlockAll        every area, boss and partner surgery open; the
//                        title skips the intro (WelcomeScreen 0x318238,
//                        SaveSlotScreen 0x404ffc/0x405410, WorldScreen
//                        0x174ddc, AreaScreen 0x28380, PreOpScreen 0x11887c,
//                        showCompletedScreen 0x4af40)
//   12 kSkipTutorials    no tutorial (SurgeryScreen::loadAssets 0x50468)
//   13 kReplayTutorials  completed tutorials played again (0x50468)
//   14 kNoAds            (AdMgr: the port has no ads)
//   15 kSkipIntroMovie   the intro movie skipped (IntroScreen 0x3a86ac)
//   16 kAllUpgradeRows   every tech-tree row open, whatever the stars
//                        (UpgradeScreen 0x193c8c, 0x196ba8)
//   17 kAllPartners      every partner as owned and rested (PartnerSelectScreen
//                        0x2c4530/0x2c6518/0x2c7aa0, PartnerInfoScreen 0x2d45bc)
//   18 kStars250         the slot's stars read as 250 (GameStateUtils::
//                        GetStarsBalance 0x138878, GetStarsBalanceFromState 0x1388b0)
//   19 kBundleData       content from the bundle's Data/, not Downloaded/
//                        (ContentManager 0x363a3c, SurgeryUtils 0x9ed48,
//                        TutorialMgr 0x264398): the port always reads the bundle
//   20                   asked by WorldScreen::loadAssets 0x174ddc, the answer unused
//   21 kLoadTestFull     (Android SurgeryScreen::onBecomeActive 0x7ab1c4) a surgery
//                        loads, then goes back to the pre-op screen (1.45 asks it
//                        for its analytics only, TrackingUtils 0x385b6c)
//   22 kLoadTestNothing  (Android loadAssets 0x7a7d90, 0x7ab1c4) loads nothing, back
//   23 kLoadTestSheets  (Android 0x7a7d90) only the sheets; Android then stayed on an
//                        empty screen: the port goes back to the pre-op as 21 and 22
//   25                   (Android AppDelegate) the Facebook bonus ×10: removed
#pragma once

namespace mt {
namespace Cheats {

enum CheatCodes {
    kHitAnywhere = 0,
    kPressWins = 5,
    kPressLoses = 6,
    kNoMisuseDamage = 7,
    kTagAlwaysFull = 8,
    kInvincible = 9,
    kReverendForever = 10,
    kUnlockAll = 11,
    kSkipTutorials = 12,
    kReplayTutorials = 13,
    kNoAds = 14,
    kSkipIntroMovie = 15,
    kAllUpgradeRows = 16,
    kAllPartners = 17,
    kStars250 = 18,
    kBundleData = 19,
    kWorldMapUnused = 20,
    kLoadTestFull = 21,
    kLoadTestNothing = 22,
    kLoadTestSheets = 23,
    kFacebookBonus = 25,
    kCheatCount = 26
};

// (0xdc080) whether a cheat is on: never without -d
bool IsCheatEnabled(CheatCodes code);
// (EnableCheat 0x3c6184 / DisableCheat 0x3c61a4: the port's debug menu)
void EnableCheat(CheatCodes code);
void DisableCheat(CheatCodes code);
void Toggle(CheatCodes code);
// (Init 0x3c6170) the switches from AS_CHEATS, in debug mode
void Init();
// the debug menu's label of a code ("" for the codes the port can't use)
const char* label(CheatCodes code);

}  // namespace Cheats

// shorthand for the call sites
inline bool cheat(Cheats::CheatCodes code) { return Cheats::IsCheatEnabled(code); }

}  // namespace mt
