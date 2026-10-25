// Splatter: drops thrown from a point (blood from a cut, fat, ink...): each
// flies off at a random turn and speed, slowing by the friction and shrinking,
// and lands as a stain where it stops (or is gone off the body). The fire
// splatters' drops become burns instead (`remove`). BloodSplatter is the
// red one tools make.
#pragma once

#include <vector>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Splatter : public InjuryGraphic {
public:
    // velocity -1: the default 7.
    Splatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1);
    ~Splatter() override;
    void render() override;
    void setup() override;
    void updateInjury() override;
    // The vacuum's nozzle at `at`: each drop landed within `radius` is
    // drawn toward it (suction over the distance a frame, as its drop
    // picture) and taken within `nozzleRadius`; the drops taken.
    int suck(Point at, int nozzleRadius, int radius, int suction);

protected:
    struct Particle {
        Sprite* sprite;  // retained
        float vel;
        float step, steps;  // frames flown, of all it flies
        unsigned landed = 0;  // a stain: its number in the trace (0 not yet)
    };
    void removeParticle(size_t index);
    // The drops' flight and landing (render's).
    void fly();

    float red = 1, green = 1, blue = 1;
    float baseVel = 7;
    int origWidth = 64, origHeight = 128;
    float particle_mod = 0.7f;
    int numParticles = 1;
    float friction = 0.9f;
    int ran = 5;  // the stain pictures (Splatter1..5)
    int minX = 60, maxX = 450, minY = 10, maxY = 370;
    bool remove = false;
    // It saps the heart while it lies (poison, ink, fat): a third of its
    // drops each damage period.
    bool saps = false;
    std::vector<Particle> particles;  // oldest first
};

class BloodSplatter : public Splatter {
public:
    BloodSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1);

protected:
    // Another kind (the kinds below): its colour, its picture ("Splatter",
    // or its own), the drop's size and how many drops.
    struct Kind {
        float r, g, b;
        const char* make;
        int width, height, particles;
    };
    BloodSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity, const Kind& kind);
};

// BlueBloodSplatter: the crab's.
class BlueBloodSplatter : public BloodSplatter {
public:
    BlueBloodSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : BloodSplatter(d, i, playerCreated, force, velocity, {0.125f, 0.4375f, 0.5f, "Splatter", 64, 128, 1}) {}
};

// DoveSplatter: what the dove drops, white, two drops (only the vacuum
// clears it).
class DoveSplatter : public BloodSplatter {
public:
    DoveSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : BloodSplatter(d, i, playerCreated, force, velocity, {1, 1, 1, "Splatter", 64, 128, 2}) {}
};

// FeatherSplatter: the dove's feathers flying when it is shocked.
class FeatherSplatter : public BloodSplatter {
public:
    FeatherSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : BloodSplatter(d, i, playerCreated, force, velocity, {1, 1, 1, "featherSplatter", 32, 64, 1}) {}
};

// FireSplatter: burning drops thrown from a fire; each lands as a burn.
class FireSplatter : public Splatter {
public:
    FireSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : Splatter(d, i, playerCreated, force, velocity) {
        make = "Splatter";
        red = green = 0.75f;
        origWidth = 64;
        origHeight = 128;
        remove = true;
        setup();
    }
};

// SaltSplatter: salt on the vampire (mission 10), white, three drops; only
// the vacuum clears it.
class SaltSplatter : public BloodSplatter {
public:
    SaltSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : BloodSplatter(d, i, playerCreated, force, velocity, {1, 1, 1, "Splatter", 64, 128, 3}) {}
};

// PoisonSplatter: what a poison blob bursts into when injected: thirteen
// green drops of its own picture that sap the heart (a third of the drops
// left, every 350 frames) until vacuumed.
class PoisonSplatter : public Splatter {
public:
    PoisonSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1);

protected:
    // Another such kind: its picture, colour, drop size and drops.
    PoisonSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, const char* picture, float r, float g,
                   float b, int width, int height, int particles);
};

// InkSplatter: the squid's ink (mission 13), fifteen black drops that sap
// the heart as poison does until vacuumed.
class InkSplatter : public PoisonSplatter {
public:
    InkSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int = -1)
        : PoisonSplatter(d, i, playerCreated, force, "Splatter", 0, 0, 0, 64, 128, 15) {}
};

// FatSplatter: fat thrown from a fat being cut out or burnt (mission 14),
// eight white drops that sap the heart until vacuumed.
class FatSplatter : public BloodSplatter {
public:
    FatSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : BloodSplatter(d, i, playerCreated, force, velocity, {1, 1, 1, "Splatter", 64, 128, 8}) {
        saps = true;
    }
};

// YellowSplatter: goo thrown from a lump of goo cut open (mission 23), a
// yellow drop only the vacuum clears.
class YellowSplatter : public Splatter {
public:
    YellowSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : Splatter(d, i, playerCreated, force, velocity) {
        make = "Splatter";
        red = green = 1;
        blue = 0.2f;
        origWidth = 64;
        origHeight = 128;
        setup();
    }
};

// PeanutSplatter: a loose peanut piece (a kernel, a broken or open shell:
// nut1..nut5) on the patient (mission 26): one,
// lying where it falls (no flight), sapping the heart until vacuumed. The
// level's (the original's four-argument init) are 128 points across; one
// thrown with a velocity, 32.
class PeanutSplatter : public Splatter {
public:
    PeanutSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : Splatter(d, i, playerCreated, force, velocity) {
        ran = 1;
        baseVel = 0;
        numParticles = 1;
        make = "peanutSplatter";
        red = green = blue = 1;
        origWidth = origHeight = velocity == -1 ? 128 : 32;
        saps = true;
        setup();
        fly();  // (it lands at once)
    }
};

// BarnacleSplatter: a dying barnacle's sea-green blood.
class BarnacleSplatter : public BloodSplatter {
public:
    BarnacleSplatter(InjuryData* d, Injury* i, bool playerCreated, bool force, int velocity = -1)
        : BloodSplatter(d, i, playerCreated, force, velocity, {0, 0.58f, 0.27f, "Splatter", 64, 128, 1}) {}
};

}  // namespace as2
