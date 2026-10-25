// cocos2d-x 2.1.1's CCSpriteFrame.cpp and CCSpriteFrameCache.cpp, ported.
// MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCSpriteFrameCache.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/Json.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCFileUtils.h"
#include "platform/Platform.h"

namespace cocos2d {

// "{{x,y},{w,h}}", "{x,y}", "{w,h}": the numbers in order
static std::vector<float> numbers(const std::string& s) {
    std::vector<float> out;
    const char* p = s.c_str();
    while (*p) {
        if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.') {
            char* end;
            out.push_back(std::strtof(p, &end));
            p = end;
        } else {
            ++p;
        }
    }
    return out;
}

CCRect CCRectFromString(const std::string& s) {
    std::vector<float> n = numbers(s);
    return n.size() >= 4 ? CCRectMake(n[0], n[1], n[2], n[3]) : CCRectZero;
}
CCPoint CCPointFromString(const std::string& s) {
    std::vector<float> n = numbers(s);
    return n.size() >= 2 ? ccp(n[0], n[1]) : CCPointZero;
}
CCSize CCSizeFromString(const std::string& s) {
    std::vector<float> n = numbers(s);
    return n.size() >= 2 ? CCSizeMake(n[0], n[1]) : CCSizeZero;
}

// ------------------------------------------------------------- frames

static float scale() { return CCDirector::sharedDirector()->getContentScaleFactor(); }
// a texture's own pixels a point (CCTexture2D::getContentScale: its art folder's)
static float scale(CCTexture2D* texture) { return texture ? texture->getContentScale() : scale(); }

CCSpriteFrame* CCSpriteFrame::create(const char* filename, const CCRect& rect) {
    return createWithTexture(CCTextureCache::sharedTextureCache()->addImage(filename), rect);
}

CCSpriteFrame* CCSpriteFrame::createWithTexture(CCTexture2D* texture, const CCRect& rect) {
    // rect in points
    const float s = scale(texture);
    CCRect px = CCRectMake(rect.origin.x * s, rect.origin.y * s, rect.size.width * s, rect.size.height * s);
    return createWithTexture(texture, px, false, CCPointZero, px.size);
}

CCSpriteFrame* CCSpriteFrame::createWithTexture(CCTexture2D* texture, const CCRect& rect, bool rotated,
                                                const CCPoint& offset, const CCSize& originalSize) {
    CCSpriteFrame* f = new CCSpriteFrame;
    f->initWithTexture(texture, rect, rotated, offset, originalSize);
    f->autorelease();
    return f;
}

bool CCSpriteFrame::initWithTexture(CCTexture2D* texture, const CCRect& rect, bool rotated, const CCPoint& offset,
                                    const CCSize& originalSize) {
    CC_SAFE_RETAIN(texture);
    CC_SAFE_RELEASE(m_pobTexture);
    m_pobTexture = texture;
    float s = scale(texture);
    m_obRectInPixels = rect;
    m_obRect = CCRectMake(rect.origin.x / s, rect.origin.y / s, rect.size.width / s, rect.size.height / s);
    m_obOffsetInPixels = offset;
    m_obOffset = ccp(offset.x / s, offset.y / s);
    m_obOriginalSizeInPixels = originalSize;
    m_obOriginalSize = CCSizeMake(originalSize.width / s, originalSize.height / s);
    m_bRotated = rotated;
    return true;
}

CCObject* CCSpriteFrame::copyWithZone(void*) {
    CCSpriteFrame* c = new CCSpriteFrame;
    c->initWithTexture(m_pobTexture, m_obRectInPixels, m_bRotated, m_obOffsetInPixels, m_obOriginalSizeInPixels);
    return c;
}

// --------------------------------------------------------------- cache

CCSpriteFrameCache* CCSpriteFrameCache::sharedSpriteFrameCache() {
    static CCSpriteFrameCache* c = new CCSpriteFrameCache;
    return c;
}

namespace {
// A frame TexturePacker trimmed to nothing (its picture wholly transparent
// when the sheet was packed): one texel standing for a larger original.
bool isBlank(CCSpriteFrame* f) {
    const CCSize& s = f->getRectInPixels().size;
    const CCSize& o = f->getOriginalSizeInPixels();
    return s.width <= 1 && s.height <= 1 && (o.width > 1 || o.height > 1);
}
}  // namespace

void CCSpriteFrameCache::addSpriteFramesWithDictionary(const json::Value& dict, CCTexture2D* texture) {
    const json::Value& metadata = dict["metadata"];
    int format = metadata.isNull() ? 0 : metadata["format"].integer();
    if (format < 0 || format > 3) return;
    for (const auto& member : dict["frames"].members) {
        const std::string& name = member.first;
        const json::Value& f = member.second;
        // A name already loaded keeps its first frame, as 2.1.1 did; except
        // (the port's) a blank one, which a later sheet's real picture of the
        // same name replaces (a frame packed blank in one sheet and drawn in
        // another loaded with it). (AS3's mini worm, whose two copies of its
        // art differ in name, is mended in its data: extract_ipa.py.)
        auto old = m_frames.find(name);
        if (old != m_frames.end() && !isBlank(old->second)) continue;
        CCSpriteFrame* frame = new CCSpriteFrame;
        if (format == 0) {
            float x = static_cast<float>(f["x"].num()), y = static_cast<float>(f["y"].num());
            float w = static_cast<float>(f["width"].num()), h = static_cast<float>(f["height"].num());
            float ox = static_cast<float>(f["offsetX"].num()), oy = static_cast<float>(f["offsetY"].num());
            int ow = std::abs(f["originalWidth"].integer()), oh = std::abs(f["originalHeight"].integer());
            frame->initWithTexture(texture, CCRectMake(x, y, w, h), false, ccp(ox, oy), CCSizeMake(ow, oh));
        } else if (format == 1 || format == 2) {
            CCRect rect = CCRectFromString(f["frame"].str());
            bool rotated = format == 2 && f["rotated"].flag();
            frame->initWithTexture(texture, rect, rotated, CCPointFromString(f["offset"].str()),
                                   CCSizeFromString(f["sourceSize"].str()));
        } else {
            CCSize spriteSize = CCSizeFromString(f["spriteSize"].str());
            CCRect textureRect = CCRectFromString(f["textureRect"].str());
            for (const json::Value& alias : f["aliases"].items) m_aliases[alias.str()] = name;
            frame->initWithTexture(texture, CCRectMake(textureRect.origin.x, textureRect.origin.y, spriteSize.width, spriteSize.height),
                                   f["textureRotated"].flag(), CCPointFromString(f["spriteOffset"].str()),
                                   CCSizeFromString(f["spriteSourceSize"].str()));
        }
        if (old != m_frames.end()) {
            if (isBlank(frame)) {
                frame->release();
                continue;
            }
            old->second->release();
        }
        m_frames[name] = frame;
    }
}

