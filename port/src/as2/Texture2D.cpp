#include "as2/Texture2D.h"

#include <algorithm>
#include <vector>

#include "platform/Assets.h"
#include "platform/Platform.h"
#include "platform/ImageFile.h"

namespace as2 {

namespace {
float gColor[4] = {1, 1, 1, 1};

unsigned pow2(unsigned n) {
    unsigned p = 1;
    while (p < n) p <<= 1;
    return p;
}
}  // namespace

void glColor(float r, float g, float b, float a) {
    gColor[0] = r;
    gColor[1] = g;
    gColor[2] = b;
    gColor[3] = a;
}

const float* drawColor() { return gColor; }

Texture2D* Texture2D::initWithImageFile(const std::string& file) {
    std::vector<unsigned char> bytes;
    if (!assets::read(file, bytes)) return nullptr;
    int w = 0, h = 0;
    std::vector<unsigned char> pixels;
    if (!platform::decodeImage(bytes.data(), bytes.size(), w, h, pixels)) return nullptr;
    const unsigned char* px = pixels.data();
    // initWithImage: the image in the top-left of a power-of-two texture,
    // premultiplied (the CGBitmapContext it was drawn into). Opaque images
    // were kept as RGB565 to save memory on the device; the port keeps all
    // eight bits a channel. Textures over 2048 were halved until they fit.
    unsigned tw = pow2(static_cast<unsigned>(w)), th = pow2(static_cast<unsigned>(h));
    float cw = static_cast<float>(w), ch = static_cast<float>(h);
    int shift = 0;
    while (tw > 2048 || th > 2048) {
        tw >>= 1;
        th >>= 1;
        cw *= 0.5f;
        ch *= 0.5f;
        ++shift;
    }
    std::vector<unsigned char> tex(static_cast<size_t>(tw) * th * 4, 0);
    for (unsigned y = 0; y < th; ++y)
        for (unsigned x = 0; x < tw; ++x) {
            unsigned sx = x << shift, sy = y << shift;
            if (sx >= static_cast<unsigned>(w) || sy >= static_cast<unsigned>(h)) continue;
            const unsigned char* s = px + (static_cast<size_t>(sy) * w + sx) * 4;
            unsigned char* d = tex.data() + (static_cast<size_t>(y) * tw + x) * 4;
            unsigned a = s[3];
            d[0] = static_cast<unsigned char>((s[0] * a + 127) / 255);
            d[1] = static_cast<unsigned char>((s[1] * a + 127) / 255);
            d[2] = static_cast<unsigned char>((s[2] * a + 127) / 255);
            d[3] = static_cast<unsigned char>(a);
        }
    Texture2D* t = new Texture2D;
    glGenTextures(1, &t->name_);
    glBindTexture(GL_TEXTURE_2D, t->name_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Clamped at its edges. (The original left GL's default, repeating, so
    // the filtering at a quad's edge blended in the picture's opposite edge:
    // faint lines along the HUD dial's right and bottom edges, from the
    // dial's rim and the score card.) Pictures drawn tiled say so: tile().
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(tw), static_cast<GLsizei>(th), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, tex.data());
    t->size_ = {cw, ch};
    t->width_ = tw;
    t->height_ = th;
    t->maxS_ = cw / tw;
    t->maxT_ = ch / th;
    return t;
}

void Texture2D::tile() {
    glBindTexture(GL_TEXTURE_2D, name_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

Texture2D::~Texture2D() {
    if (name_) platform::releaseTexture(name_);
}

void Texture2D::drawAtPoint(float x, float y, platform::DrawId owner) {
    float w = width_ * maxS_, h = height_ * maxT_;
    platform::Sprite s;
    s.pos[0] = x;
    s.pos[1] = y;
    float xy[8] = {-w / 2, -h / 2, w / 2, -h / 2, -w / 2, h / 2, w / 2, h / 2};
    std::copy(xy, xy + 8, s.xy);
    float uv[8] = {0, 0, maxS_, 0, 0, maxT_, maxS_, maxT_};
    platform::drawSprite(owner, name_, s, uv, gColor, GL_ONE);
}

void Texture2D::drawInRect(float x, float y, float w, float h, const float st[4], platform::DrawId owner) {
    platform::Sprite s;
    s.pos[0] = x;
    s.pos[1] = y;
    float xy[8] = {0, 0, w, 0, 0, h, w, h};
    std::copy(xy, xy + 8, s.xy);
    float uv[8] = {st[0], st[1], st[2], st[1], st[0], st[3], st[2], st[3]};
    platform::drawSprite(owner, name_, s, uv, gColor, GL_ONE);
}

SingletonTextureManager& SingletonTextureManager::sharedTextureManagerInstance() {
    static SingletonTextureManager instance;
    return instance;
}

Texture2D* SingletonTextureManager::getTexture2D(const std::string& name, bool keep) {
    std::string key = name, file = name + ".png";
    auto ends = [&](const char* suffix) {
        size_t n = std::char_traits<char>::length(suffix);
        return name.size() >= n && name.compare(name.size() - n, n, suffix) == 0;
    };
    if (ends(".png") || ends(".jpg")) {
        key = name.substr(0, name.size() - 4);
        file = name;
    }
    Texture2D* t = nullptr;
    auto it = textureList.find(key);
    if (it != textureList.end()) {
        t = it->second;
    } else {
        t = Texture2D::initWithImageFile(file);
        if (!t) return nullptr;
        textureList[key] = t;  // the list keeps the +1
        // (For reading AS_FILM logs, whose draws name only the GL texture.)
        platform::debugTrace("texture %u %s", t->name(), key.c_str());
    }
    if (keep && !keepList.count(key)) keepList[key] = retainRef(t);
    return t;
}

void SingletonTextureManager::removeTextureByName(const std::string& name) {
    auto it = textureList.find(name);
    if (it == textureList.end()) return;
    releaseRef(it->second);
    textureList.erase(it);
}

void SingletonTextureManager::removeTextures() {
    std::vector<std::string> names;
    for (const auto& kv : textureList)
        if (!keepList.count(kv.first)) names.push_back(kv.first);
    for (const std::string& n : names) removeTextureByName(n);
}

}  // namespace as2
