// Amateur Surgeon 2 HD (iPad, Adult Swim, 2011), ported from its iOS app:
// its classes mirrored in port/src/as2, its content the package
// port/games/as2 (extract_ipa.py, as2_data.py).
#include <SDL3/SDL_main.h>

#include "as2/AS2App.h"
#include "platform/GameMain.h"

int main(int argc, char** argv) {
    platform::GameInfo info{"Amateur Surgeon 2", 1024, 768, "games/as2", "AmateurSurgeon2"};
    info.passSeconds = 1.0 / 60;  // the display link's 60 frames a second
    return platform::runGame<as2::AS2App>(argc, argv, info);
}
