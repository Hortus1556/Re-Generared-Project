// The things taken out of the patient other than the angled ones
// (AngledExtraction.h):
// - SurfaceExtraction (vtable 0x4159a0; base 26): lying on the skin (a
//   poison dart, the giant worm's head): taken with the tongs it comes away
//   at once, to be dropped on the tray; its injury_to_create_on_extraction
//   appears (the dart's poison);
// - UnderSurfaceExtraction (vtable 0x415a40; base 21): under the skin, not
//   drawn (the scanner shows it): cut open with the pizza cutter, it leaves
//   the object to pull out (its injury_to_create_at_end), with a spurt of
//   blood;
// - Transplant (vtable 0x415ad0; base 20): an organ or limb replaced. Its
//   pieces come from its injury_data_file: the chainsaw cuts round the old
//   organ (Fixed), then along its cuts (Free): a piece whose cuts are all
//   done comes free (it slides off a little) and is taken with the tongs to
//   the tray; with every piece on the tray the new organ (the replacement
//   piece) comes up on the tray, is taken with the tongs and dropped on the
//   patient (Replacing); then the lighter and the cream (Replaced, Burnt).
//   The screen's tray does the carrying (SurgeryScreen's touchBegan,
//   touchEnded, useTool's tongs).
#pragma once

#include <vector>

#include "as3/InjuryState.h"
#include "as3/ParticleSystem.h"

namespace mt {

class SurfaceExtraction : public InjuryState {
public:
    SurfaceExtraction();  // 0x99e24
    void start() override;  // vf2 0x883cc
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x87f30
    void toolStopped(int) override { path.clear(); }  // vf23 0x87ea8
    void setRotation(float degrees) override;  // vf31 0x87eb8
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x87ee4
    // (0x77c4c, base 26) its picture, its one piece the picture's size about its place
    void buildPictures() override;
    // (vf13) its piece's rect, and its art there (ArtHit.h)
    bool pieceContains(const PieceState& piece, const cocos2d::CCPoint& touch) const override;

    bool holding = false;          // +0xfd98: in the tongs, not yet off
    bool takenOut = false;         // +0xfd99: off, in the tongs
    // (+0xfd9a perfect: InjuryState's)
    bool comboGiven = false;       // +0xfd9c
    int fda8 = 40;                 // +0xfda8 (unused)
    int extractDistance = 0;       // +0xfdac: never set: it comes off at once
    bool extractedEventSent = false;  // +0xfdb0
    cocos2d::CCPoint lastTouch;    // +0xfdb4
    int angleOffset = 0;           // +0xfdbc: random() % 1
    std::vector<cocos2d::CCPoint> path;  // +0xfdc8
};

class UnderSurfaceExtraction : public InjuryState {
public:
    void onFinished() override;  // vf20 0x8672c: the incision's sound, a blood spurt
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x86a64: the cutter on it, the scanner elsewhere
};

class Transplant : public InjuryState {
public:
    // A piece's state (the original's piece record, 0xa08 bytes at +0x6704).
    struct Piece {
        int state = -1;           // +0: the state it belongs to (its cuts' state; 6 for the replacement)
        bool inBody = true;       // +0x9f8: not on the tray yet
        bool attached = true;     // +0x9f9: not cut free yet
        bool interactable = true; // +0x9fa
        bool shown = true;        // +0x9fb: its picture shown (the replacement once placed)
        bool comboGiven = false;  // +0x9fc
    };

    Transplant();  // (0x908c4 case 0x14)
    ~Transplant() override;
    void start() override;   // vf2 0x888fc
    void vf4(float dt) override;  // vf4 0x88dac: its dust
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x88dd4
    void completeStage() override;  // vf18 0x89338
    void onFinished() override;     // vf20 0x88c08: its stat (1.45 0xb0aa4)
    void toolStopped(int) override { holdingPiece = false; }  // vf23 0x88c2c
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x88c38
    // (0x77c4c, base 20) a picture for each piece shown, at its z order;
    // the replacement's held where its active rect's middle is
    void buildPictures() override;

    // the pieces of a state that can be handled (interactable or the
    // replacement) (0x861c8), as indices into `pieces`
    std::vector<size_t> piecesOf(int state) const;
    // the hit area of the current state named `name` (0x86184: the last of that name)
    HitAreaState* areaNamed(const std::string& name);
    // a piece of the old organ was dropped on the tray: whether every piece
    // of its state is off (the screen then finishes the stage)
    bool allOnTray() const;
    // the replacement piece (the one of the Replacing state), or -1
    int replacementIndex() const;
    // the new organ dropped on the patient (touchEnded): shown in place
    void placeReplacement(size_t index);

    std::vector<Piece> flags;          // beside `pieces`
    int slideBeyond = 50;              // +0xfda8: a freed piece's middle this far from its own slides
    int slideBy = 10;                  // +0xfdac
    bool holdingPiece = false;         // +0xfdb0: a piece in the tongs
    bool cutThisFrame = false;         // +0xfdb1
    ParticleSystem* dust = nullptr;    // +0xfdb4: the chainsaw's dust (the giant worm's wrap only)
    bool allCutFree = false;           // +0xfdb8: every cut of its Free stage done

private:
    void placePieces();  // vf2's records: each piece's rect about its place
};

}  // namespace mt
