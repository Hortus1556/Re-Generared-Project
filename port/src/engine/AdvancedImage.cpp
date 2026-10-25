#include "engine/AdvancedImage.h"

#include <algorithm>

#include "engine/Resource.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"

namespace {

constexpr GLuint kNoTexture = static_cast<GLuint>(-1);

// What every AdvancedImage draw call shared in the original: straight alpha
// blending (or additive-looking GL_ONE for some opaque draws), colour
// modulation, and the transform
//   translate(pos + offset + pivotIn) * rotate(angle) * translate(-pivotOut)
// which is handed to the recorder as parameters rather than GL matrix calls,
// so that the presentation can animate position and angle smoothly while
// the frame's offset and corners stay content (platform::Sprite).
// Texture coordinates moved half a texel into their rectangle: drawn larger
// than 1:1 (any window bigger than the original screen), linear filtering would
// otherwise blend a region's edge with what lies next to it in the texture,
// leaving seams where pieces of one picture join (the lamp light in Bleed's
// street scene) and fringes from neighbouring sheet cells and glyphs.
void insetHalfTexel(float t[8], float tw, float th) {
    for (int axis = 0; axis < 2; ++axis) {
        float lo = std::min({t[axis], t[2 + axis], t[4 + axis], t[6 + axis]});
        float hi = std::max({t[axis], t[2 + axis], t[4 + axis], t[6 + axis]});
        float half = std::min(0.5f / (axis == 0 ? tw : th), (hi - lo) / 2);
        for (int i = axis; i < 8; i += 2) t[i] += t[i] == lo ? half : t[i] == hi ? -half : 0;
    }
}

void sprite(platform::DrawId owner, const platform::Sprite& s, const float in[8], GLuint tex, float tw, float th,
            float r, float g, float b, float a, GLenum srcBlend = GL_SRC_ALPHA, bool setFilters = false) {
    float t[8];
    if (in) {
        std::copy_n(in, 8, t);
        insetHalfTexel(t, tw, th);
    }
    if (!in) tex = kNoTexture;
    bool textured = tex != kNoTexture;
    if (textured && setFilters) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    const float rgba[4] = {r, g, b, a};
    platform::drawSprite(owner, textured ? tex : 0, s, textured ? t : nullptr, rgba, srcBlend);
}

platform::Sprite placed(float px, float py, float ox, float oy, float pinX, float pinY, float poutX, float poutY,
                        float angle) {
    platform::Sprite s;
    s.pos[0] = px;
    s.pos[1] = py;
    s.offset[0] = ox;
    s.offset[1] = oy;
    s.pivotIn[0] = pinX;
    s.pivotIn[1] = pinY;
    s.pivotOut[0] = poutX;
    s.pivotOut[1] = poutY;
    s.angle = angle;
    return s;
}

// Quad corners (x0,y0) (x1,y0) (x0,y1) (x1,y1), the order used throughout.
void corners(float out[8], float x0, float y0, float x1, float y1) {
    out[0] = x0; out[1] = y0;
    out[2] = x1; out[3] = y0;
    out[4] = x0; out[5] = y1;
    out[6] = x1; out[7] = y1;
}

}  // namespace

