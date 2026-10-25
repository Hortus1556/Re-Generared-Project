// cocos2d-x 2.1.1's CCLabelBMFont.cpp, ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCLabelBMFont.h"

#include <cstdio>
#include <cstring>

#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCFileUtils.h"
#include "platform/Platform.h"

namespace cocos2d {

// ------------------------------------------------------------ helpers

std::vector<unsigned short> cc_utf16_from_utf8(const char* s) {
    std::vector<unsigned short> out;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s ? s : "");
    while (*p) {
        unsigned int c = *p++;
        int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
        if (extra) c &= (0x3f >> extra);
        for (int i = 0; i < extra && (*p & 0xc0) == 0x80; ++i) c = (c << 6) | (*p++ & 0x3f);
        out.push_back(static_cast<unsigned short>(c));
    }
    return out;
}

static bool isspace_unicode(unsigned short ch) {
    return (ch >= 0x0009 && ch <= 0x000D) || ch == 0x0020 || ch == 0x0085 || ch == 0x00A0 || ch == 0x1680 ||
           (ch >= 0x2000 && ch <= 0x200A) || ch == 0x2028 || ch == 0x2029 || ch == 0x202F || ch == 0x205F ||
           ch == 0x3000;
}

// the index of the last character that is not c (-1 if none)
static int cc_utf8_find_last_not_char(const std::vector<unsigned short>& str, unsigned short c) {
    int i = static_cast<int>(str.size()) - 1;
    for (; i >= 0; --i)
        if (str[i] != c) return i;
    return i;
}

static void cc_utf8_trim_from(std::vector<unsigned short>* str, int index) {
    int size = static_cast<int>(str->size());
    if (index >= size || index < 0) return;
    str->erase(str->begin() + index, str->end());
}

// trailing whitespace off (only if the last character is whitespace)
static void cc_utf8_trim_ws(std::vector<unsigned short>* str) {
    int len = static_cast<int>(str->size());
    if (len <= 0) return;
    int last_index = len - 1;
    if (isspace_unicode((*str)[last_index])) {
        for (int i = last_index - 1; i >= 0; --i) {
            if (isspace_unicode((*str)[i])) last_index = i;
            else break;
        }
        cc_utf8_trim_from(str, last_index);
    }
}

static float contentScale() { return CCDirector::sharedDirector()->getContentScaleFactor(); }

// ------------------------------------------------------- configuration

static std::map<std::string, CCBMFontConfiguration*>& configurations() {
    static std::map<std::string, CCBMFontConfiguration*> c;
    return c;
}

static CCBMFontConfiguration* FNTConfigLoadFile(const char* fntFile) {
    auto it = configurations().find(fntFile);
    if (it != configurations().end()) return it->second;
    CCBMFontConfiguration* c = CCBMFontConfiguration::create(fntFile);
    if (c) {
        c->retain();
        configurations()[fntFile] = c;
    }
    return c;
}

void CCLabelBMFont::purgeCachedData() {
    for (auto& c : configurations()) c.second->release();
    configurations().clear();
}

CCBMFontConfiguration* CCBMFontConfiguration::create(const char* fntFile) {
    CCBMFontConfiguration* c = new CCBMFontConfiguration;
    if (c->initWithFNTfile(fntFile)) {
        c->autorelease();
        return c;
    }
    delete c;
    return nullptr;
}

bool CCBMFontConfiguration::initWithFNTfile(const char* fntFile) { return parseConfigFile(fntFile); }

// The value after `key` up to the next space (2.1.1 finds the first match in the line).
static std::string field(const std::string& line, const char* key) {
    size_t index = line.find(key);
    if (index == std::string::npos) return "";
    size_t index2 = line.find(' ', index);
    return line.substr(index, index2 == std::string::npos ? std::string::npos : index2 - index);
}

