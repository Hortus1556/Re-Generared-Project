// Amateur Surgeon 3 (iOS, Adult Swim / Mediatonic, 2013), ported from its
// iOS app: its classes mirrored in port/src/as3 on the engine's cocos layer
// (port/src/engine/cocos), its content the package port/games/as3
// (extract_ipa.py). docs/as3.md.
#include <SDL3/SDL_main.h>

#include "as3/AppDelegate.h"
#include "platform/GameMain.h"

int main(int argc, char** argv) {
    // The screen in canvas points: the iPhone 5's view of the game's canvas.
    platform::GameInfo info{"Amateur Surgeon 3", 568, 320, "games/as3", "AmateurSurgeon3"};
    info.passSeconds = 1.0 / 60;  // the director's animation interval
    return platform::runGame<as3::AppDelegate>(argc, argv, info);
}
