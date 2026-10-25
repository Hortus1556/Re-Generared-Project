// The Director's cut (the port's): content the originals ship but never use
// (docs/as3-unused.md), put back as an optional variant of the main
// surgeries. A slot's switch (SavedGame::directorsCut, saved; off by
// default) on the achievements list (AchievementsScreen), which opens once
// the slot has earned kDirectorsCutAchievements achievements; the F1 debug
// menu sets it at once. With it off, every surgery is exactly the shipped one.
//
// With it on, a main surgery (not its challenges, but for the whale's
// bubbles) is changed as it loads:
// - cut injuries take the place of shipped ones with the same numbers, so
//   the difficulty stays (RestoredContent.cpp, kSwaps):
//   Dale Break: the LighterShiv (a lighter on two prongs, art in his sheet,
//   no definition: one from the toothbrushes') for the razor blade, and
//   Toothbrush_Green (a definition no surgery placed) for the yellow one;
//   the Reverend's Organ: OrganShard1-3 (art in surgery_organ1_sheet, no
//   definition: the organ's own RoborpionShard numbers, the size of their
//   frames) for stage 5's three scorpion shards; Mr Giblets: MonitorShard1
//   (placed by no surgery) for MonitorShard3; Earnest Pickle: the right knee,
//   kneeRTransplant (his sheets carry its art), for the left, mirrored;
//   Hubris 2: GiantWormWrapChest, the older copy of the chest wrap;
// - the whale's bubbles (1.45's stage "effect" BubbleEffect, which no
//   surgery file sets) over its stages in the tank's water, in all three
//   modes (the one change that applies to challenges too);
// - the three finished tutorials no surgery names (Batshift's bats, the
//   Reverend's robo-scorpions, Useful Brother 1's ice ants) are named, and
//   Batshift's three bats get the names the tutorial asks for (bat_1..3).
#pragma once

#include <string>

#include "core/Json.h"

namespace mt {
namespace restored {

constexpr int kDirectorsCutAchievements = 11;

// the current slot's switch, and whether its achievements opened it
bool directorsCut();
bool directorsCutUnlocked();
void setDirectorsCut(bool on);

// A main surgery's file changed for the Director's cut (nothing when it is
// off, or for a challenge).
void applyToSurgery(const std::string& surgeryId, int mode, json::Value& root);

// The definition of a restored injury the package lacks (LighterShiv,
// OrganShard1-3): false for every other class.
bool injuryJson(const std::string& className, json::Value& out);

}  // namespace restored
}  // namespace mt
