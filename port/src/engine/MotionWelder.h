// A MotionWelder animation (the original's M2DMotionWelder and
// M2DResourceMotionWelderData, Amateur Surgeon Christmas): a .anu file of
// animations made of frames, each frame a set of clips cut from one image
// sheet and placed relative to the animation's position.
//
// The .anu layout (big-endian): 7 header bytes; animation table (byte
// count, then first and last frame per animation); frame table (short
// count, then per frame: clip pool index, delay byte, two unused shorts);
// a short, then the clip pools (short count, then per pool: short clip
// count and per clip: image clip index, x, y, flag byte); a short, then the
// image clips (byte image count, per image: short count and x, y, w, h per
// clip); ellipse, line, rectangle and round-rectangle tables (unused by the
// game's files); and the positioner table (short count, w and h each).
//
// Clip flags: 0, 2 and 4 draw an image clip (2 flipped horizontally, 4
// vertically); 15 is a touch area of the positioner's size. The game's two
// files use only 0 and 15.
//
// Timing: with a duration ratio of 0 (all the game uses) the animation
// steps one frame per cycle(). Without a loop offset (-1) it stops on its
// last frame, which isEndAnimation() then reports.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/Object.h"
#include "engine/AdvancedImage.h"

class MotionWelder : public Object {
public:
    // nullptr when the image or the .anu cannot be loaded.
    static MotionWelder* initWithImageNameData(const std::string& image, const std::string& anu);
    ~MotionWelder() override;

    void setAnimationWithIndex(int animation);
    void setAnimationOffsetWithIndex(int offset);  // loop back to this frame (-1: stop at the end)
    void setDurationRatio(int ratio) { durationRatio = ratio; }
    void setPositionWithXY(float x, float y) {
        posX = x;
        posY = y;
    }
    void setFrameWithIndex(int frame);
    int getCurrentFrame() const { return frame; }
    int getFrameCount() const { return frameCount; }
    int getAnimation() const { return animation; }
    int getAnimationCount() const;
    bool isEndAnimation() const { return !isPlaying || isLoopBack; }

    void cycle();
    void draw();
    // The touch areas (flag 15 clips) of the current frame, in screen
    // coordinates, in clip order.
    std::vector<Rect> getCollisionRects() const;
    // The index of the first touch area containing (x, y), or -1.
    int checkCollision(float x, float y) const;

    struct Data;  // the parsed .anu (opaque)

private:
    MotionWelder() = default;
    const std::vector<int>& currentClips() const;

    std::shared_ptr<const Data> data;
    AdvancedImage* sheet = nullptr;
    int animation = 0, frame = 0, frameCount = 0, endFrame = 0, loopOffset = -1;
    int durationRatio = 0, cycleCount = 0;
    bool isPlaying = false, isLoopBack = false;
    float posX = 0, posY = 0;
};
