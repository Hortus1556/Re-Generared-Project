// A vector animation played as a movie: frames rasterised from SVG as they
// are needed, at the display's resolution, instead of decoded from video.
//
// The file (written by tools/xmas_intro.py from the original Flash game) is
// an SVG whose <defs> hold every shape once, each wrapped as <g id="u_<key>">,
// and whose frames are hidden groups <g id="f<n>" display="none"
// data-defs="<keys>"> listing the definitions they use; the root carries
// data-frames and data-fps. For each frame a small SVG of just that frame is
// put together and rendered with lunasvg on worker threads, a few frames
// ahead of the clock. The soundtrack is the .mp3 of the same name.
#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/MoviePlayer.h"

class VectorMovie : public MovieSource {
public:
    // nullptr when the bundle file is missing or not a vector movie.
    static std::unique_ptr<VectorMovie> open(const std::string& bundleName);
    ~VectorMovie() override;

    bool readAudio(std::vector<float>& pcm, int& channels, int& sampleRate) override;
    bool nextFrame(std::vector<unsigned char>& bgra, int& width, int& height, double& seconds) override;

private:
    VectorMovie() = default;
    std::string frameDocument(int frame) const;
    void work();

    std::string text, audioName;
    std::string head;                                      // the root element and background
    std::map<std::string, std::pair<size_t, size_t>> units;  // definition -> its text's span
    struct Frame {
        size_t begin = 0, end = 0;
        std::vector<std::string> defs;
    };
    std::vector<Frame> frames;
    double fps = 25;
    int width = 0, height = 0;  // pixels of the rasterised frames

    std::vector<std::thread> workers;
    std::mutex mutex;
    std::condition_variable changed;
    std::map<int, std::vector<unsigned char>> done;  // rendered frames not yet shown
    int nextToRender = 0, nextToShow = 0;
    bool stopping = false;
};