bool CCBMFontConfiguration::parseConfigFile(const char* controlFile) {
    std::string contents;
    if (!CCFileUtils::sharedFileUtils()->readText(controlFile, contents)) {
        platform::debugTrace("cocos2d: Error parsing FNTfile %s", controlFile);
        return false;
    }
    std::string line;
    size_t start = 0;
    while (start < contents.size()) {
        size_t pos = contents.find('\n', start);
        line = contents.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
        start = pos == std::string::npos ? contents.size() : pos + 1;
        if (line.compare(0, 9, "info face") == 0) {
            parseInfoArguments(line);
        } else if (line.compare(0, 17, "common lineHeight") == 0) {
            parseCommonArguments(line);
        } else if (line.compare(0, 7, "page id") == 0) {
            parseImageFileName(line, controlFile);
        } else if (line.compare(0, 7, "chars c") == 0) {
            // ignored
        } else if (line.compare(0, 4, "char") == 0) {
            ccBMFontDef def;
            parseCharacterDefinition(line, &def);
            m_fontDefs[def.charID] = def;
            m_characterSet.insert(def.charID);
        } else if (line.compare(0, 13, "kerning first") == 0) {
            parseKerningEntry(line);
        }
    }
    return true;
}

void CCBMFontConfiguration::parseImageFileName(const std::string& line, const char* fntFile) {
    // page id=0 file="bitmapFontTest.png"
    size_t index = line.find('"') + 1;
    size_t index2 = line.find('"', index);
    std::string value = line.substr(index, index2 - index);
    m_sAtlasName = CCFileUtils::sharedFileUtils()->fullPathFromRelativeFile(value.c_str(), fntFile);
}

void CCBMFontConfiguration::parseInfoArguments(const std::string& line) {
    std::string value = field(line, "padding=");
    std::sscanf(value.c_str(), "padding=%d,%d,%d,%d", &m_tPadding.top, &m_tPadding.right, &m_tPadding.bottom,
                &m_tPadding.left);
}

void CCBMFontConfiguration::parseCommonArguments(const std::string& line) {
    std::string value = field(line, "lineHeight=");
    std::sscanf(value.c_str(), "lineHeight=%d", &m_nCommonHeight);
}

void CCBMFontConfiguration::parseCharacterDefinition(const std::string& line, ccBMFontDef* d) {
    // char id=32   x=0     y=0     width=0     height=0     xoffset=0     yoffset=44    xadvance=14     page=0  chnl=0
    std::sscanf(field(line, "id=").c_str(), "id=%u", &d->charID);
    std::sscanf(field(line, "x=").c_str(), "x=%f", &d->rect.origin.x);
    std::sscanf(field(line, "y=").c_str(), "y=%f", &d->rect.origin.y);
    std::sscanf(field(line, "width=").c_str(), "width=%f", &d->rect.size.width);
    std::sscanf(field(line, "height=").c_str(), "height=%f", &d->rect.size.height);
    std::sscanf(field(line, "xoffset=").c_str(), "xoffset=%hd", &d->xOffset);
    std::sscanf(field(line, "yoffset=").c_str(), "yoffset=%hd", &d->yOffset);
    std::sscanf(field(line, "xadvance=").c_str(), "xadvance=%hd", &d->xAdvance);
}

void CCBMFontConfiguration::parseKerningEntry(const std::string& line) {
    // kerning first=121  second=44  amount=-7
    int first = 0, second = 0, amount = 0;
    std::sscanf(field(line, "first=").c_str(), "first=%d", &first);
    std::sscanf(field(line, "second=").c_str(), "second=%d", &second);
    std::sscanf(field(line, "amount=").c_str(), "amount=%d", &amount);
    m_kernings[(static_cast<unsigned int>(first) << 16) | (second & 0xffff)] = amount;
}

// --------------------------------------------------------------- label

CCLabelBMFont* CCLabelBMFont::create() {
    CCLabelBMFont* l = new CCLabelBMFont;
    if (l->init()) {
        l->autorelease();
        return l;
    }
    delete l;
    return nullptr;
}

CCLabelBMFont* CCLabelBMFont::create(const char* str, const char* fntFile, float width, CCTextAlignment alignment,
                                     CCPoint imageOffset) {
    CCLabelBMFont* l = new CCLabelBMFont;
    if (l->initWithString(str, fntFile, width, alignment, imageOffset)) {
        l->autorelease();
        return l;
    }
    delete l;
    return nullptr;
}

CCLabelBMFont::~CCLabelBMFont() { CC_SAFE_RELEASE(m_pConfiguration); }

bool CCLabelBMFont::init() { return initWithString(nullptr, nullptr, kCCLabelAutomaticWidth, kCCTextAlignmentLeft, CCPointZero); }

