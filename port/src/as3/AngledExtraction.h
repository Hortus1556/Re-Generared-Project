// mt::AngledExtraction (vtable 0x415900; base class 0): an object stuck in
// the body at an angle (a monitor shard, a splinter). Held with the tongs it
// is pulled out along its own direction: the pull's direction is checked
// every three moves against the object's, and too far off (30 degrees, 45 on
// a small screen) or too fast (beyond the tongs' reach) it slips back in.
// Pulled out by half its length, it comes free in the tongs (and its
// "injury_to_create_on_extraction" appears), to be dropped on the tray.
#pragma once

#include <vector>

#include "as3/InjuryState.h"

namespace cocos2d {
class CCClippingNode;
}

namespace mt {

class AngledExtraction : public InjuryState {
public:
    AngledExtraction();  // 0x99f6c
    void start() override;  // vf2 0x8790c
    HitResult hitTest(const cocos2d::CCPoint& touch, ToolState* tool) override;  // vf8 0x86fa0
    void onFinished() override;  // vf20 0x87e94: its stat (1.45 0xafa78)
    int toolFor(const cocos2d::CCPoint& touch) override;  // vf27 0x86f24
    void placeNodes() override;     // vf30 0x878e0
    void setRotation(float degrees) override;  // vf31 0x86f60
    void buildPictures() override;  // 0x77c4c, base 0
    // (vf13) its piece's rect, and its art showing there (ArtHit.h)
    bool pieceContains(const PieceState& piece, const cocos2d::CCPoint& touch) const override;
    // the tongs let go of it (vf23 0x86f00: holding, taken out and the path
    // reset when the tongs are put down). Not overridden as toolStopped: the
    // port's screen calls the injuries' toolStopped before its let-go, which
    // looks at `holding` to slip a pulled object back (the original's looked
    // at the tongs' target), and that let-go (slipBack, releasePiece) resets
    // the same fields
    void released();
    // the object back in the body, as it was (its sprite to its place)
    void slipBack();

    bool holding = false;          // +0xfd98: the tongs pull it
    bool takenOut = false;         // +0xfd99: out, in the tongs
    // (+0xfd9a perfect: InjuryState's)
    bool comboGiven = false;       // +0xfd9c
    int tolerance = 30;            // +0xfda8: degrees
    int extractDistance = 0;       // +0xfdac: points, half the object's length
    bool extractedEventSent = false;  // +0xfdb0
    cocos2d::CCPoint lastTouch;    // +0xfdb4
    cocos2d::CCClippingNode* clip = nullptr;  // +0xfdbc: the object under the skin line
    int angleOffset = 0;           // +0xfdc0
    std::vector<cocos2d::CCPoint> path;  // +0xfdcc: where the pull began
    std::vector<int> samples;      // +0xfdd8: the pull's angle off, degrees
};

}  // namespace mt
