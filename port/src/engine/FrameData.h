// One animation frame of an AdvancedImage: where the frame sits in the
// texture (start*, frame*) and how it is placed when drawn (offset*, angle,
// alpha). Port of the FrameData class.
#pragma once

#include "core/Object.h"

class FrameData : public Object {
public:
    int offsetX = 0, offsetY = 0;
    int startX = 0, startY = 0;
    int frameWidth = 0, frameHeight = 0;
    float alpha = 1.0f;
    float angle = 0.0f;

    FrameData() = default;

    static FrameData* initWithStartXStartYOffsetXOffsetYFrameWidthFrameHeight(float sx, float sy, float ox, float oy,
                                                                             float fw, float fh) {
        FrameData* f = new FrameData;
        f->startX = static_cast<int>(sx);
        f->startY = static_cast<int>(sy);
        f->offsetX = static_cast<int>(ox);
        f->offsetY = static_cast<int>(oy);
        f->frameWidth = static_cast<int>(fw);
        f->frameHeight = static_cast<int>(fh);
        return f;
    }
    static FrameData* initWithOffsetXOffsetYFrameWidthFrameHeight(float ox, float oy, float fw, float fh) {
        FrameData* f = new FrameData;
        f->offsetX = static_cast<int>(ox);
        f->offsetY = static_cast<int>(oy);
        f->frameWidth = static_cast<int>(fw);
        f->frameHeight = static_cast<int>(fh);
        return f;
    }
};