AdvancedImage* AdvancedImage::initWithPathRowColData(const std::string& path, int row, int col,
                                                     const Array<FrameData>* data) {
    ResourceManager& rm = ResourceManager::Instance();
    int id = rm.createResourceTexture(path);
    auto* tex = static_cast<ResourceTexture*>(rm.getResource(id));
    if (!tex) return nullptr;

    AdvancedImage* img = new AdvancedImage;
    img->resourceID = id;
    std::vector<int> xywh = ResourcePackedImageData::resourceGetPackedImageXYWH(path);
    img->packedImageOffsetX = xywh[0];
    img->packedImageOffsetY = xywh[1];
    img->packedImageOffsetW = xywh[2];
    img->packedImageOffsetH = xywh[3];
    img->filename = path;
    img->textureID = tex->getOPENGLTextureID();
    img->row = row;
    img->col = col;
    // Frame sizes come from the image (or packed region) size...
    img->textureWidth = static_cast<float>(tex->getImageWidth());
    img->textureHeight = static_cast<float>(tex->getImageHeight());
    if (!data) {
        auto makeFrame = [&] {
            FrameData* f = new FrameData;
            if (img->packedImageOffsetW == 0) {
                f->frameWidth = static_cast<int>(img->textureWidth / static_cast<float>(col));
                f->frameHeight = static_cast<int>(img->textureHeight / static_cast<float>(row));
            } else {
                f->frameWidth = img->packedImageOffsetW / col;
                f->frameHeight = img->packedImageOffsetH / row;
            }
            return f;
        };
        if (row == 1 && col == 1) {
            img->totalFrame = 1;
            FrameData* f = makeFrame();
            img->frameDataArray.addObject(f);
            f->release();
        } else {
            img->totalFrame = row * col;
            for (int r = 0; r < row; ++r)
                for (int c = 0; c < col; ++c) {
                    FrameData* f = makeFrame();
                    f->startX = c * f->frameWidth;
                    f->startY = r * f->frameHeight;
                    img->frameDataArray.addObject(f);
                    f->release();
                }
        }
    } else {
        img->totalFrame = data->count();
        for (FrameData* f : *data) img->frameDataArray.addObject(f);
    }
    // ...while texture coordinates divide by the power-of-two texture size.
    img->textureWidth = static_cast<float>(tex->getTextureWidth());
    img->textureHeight = static_cast<float>(tex->getTextureHeight());
    return img;
}

AdvancedImage::~AdvancedImage() { ResourceManager::Instance().deleteResource(resourceID); }

float AdvancedImage::getFrameWidth() const { return totalFrame ? current()->frameWidth * ratio : 0; }
float AdvancedImage::getFrameHeight() const { return totalFrame ? current()->frameHeight * ratio : 0; }

void AdvancedImage::drawRectangleWithXYWHA(float x, float y, float w, float h, float a, float r, float g, float b) {
    float v[8];
    corners(v, x, y, x + w, y + h);
    const float rgba[4] = {r, g, b, a};
    platform::drawQuad(0, 0, v, nullptr, rgba, GL_SRC_ALPHA);
}

void AdvancedImage::drawPlaneWithColorAlpha(float r, float g, float b, float a) {
    float v[8];
    corners(v, 0, 0, platform::screenWidth(), platform::screenHeight());
    const float rgba[4] = {r, g, b, a};
    platform::drawQuad(0, 0, v, nullptr, rgba, GL_SRC_ALPHA);
}

void AdvancedImage::draw() {
    if (totalFrame == 0) return;
    const FrameData* fd = current();
    float w = fd->frameWidth * ratio, h = fd->frameHeight * ratio;
    float sx = static_cast<float>(packedImageOffsetX + fd->startX);
    float sy = static_cast<float>(packedImageOffsetY + fd->startY);
    float u0 = sx / textureWidth, u1 = (sx + fd->frameWidth) / textureWidth;
    float v0 = sy / textureHeight, v1 = (sy + fd->frameHeight) / textureHeight;
    if (flipVertically) std::swap(v0, v1);
    float t[8];
    corners(t, u0, v0, u1, v1);
    platform::Sprite s = placed(positionX, positionY, fd->offsetX, fd->offsetY, anchorPointX, anchorPointY,
                                anchorPointX, anchorPointY, orientation);
    if (flipHorizontally) corners(s.xy, w, 0, 0, h);
    else corners(s.xy, 0, 0, w, h);
    sprite(serial(), s, t, textureID, textureWidth, textureHeight, colorR, colorG, colorB, alpha, GL_SRC_ALPHA, true);
}