bool CCLabelBMFont::initWithString(const char* theString, const char* fntFile, float width, CCTextAlignment alignment,
                                   CCPoint imageOffset) {
    CCTexture2D* texture = nullptr;
    if (fntFile) {
        CCBMFontConfiguration* newConf = FNTConfigLoadFile(fntFile);
        if (!newConf) {
            platform::debugTrace("cocos2d: WARNING. CCLabelBMFont: Impossible to create font. Please check file: '%s'", fntFile);
            return false;
        }
        newConf->retain();
        CC_SAFE_RELEASE(m_pConfiguration);
        m_pConfiguration = newConf;
        m_sFntFile = fntFile;
        texture = CCTextureCache::sharedTextureCache()->addImage(m_pConfiguration->getAtlasName().c_str());
    }
    if (!theString) theString = "";
    CCSpriteBatchNode::initWithTexture(texture, static_cast<unsigned int>(std::strlen(theString)));
    m_pAlignment = alignment;
    m_tImageOffset = imageOffset;
    m_fWidth = width;
    m_cOpacity = 255;
    m_tColor = ccWHITE;
    m_obContentSize = CCSizeZero;
    m_bIsOpacityModifyRGB = texture && texture->hasPremultipliedAlpha();
    m_obAnchorPoint = ccp(0.5f, 0.5f);
    setString(theString);
    return true;
}

int CCLabelBMFont::kerningAmountForFirst(unsigned short first, unsigned short second) {
    auto it = m_pConfiguration->m_kernings.find((static_cast<unsigned int>(first) << 16) | second);
    return it == m_pConfiguration->m_kernings.end() ? 0 : it->second;
}

void CCLabelBMFont::createFontChars() {
    int nextFontPositionX = 0, nextFontPositionY = 0;
    int kerningAmount = 0;
    CCSize tmpSize = CCSizeZero;
    int longestLine = 0;
    unsigned int totalHeight = 0;
    unsigned int quantityOfLines = 1;
    unsigned int stringLen = static_cast<unsigned int>(m_sString.size());
    if (stringLen == 0 || !m_pConfiguration) return;
    std::set<unsigned int>* charSet = m_pConfiguration->getCharacterSet();
    for (unsigned int i = 0; i < stringLen - 1; ++i)
        if (m_sString[i] == '\n') quantityOfLines++;
    totalHeight = m_pConfiguration->m_nCommonHeight * quantityOfLines;
    nextFontPositionY = 0 - (m_pConfiguration->m_nCommonHeight - m_pConfiguration->m_nCommonHeight * quantityOfLines);
    float s = contentScale();
    ccBMFontDef fontDef;
    for (unsigned int i = 0; i < stringLen; i++) {
        unsigned short c = m_sString[i];
        if (c == '\n') {
            nextFontPositionX = 0;
            nextFontPositionY -= m_pConfiguration->m_nCommonHeight;
            continue;
        }
        if (charSet->find(c) == charSet->end()) continue;  // not in the font
        fontDef = m_pConfiguration->m_fontDefs[c];
        CCRect rect = CCRectMake(fontDef.rect.origin.x / s, fontDef.rect.origin.y / s, fontDef.rect.size.width / s,
                                 fontDef.rect.size.height / s);
        rect.origin.x += m_tImageOffset.x;
        rect.origin.y += m_tImageOffset.y;
        CCSprite* fontChar = static_cast<CCSprite*>(getChildByTag(i));
        if (!fontChar) {
            fontChar = new CCSprite;
            fontChar->initWithTexture(m_pobTexture, rect);
            addChild(fontChar, i, i);
            fontChar->release();
        } else {
            // the previous sprite, its defaults back
            fontChar->setTextureRect(rect, false, rect.size);
            fontChar->setVisible(true);
            fontChar->setOpacity(255);
        }
        // issue 1343: the sign kept
        int yOffset = m_pConfiguration->m_nCommonHeight - fontDef.yOffset;
        CCPoint fontPos = ccp(static_cast<float>(nextFontPositionX) + fontDef.xOffset + fontDef.rect.size.width * 0.5f + kerningAmount,
                              static_cast<float>(nextFontPositionY) + yOffset - rect.size.height * 0.5f * s);
        fontChar->setPosition(ccp(fontPos.x / s, fontPos.y / s));
        nextFontPositionX += fontDef.xAdvance + kerningAmount;
        fontChar->setOpacityModifyRGB(m_bIsOpacityModifyRGB);
        // the colour before the opacity, which may change it
        fontChar->setColor(m_tColor);
        if (m_cOpacity != 255) fontChar->setOpacity(m_cOpacity);  // issue #610
        if (longestLine < nextFontPositionX) longestLine = nextFontPositionX;
    }
    // a last character drawn wider than its advance widens the box
    tmpSize.width = fontDef.xAdvance < fontDef.rect.size.width ? longestLine + fontDef.rect.size.width - fontDef.xAdvance
                                                                : longestLine;
    tmpSize.height = static_cast<float>(totalHeight);
    setContentSize(CCSizeMake(tmpSize.width / s, tmpSize.height / s));
}

