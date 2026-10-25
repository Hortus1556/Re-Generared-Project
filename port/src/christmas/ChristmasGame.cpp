#include "christmas/ChristmasGame.h"

#include "christmas/StageExtras.h"
#include "surgeon/StageScreen.h"

namespace {

FlashSurgeonGame::Setup christmasSetup() {
    FlashSurgeonGame::Setup s;
    s.labelText = 289;
    // the plug's, the banners' and panels' blockers, the menu bar, help, credits
    s.noHandCursor = {772, 774, 4479, 4409, 6045, 4651, 5833, 4219};
    // The SharedObject "user..." is kept as christmas_save.txt (its levels
    // as "grade<n>", no "balls": the file the port always wrote).
    s.sharedObject = "user_9848945498456554546545454";
    s.saveFormat = {"christmas_save.txt", {{"level", "grade"}}, "balls"};
    s.hiddenCharacters = {6351, 6352};
    s.operationSprite = 6233;
    s.patients = ChristmasGame::kMissions;
    s.musicOff = 6112, s.musicOn = 6114, s.effectsOff = 6120, s.effectsOn = 6122;
    s.flags = {"pressed"};  // (mission 4: its second scene before the operation was shown)
    s.extend = [](StageScreen& st) {
        st.addModule(5665, [](StageScreen& stage, const std::string& path) { return new LevelsBack(stage, path); });
        st.addModule(5688, [](StageScreen& stage, const std::string& path) { return new WorkshopEngine(stage, path); });
    };
    return s;
}

}  // namespace

ChristmasGame::ChristmasGame() : FlashSurgeonGame(christmasSetup()) {}
