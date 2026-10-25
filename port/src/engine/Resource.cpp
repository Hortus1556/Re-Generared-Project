#include "engine/Resource.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <sstream>

#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "lunasvg.h"
#include "platform/Platform.h"
#include "stb_image.h"

// ---------------------------------------------------------------- Resource

Resource::Resource(int id, const std::string& filename, Type type)
    : path_(assets::path(filename)), id_(id), filename_(filename), type_(type) {}

// --------------------------------------------------------- ResourceTexture

ResourceTexture::ResourceTexture(int id, const std::string& filename) : Resource(id, filename, kTexture) {
    if (!path_.empty()) load();
}

namespace {
std::vector<ResourceTexture*> gVectorTextures;
}  // namespace

ResourceTexture::~ResourceTexture() {
    gVectorTextures.erase(std::remove(gVectorTextures.begin(), gVectorTextures.end(), this), gVectorTextures.end());
    platform::releaseTexture(texture_);
}

namespace {
int nextPowerOfTwo(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
}  // namespace

// Port of ResourceTexture::loadNonPVRTexture. The original drew the image
// into a premultiplied CoreGraphics bitmap and then divided the colour back by
// alpha, i.e. the texture holds straight (non-premultiplied) RGBA with the
// image in the top-left corner of a power-of-two texture.
//
// The original also halved the image until its texture fit in 1024 (an iPad
// limit) while texture coordinates kept dividing image pixels by that halved
// size, and the art compensates (the lose screen's face is drawn at ratio 2).
// The port keeps the image at full resolution in a texture of its full
// power-of-two size and reports the halved size as getTextureWidth/Height:
// texture coordinates then land on the same image points, with all the
// detail. Only beyond GL_MAX_TEXTURE_SIZE is the image still halved.
//
// Sharper versions of an image are loaded in its place, reported at the
// image's own sizes (so frame sizes and texture coordinates are unchanged)
// with more texels behind them:
// - images-svg/<name>.svg: its vector source (the Christmas game's art from
//   the Flash original, tools/xmas_flash.py), rasterised at a whole multiple
//   of the image's size that matches the display's resolution, and again
//   when that changes (refreshVectorTextures);
// - images-hd/<name>: the same picture at a whole multiple of its size
//   (AS1's art at 2x, a faithful upscale at 4x: tools/xmas_hd.py,
//   tools/xmas_upscale.py), the multiple found from the original's size.
//
// The assets are standard PNGs (tools/extract_ipa.py converts Apple's CgBI
// ones); CgBI files from an older extraction still load, stb_image undoing
// their BGRA order and premultiplied alpha.
void ResourceTexture::load() {
    std::vector<unsigned char> file;
    if (assets::read("images-svg/" + getFilename() + ".svg", file)) {
        svg_.assign(file.begin(), file.end());
        rasterizeSvg();
        if (status_ == 1) {
            if (gVectorTextures.empty()) platform::onDisplayScaleChange(&ResourceTexture::refreshVectorTextures);
            gVectorTextures.push_back(this);
            return;
        }
        svg_.clear();
    }
    int scale = 1;
    if (assets::read("images-hd/" + getFilename(), file)) {
        std::vector<unsigned char> original;
        int ow = 0, oh = 0, oc = 0, hw = 0, hh = 0, hc = 0;
        if (assets::read(getFilename(), original) &&
            stbi_info_from_memory(original.data(), static_cast<int>(original.size()), &ow, &oh, &oc) &&
            stbi_info_from_memory(file.data(), static_cast<int>(file.size()), &hw, &hh, &hc) && ow > 0 &&
            hw % ow == 0 && hh == oh * (hw / ow))
            scale = hw / ow;
        if (scale < 2) {
            SDL_Log("images-hd/%s is not a whole multiple of the image; using the image", getFilename().c_str());
            scale = 1;
            file = std::move(original);
        }
    } else if (!assets::read(getFilename(), file)) {
        return;
    }

    stbi_convert_iphone_png_to_rgb_thread(1);
    stbi_set_unpremultiply_on_load_thread(1);
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(file.data(), static_cast<int>(file.size()), &w, &h, &comp, 4);
    if (!data) {
        SDL_Log("Fail %s: %s", getFilename().c_str(), stbi_failure_reason());
        return;
    }
    std::vector<unsigned char> pixels(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    imageWidth_ = w / scale;
    imageHeight_ = h / scale;
    upload(pixels, scale, comp == 4 || comp == 2, false);
}

// The vector source at the smallest whole multiple of the image's size that
// has at least as many pixels as the display shows it with (the game's art
// scale times the display scale), within the texture size limit.
void ResourceTexture::rasterizeSvg() {
    auto doc = lunasvg::Document::loadFromData(svg_);
    if (!doc) return;
    imageWidth_ = static_cast<int>(doc->width() + 0.5f);
    imageHeight_ = static_cast<int>(doc->height() + 0.5f);
    if (imageWidth_ <= 0 || imageHeight_ <= 0) return;
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    float wanted = platform::game().artScale * platform::displayScale();
    int scale = std::max(1, static_cast<int>(std::ceil(wanted - 0.05f)));
    while (scale > 1 && nextPowerOfTwo(imageWidth_) * scale > maxSize) --scale;
    while (scale > 1 && nextPowerOfTwo(imageHeight_) * scale > maxSize) --scale;
    if (scale == svgScale_ && status_ == 1) return;
    lunasvg::Bitmap bitmap = doc->renderToBitmap(imageWidth_ * scale, imageHeight_ * scale, 0x00000000);
    if (bitmap.isNull()) return;
    bitmap.convertToRGBA();  // straight alpha, RGBA byte order
    std::vector<unsigned char> pixels(static_cast<size_t>(bitmap.width()) * bitmap.height() * 4);
    for (int y = 0; y < bitmap.height(); ++y)
        std::copy_n(bitmap.data() + static_cast<size_t>(y) * bitmap.stride(), static_cast<size_t>(bitmap.width()) * 4,
                    pixels.data() + static_cast<size_t>(y) * bitmap.width() * 4);
    svgScale_ = scale;
    upload(pixels, scale, true, true);
}

void ResourceTexture::refreshVectorTextures() {
    for (ResourceTexture* t : gVectorTextures) t->rasterizeSvg();
}

// The image (imageWidth_ x imageHeight_ at `scale` times) goes in the
// top-left corner of a texture `scale` times the power-of-two size reported.
void ResourceTexture::upload(std::vector<unsigned char>& pixels, int scale, bool hasAlpha, bool mipmaps) {
    const int w = imageWidth_ * scale, h = imageHeight_ * scale;
    const int potW = nextPowerOfTwo(imageWidth_), potH = nextPowerOfTwo(imageHeight_);
    int originalShift = 0;
    while ((potW >> originalShift) > 1024 || (potH >> originalShift) > 1024) ++originalShift;
    textureWidth_ = potW >> originalShift;
    textureHeight_ = potH >> originalShift;
    int tw = potW * scale, th = potH * scale;
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    int shift = 0;
    while (maxSize > 0 && ((tw >> shift) > maxSize || (th >> shift) > maxSize)) ++shift;
    tw >>= shift;
    th >>= shift;

    std::vector<unsigned char> buf(static_cast<size_t>(tw) * th * 4, 0);
    int step = 1 << shift;
    for (int y = 0; y < h / step && y < th; ++y)
        for (int x = 0; x < w / step && x < tw; ++x)
            std::copy_n(pixels.data() + (static_cast<size_t>(y * step) * w + x * step) * 4, 4,
                        buf.data() + (static_cast<size_t>(y) * tw + x) * 4);

    // Sprites drawn off the pixel grid (odd sizes centred, scaled, turned)
    // sample half a texel past the image: repeat its last column and row
    // into the padding and clamp at the texture edge, so those samples see
    // the image's own edge instead of transparent black (which showed as
    // thin lines along the bottom and right edges).
    int iw = std::min(w / step, tw), ih = std::min(h / step, th);
    auto texel = [&](int x, int y) { return buf.data() + (static_cast<size_t>(y) * tw + x) * 4; };
    if (iw < tw)
        for (int y = 0; y < ih; ++y) std::copy_n(texel(iw - 1, y), 4, texel(iw, y));
    if (ih < th) std::copy_n(texel(0, ih - 1), static_cast<size_t>(std::min(iw + 1, tw)) * 4, texel(0, ih));

    if (!texture_) glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Vector art is rasterised for the display's resolution: mipmaps keep
    // it smooth where it is drawn smaller.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmaps ? GL_TRUE : GL_FALSE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, hasAlpha ? GL_RGBA8 : GL_RGB8, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
    status_ = 1;
}

// --------------------------------------------------------- ResourceManager

ResourceManager& ResourceManager::Instance() {
    static ResourceManager instance;
    return instance;
}

int ResourceManager::findResource(const std::string& filename, Resource::Type type) const {
    for (Resource* r : resources_)
        if (r->getType() == type && r->getFilename() == filename) return r->getResourceID();
    return -1;
}

int ResourceManager::add(Resource* r) {
    if (r->getStatus() != 1) {
        delete r;
        return -1;
    }
    r->setCount(1);
    resources_.push_back(r);
    return nextId_++;
}

int ResourceManager::createResourceTexture(const std::string& name) {
    return createResource<ResourceTexture>(ResourcePackedImageData::resourceGetPackedImageName(name),
                                           Resource::kTexture);
}

Resource* ResourceManager::getResource(int id) const {
    for (Resource* r : resources_)
        if (r->getResourceID() == id) return r;
    return nullptr;
}

void ResourceManager::deleteResource(int id) {
    Resource* r = getResource(id);
    if (!r) return;
    r->setCount(r->getCount() - 1);
    if (r->getCount() == 0) {
        resources_.remove(r);
        delete r;
    }
}

// ------------------------------------------------ ResourcePackedImageData

namespace ResourcePackedImageData {
namespace {

struct Entry {
    std::string atlas;
    int x0, y0, x1, y1;
};

const std::unordered_map<std::string, Entry>& table() {
    static const std::unordered_map<std::string, Entry> t = [] {
        std::unordered_map<std::string, Entry> m;
        std::string text;
        if (!assets::readText("PNGpackinglist.txt", text)) return m;
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            std::istringstream f(line);
            std::string name;
            Entry e{};
            if (f >> name >> e.atlas >> e.x0 >> e.y0 >> e.x1 >> e.y1) m[name] = e;
        }
        // The list makes the pause board's two help notes 255 pixels wide,
        // into their neighbours on the sheet; the notes (and the bundle's
        // standalone copies of them) are 225.
        for (const char* note : {"ipad_help_pause_off.png", "ipad_help_pause_on.png"}) {
            auto it = m.find(note);
            if (it != m.end() && it->second.x1 - it->second.x0 == 255) it->second.x1 = it->second.x0 + 225;
        }
        return m;
    }();
    return t;
}

}  // namespace

std::string resourceGetPackedImageName(const std::string& name) {
    auto it = table().find(name);
    return it == table().end() ? name : it->second.atlas;
}

std::vector<int> resourceGetPackedImageXYWH(const std::string& name) {
    auto it = table().find(name);
    if (it == table().end()) return {0, 0, 0, 0};
    const Entry& e = it->second;
    return {e.x0, e.y0, e.x1 - e.x0, e.y1 - e.y0};
}

}  // namespace ResourcePackedImageData
