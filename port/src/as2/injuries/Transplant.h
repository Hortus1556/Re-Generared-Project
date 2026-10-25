// Transplant: a diseased part replaced by a healthy one. Each of its parts
// (transplantHitsCount) has a dashed cut line around it (its hits), sawn
// with the chainsaw; then its piece is taken with the tongs to the tray, the
// healthy one taken from the tray and dropped in its place, and the cut is
// burnt and creamed (the stages' tools). removable[i] is part i's piece
// once it is cut loose.
//
// BoneTransplant is the kind with one part (the pelvis, shoulder, spine, knee,
// thighs, thumbs, nose, eye, ribs, canister, burners, arms, buttocks,
// lungs, the amp, the brain and the skull's cap: one
// routine in the binary, copied per class): one part,
// the diseased and the healthy pictures, one or two pieces of the body
// drawn over them, and the cut line's boxes from the binary's tables.
#pragma once

#include <vector>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class Transplant : public InjuryGraphic {
public:
    Transplant(InjuryData* d, Injury* i, bool playerCreated, bool force);
    ~Transplant() override;
    // All pieces taken: the next stage; a "correct" at the finger.
    void updateInjury() override;
    // Every part's boxes: its cut line, or its cut-out piece (" piece").
    void traceBoxes() const override;
    // The tongs took part i's piece (to the tray).
    void removeRemovable(int i);
    // Part i's line all sawn: its boxes go, it is complete.
    void removeHit(int i);

    struct Part {
        HitBox* hits = nullptr;  // its boxes (owned)
        int count = 0;
        bool complete = false;  // sawn through
        bool removed = false;   // its piece taken away
        DraggableWrapper* removable = nullptr;  // retained, once cut loose
    };
    std::vector<Part> parts;
    int inLayer = 40;  // where the healthy piece is put back

protected:
    void freeHits(Part& p);
    // Each part whose boxes are all done is complete (its boxes freed).
    void updateRemovables();
};

class BoneTransplant : public Transplant {
public:
    BoneTransplant(InjuryData* d, Injury* i, bool playerCreated, bool force);
    // The classes it covers (null-terminated), for the registry.
    static const char* const* kinds();
    ~BoneTransplant() override;
    void setup() override;
    // Sawn: sparks and a chip of bone at the finger.
    void createEffect(int count, int velocity) override;
    // A gas canister, held in the tongs and let go out of place: it blows
    // up (two fires there, the heart hurt).
    void interact() override;

    struct Kind;
    struct Organ;

protected:
    // The cut line (stage 1: to saw; 3 and 4: to burn and cream along).
    void lineBoxes(Part& p);
    // The piece's box, and its removable (stage 1 once cut; 2: the healthy
    // one's, the tongs move it onto the tray).
    void pieceBox(Part& p);

    // An organ of three parts.
    void organ(const Organ& o);

    // Each part's cut line and where its piece is (from the injury); the
    // line the last stages burn and cream along.
    struct PartSpec {
        const float (*points)[3];
        int count;
        float ox, oy;
    };
    std::vector<PartSpec> specs;
    const float (*heal)[3] = nullptr;
    int healCount = 0;
    int healthyIndex = 1;  // the healthy part's sprite
    int boxW = 80, boxH = 64;
    int lineDx = 0, lineDy = 0;
    bool nudgeLeft = false;
    const Kind* kind = nullptr;
    Sprite* mSpriteBG1 = nullptr;
    Sprite* mSpriteBG2 = nullptr;
};

}  // namespace as2
