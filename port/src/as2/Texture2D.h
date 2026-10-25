// Texture2D (Apple's sample class the game draws with) and
// SingletonTextureManager (the game's textures by name, some kept across
// missions).
//
// The original drew with GL ES 1.1 calls; the port records the same draws
// (platform/DrawList.h) under the GL matrices the game sets, so textures
// are premultiplied as the original's were and drawn with
// (GL_ONE, GL_ONE_MINUS_SRC_ALPHA), its blend function. `drawColor` stands
// for the glColor4f state the original's draws used.
#pragma once

#include <map>
#include <string>

#include "core/Object.h"
#include "platform/DrawList.h"
#include "platform/GL.h"

namespace as2 {

// The colour later draws are tinted with (glColor4f).
void glColor(float r, float g, float b, float a);
const float* drawColor();

class Texture2D : public Object {
public:
    // initWithImage: of the bundle image `file` ("name.png"); nullptr when
    // it cannot be read.
    static Texture2D* initWithImageFile(const std::string& file);
    ~Texture2D() override;

    GLuint name() const { return name_; }
    // The power-of-two texture; the image fills its top-left contentSize.
    unsigned pixelsWide() const { return width_; }
    unsigned pixelsHigh() const { return height_; }
    Size contentSize() const { return size_; }
    float maxS() const { return maxS_; }
    float maxT() const { return maxT_; }
    // Tiled when drawn past its edge (texture coordinates over 1: an
    // extraction's long object). Otherwise it is clamped at its edges.
    void tile();

    // Drawing (the original's Texture2D (Drawing) category): the whole image
    // centred on a point; a rect with its own texture coordinates
    // (s0, t0, s1, t1).
    void drawAtPoint(float x, float y, platform::DrawId owner);
    void drawInRect(float x, float y, float w, float h, const float st[4], platform::DrawId owner);

private:
    Texture2D() = default;
    GLuint name_ = 0;
    Size size_;
    unsigned width_ = 0, height_ = 0;
    float maxS_ = 0, maxT_ = 0;
};

class SingletonTextureManager {
public:
    static SingletonTextureManager& sharedTextureManagerInstance();

    // The texture of a bundle image ("name", "name.png" or "name.jpg"),
    // loaded on first use; kept ones survive removeTextures.
    Texture2D* getTexture2D(const std::string& name, bool keep = false);
    void removeTextureByName(const std::string& name);
    // Releases every texture not on the keep list.
    void removeTextures();
    int getTextureListTotal() const { return static_cast<int>(textureList.size()); }
    int getKeepListTotal() const { return static_cast<int>(keepList.size()); }

private:
    std::map<std::string, Texture2D*> textureList;  // retained
    std::map<std::string, Texture2D*> keepList;     // retained
};

}  // namespace as2
