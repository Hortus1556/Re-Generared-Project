// What the tongs pull out. SurfaceExtraction: an object stuck in a wound
// (glass, a bone, money, a key...): pulled along its direction it slides out
// (setExtractionLength), then it is carried to the tray; the wound is then
// stitched, burnt and creamed. Its object is its removable (a DraggableWrapper),
// the thing the tongs hold.
#pragma once

#include <string>

#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

class SurfaceExtraction : public InjuryGraphic {
public:
    SurfaceExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force);
    ~SurfaceExtraction() override;
    void render() override;
    void setup() override;
    void updateInjury() override;
    // Held: the boxes grow, so the finger stays on them more easily.
    void extendHitBox() override;
    // How far it has come out (points pulled).
    virtual void setExtractionLength(float length);

    float startLen = 112;  // pixels of it showing at first
    int angleBuffer = 35;  // degrees off its direction a pull may be
    int removableLength = 60;
    int spriteLength = 128;
    bool hasPoison = false;
    float moved = 0;  // the clown's
    float origRX = -1, origHX = -1;

protected:
    // initWithInjuryData:...NoSetup: (the Under... kinds): InjuryGraphic's
    // part only; the subclass sets the rest up.
    SurfaceExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force, bool noSetup);
    // The wound (mSpriteArray[0]), not yet shown.
    Sprite* makeWound();
    // The object (mSpriteArray[1]) as the tongs' removable.
    void addRemovable();
    // The object's sprite as in the wound at first (its picture cut to
    // startLen, turned along the injury).
    void placeObject(float uvBottom);
    // The one box the object is taken by (64 points wide, `height` high).
    void objectBox(int height);
    // The 5 boxes of the wound left, stitched (no object to take now).
    void stitchBoxes();
};

// UnderSurfaceExtraction: an object under the skin (money, a card...),
// hidden: cut open (the cutter, at once), then taken out like a surface
// extraction; poisonous ones splash poison when opened.
class UnderSurfaceExtraction : public SurfaceExtraction {
public:
    UnderSurfaceExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void setup() override;
    void updateInjury() override;
};

// UnderSurfaceExtractionClown: the clown's endless scarf, under the skin:
// cut open, then pulled with the tongs 800 points (between pulls it slips
// back in), out comes a bunny; the wound is then stitched, burnt and creamed.
class UnderSurfaceExtractionClown : public SurfaceExtraction {
public:
    UnderSurfaceExtractionClown(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
    void updateInjury() override;
    void setExtractionLength(float length) override;
};

// Extraction: a thing the tongs simply lift away (a bunny, a chicken leg,
// an egg...): taken at once ("correct"), carried to the tray it is done.
class Extraction : public InjuryGraphic {
public:
    Extraction(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void updateInjury() override;
};

// An Extraction shown by one picture (`size` points square, about its
// middle, in `layer`), taken by one box at it (box_width x box_height): an
// egg, a cyst, a peanut.
class PictureExtraction : public Extraction {
public:
    void setup() override;

protected:
    PictureExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force, const char* stages,
                      const char* picture, int size, int box, int layer = 38);
};

// ChickenLeg: a chicken leg (mission 23), lifted away.
class ChickenLeg : public PictureExtraction {
public:
    ChickenLeg(InjuryData* d, Injury* i, bool playerCreated, bool force)
        : PictureExtraction(d, i, playerCreated, force, "ChickenLeg", "chickenleg", 128, 80) {
        make = "extractionChickenLeg";
        setup();
    }
};

// PeanutExtraction: a peanut (mission 26), lifted away.
class PeanutExtraction : public PictureExtraction {
public:
    PeanutExtraction(InjuryData* d, Injury* i, bool playerCreated, bool force)
        : PictureExtraction(d, i, playerCreated, force, "PeanutExtraction", "nutty2", 128, 80, 35) {
        make = "extractionPeanut";
        setup();
    }
};

// Bunny: what comes out with the clown's scarf, to lift away.
class Bunny : public Extraction {
public:
    Bunny(InjuryData* d, Injury* i, bool playerCreated, bool force);
    void render() override;
    void setup() override;
};

}  // namespace as2

