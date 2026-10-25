#include "as1/As1Game.h"

namespace {

FlashSurgeonGame::Setup as1Setup() {
    FlashSurgeonGame::Setup s;
    s.labelText = 272;
    s.noHandCursor = {3476, 3569, 3572, 4558, 3353};  // the banners' and panels' blockers, the menu bar, help, credits
    // The SharedObject "user_data..." is kept as as1_save.txt (its keys as
    // the scripts wrote them).
    s.sharedObject = "user_data44885485484";
    s.saveFormat = {"as1_save.txt", {}, ""};
    s.hiddenCharacters = {4978, 4979};
    s.operationSprite = 4950;
    s.patients = As1Game::kPatients;
    s.musicOff = 4827, s.musicOn = 4831, s.effectsOff = 4811, s.effectsOn = 4815;
    return s;
}

}  // namespace

As1Game::As1Game() : FlashSurgeonGame(as1Setup()) {}
