// cocos2d-x 2.1.1's CCTexture2D.cpp and CCTextureCache.cpp, ported.
// MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCTexture2D.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>

#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCFileUtils.h"
#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/ImageFile.h"
#include "platform/Platform.h"

namespace cocos2d {

CCTexture2D::~CCTexture2D() {
    if (m_uName) platform::releaseTexture(m_uName);
}

int CCTexture2D::alphaAt(float x, float y) const {
    if (m_alpha.empty()) return -1;
    if (x < 0 || y < 0) return 0;
    const int cx = static_cast<int>(x) / kAlphaCell, cy = static_cast<int>(y) / kAlphaCell;
    if (cx >= m_alphaW || cy >= m_alphaH) return 0;
    return m_alpha[static_cast<size_t>(cy) * m_alphaW + cx];
}

static bool PVRHaveAlphaPremultiplied_ = false;
void CCTexture2D::PVRImagesHavePremultipliedAlpha(bool have) { PVRHaveAlphaPremultiplied_ = have; }

namespace {

// (Debug, AS_PROFILE: each texture made is traced with its time.)
bool profiling() {
    static const bool on = platform::debugMode() && std::getenv("AS_PROFILE");
    return on;
}

}  // namespace

// A picture's file decoded: straight RGBA, or premultiplied as stored.
struct Decoded {
    bool ok = false;
    int w = 0, h = 0;
    std::vector<unsigned char> rgba;
    bool premultiplied = false;
    double ms = 0;  // (the decoding's time, for AS_PROFILE)
    // (the port's) its alpha by cells of kAlphaCell pixels square, each the
    // most opaque pixel of its cell: what a hit test on the art reads
    // (CCTexture2D::alphaAt), a 64th of the picture's bytes
    std::vector<unsigned char> alpha;
    int alphaW = 0, alphaH = 0;
};

namespace {

Decoded decode(const unsigned char* bytes, size_t size) {
    Decoded d;
    const double t0 = platform::now();
    bool stored = false;
    if (!platform::decodeImage(bytes, size, d.w, d.h, d.rgba, &stored)) return d;
    if (platform::isPvr(bytes, size)) {
        d.premultiplied = stored || PVRHaveAlphaPremultiplied_;  // as stored
    } else {
        // as iOS's CCImage drew it: into a premultiplied bitmap
        for (size_t i = 0; i < d.rgba.size(); i += 4) {
            unsigned a = d.rgba[i + 3];
            for (int c = 0; c < 3; ++c) d.rgba[i + c] = static_cast<unsigned char>((d.rgba[i + c] * a + 127) / 255);
        }
        d.premultiplied = true;
    }
    d.ok = true;
    const int c = CCTexture2D::kAlphaCell;
    d.alphaW = (d.w + c - 1) / c;
    d.alphaH = (d.h + c - 1) / c;
    d.alpha.assign(static_cast<size_t>(d.alphaW) * d.alphaH, 0);
    for (int y = 0; y < d.h; ++y) {
        unsigned char* row = d.alpha.data() + static_cast<size_t>(y / c) * d.alphaW;
        const unsigned char* px = d.rgba.data() + static_cast<size_t>(y) * d.w * 4 + 3;
        for (int x = 0; x < d.w; ++x, px += 4)
            if (*px > row[x / c]) row[x / c] = *px;
    }
    d.ms = (platform::now() - t0) * 1000;
    return d;
}

// (also on a worker thread: `full` is resolved already, and assets::read
// keeps no state, where CCFileUtils's lookups are cached)
Decoded decodeFile(const std::string& full) {
    std::vector<unsigned char> bytes;
    if (!assets::read(full, bytes)) return Decoded{};
    return decode(bytes.data(), bytes.size());
}

}  // namespace


bool CCTexture2D::initWithPixels(int width, int height, const std::vector<unsigned char>& rgba, bool premultiplied) {
    m_bHasPremultipliedAlpha = premultiplied;
    glGenTextures(1, &m_uName);
    glBindTexture(GL_TEXTURE_2D, m_uName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    m_uPixelsWide = width;
    m_uPixelsHigh = height;
    m_tContentSize = CCSizeMake(width, height);
    m_fMaxS = m_fMaxT = 1;  // the whole picture: no padding to a power of two
    return true;
}

CCSize CCTexture2D::getContentSize() {
    float s = getContentScale();
    return CCSizeMake(m_tContentSize.width / s, m_tContentSize.height / s);
}

float CCTexture2D::getContentScale() const {
    return m_fContentScale > 0 ? m_fContentScale : CCDirector::sharedDirector()->getContentScaleFactor();
}

float CCTexture2D::folderScale(const std::string& path) {
    // the last folder named "<number>x" in the path
    float scale = 0;
    size_t start = 0;
    while (start < path.size()) {
        size_t slash = path.find('/', start);
        if (slash == std::string::npos) break;
        std::string dir = path.substr(start, slash - start);
        if (dir.size() >= 2 && dir.back() == 'x') {
            char* end = nullptr;
            float v = std::strtof(dir.c_str(), &end);
            if (end == dir.c_str() + dir.size() - 1 && v > 0) scale = v;
        }
        start = slash + 1;
    }
    return scale;
}

void CCTexture2D::setAntiAliasTexParameters() {
    glBindTexture(GL_TEXTURE_2D, getName());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void CCTexture2D::setAliasTexParameters() {
    glBindTexture(GL_TEXTURE_2D, getName());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// ------------------------------------------------------------ the cache

CCTextureCache* CCTextureCache::sharedTextureCache() {
    static CCTextureCache* c = new CCTextureCache;
    return c;
}

// A texture taken off the cache is kept a while (the port's): the game
// unloads its sheets as a screen goes and loads them again as the next
// comes in, and iOS read its PVRTC sheets straight into the GPU where the
// PC decodes them on the CPU (some 50 ms for a 4096x4096 sheet, 64 MB). Kept
// up to a sixteenth of the machine's memory, from 64 MB to 1 GB: AS3's menus
// use some 900 MB of sheets.
static size_t retiredBudget() {
    static const size_t budget = [] {
        size_t mb = static_cast<size_t>(std::max(0, platform::systemMemoryMB())) / 16;
        return std::min<size_t>(1024, std::max<size_t>(64, mb)) << 20;
    }();
    return budget;
}

size_t CCTextureCache::textureBytes(CCTexture2D* t) {
    return static_cast<size_t>(t->getPixelsWide()) * t->getPixelsHigh() * 4;
}

CCTexture2D* CCTextureCache::revive(const std::string& full) {
    for (auto it = m_retired.begin(); it != m_retired.end(); ++it)
        if (it->first == full) {
            CCTexture2D* t = it->second;
            m_retiredBytes -= textureBytes(t);
            m_retired.erase(it);
            m_textures[full] = t;  // the kept reference is the cache's again
            if (profiling()) platform::debugTrace("texture %s: kept", full.c_str());
            return t;
        }
    return nullptr;
}

void CCTextureCache::retire(const std::string& full, CCTexture2D* t) {
    m_retired.emplace_back(full, t);
    m_retiredBytes += textureBytes(t);
    while (m_retiredBytes > retiredBudget() && !m_retired.empty()) {
        CCTexture2D* old = m_retired.front().second;
        m_retiredBytes -= textureBytes(old);
        m_retired.pop_front();
        old->release();
    }
}

CCTexture2D* CCTextureCache::upload(const std::string& full, Decoded& d, const char* how) {
    const double t0 = platform::now();
    CCTexture2D* t = new CCTexture2D;
    t->initWithPixels(d.w, d.h, d.rgba, d.premultiplied);
    t->m_alpha = std::move(d.alpha);
    t->m_alphaW = d.alphaW;
    t->m_alphaH = d.alphaH;
    t->m_fContentScale = CCTexture2D::folderScale(full);
    m_textures[full] = t;  // the cache's reference
    if (profiling())
        platform::debugTrace("texture %s %dx%d: %s %.1f ms, upload %.1f ms", full.c_str(), d.w, d.h, how, d.ms,
                             (platform::now() - t0) * 1000);
    return t;
}

bool CCTexture2D::decodingDone() const {
    return !m_pending || m_pending->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void CCTexture2D::finishDecoding() {
    if (!m_pending) return;
    const double t0 = platform::now();
    const bool waited = !decodingDone();
    std::shared_ptr<Decoded> d = m_pending->get();
    m_pending.reset();
    const double t1 = platform::now();
    if (!d->ok || d->w != static_cast<int>(m_uPixelsWide) || d->h != static_cast<int>(m_uPixelsHigh)) {
        // (the sprite sheet's size was wrong: nothing drawn)
        platform::debugTrace("image not found or not its sheet's size: %s", m_pendingPath.c_str());
        return;
    }
    if (d->premultiplied != m_bHasPremultipliedAlpha)
        platform::debugTrace("image %s: premultiplied %d, foreseen %d", m_pendingPath.c_str(), d->premultiplied,
                             m_bHasPremultipliedAlpha);
    initWithPixels(d->w, d->h, d->rgba, m_bHasPremultipliedAlpha);
    m_alpha = std::move(d->alpha);
    m_alphaW = d->alphaW;
    m_alphaH = d->alphaH;
    if (profiling())
        platform::debugTrace("texture %s %dx%d: decoded %.1f ms on a worker, %s%.1f ms, upload %.1f ms",
                             m_pendingPath.c_str(), d->w, d->h, d->ms, waited ? "WAITED " : "waited ",
                             (t1 - t0) * 1000, (platform::now() - t1) * 1000);
}

CCTexture2D* CCTextureCache::addImage(const char* path, int width, int height) {
    if (!path) return nullptr;
    std::string full = CCFileUtils::sharedFileUtils()->fullPathForFilename(path);
    auto it = m_textures.find(full);
    if (it != m_textures.end()) return it->second;
    if (CCTexture2D* t = revive(full)) return t;
    // Premultiplied as it will be: a PNG or JPEG always; a PVR as the game
    // says its PVRs are (one stored premultiplied otherwise can't be foreseen:
    // decoded at once).
    const bool pvr = full.find(".pvr") != std::string::npos;
    if (width <= 0 || height <= 0 || (pvr && !PVRHaveAlphaPremultiplied_) ||
        !CCFileUtils::sharedFileUtils()->isFileExist(full))
        return addImage(path);
    CCTexture2D* t = new CCTexture2D;
    t->m_uPixelsWide = width;
    t->m_uPixelsHigh = height;
    t->m_tContentSize = CCSizeMake(width, height);
    t->m_bHasPremultipliedAlpha = true;
    t->m_fContentScale = CCTexture2D::folderScale(full);
    t->m_pending = std::make_shared<std::shared_future<std::shared_ptr<Decoded>>>(
        std::async(std::launch::async, [full] { return std::make_shared<Decoded>(decodeFile(full)); }).share());
    t->m_pendingPath = full;
    m_textures[full] = t;  // the cache's reference
    return t;
}

bool CCTextureCache::decodingsDone() {
    for (auto& t : m_textures)
        if (!t.second->decodingDone()) return false;
    return true;
}

void CCTextureCache::finishDecodings() {
    for (auto& t : m_textures) t.second->finishDecoding();
}

bool CCTextureCache::uploadDecoded(double seconds) {
    const double end = platform::now() + seconds;
    bool left = false;
    for (auto& t : m_textures) {
        if (!t.second->isDecoding()) continue;
        if (t.second->decodingDone() && platform::now() < end) t.second->finishDecoding();
        else left = true;
    }
    return !left;
}

CCTexture2D* CCTextureCache::addImage(const char* path) {
    if (!path) return nullptr;
    std::string full = CCFileUtils::sharedFileUtils()->fullPathForFilename(path);
    auto it = m_textures.find(full);
    if (it != m_textures.end()) return it->second;  // (still decoding: drawn when done)
    if (CCTexture2D* t = revive(full)) return t;
    Decoded d = decodeFile(full);
    if (!d.ok) {
        platform::debugTrace("image not found: %s", path);
        return nullptr;
    }
    return upload(full, d, "decode");
}

CCTexture2D* CCTextureCache::addImageData(const unsigned char* bytes, size_t size, const char* key) {
    if (!bytes || !key) return nullptr;
    std::string full = CCFileUtils::sharedFileUtils()->fullPathForFilename(key);
    if (full.empty()) full = key;
    auto it = m_textures.find(full);
    if (it != m_textures.end()) return it->second;
    if (CCTexture2D* t = revive(full)) return t;
    Decoded d = decode(bytes, size);
    if (!d.ok) return nullptr;
    return upload(full, d, "decode");
}

CCTexture2D* CCTextureCache::textureForKey(const char* key) {
    auto it = m_textures.find(CCFileUtils::sharedFileUtils()->fullPathForFilename(key));
    return it == m_textures.end() ? nullptr : it->second;
}

void CCTextureCache::removeAllTextures() {
    // (the kept ones too)
    for (auto& t : m_textures) t.second->release();
    m_textures.clear();
    for (auto& r : m_retired) r.second->release();
    m_retired.clear();
    m_retiredBytes = 0;
}

void CCTextureCache::removeUnusedTextures() {
    // the ones only the cache holds (kept a while: retire)
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (it->second->retainCount() == 1) {
            retire(it->first, it->second);
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }
}

void CCTextureCache::removeTexture(CCTexture2D* texture) {
    for (auto it = m_textures.begin(); it != m_textures.end(); ++it)
        if (it->second == texture) {
            retire(it->first, texture);
            m_textures.erase(it);
            return;
        }
}

void CCTextureCache::removeTextureForKey(const char* key) {
    auto it = m_textures.find(CCFileUtils::sharedFileUtils()->fullPathForFilename(key));
    if (it != m_textures.end()) {
        retire(it->first, it->second);
        m_textures.erase(it);
    }
}

}  // namespace cocos2d
