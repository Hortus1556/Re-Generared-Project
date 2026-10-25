// cocos2d-x 2.1.1's CCLabelBMFont and CCBMFontConfiguration, ported to Bleed
// Engine. MIT: LICENSE-cocos2d-x.txt.
//
// A label in a bitmap font (AngelCode's .fnt: the characters' rects in one
// page, their offsets and advances, kernings): one sprite a character, its
// tag the character's index. With a width, lines are broken at spaces (or
// anywhere, with setLineBreakWithoutSpace) as 2.1.1 broke them, and aligned
// left, centred or right. Its quirks are kept: text breaks where it did on
// the device.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "engine/cocos/CCSprite.h"

namespace cocos2d {

#define kCCLabelAutomaticWidth -1

struct ccBMFontDef {
    unsigned int charID = 0;
    CCRect rect;
    short xOffset = 0, yOffset = 0, xAdvance = 0;
};

struct ccBMFontPadding {
    int left = 0, top = 0, right = 0, bottom = 0;
};

class CCBMFontConfiguration : public CCObject {
public:
    static CCBMFontConfiguration* create(const char* fntFile);
    bool initWithFNTfile(const char* fntFile);
    const std::string& getAtlasName() { return m_sAtlasName; }
    std::set<unsigned int>* getCharacterSet() { return &m_characterSet; }

    std::map<unsigned int, ccBMFontDef> m_fontDefs;
    int m_nCommonHeight = 0;
    ccBMFontPadding m_tPadding;
    std::string m_sAtlasName;
    std::map<unsigned int, int> m_kernings;  // (first << 16) | second -> amount

private:
    bool parseConfigFile(const char* controlFile);
    void parseCharacterDefinition(const std::string& line, ccBMFontDef* def);
    void parseInfoArguments(const std::string& line);
    void parseCommonArguments(const std::string& line);
    void parseImageFileName(const std::string& line, const char* fntFile);
    void parseKerningEntry(const std::string& line);
    std::set<unsigned int> m_characterSet;
};

class CCLabelProtocol {
public:
    virtual ~CCLabelProtocol() = default;
    virtual void setString(const char* label) = 0;
    virtual const char* getString() = 0;
};

class CCLabelBMFont : public CCSpriteBatchNode, public CCLabelProtocol, public CCRGBAProtocol {
public:
    static void purgeCachedData();
    static CCLabelBMFont* create();
    static CCLabelBMFont* create(const char* str, const char* fntFile, float width = kCCLabelAutomaticWidth,
                                 CCTextAlignment alignment = kCCTextAlignmentLeft, CCPoint imageOffset = CCPointZero);
    ~CCLabelBMFont() override;
    bool init() override;
    bool initWithString(const char* str, const char* fntFile, float width = kCCLabelAutomaticWidth,
                        CCTextAlignment alignment = kCCTextAlignmentLeft, CCPoint imageOffset = CCPointZero);

    void createFontChars();
    void setString(const char* newString) override { setString(newString, false); }
    void setString(const char* newString, bool fromUpdate);
    const char* getString() override { return m_sInitialString.c_str(); }
    void setCString(const char* label) { setString(label); }
    void updateLabel();
    void setAlignment(CCTextAlignment alignment);
    void setWidth(float width);
    void setLineBreakWithoutSpace(bool breakWithoutSpace);
    void setScale(float scale) override;
    void setScaleX(float scaleX) override;
    void setScaleY(float scaleY) override;
    void setAnchorPoint(const CCPoint& point) override;
    void setFntFile(const char* fntFile);
    const char* getFntFile() { return m_sFntFile.c_str(); }
    CCBMFontConfiguration* getConfiguration() { return m_pConfiguration; }

    // CCRGBAProtocol
    void setColor(const ccColor3B& color) override;
    const ccColor3B& getColor() override { return m_tColor; }
    GLubyte getOpacity() override { return m_cOpacity; }
    void setOpacity(GLubyte opacity) override;
    void setOpacityModifyRGB(bool v) override;
    bool isOpacityModifyRGB() override { return m_bIsOpacityModifyRGB; }

private:
    int kerningAmountForFirst(unsigned short first, unsigned short second);
    void updateString(bool fromUpdate);
    float getLetterPosXLeft(CCSprite* sp);
    float getLetterPosXRight(CCSprite* sp);

    std::vector<unsigned short> m_sString;  // UTF-16, as shown (the broken lines)
    std::string m_sInitialString;           // as given (UTF-8)
    std::string m_sFntFile;
    CCTextAlignment m_pAlignment = kCCTextAlignmentLeft;
    float m_fWidth = -1;
    CCBMFontConfiguration* m_pConfiguration = nullptr;
    bool m_bLineBreakWithoutSpaces = false;
    CCPoint m_tImageOffset;
    GLubyte m_cOpacity = 0;
    ccColor3B m_tColor = ccWHITE;
    bool m_bIsOpacityModifyRGB = false;
};

// UTF-8 to UTF-16 (cc_utf16_from_utf8)
std::vector<unsigned short> cc_utf16_from_utf8(const char* str);

}  // namespace cocos2d