void CCSpriteFrameCache::addSpriteFramesWithFile(const char* plist) {
    if (m_loadedFileNames.count(plist)) return;
    std::string text;
    json::Value dict;
    if (!CCFileUtils::sharedFileUtils()->readText(plist, text) || !json::parse(text, dict)) {
        platform::debugTrace("sprite sheet not found: %s", plist);
        return;
    }
    std::string texturePath = dict["metadata"]["textureFileName"].str();
    if (!texturePath.empty()) {
        // beside the plist
        texturePath = CCFileUtils::sharedFileUtils()->fullPathFromRelativeFile(texturePath.c_str(), plist);
    } else {
        texturePath = plist;
        texturePath = texturePath.erase(texturePath.find_last_of('.')).append(".png");
    }
    // (the port's: the sheet's size, so its texture is decoded on worker
    // threads while the game goes on: CCTextureCache::addImage)
    int width = 0, height = 0;
    std::sscanf(dict["metadata"]["size"].str().c_str(), "{%d,%d}", &width, &height);
    if (CCTexture2D* texture = CCTextureCache::sharedTextureCache()->addImage(texturePath.c_str(), width, height)) {
        addSpriteFramesWithDictionary(dict, texture);
        m_loadedFileNames.insert(plist);
    }
}

void CCSpriteFrameCache::addSpriteFramesWithFile(const char* plist, CCTexture2D* texture) {
    std::string text;
    json::Value dict;
    if (CCFileUtils::sharedFileUtils()->readText(plist, text) && json::parse(text, dict))
        addSpriteFramesWithDictionary(dict, texture);
}

void CCSpriteFrameCache::addSpriteFramesWithFile(const char* plist, const char* textureFileName) {
    if (CCTexture2D* texture = CCTextureCache::sharedTextureCache()->addImage(textureFileName))
        addSpriteFramesWithFile(plist, texture);
}

void CCSpriteFrameCache::addSpriteFrame(CCSpriteFrame* frame, const char* name) {
    frame->retain();
    auto it = m_frames.find(name);
    if (it != m_frames.end()) it->second->release();
    m_frames[name] = frame;
}

void CCSpriteFrameCache::removeSpriteFrames() {
    for (auto& f : m_frames) f.second->release();
    m_frames.clear();
    m_aliases.clear();
    m_loadedFileNames.clear();
}

void CCSpriteFrameCache::removeUnusedSpriteFrames() {
    bool removed = false;
    for (auto it = m_frames.begin(); it != m_frames.end();) {
        if (it->second->retainCount() == 1) {
            it->second->release();
            it = m_frames.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (removed) m_loadedFileNames.clear();  // as 2.1.1: the sheets may be loaded again
}

void CCSpriteFrameCache::removeSpriteFrameByName(const char* name) {
    if (!name) return;
    std::string key = name;
    auto alias = m_aliases.find(key);
    if (alias != m_aliases.end()) {
        key = alias->second;
        m_aliases.erase(alias);
    }
    auto it = m_frames.find(key);
    if (it != m_frames.end()) {
        it->second->release();
        m_frames.erase(it);
    }
    m_loadedFileNames.clear();
}

void CCSpriteFrameCache::removeSpriteFramesFromFile(const char* plist) {
    std::string text;
    json::Value dict;
    if (!CCFileUtils::sharedFileUtils()->readText(plist, text) || !json::parse(text, dict)) return;
    for (const auto& member : dict["frames"].members) {
        auto it = m_frames.find(member.first);
        if (it != m_frames.end()) {
            it->second->release();
            m_frames.erase(it);
        }
    }
    m_loadedFileNames.erase(plist);
}

void CCSpriteFrameCache::removeSpriteFramesFromTexture(CCTexture2D* texture) {
    for (auto it = m_frames.begin(); it != m_frames.end();) {
        if (it->second->getTexture() == texture) {
            it->second->release();
            it = m_frames.erase(it);
        } else {
            ++it;
        }
    }
}

CCSpriteFrame* CCSpriteFrameCache::spriteFrameByName(const char* name) {
    auto it = m_frames.find(name);
    if (it == m_frames.end()) {
        auto alias = m_aliases.find(name);
        if (alias != m_aliases.end()) it = m_frames.find(alias->second);
    }
    if (it == m_frames.end()) {
        platform::debugTrace("sprite frame not found: %s", name);
        return nullptr;
    }
    return it->second;
}

}  // namespace cocos2d
