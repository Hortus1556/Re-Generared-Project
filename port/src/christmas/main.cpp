// Amateur Surgeon Christmas Edition, rebuilt from the original Flash game
// (Adult Swim / Mediatonic, 2008) on the engine: the movie's art and sounds
// as assets, its scripts run on the stage, its operation included (as AS1).
#include <SDL3/SDL_main.h>

#include "christmas/ChristmasGame.h"
#include "platform/GameMain.h"

int main(int argc, char** argv) {
    platform::GameInfo info{"Amateur Surgeon Christmas", 600, 460, "games/christmas", "AmateurSurgeonChristmas"};
    info.passSeconds = 1.0 / 25;  // the movie's frame rate: one pass per frame
    return platform::runGame<ChristmasGame>(argc, argv, info);
}
