// The blood packs, the original's energy (1.45 GameStateUtils 0x1388e4..
// 0x138f2c; 1.02 0xe8c9c..0xe92f0; Android 1.43 0x4b4de8..0x4b5954). Free in
// the port: never sold, only waited for (docs/as3.md "Blood packs").
//
// A slot holds numBloodPacks and lastBloodPackTimestamp (SaveData.h). A new
// slot's count is -1 until it is first recharged, which gives it
// start_bloodpacks. Below max_bloodpacks one comes back every
// bloodpack_recharge_secs counted from the timestamp, which moves on by that
// much for each one back (so time away counts, capped at the most); taking
// one from a full slot starts the count at that moment.
//
// After the first two surgeries (Mr Giblets and Proxy 4 won), starting a
// surgery from the partner select takes one (PartnerSelectScreen), none left
// shows "bloodpacks_out" instead (GeneralTipPopupScreen); a win gives it back
// (SurgeryScreen::finishWin), so a loss, a quit or a retry is what uses one
// up, as the "Introducing Bloodpacks!" popup (the first surgery after those
// two, once a slot: stat UsedBloodpack) says. The menu screens recharge the
// current slot as their top bar (GlobalHud) shows the count, the most and
// the time to the next, saving when one came back.
#pragma once

#include <string>

namespace mt {

struct SavedGame;

namespace GameStateUtils {

int MaxBloodPacks();    // max_bloodpacks (3)
int StartBloodPacks();  // start_bloodpacks (3): a new slot's
int RechargeSecs();     // bloodpack_recharge_secs (1800)

// (0x1388e4) a new slot given its start packs, then those the time since the
// timestamp brought back, up to the most; whether anything changed (to save)
bool UpdateBloodPacksAndTime(SavedGame& slot);
// (0x138d7c) the current slot recharged (saved if one came back), then the
// time to its next pack, "m:ss", or "Full"
std::string GetTimeUntilNextBloodpack();
// (0x138eec) one taken; from a full slot, the count to the next starts now
void UseBloodPack(SavedGame& slot);
// (0x138f20) n given (a win's: SurgeryScreen), never past the most
void AddBloodPacks(SavedGame& slot, int n);
// (0x138f2c) the current slot's full, saved (a purchase's in the original;
// the debug menu's in the port)
void RefillBloodPacks();
// the screens' updateStatus (AreaScreen 0x2e018 ...): the current slot
// recharged, saved when one came back
void UpdateBloodPacksAndSave();
// whether starting a surgery from the menus costs a pack: after the first
// two surgeries (PartnerSelectScreen::clickedButton 0x2c3740; the same test in
// showCompletedScreen 0x4af40, SurgeryScreen::onBecomeActive 0x51b7c)
bool BloodPacksInUse(const SavedGame& slot);

}  // namespace GameStateUtils

}  // namespace mt
