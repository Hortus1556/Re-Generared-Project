// The pre-op screen's star panel (iOS 1.45; not in 1.02): beside the patient,
// what the surgery asks. For the main surgery its three ratings, each with
// the score and the time it takes (the surgery file's min_score_to_N_star
// and, for one star, the first stage's time, for two and three
// min_time_to_N_star: SurgeryUtils::LoadSurgeryScoreDef 0x9f368); for a
// challenge a big star, blue or red, with the challenge's overview
// (surgeries_info.json's bonus1_overview, bonus2_overview).
// PreOpScreen::showSideScoreBar 0x11b71c shows it as the screen shows,
// hideSideScorebar 0x11e52c hides it under the partner select. 1.45's end
// screen layout has the same items, always hidden
// (SurgeryEndScreen::hideSideBarAndFBButton).
#pragma once

#include "as3/SurgeryInfo.h"
#include "as3/UI.h"

namespace mt {

void hideStarPanel(UIInstance& ui);
// mode: 0 the main surgery, 1 and 2 its challenges
void showStarPanel(UIInstance& ui, const SurgeryInfo& surgery, int mode);

}  // namespace mt
