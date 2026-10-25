// Remains (the port's own, on the user's request; Flash took a dead monster
// away): what a monster drawn as the Flash AS2's leaves where it died, its
// last death picture (X-eyed), turned as it was, lying still (what plays in
// it held) on the body to lift away with the tongs to the tray (an
// Extraction). Taken by one box about its hit areas, turned with it.
//
// DeadDove: the dove's `DEAD` frame (the live dove and its shadow, which
// Flash's frame kept under it, hidden).
// DeadWasp: the wasp's (InsectBoss) `DEAD` frame.
// DeadSquid: the squid with its head on its `dead` look, taken by its
// body's box (`hit6`).
// DeadCentipede: the small centipede's head, sawn off last (it has no death
// picture: its pieces go one by one), taken by its art's box.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "as2/FlashCreature.h"
#include "as2/injuries/Extraction.h"

namespace as2 {

class Remains : public Extraction {
public:
    void setup() override;

protected:
    // A kind of remains: its Flash symbol, the label of its picture ("": the
    // first frame), the pieces not drawn, inner clips held on a label of
    // theirs, the hit areas it is taken by, and the iPad's picture under it
    // were the art missing.
    struct Kind {
        const char* symbol;
        const char* label;
        std::vector<std::string> hidden;
        std::vector<std::pair<std::string, std::string>> innerLabels;
        const char* hitGroup;  // ("": the art's box)
        const char* picture;
        int size;
    };
    Remains(InjuryData* d, Injury* i, bool playerCreated, bool force, const Kind& kind);

private:
    std::unique_ptr<FlashCreature> flash;
    std::string label, hitGroup;
};

class DeadDove : public Remains {
public:
    DeadDove(InjuryData* d, Injury* i, bool playerCreated, bool force)
        : Remains(d, i, playerCreated, force,
                  {"Dove", "DEAD", {"hit", "shadow", "alive"}, {}, "hit", "doveGlide001", 256}) {}
};

class DeadWasp : public Remains {
public:
    DeadWasp(InjuryData* d, Injury* i, bool playerCreated, bool force)
        : Remains(d, i, playerCreated, force, {"InsectBoss", "DEAD", {"hit"}, {}, "hit", "waspMove_0001", 256}) {}
};

class DeadSquid : public Remains {
public:
    DeadSquid(InjuryData* d, Injury* i, bool playerCreated, bool force)
        : Remains(d, i, playerCreated, force,
                  {"Squid", "", {"hit1", "hit2", "hit3", "hit4", "hit5", "hit6"}, {{"head", "dead"}}, "hit6",
                   "squidNormal", 512}) {}
};

class DeadCentipede : public Remains {
public:
    DeadCentipede(InjuryData* d, Injury* i, bool playerCreated, bool force)
        : Remains(d, i, playerCreated, force, {"head", "", {}, {}, "", "centipede_Head0001", 128}) {}
};

}  // namespace as2
