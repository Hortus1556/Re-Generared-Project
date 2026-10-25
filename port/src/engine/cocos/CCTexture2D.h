// cocos2d-x 2.1.1's CCTexture2D and CCTextureCache, ported to Bleed Engine:
// pictures decoded by platform::decodeImage (PNG, JPEG, PVR/CCZ), uploaded
// whole (GL_LINEAR, clamped), released through the engine's recorded drawing
// once no shown frame uses them. A texture's content size is in points: its
// pixels over its content scale (its art folder's, "3.5x/", else the
// director's content scale factor; getContentScale).
// Premultiplied as on iOS: a PNG or JPEG is premultiplied when loaded (iOS's
// CCImage drew it into a premultiplied bitmap); a PVR is as stored, and
// premultiplied if the game says its PVRs are (PVRImagesHavePremultipliedAlpha).
// Sprites then blend as 2.1.1 chose for them (CCSprite::updateBlendFunc).
// The port's: a sheet's texture (its size known from its sprite sheet) is
// decoded on worker threads, the texture standing in at once and uploaded
// when first drawn or when the screen manager shows the screen; textures
// taken off the cache are kept a while (CCTextureCache below).
// MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include <future>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/cocos/CCObject.h"
#include "engine/cocos/ccTypes.h"

namespace cocos2d {

struct Decoded;  // (CCTexture2D.cpp)

class CCTexture2D : public CCObject {
public:
    ~CCTexture2D() override;
    // RGBA, top row first
    bool initWithPixels(int width, int height, const std::vector<unsigned char>& rgba, bool premultiplied);
    static void PVRImagesHavePremultipliedAlpha(bool have);
    // (a texture still being decoded is finished first: waited for, uploaded)
    GLuint getName() {
        if (m_pending) finishDecoding();
        return m_uName;
    }
    // (the port's) Whether its picture is still being decoded, and whether
    // that is done (finishDecoding then only uploads it).
    bool isDecoding() const { return m_pending != nullptr; }
    bool decodingDone() const;
    void finishDecoding();
    unsigned int getPixelsWide() const { return m_uPixelsWide; }
    unsigned int getPixelsHigh() const { return m_uPixelsHigh; }
    CCSize getContentSize();  // points
    const CCSize& getContentSizeInPixels() { return m_tContentSize; }
    // (the port's) Its pixels a point. A picture from a folder of art drawn
    // for one content scale ("3.5x/...": AS3's Hospital, from the Android
    // build) has that folder's; any other the director's content scale
    // factor (2.1.1 had one scale for every texture). Sprite frames, sprites
    // and particles turn its pixels into points by it.
    float getContentScale() const;
    static float folderScale(const std::string& path);  // "…/3.5x/…" -> 3.5; 0 if none
    GLfloat getMaxS() { return m_fMaxS; }
    GLfloat getMaxT() { return m_fMaxT; }
    bool hasPremultipliedAlpha() { return m_bHasPremultipliedAlpha; }
    void setAntiAliasTexParameters();
    void setAliasTexParameters();
    // the picture's own pixels, while kept (for hit tests on the art)
    const std::vector<unsigned char>& pixels() const { return m_pixels; }
    void keepPixels(std::vector<unsigned char> rgba) { m_pixels = std::move(rgba); }
    // (the port's) Its alpha at a pixel (x right, y down from the top left),
    // by cells of kAlphaCell pixels square (each its most opaque pixel's),
    // kept for a file's picture (CCTextureCache) for hit tests on the art:
    // 0..255, 0 off the picture, -1 if not kept (CCSprite::opaqueAt).
    static constexpr int kAlphaCell = 4;
    int alphaAt(float x, float y) const;

private:
    friend class CCTextureCache;
    GLuint m_uName = 0;
    // its decoding under way (the port's), and the file's name
    std::shared_ptr<std::shared_future<std::shared_ptr<Decoded>>> m_pending;
    std::string m_pendingPath;
    unsigned int m_uPixelsWide = 0, m_uPixelsHigh = 0;
    CCSize m_tContentSize;  // pixels
    GLfloat m_fMaxS = 1, m_fMaxT = 1;
    bool m_bHasPremultipliedAlpha = false;
    float m_fContentScale = 0;  // its folder's scale; 0: the director's
    std::vector<unsigned char> m_pixels;
    std::vector<unsigned char> m_alpha;  // (alphaAt)
    int m_alphaW = 0, m_alphaH = 0;
};

class CCTextureCache : public CCObject {
public:
    static CCTextureCache* sharedTextureCache();
    // the file's texture, loaded once (by its full path; nullptr if missing)
    CCTexture2D* addImage(const char* path);
    // (the port's) The same for a picture whose size in pixels is known (a
    // sprite sheet's): decoded on worker threads, the texture given at once
    // and uploaded when first drawn (CCTexture2D::getName) or finished.
    CCTexture2D* addImage(const char* path, int width, int height);
    // Whether every texture's decoding is done; all of them uploaded
    // (waited for if not done); those done uploaded for some `seconds` (at
    // least one), and whether none is left.
    bool decodingsDone();
    void finishDecodings();
    bool uploadDecoded(double seconds);
    // A picture given as its file's bytes (PNG, JPEG, PVR), cached under
    // `key` (its full path if it names a file, else as given): 2.1.1's
    // addUIImage with a CCImage, as a particle system's embedded texture.
    CCTexture2D* addImageData(const unsigned char* bytes, size_t size, const char* key);
    CCTexture2D* textureForKey(const char* key);
    void removeAllTextures();
    void removeUnusedTextures();
    void removeTexture(CCTexture2D* texture);
    void removeTextureForKey(const char* key);

private:
    CCTexture2D* revive(const std::string& full);
    void retire(const std::string& full, CCTexture2D* t);
    static size_t textureBytes(CCTexture2D* t);
    CCTexture2D* upload(const std::string& full, Decoded& d, const char* how);

    std::map<std::string, CCTexture2D*> m_textures;  // retained
    // (the port's) Taken off the cache but kept, oldest first, up to a
    // budget of their pixels: a file asked for again gets its texture back
    // with no decoding. Not seen by textureForKey, as 2.1.1's cache had them
    // gone.
    std::list<std::pair<std::string, CCTexture2D*>> m_retired;  // retained
    size_t m_retiredBytes = 0;
};

}  // namespace cocos2d
