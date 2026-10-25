// Full-screen movie playback (the original's MPMoviePlayerController):
// frames drawn over the whole game screen, keeping their shape (cropped:
// AS1's scaling mode "aspect fill", or letterboxed: "aspect fit", the
// player's default, AS2), with the soundtrack played as a sound effect.
// Frames follow the wall clock. They come from a video file
// (platform::MovieDecoder) or, for a .svg, a vector animation rasterised as
// it plays (VectorMovie).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "platform/GL.h"
#include "platform/MovieDecoder.h"

class SoundEffectTrack;

// Where a movie's frames (32-bit BGRA, top row first, with their time) and
// soundtrack (interleaved float samples) come from.
class MovieSource {
public:
    virtual ~MovieSource() = default;
    // The whole soundtrack; false when there is none.
    virtual bool readAudio(std::vector<float>& pcm, int& channels, int& sampleRate) = 0;
    // The soundtrack piece by piece (platform::decodeAudioFileStreamed's
    // way), on a worker thread: by default the whole of it at once.
    virtual bool streamAudio(const platform::AudioFormatFn& format, const platform::AudioSamplesFn& samples) {
        std::vector<float> pcm;
        int channels = 0, rate = 0;
        if (!readAudio(pcm, channels, rate)) return false;
        format(channels, rate);
        samples(pcm.data(), pcm.size());
        return true;
    }
    // The next frame; false at the end.
    virtual bool nextFrame(std::vector<unsigned char>& bgra, int& width, int& height, double& seconds) = 0;
};

class MoviePlayer {
public:
    // nullptr when the bundle file is missing or cannot be decoded.
    static std::unique_ptr<MoviePlayer> open(const std::string& bundleName);
    ~MoviePlayer();

    // Starts the clock and the soundtrack.
    void start();
    // Moves to the frame due now; false once the movie is over.
    bool update();
    void draw();
    // MPMovieScalingModeAspectFill (true, the default here) or AspectFit.
    bool aspectFill = true;

private:
    MoviePlayer() = default;

    std::unique_ptr<MovieSource> decoder;
    SoundEffectTrack* soundtrack = nullptr;
    GLuint texture = 0;
    std::vector<unsigned char> pending;  // the next frame, decoded ahead
    int width = 0, height = 0;
    double pendingTime = 0, startTime = 0;
    bool havePending = false, haveFrame = false, finished = false;
};