void CCLabelBMFont::setString(const char* newString, bool fromUpdate) {
    m_sString = cc_utf16_from_utf8(newString);
    if (m_sInitialString != newString) m_sInitialString = newString;
    updateString(fromUpdate);
}

void CCLabelBMFont::updateString(bool fromUpdate) {
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data) static_cast<CCNode*>(o)->setVisible(false);
    createFontChars();
    if (!fromUpdate) updateLabel();
}

void CCLabelBMFont::setColor(const ccColor3B& var) {
    m_tColor = var;
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data) static_cast<CCSprite*>(o)->setColor(m_tColor);
}

void CCLabelBMFont::setOpacity(GLubyte var) {
    m_cOpacity = var;
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data)
            if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(o)) p->setOpacity(m_cOpacity);
}

void CCLabelBMFont::setOpacityModifyRGB(bool var) {
    m_bIsOpacityModifyRGB = var;
    if (m_pChildren)
        for (CCObject* o : m_pChildren->data)
            if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(o)) p->setOpacityModifyRGB(m_bIsOpacityModifyRGB);
}

void CCLabelBMFont::setAnchorPoint(const CCPoint& point) {
    if (!point.equals(m_obAnchorPoint)) {
        CCSpriteBatchNode::setAnchorPoint(point);
        updateLabel();
    }
}

void CCLabelBMFont::updateLabel() {
    setString(m_sInitialString.c_str(), true);
    if (m_fWidth > 0) {
        // Step 1: the lines broken
        std::vector<unsigned short> str_whole = m_sString;
        unsigned int stringLength = static_cast<unsigned int>(str_whole.size());
        std::vector<unsigned short> multiline_string, last_word;
        multiline_string.reserve(stringLength);
        last_word.reserve(stringLength);
        unsigned int line = 1, i = 0;
        bool start_line = false, start_word = false;
        float startOfLine = -1, startOfWord = -1;
        int skip = 0;
        CCArray* children = getChildren();
        for (unsigned int j = 0; children && j < children->count(); j++) {
            CCSprite* characterSprite;
            while (!(characterSprite = static_cast<CCSprite*>(getChildByTag(j + skip)))) skip++;
            if (!characterSprite->isVisible()) continue;
            if (i >= stringLength) break;
            unsigned short character = str_whole[i];
            if (!start_word) {
                startOfWord = getLetterPosXLeft(characterSprite);
                start_word = true;
            }
            if (!start_line) {
                startOfLine = startOfWord;
                start_line = true;
            }
            // newline
            if (character == '\n') {
                cc_utf8_trim_ws(&last_word);
                last_word.push_back('\n');
                multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());
                last_word.clear();
                start_word = false;
                start_line = false;
                startOfWord = -1;
                startOfLine = -1;
                i++;
                line++;
                if (i >= stringLength) break;
                character = str_whole[i];
                // (2.1.1 tests the floats here, always false after the -1 above)
                if (!startOfWord) {
                    startOfWord = getLetterPosXLeft(characterSprite);
                    start_word = true;
                }
                if (!startOfLine) {
                    startOfLine = startOfWord;
                    start_line = true;
                }
            }
            // whitespace
            if (isspace_unicode(character)) {
                last_word.push_back(character);
                multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());
                last_word.clear();
                start_word = false;
                startOfWord = -1;
                i++;
                continue;
            }
            // out of bounds
            if (getLetterPosXRight(characterSprite) - startOfLine > m_fWidth) {
                if (!m_bLineBreakWithoutSpaces) {
                    last_word.push_back(character);
                    int found = cc_utf8_find_last_not_char(multiline_string, ' ');
                    if (found != -1) cc_utf8_trim_ws(&multiline_string);
                    else multiline_string.clear();
                    if (!multiline_string.empty()) multiline_string.push_back('\n');
                    line++;
                    start_line = false;
                    startOfLine = -1;
                    i++;
                } else {
                    cc_utf8_trim_ws(&last_word);
                    last_word.push_back('\n');
                    multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());
                    last_word.clear();
                    start_word = false;
                    start_line = false;
                    startOfWord = -1;
                    startOfLine = -1;
                    line++;
                    if (i >= stringLength) break;
                    if (!startOfWord) {
                        startOfWord = getLetterPosXLeft(characterSprite);
                        start_word = true;
                    }
                    if (!startOfLine) {
                        startOfLine = startOfWord;
                        start_line = true;
                    }
                    j--;
                }
                continue;
            }
            // a normal character
            last_word.push_back(character);
            i++;
        }
        multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());
        (void)line;  // counted by 2.1.1 too, never read
        m_sString = multiline_string;
        updateString(true);
    }
    // Step 2: the alignment
    if (m_pAlignment != kCCTextAlignmentLeft) {
        int i = 0, lineNumber = 0;
        int str_len = static_cast<int>(m_sString.size());
        std::vector<unsigned short> last_line;
        for (int ctr = 0; ctr <= str_len; ++ctr) {
            unsigned short ch = ctr < str_len ? m_sString[ctr] : 0;
            if (ch == '\n' || ch == 0) {
                unsigned int line_length = static_cast<unsigned int>(last_line.size());
                if (line_length == 0) {  // an empty line: on to the next
                    lineNumber++;
                    continue;
                }
                int index = i + line_length - 1 + lineNumber;
                if (index < 0) continue;
                CCSprite* lastChar = static_cast<CCSprite*>(getChildByTag(index));
                if (!lastChar) continue;
                float lineWidth = lastChar->getPosition().x + lastChar->getContentSize().width / 2.0f;
                float shift = 0;
                switch (m_pAlignment) {
                    case kCCTextAlignmentCenter: shift = getContentSize().width / 2.0f - lineWidth / 2.0f; break;
                    case kCCTextAlignmentRight: shift = getContentSize().width - lineWidth; break;
                    default: break;
                }
                if (shift != 0) {
                    for (unsigned j = 0; j < line_length; j++) {
                        index = i + j + lineNumber;
                        if (index < 0) continue;
                        if (CCSprite* characterSprite = static_cast<CCSprite*>(getChildByTag(index)))
                            characterSprite->setPosition(ccpAdd(characterSprite->getPosition(), ccp(shift, 0.0f)));
                    }
                }
                i += line_length;
                lineNumber++;
                last_line.clear();
                continue;
            }
            last_line.push_back(ch);
        }
    }
}

