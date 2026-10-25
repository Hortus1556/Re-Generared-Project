#include "as3/Cheats.h"

#include <cstdlib>
#include <sstream>
#include <string>

#include "as3/Log.h"
#include "platform/Platform.h"

namespace mt {
namespace Cheats {

namespace {

bool gOn[kCheatCount] = {};

bool valid(int code) { return code >= 0 && code < kCheatCount; }

}  // namespace

bool IsCheatEnabled(CheatCodes code) {
    // (the shipped body: `return 0`; the port's switches, debug mode only)
    return platform::debugMode() && valid(code) && gOn[code];
}

void EnableCheat(CheatCodes code) {
    if (!platform::debugMode() || !valid(code)) return;
    gOn[code] = true;
    log("cheat %d (%s) on", static_cast<int>(code), label(code));
}

void DisableCheat(CheatCodes code) {
    if (!valid(code)) return;
    gOn[code] = false;
    log("cheat %d (%s) off", static_cast<int>(code), label(code));
}

void Toggle(CheatCodes code) {
    if (IsCheatEnabled(code)) DisableCheat(code);
    else EnableCheat(code);
}

void Init() {
    if (!platform::debugMode()) return;
    const char* s = std::getenv("AS_CHEATS");
    if (!s) return;
    std::istringstream items(s);
    std::string item;
    while (std::getline(items, item, ',')) {
        if (item.empty()) continue;
        int code = static_cast<int>(std::strtol(item.c_str(), nullptr, 0));
        if (valid(code)) EnableCheat(static_cast<CheatCodes>(code));
    }
}

const char* label(CheatCodes code) {
    switch (code) {
        case kHitAnywhere: return "Hit anywhere";
        case kPressWins: return "Tap wins";
        case kPressLoses: return "Tap loses";
        case kNoMisuseDamage: return "No misuse harm";
        case kTagAlwaysFull: return "Tag always full";
        case kInvincible: return "No damage, no clock";
        case kReverendForever: return "Reverend forever";
        case kUnlockAll: return "Unlock all";
        case kSkipTutorials: return "Skip tutorials";
        case kReplayTutorials: return "Replay tutorials";
        case kSkipIntroMovie: return "Skip intro movie";
        case kAllUpgradeRows: return "All upgrade rows";
        case kAllPartners: return "All partners";
        case kStars250: return "250 stars";
        case kLoadTestFull: return "Load test: all";
        case kLoadTestNothing: return "Load test: none";
        case kLoadTestSheets: return "Load test: sheets";
        default: return "";
    }
}

}  // namespace Cheats
}  // namespace mt
