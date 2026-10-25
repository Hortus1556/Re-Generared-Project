// A textured sprite with optional animation frames (port of AdvancedImage).
//
// The texture comes from ResourceManager (shared per file; packed sprites
// resolve to their atlas plus an offset). Frames are either a row x col grid
// over the image or an explicit FrameData list.
#pragma once

#include <string>

#include "core/Object.h"
#include "engine/FrameData.h"
#include "platform/GL.h"

class AdvancedImage : public Object {
public:
    // Loads `path` (a bundle image name). Returns nullptr when the image
    // cannot be loaded, as the original returned nil.
    static AdvancedImage* initWithPathRowColData(const std::string& path, int row, int col,
                                                 const Array<FrameData>* data);
    ~AdvancedImage() override;

    // Black rectangle with alpha `a` in screen coordinates.
    static void drawRectangleWithXYWHA(float x, float y, float w, float h, float a, float r = 0, float g = 0,
                                       float b = 0);
    // Full-screen plane of a colour.
    static void drawPlaneWithColorAlpha(float r, float g, float b, float a);

    void draw();
    void drawAtCenter();
    void drawWithFrameData(const FrameData* fd);
    void drawWithImageRegionWithXYWidthHeight(int x, int y, int w, int h);
    void drawFrameWithImageRegionWithXYWidthHeight(int x, int y, int w, int h);

    void setPositionWithXY(int x, int y) { setPositionWithXYZ(x, y, 0); }
    void setPositionWithXYZ(int x, int y, int z) {
        positionX = x;
        positionY = y;
        positionZ = z;
    }
    int getPositionX() const { return positionX; }
    int getPositionY() const { return positionY; }

    void setFrame(int f) {
        if (f >= 0 && f < totalFrame) currentFrame = f;
    }
    int getFrame() const { return currentFrame; }
    void nextFrame() { currentFrame = currentFrame == totalFrame - 1 ? 0 : currentFrame + 1; }

    // The original also released the frame array here (leaving a dangling
    // pointer); keeping the array alive gives the same observable behaviour.

    float getFrameWidth() const;   // current frame width * ratio
    float getFrameHeight() const;  // current frame height * ratio

    void setAlpha(float a) { alpha = a; }
    float getAlpha() const { return alpha; }
    void setRatio(float r) { ratio = r; }
    float getRatio() const { return ratio; }
    void setOrientation(float o) { orientation = o; }
    float getOrientation() const { return orientation; }
    void setAnchorPointXY(float x, float y) {
        anchorPointX = x;
        anchorPointY = y;
    }
    void setRGB(float r, float g, float b) {
        colorR = r;
        colorG = g;
        colorB = b;
    }
    // axis 0 mirrors horizontally, 1 vertically.
    void setMirrorWith(int axis, bool on) {
        if (axis == 0) flipHorizontally = on;
        else if (axis == 1) flipVertically = on;
    }
    int getResourceID() const { return resourceID; }
    const std::string& getImageName() const { return filename; }

    // Public like the Objective-C ivars, which game code accessed directly.
    GLuint textureID = 0;
    int resourceID = -1;
    int row = 0, col = 0;
    int currentFrame = 0, totalFrame = 0;
    int positionX = 0, positionY = 0, positionZ = 0;
    float textureWidth = 0, textureHeight = 0;
    bool flipHorizontally = false, flipVertically = false;
    float orientation = 0;
    float alpha = 1;
    float ratio = 1;
    float anchorPointX = 0, anchorPointY = 0;
    int packedImageOffsetX = 0, packedImageOffsetY = 0, packedImageOffsetW = 0, packedImageOffsetH = 0;
    std::string filename;
    Array<FrameData> frameDataArray;
    float colorR = 1, colorG = 1, colorB = 1;

private:
    AdvancedImage() = default;
    const FrameData* current() const { return frameDataArray.objectAtIndex(currentFrame); }
};