void AdvancedImage::drawAtCenter() {
    if (totalFrame == 0) return;
    const FrameData* fd = current();
    float hw = fd->frameWidth * ratio * 0.5f, hh = fd->frameHeight * ratio * 0.5f;
    float sx = static_cast<float>(packedImageOffsetX + fd->startX);
    float sy = static_cast<float>(packedImageOffsetY + fd->startY);
    float u0 = sx / textureWidth, u1 = (sx + fd->frameWidth) / textureWidth;
    float v0 = sy / textureHeight, v1 = (sy + fd->frameHeight) / textureHeight;
    if (flipVertically) std::swap(v0, v1);
    float t[8];
    corners(t, u0, v0, u1, v1);
    // Unlike draw(), the original ignores the RGB tint here and applies only
    // the negative anchor translation before rotating.
    platform::Sprite s =
        placed(positionX, positionY, fd->offsetX, fd->offsetY, -anchorPointX, -anchorPointY, 0, 0, orientation);
    if (flipHorizontally) corners(s.xy, hw, -hh, -hw, hh);
    else corners(s.xy, -hw, -hh, hw, hh);
    sprite(serial(), s, t, textureID, textureWidth, textureHeight, 1, 1, 1, alpha);
}

void AdvancedImage::drawWithFrameData(const FrameData* fd) {
    float w = fd->frameWidth * ratio, h = fd->frameHeight * ratio;
    float ox = static_cast<float>(fd->offsetX), oy = static_cast<float>(fd->offsetY);
    float sx = static_cast<float>(packedImageOffsetX + fd->startX);
    float sy = static_cast<float>(packedImageOffsetY + fd->startY);
    float u0 = sx / textureWidth, u1 = (sx + fd->frameWidth) / textureWidth;
    float v0 = sy / textureHeight, v1 = (sy + fd->frameHeight) / textureHeight;
    if (flipVertically) std::swap(v0, v1);
    float v[8], t[8];
    if (flipHorizontally) corners(v, ox + w, oy, ox, oy + h);
    else corners(v, ox, oy, ox + w, oy + h);
    corners(t, u0, v0, u1, v1);
    platform::Sprite s =
        placed(positionX, positionY, 0, 0, anchorPointX, anchorPointY, anchorPointX, anchorPointY, orientation);
    std::copy_n(v, 8, s.xy);
    sprite(serial(), s, t, textureID, textureWidth, textureHeight, colorR, colorG, colorB, alpha);
}

void AdvancedImage::drawWithImageRegionWithXYWidthHeight(int x, int y, int w, int h) {
    float sx = static_cast<float>(x + packedImageOffsetX), sy = static_cast<float>(y + packedImageOffsetY);
    float u0 = sx / textureWidth, u1 = (sx + w) / textureWidth;
    float v0 = sy / textureHeight, v1 = (sy + h) / textureHeight;
    if (flipVertically) std::swap(v0, v1);
    float rw = w * ratio, rh = h * ratio;
    float v[8], t[8];
    if (flipHorizontally) corners(v, rw, 0, 0, rh);
    else corners(v, 0, 0, rw, rh);
    corners(t, u0, v0, u1, v1);
    platform::Sprite s =
        placed(positionX, positionY, 0, 0, anchorPointX, anchorPointY, anchorPointX, anchorPointY, orientation);
    std::copy_n(v, 8, s.xy);
    sprite(serial(), s, t, textureID, textureWidth, textureHeight, colorR, colorG, colorB, alpha);
}

void AdvancedImage::drawFrameWithImageRegionWithXYWidthHeight(int x, int y, int w, int h) {
    const FrameData* fd = current();
    float sx = static_cast<float>(x + packedImageOffsetX + fd->startX);
    float sy = static_cast<float>(y + packedImageOffsetY + fd->startY);
    float u0 = sx / textureWidth, u1 = (sx + w) / textureWidth;
    float v0 = sy / textureHeight, v1 = (sy + h) / textureHeight;
    if (flipVertically) {
        // As in the original, the vertical flip drops the frame's startY.
        float base = static_cast<float>(y + packedImageOffsetY);
        v0 = (base + h) / textureHeight;
        v1 = base / textureHeight;
    }
    float rw = w * ratio, rh = h * ratio;
    float v[8], t[8];
    if (flipHorizontally) corners(v, rw, 0, 0, rh);
    else corners(v, 0, 0, rw, rh);
    corners(t, u0, v0, u1, v1);
    platform::Sprite s =
        placed(positionX, positionY, 0, 0, anchorPointX, anchorPointY, anchorPointX, anchorPointY, orientation);
    std::copy_n(v, 8, s.xy);
    sprite(serial(), s, t, textureID, textureWidth, textureHeight, colorR, colorG, colorB, alpha);
}
