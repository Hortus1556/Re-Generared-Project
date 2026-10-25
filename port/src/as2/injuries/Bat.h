// Bat (mission 10; the Flash AS2's, its logic and art: Bat.as): a bat
// hanging upside down, asleep. The lighter (its hit areas worked 4 times)
// wakes it: it flaps over the spot, and each shot of the injection takes its
// body a step paler; the third kills it: its death plays, it bursts in blood
// and is gone. Not killed within 12 seconds it heals (its body back to
// itself) and spits blood about for 8 more, then hangs asleep again (the
// lighter again). The injection on a sleeping bat is a wrong use
// (Injection), and the lighter does nothing to a flying one
// (Game::findCorrectInjury).
#pragma once

#include <memory>

#include "as2/FlashCreature.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Bat : public InjuryGraphic {
public:
    enum State { kHanging = 0, kFlying = 1, kSplatting = 2, kDead = 3 };

    Bat(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    // Lit: off it flies. Injected: paler, then dead.
    void updateInjury() override;
    void special() override;
    // Burnt or injected: blood at the finger.
    void createEffect(int count, int velocity) override;
    int state() const { return state_; }
    std::string toolNow() const override {
        return state_ == kHanging ? "lighter" : state_ == kFlying ? "injection" : "";
    }

private:
    // Its hit areas (Flash's `hit`) on the state's frame, each worked
    // `health` times.
    void boxes(int health);
    const char* label() const;
    void showFlash();
    Point at() const { return flashPlace(); }
    Point picturePlace() const override { return at(); }

    std::unique_ptr<FlashCreature> flash;
    int state_ = kHanging;
    int shots = 0;                   // injections taken (the third kills)
    int bodyFrame = 1, bodyTarget = 1;  // its body's look (paler by shots)
    int bodyEnd = 1;                 // its body's last frame (dying plays to it)
};

}  // namespace as2
