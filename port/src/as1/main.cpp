// Amateur Surgeon, rebuilt from the original Flash game (Adult Swim /
// Mediatonic, 2008) on the engine: the movie's art and sounds as assets, its
// scripts run on the stage, its operation included.
#include <SDL3/SDL_main.h>

#include "as1/As1Game.h"
#include "platform/GameMain.h"

int main(int argc, char** argv) {
    platform::GameInfo info{"Amateur Surgeon", 600, 460, "games/as1", "AmateurSurgeon"};
    info.passSeconds = 1.0 / 25;  // the movie's frame rate: one pass per frame
    return platform::runGame<As1Game>(argc, argv, info);
}