void CCLabelBMFont::setAlignment(CCTextAlignment alignment) {
    m_pAlignment = alignment;
    updateLabel();
}

void CCLabelBMFont::setWidth(float width) {
    m_fWidth = width;
    updateLabel();
}

void CCLabelBMFont::setLineBreakWithoutSpace(bool breakWithoutSpace) {
    m_bLineBreakWithoutSpaces = breakWithoutSpace;
    updateLabel();
}

void CCLabelBMFont::setScale(float scale) {
    CCSpriteBatchNode::setScale(scale);
    updateLabel();
}

void CCLabelBMFont::setScaleX(float scaleX) {
    CCSpriteBatchNode::setScaleX(scaleX);
    updateLabel();
}

void CCLabelBMFont::setScaleY(float scaleY) {
    CCSpriteBatchNode::setScaleY(scaleY);
    updateLabel();
}

float CCLabelBMFont::getLetterPosXLeft(CCSprite* sp) {
    return sp->getPosition().x * m_fScaleX - (sp->getContentSize().width * m_fScaleX * sp->getAnchorPoint().x);
}

float CCLabelBMFont::getLetterPosXRight(CCSprite* sp) {
    return sp->getPosition().x * m_fScaleX + (sp->getContentSize().width * m_fScaleX * sp->getAnchorPoint().x);
}

void CCLabelBMFont::setFntFile(const char* fntFile) {
    if (fntFile && m_sFntFile != fntFile) {
        CCBMFontConfiguration* newConf = FNTConfigLoadFile(fntFile);
        if (!newConf) return;
        m_sFntFile = fntFile;
        CC_SAFE_RETAIN(newConf);
        CC_SAFE_RELEASE(m_pConfiguration);
        m_pConfiguration = newConf;
        setTexture(CCTextureCache::sharedTextureCache()->addImage(m_pConfiguration->getAtlasName().c_str()));
        createFontChars();
    }
}

}  // namespace cocos2d
