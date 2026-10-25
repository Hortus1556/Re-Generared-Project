#include "as3/StarPanel.h"

#include <cstdio>

#include "as3/Content.h"
#include "as3/SurgeryData.h"

namespace mt {

namespace {

// the main surgery's ratings
const char* const kRatingItems[] = {"starpanel_stopwatch", "starpanel_score",  "starpanel_smallbox01",
                                    "starpanel_smallbox02", "starpanel_smallbox03", "starpanel_star01",
                                    "starpanel_star02",     "starpanel_star03", "starpanel_star04",
                                    "starpanel_star05",     "starpanel_star06", "starpanel_star07",
                                    "starpanel_star08",     "starpanel_star09"};
const char* const kRatingTexts[] = {"starpanel_scorebox_1stars", "starpanel_scorebox_2stars", "starpanel_scorebox_3stars",
                                    "starpanel_timebox_1stars",  "starpanel_timebox_2stars",  "starpanel_timebox_3stars"};
// a challenge's
const char* const kChallengeItems[] = {"star_panel_bigstar_blue", "starpanel_bigstar_red", "starpanel_bigbox"};

std::string clock(int seconds) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d", seconds / 60, seconds % 60);
    return buf;
}

}  // namespace

void hideStarPanel(UIInstance& ui) {
    ui.setVisible("starpanel_plaque", false);
    for (const char* n : kChallengeItems) ui.setVisible(n, false);
    for (const char* n : kRatingItems) ui.setVisible(n, false);
    for (const char* n : kRatingTexts) ui.setText(n, "");
    ui.setText("starpanel_bigstar", "");
}

void showStarPanel(UIInstance& ui, const SurgeryInfo& surgery, int mode) {
    ui.setVisible("starpanel_plaque", true);
    if (mode == 1 || mode == 2) {
        ui.setVisible("star_panel_bigstar_blue", mode == 1);
        ui.setVisible("starpanel_bigstar_red", mode == 2);
        ui.setText("starpanel_bigstar", mode == 1 ? surgery.bonus1Overview : surgery.bonus2Overview);
        ui.setVisible("starpanel_bigbox", true);
        for (const char* n : kRatingItems) ui.setVisible(n, false);
        for (const char* n : kRatingTexts) ui.setText(n, "");
        return;
    }
    for (const char* n : kChallengeItems) ui.setVisible(n, false);
    ui.setText("starpanel_bigstar", "");
    for (const char* n : kRatingItems) ui.setVisible(n, true);
    // (LoadSurgeryScoreDef 0x9f368: the main surgery's file)
    json::Value root;
    readContent(surgeryFile(surgery.id, 0, ""), root);
    const int score[3] = {root["min_score_to_1_star"].integer(), root["min_score_to_2_star"].integer(),
                          root["min_score_to_3_star"].integer()};
    const json::Value& stages = root["stages"];
    const int time[3] = {stages.size() ? stages[0]["time"].integer() : 0, root["min_time_to_2_star"].integer(),
                         root["min_time_to_3_star"].integer()};
    for (int i = 0; i < 3; ++i) {
        ui.setText(kRatingTexts[i], std::to_string(score[i]));
        ui.setText(kRatingTexts[3 + i], clock(time[i]));
    }
}

}  // namespace mt
