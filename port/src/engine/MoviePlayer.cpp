#include "engine/MoviePlayer.h"

#include <algorithm>

#include "engine/MediaPlayer.h"
#include "engine/VectorMovie.h"
#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/GL.h"
#include "platform/Platform.h"

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace {

// A video file, decoded by the platform.
class VideoSource : public MovieSource {
public:
    explicit VideoSource(std::unique_ptr<platform::MovieDecoder> d) : decoder(std::move(d)) {}
    bool readAudio(std::vector<float>& pcm, int& channels, int& rate) override {
        return decoder->readAudio(pcm, channels, rate);
    }
    bool streamAudio(const platform::AudioFormatFn& format, const platform::AudioSamplesFn& samples) override {
        return decoder->streamAudio(format, samples);
    }
    bool nextFrame(std::vector<unsigned char>& bgra, int& w, int& h, double& seconds) override {
        return decoder->nextFrame(bgra, w, h, seconds);
    }

private:
    std::unique_ptr<platform::MovieDecoder> decoder;
};

std::unique_ptr<MovieSource> openSource(const std::string& bundleName) {
    if (bundleName.size() > 4 && bundleName.compare(bundleName.size() - 4, 4, ".svg") == 0)
        return VectorMovie::open(bundleName);
    std::string path = assets::path(bundleName);
    if (path.empty()) return nullptr;
    auto decoder = platform::MovieDecoder::open(path);
    if (!decoder) return nullptr;
    return std::make_unique<VideoSource>(std::move(decoder));
}

}  // namespace

std::unique_ptr<MoviePlayer> MoviePlayer::open(const std::string& bundleName) {
    std::unique_ptr<MoviePlayer> m(new MoviePlayer);
    m->decoder = openSource(bundleName);
    if (!m->decoder) return nullptr;
    // the soundtrack played as it is decoded (the frames' decoder is not
    // waited on: the platform's sound decoding can be slow, Android's)
    MovieSource* source = m->decoder.get();
    m->soundtrack = SoundEffectTrack::initStreamed(
        [source](const platform::AudioFormatFn& f, const platform::AudioSamplesFn& s) { return source->streamAudio(f, s); });
    m->havePending = m->decoder->nextFrame(m->pending, m->width, m->height, m->pendingTime);
    if (!m->havePending) return nullptr;
    glGenTextures(1, &m->texture);
    glBindTexture(GL_TEXTURE_2D, m->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // (the frames in the platform's texel order: BGRA on the desktop, RGBA on
    // OpenGL ES, whose internal format must be the format)
#if defined(__ANDROID__)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m->width, m->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m->width, m->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
#endif
    glBindTexture(GL_TEXTURE_2D, 0);
    return m;
}

MoviePlayer::~MoviePlayer() {
    if (soundtrack) soundtrack->stop();
    releaseRef(soundtrack);
    platform::releaseTexture(texture);
}

void MoviePlayer::start() {
    startTime = platform::now();
    if (soundtrack) soundtrack->play();
}

bool MoviePlayer::update() {
    double now = platform::now() - startTime;
    platform::movieShots(now);
    // Show the latest frame that is due (late frames are skipped).
    while (havePending && pendingTime <= now) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        platform::toTexelOrder(pending.data(), static_cast<size_t>(width) * height);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, platform::kTexelFormat, GL_UNSIGNED_BYTE, pending.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        haveFrame = true;
        havePending = decoder->nextFrame(pending, width, height, pendingTime);
    }
    if (!havePending) finished = true;
    return !finished;
}

void MoviePlayer::draw() {
    platform::clearScreen(0, 0, 0, 1);
    if (!haveFrame) return;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, platform::screenWidth(), platform::screenHeight(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // Aspect fill: cover the screen, cropping the overflow; aspect fit: all
    // of the picture, letterboxed.
    float sx = static_cast<float>(platform::screenWidth()) / width;
    float sy = static_cast<float>(platform::screenHeight()) / height;
    float scale = aspectFill ? std::max(sx, sy) : std::min(sx, sy);
    float w = width * scale, h = height * scale;
    float x = (platform::screenWidth() - w) * 0.5f, y = (platform::screenHeight() - h) * 0.5f;
    const float xy[8] = {x, y, x + w, y, x, y + h, x + w, y + h};
    const float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
    const float white[4] = {1, 1, 1, 1};
    platform::drawQuad(reinterpret_cast<platform::DrawId>(this), texture, xy, uv, white, GL_SRC_ALPHA);
}
