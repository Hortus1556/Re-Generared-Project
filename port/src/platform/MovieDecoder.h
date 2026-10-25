// Movie and sound file decoding (the iPad's MPMoviePlayerController did this), on
// Windows Media Foundation, on Android the NDK's media codecs: video frames
// as 32-bit BGRA, top row first, with their time; the soundtrack as
// interleaved float samples.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace platform {

// A sound file the platform reads (AAC in .m4a, AS2's and AS3's music)
// decoded whole into interleaved float samples; false if it cannot.
bool decodeAudioFile(const std::string& path, std::vector<float>& pcm, int& channels, int& sampleRate);

// The same piece by piece, for playing while it decodes (on Android the
// codec is slow: a movie's soundtrack took seconds whole): `format` once,
// with the channels and rate, before any samples; then `samples` with each
// piece of interleaved float samples in order (the gapless trim done),
// which may return false to stop. False if nothing could be read. (On
// Windows, where Media Foundation is quick, the whole sound in one piece.)
using AudioFormatFn = std::function<void(int channels, int sampleRate)>;
using AudioSamplesFn = std::function<bool(const float* pcm, size_t count)>;
bool decodeAudioFileStreamed(const std::string& path, const AudioFormatFn& format, const AudioSamplesFn& samples);

class MovieDecoder {
public:
    // nullptr when the file cannot be opened or has no video.
    static std::unique_ptr<MovieDecoder> open(const std::string& path);
    ~MovieDecoder();

    // The whole soundtrack; false when there is none.
    bool readAudio(std::vector<float>& pcm, int& channels, int& sampleRate);
    // The soundtrack piece by piece (decodeAudioFileStreamed); may be called
    // on another thread than the frames.
    bool streamAudio(const AudioFormatFn& format, const AudioSamplesFn& samples);
    // The next frame; false at the end.
    bool nextFrame(std::vector<unsigned char>& bgra, int& width, int& height, double& seconds);

    struct Impl;

private:
    explicit MovieDecoder(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl;
};

}  // namespace platform
