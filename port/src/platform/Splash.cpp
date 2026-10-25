#include "platform/Splash.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "lunasvg.h"
#include "platform/BleedSplashArt.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace platform {

namespace {

// A click or Esc: the splash is cut short.
class SkipSink : public TouchSink {
public:
    bool skip = false;
    void touchBegan(float, float) override { skip = true; }
    void touchMoved(float, float, float, float) override {}
    void touchEnded(float, float, float, float) override {}
    void escapeKey() override { skip = true; }
};

// The wordmark rasterised at w x h pixels, into `texture` (made on first use).
void rasterise(lunasvg::Document& doc, int w, int h, GLuint& texture) {
    lunasvg::Bitmap bitmap = doc.renderToBitmap(w, h, 0x00000000);
    if (bitmap.isNull()) return;
    bitmap.convertToRGBA();  // straight alpha, RGBA byte order
    std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y)
        std::copy_n(bitmap.data() + static_cast<size_t>(y) * bitmap.stride(), static_cast<size_t>(w) * 4,
                    pixels.data() + static_cast<size_t>(y) * w * 4);
    if (texture) releaseTexture(texture);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace

bool showEngineSplash(const std::function<bool()>& ready) {
    if (automationActive()) return true;
    auto doc = lunasvg::Document::loadFromData(kBleedSplashSvg);
    if (!doc || doc->width() <= 0 || doc->height() <= 0) return true;
    // Centred on black, 70% of the screen's width (or half its height, on a
    // screen too narrow for that).
    const float sw = static_cast<float>(screenWidth()), sh = static_cast<float>(screenHeight());
    const float unit = std::min(sw * 0.7f / doc->width(), sh * 0.5f / doc->height());
    const float w = doc->width() * unit, h = doc->height() * unit;
    const float x = (sw - w) * 0.5f, y = (sh - h) * 0.5f;
    const float xy[8] = {x, y, x + w, y, x, y + h, x + w, y + h};
    const float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
    static const char owner = 0;

    const double fadeIn = 0.6, hold = 1.8, fadeOut = 0.6;
    GLuint texture = 0;
    float rasterisedAt = 0;
    SkipSink sink;
    double start = now();
    bool open = true;
    while ((open = pollEvents(sink))) {
        double t = now() - start;
        float alpha = t < fadeIn ? static_cast<float>(t / fadeIn)
                      : t < fadeIn + hold ? 1.0f
                                          : static_cast<float>(1 - (t - fadeIn - hold) / fadeOut);
        // Held at full brightness until the game's content is read (its
        // window answering meanwhile), then faded out.
        if (ready && t >= fadeIn + hold && !ready()) {
            start = now() - (fadeIn + hold);
            alpha = 1;
            sink.skip = false;
        }
        if (sink.skip) {
            // Straight on to fading out, from as bright as it is now.
            sink.skip = false;
            if (t < fadeIn + hold) {
                start = now() - (fadeIn + hold + (1 - alpha) * fadeOut);
                continue;
            }
        }
        if (alpha <= 0) break;
        // Sharp at the window's size: rasterised again when it changes.
        if (displayScale() != rasterisedAt) {
            rasterisedAt = displayScale();
            rasterise(*doc, std::max(1, static_cast<int>(std::lround(w * rasterisedAt))),
                      std::max(1, static_cast<int>(std::lround(h * rasterisedAt))), texture);
        }
        clearScreen(0, 0, 0, 1);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, sw, sh, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        const float rgba[4] = {1, 1, 1, alpha};
        if (texture) drawQuad(reinterpret_cast<DrawId>(&owner), texture, xy, uv, rgba, GL_SRC_ALPHA);
        // Shown, not counted as one of the game's frames.
        commitFrame();
        show(1);
    }
    // The game starts from black.
    clearScreen(0, 0, 0, 1);
    commitFrame();
    if (texture) releaseTexture(texture);
    return open;
}

}  // namespace platform
