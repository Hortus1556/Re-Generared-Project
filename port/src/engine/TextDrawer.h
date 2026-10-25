// Bitmap-font text renderer (port of A3ITextDrawer).
//
// A font is one image holding 95 glyphs for ASCII '!'..'~' plus one more;
// the metric table gives each glyph's {x, y, width} in that image, all glyphs
// share fontHeight. Characters at or below '!' use glyph 0; spaces advance by
// spaceWidth.
#pragma once

#include <string>
#include <vector>

#include "core/Object.h"
#include "engine/AdvancedImage.h"

class TextDrawer : public Object {
public:
    static constexpr int kGlyphs = 95;

    static TextDrawer* initWithPathSpacingFontHeightSpaceWidth(const std::string& path, int spacing,
                                                               int fontHeight, int spaceWidth);
    ~TextDrawer() override;

    // Glyph metrics for the first `glyphs` characters; the rest stay empty
    // (the digit fonts only define '!'..'9' or '!'..':').
    void setMatrix(const int (*m)[3], int glyphs = kGlyphs);
    void setFontRGB(float r, float g, float b);

    int getFontHeightWithRatio(float ratio) const { return static_cast<int>(fontHeight * ratio); }
    int getFontWidthRatio(const std::string& s, float ratio) const;

    void drawStringWithStringXYRatioOrientationAlpha(const std::string& s, int x, int y, float ratio,
                                                     float orientation, float alpha);
    // Draws one line per entry, advancing y by fontHeight * ratio.
    void drawStringWithArrayXYRatioOrientationAlpha(const std::vector<std::string>& lines, int x, int y,
                                                    float ratio, float orientation, float alpha);

    // Splits on "\n", then word-wraps each part to `width` pixels.
    std::vector<std::string> wrapStringWidthRatio(const std::string& s, int width, float ratio);

    int fontHeight = 0;
    int spaceWidth = 0;
    int characterSpacing = 0;
    int fontMatric[kGlyphs][3] = {};
    AdvancedImage* fontImage = nullptr;

private:
    TextDrawer() = default;
    static int glyphIndex(unsigned char c);
    int next(const std::string& s, int width, float ratio);
    std::vector<std::string> wrap(const std::string& s, int width, float ratio);
    int wrapPos = 0;
};
