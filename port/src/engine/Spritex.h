// Cut-out sprite animation (port of Spritex and ResourceSpritexData).
//
// A spritex is a set of source images plus a .txt description, exported from
// the original Flash animations. The .txt has four lines; numbers inside a
// record are separated by 'a', records by 'b', groups by 'c':
//   0  pieces:  x a y a w a h               (rectangles in the stacked images)
//   1  frames:  c-separated; each part = piece a x a y a mirror
//   2  bounds:  c-separated per frame: x a y a w a h [a e1 a e2], or empty to
//               derive from the pieces; the last group lists the images
//   3  actions: c-separated; each step = frame a durationMs
// Animation advances on wall-clock time (durations in milliseconds).
#pragma once

#include <string>
#include <vector>

#include "core/Object.h"
#include "engine/AdvancedImage.h"
#include "engine/Resource.h"

class ResourceSpritexData : public Resource {
public:
    ResourceSpritexData(int id, const std::string& filename);

    // pieces[i] = {x, y, w, h, imageIndex}; y is relative to its image.
    std::vector<std::vector<int>> pieces;
    // frames[f][part] = {piece, x, y, mirror}
    std::vector<std::vector<std::vector<int>>> frames;
    // actions[a][step] = {frame, durationMs}
    std::vector<std::vector<std::vector<int>>> actions;
    // frameBounds[f] = {left, right, top, bottom, e1, e2}
    std::vector<std::vector<int>> frameBounds;
    // images[i] = {?, height, ...}: the source images stacked vertically
    std::vector<std::vector<int>> images;

private:
    void createSpritexData(const std::string& text);
    int getImageIndex(int piece);
};

class Spritex : public Object {
public:
    // `images`: comma-separated image names; `data`: the .txt bundle name.
    static Spritex* initWithData(const std::string& images, const std::string& data);
    ~Spritex() override;

    void drawSpritex();
    void cycleSpritex() { nextSpritexFrame(); }
    // Frames last at least this long (a frame's own duration is often 0:
    // "next update"). The iPad could not run the game's updates at a full 20
    // a second, so such flickers were slower than here; a port choice, used
    // for story scene backgrounds (the street lamps).
    void setMinFrameMs(double ms) { minFrameMs = ms; }
    void nextSpritexFrame();
    // type 0 loops, 1 stops on the last step, 2 stops back on the first.
    void setSpritexStateWithType(int state, int type);
    bool isAnimating() const { return actionEnable; }
    // The image names it was made from (Christmas: the story tells characters apart by them).
    const std::string& getImgName() const { return imgName; }

    int getSpritexFrameWidth() const;
    int getSpritexFrameHeight() const;
    void setSpritexPositionXY(int x, int y) {
        posX = x;
        posY = y;
    }
    void setSpritexScaleXWithScaleY(float sx, float sy) {
        scaleX = sx;
        scaleY = sy;
    }
    void setSpritexMirror(bool m) { mirror = m ? 2 : 0; }
    void setSpritexOrientation(float deg) { rotateAngle = static_cast<int>(deg); }

    std::vector<AdvancedImage*> spritexImage;
    ResourceSpritexData* res = nullptr;  // null when the .txt is missing
    int resID = -1;
    int actionState = 0, actionFrameState = 0, frameState = 0;
    double currentTime = 0;  // milliseconds
    double minFrameMs = 0;   // see setMinFrameMs
    int actionType = 0;
    bool actionEnable = true;
    std::string imgName;
    int mirror = 0;  // 0 or 2, compared against each part's mirror flag
    int rotateAngle = 0;
    int posX = 0, posY = 0;
    float scaleX = 1, scaleY = 1;

private:
    Spritex() = default;
    void loadSpritexImage(const std::string& images);
};
