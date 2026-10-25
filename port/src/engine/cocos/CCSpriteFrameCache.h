// cocos2d-x 2.1.1's CCSpriteFrame and CCSpriteFrameCache, ported to Bleed
// Engine: sprite sheets (TexturePacker/Zwoptex property lists, formats 0-3,
// which the package keeps as JSON) give named frames of their texture, in
// pixels and in points; a frame may be rotated (stored turned 90 degrees)
// and trimmed (its offset and original size). MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include <map>
#include <set>
#include <string>

#include "engine/cocos/CCTexture2D.h"

namespace json {
struct Value;
}

namespace cocos2d {

CCRect CCRectFromString(const std::string& s);
CCPoint CCPointFromString(const std::string& s);
CCSize CCSizeFromString(const std::string& s);

class CCSpriteFrame : public CCObject {
public:
    ~CCSpriteFrame() override { CC_SAFE_RELEASE(m_pobTexture); }
    static CCSpriteFrame* create(const char* filename, const CCRect& rect);
    static CCSpriteFrame* createWithTexture(CCTexture2D* texture, const CCRect& rect);
    static CCSpriteFrame* createWithTexture(CCTexture2D* texture, const CCRect& rect, bool rotated,
                                            const CCPoint& offset, const CCSize& originalSize);
    bool initWithTexture(CCTexture2D* texture, const CCRect& rect, bool rotated, const CCPoint& offset,
                         const CCSize& originalSize);
    CCObject* copyWithZone(void*) override;

    const CCRect& getRectInPixels() { return m_obRectInPixels; }
    bool isRotated() { return m_bRotated; }
    void setRotated(bool r) { m_bRotated = r; }
    const CCRect& getRect() { return m_obRect; }
    const CCPoint& getOffsetInPixels() { return m_obOffsetInPixels; }
    const CCSize& getOriginalSizeInPixels() { return m_obOriginalSizeInPixels; }
    const CCSize& getOriginalSize() { return m_obOriginalSize; }
    const CCPoint& getOffset() { return m_obOffset; }
    CCTexture2D* getTexture() { return m_pobTexture; }

private:
    CCPoint m_obOffset;
    CCSize m_obOriginalSize;
    CCRect m_obRectInPixels;
    bool m_bRotated = false;
    CCRect m_obRect;
    CCPoint m_obOffsetInPixels;
    CCSize m_obOriginalSizeInPixels;
    CCTexture2D* m_pobTexture = nullptr;
};

class CCSpriteFrameCache : public CCObject {
public:
    static CCSpriteFrameCache* sharedSpriteFrameCache();
    // the sheet's frames (its texture named in its metadata, beside it)
    void addSpriteFramesWithFile(const char* plist);
    void addSpriteFramesWithFile(const char* plist, CCTexture2D* texture);
    void addSpriteFramesWithFile(const char* plist, const char* textureFileName);
    void addSpriteFrame(CCSpriteFrame* frame, const char* name);
    void removeSpriteFrames();
    void removeUnusedSpriteFrames();
    void removeSpriteFrameByName(const char* name);
    void removeSpriteFramesFromFile(const char* plist);
    void removeSpriteFramesFromTexture(CCTexture2D* texture);
    // nullptr (traced) when no sheet loaded has it
    CCSpriteFrame* spriteFrameByName(const char* name);

private:
    void addSpriteFramesWithDictionary(const json::Value& dict, CCTexture2D* texture);
    std::map<std::string, CCSpriteFrame*> m_frames;  // retained
    std::map<std::string, std::string> m_aliases;
    std::set<std::string> m_loadedFileNames;
};

}  // namespace cocos2d
