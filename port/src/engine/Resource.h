// Shared, reference-counted game resources (port of the original C++
// ResourceManager / Resource / ResourceTexture / ResourcePackedImageData).
//
// Resources are identified by small integer ids handed out by the manager and
// shared by file name: creating a resource that already exists bumps its use
// count, deleteResource() drops it and frees it at zero.
#pragma once

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "platform/GL.h"

class Resource {
public:
    enum Type { kModel = 1, kTexture = 2, kSpritex = 3 };

    Resource(int id, const std::string& filename, Type type);
    virtual ~Resource() = default;

    int getResourceID() const { return id_; }
    int getStatus() const { return status_; }  // 1 = loaded
    const std::string& getFilename() const { return filename_; }
    Type getType() const { return type_; }
    int getCount() const { return count_; }
    void setCount(int c) { count_ = c; }

protected:
    int status_ = 0;
    std::string path_;  // full path, "" when the bundle has no such file

private:
    int id_;
    std::string filename_;
    Type type_;
    int count_ = 0;
};

class ResourceTexture : public Resource {
public:
    ResourceTexture(int id, const std::string& filename);
    ~ResourceTexture() override;

    GLuint getOPENGLTextureID() const { return texture_; }
    // Size of the image itself.
    int getImageWidth() const { return imageWidth_; }
    int getImageHeight() const { return imageHeight_; }
    // Size of the power-of-two texture the image sits in (top-left aligned).
    int getTextureWidth() const { return textureWidth_; }
    int getTextureHeight() const { return textureHeight_; }

    // Rasterises every vector texture again for the current display scale.
    static void refreshVectorTextures();

private:
    void load();
    // Straight RGBA `pixels` of the image at `scale` times its size, into
    // the GL texture.
    void upload(std::vector<unsigned char>& pixels, int scale, bool hasAlpha, bool mipmaps);
    void rasterizeSvg();

    GLuint texture_ = 0;
    int imageWidth_ = 0, imageHeight_ = 0;
    int textureWidth_ = 0, textureHeight_ = 0;
    std::string svg_;  // vector source, kept to rasterise again
    int svgScale_ = 0;
};

class ResourceManager {
public:
    static ResourceManager& Instance();

    // Returns the id of the texture for `name` (resolving packed sprites to
    // their atlas), or -1 if the image cannot be loaded.
    int createResourceTexture(const std::string& name);
    // Takes ownership of an already-constructed resource of another kind if
    // none with the same type and file name exists; returns its id or -1.
    template <class R>
    int createResource(const std::string& filename, Resource::Type type);

    Resource* getResource(int id) const;
    void deleteResource(int id);

private:
    int findResource(const std::string& filename, Resource::Type type) const;
    int add(Resource* r);

    std::list<Resource*> resources_;
    int nextId_ = 0;
};

template <class R>
int ResourceManager::createResource(const std::string& filename, Resource::Type type) {
    int id = findResource(filename, type);
    if (id != -1) {
        Resource* r = getResource(id);
        r->setCount(r->getCount() + 1);
        return id;
    }
    return add(new R(nextId_, filename));
}

// Atlas map from PNGpackinglist.txt: "<sprite> <atlas> x0 y0 x1 y1".
namespace ResourcePackedImageData {
// Atlas file holding `name`, or `name` itself when it is not packed.
std::string resourceGetPackedImageName(const std::string& name);
// {x, y, w, h} of `name` inside its atlas, or all zeros when not packed.
std::vector<int> resourceGetPackedImageXYWH(const std::string& name);
}  // namespace ResourcePackedImageData
