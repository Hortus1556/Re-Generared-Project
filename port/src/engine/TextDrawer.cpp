#include "engine/TextDrawer.h"

#include <functional>

#include "platform/DrawList.h"
#include "platform/GL.h"

namespace {
constexpr unsigned char kSpace = ' ';  // SPACE_ASCII in the original
}

TextDrawer* TextDrawer::initWithPathSpacingFontHeightSpaceWidth(const std::string& path, int spacing,
                                                                int fontHeight, int spaceWidth) {
    TextDrawer* t = new TextDrawer;
    t->fontImage = AdvancedImage::initWithPathRowColData(path, 1, 1, nullptr);
    t->fontHeight = fontHeight;
    t->characterSpacing = spacing;
    t->spaceWidth = spaceWidth;
    t->setFontRGB(0, 0, 0);
    return t;
}

TextDrawer::~TextDrawer() { releaseRef(fontImage); }

void TextDrawer::setMatrix(const int (*m)[3], int glyphs) {
    for (int i = 0; i < kGlyphs; ++i)
        for (int k = 0; k < 3; ++k) fontMatric[i][k] = i < glyphs ? m[i][k] : 0;
}

void TextDrawer::setFontRGB(float r, float g, float b) {
    if (fontImage) fontImage->setRGB(r, g, b);
}

int TextDrawer::glyphIndex(unsigned char c) { return c - kSpace < 2 ? 0 : c - kSpace - 1; }

// Note: like the original, spaces are measured with glyph 0's width here
// although drawing advances them by spaceWidth.
int TextDrawer::getFontWidthRatio(const std::string& s, float ratio) const {
    int w = 0;
    for (unsigned char c : s) w = static_cast<int>(w + (fontMatric[glyphIndex(c)][2] * ratio + characterSpacing));
    return w;
}

void TextDrawer::drawStringWithStringXYRatioOrientationAlpha(const std::string& s, int x, int y, float ratio,
                                                             float orientation, float alpha) {
    if (!fontImage) return;
    // Its glyphs are paired across frames only with the same string's (so a
    // name being typed never borrows letters from other text on screen).
    platform::DrawOwner owner(std::hash<std::string>()(s) ^ serial() * 0x9E3779B97F4A7C15ull);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(static_cast<float>(x), static_cast<float>(y), 0);
    glRotatef(orientation, 0, 0, 1);
    int pen = 0;
    for (unsigned char c : s) {
        if (c == kSpace) {
            pen = static_cast<int>(pen + spaceWidth * ratio);
            continue;
        }
        const int* m = fontMatric[glyphIndex(c)];
        fontImage->setPositionWithXY(pen, 0);
        fontImage->setAlpha(alpha);
        fontImage->setRatio(ratio);
        fontImage->drawWithImageRegionWithXYWidthHeight(m[0], m[1], m[2], fontHeight);
        pen += static_cast<int>((m[2] + characterSpacing) * ratio);
    }
    glPopMatrix();
}

void TextDrawer::drawStringWithArrayXYRatioOrientationAlpha(const std::vector<std::string>& lines, int x, int y,
                                                            float ratio, float orientation, float alpha) {
    for (const std::string& line : lines) {
        drawStringWithStringXYRatioOrientationAlpha(line, x, y, ratio, orientation, alpha);
        y = static_cast<int>(y + fontHeight * ratio);
    }
}

// -_next:Width:Ratio: — advances wrapPos to the end of the longest run of
// whole words (from the current wrapPos) that fits in `width`; a single word
// longer than the width is cut at the longest prefix that fits. Returns the
// new wrapPos, or -1 at the end of the string.
int TextDrawer::next(const std::string& s, int width, float ratio) {
    int len = static_cast<int>(s.size());
    int start = wrapPos;
    if (len <= start) return -1;
    for (int i = start;; ++i) {
        if (i >= len || static_cast<unsigned char>(s[i]) < 0x21) {
            int w = getFontWidthRatio(s.substr(start, i - start), ratio);
            if (start == wrapPos && width < w) {
                // First word alone is too wide: shrink it to what fits.
                int n = i - start;
                while (getFontWidthRatio(s.substr(start, n), ratio) > width && n > 0) --n;
                wrapPos = start + n;
                return wrapPos;
            }
            if (w <= width) wrapPos = i;
            if (i >= len || width < w || s[i] == '\n') return wrapPos;
        }
    }
}

std::vector<std::string> TextDrawer::wrap(const std::string& s, int width, float ratio) {
    std::vector<std::string> lines;
    wrapPos = 0;
    int prev = 0;
    int pos = next(s, width, ratio);
    while (pos != -1) {
        if (prev < static_cast<int>(s.size()) && s[prev] == ' ') ++prev;
        lines.push_back(pos > prev ? s.substr(prev, pos - prev) : std::string());
        int after = next(s, width, ratio);
        if (after == pos) break;  // no progress (a glyph wider than the line); the original looped forever
        prev = pos;
        pos = after;
    }
    return lines;
}

std::vector<std::string> TextDrawer::wrapStringWidthRatio(const std::string& s, int width, float ratio) {
    std::vector<std::string> out;
    std::string::size_type begin = 0;
    while (true) {
        std::string::size_type nl = s.find('\n', begin);
        std::vector<std::string> part =
            wrap(s.substr(begin, nl == std::string::npos ? std::string::npos : nl - begin), width, ratio);
        out.insert(out.end(), part.begin(), part.end());
        if (nl == std::string::npos) break;
        begin = nl + 1;
    }
    return out;
}
