// The game's movie player (an Objective-C wrapper of MPMoviePlayerController
// in the original: a singleton at 0x2a9ec4; playVideo:withExtension:,
// isPlaying, stop): the movie over the whole screen, on the engine's
// MoviePlayer, drawn by a node the director shows above the running scene
// (its notification node). Escape stops it (not in the original, which a
// double tap skipped: the screen that plays it handles that).
#pragma once

#include <memory>
#include <string>

class MoviePlayer;

namespace mt {

class VideoPlayer {
public:
    static VideoPlayer* sharedVideoPlayer();  // 0x2a9ec4
    void playVideo(const std::string& name, const std::string& ext);  // 0x2aa3ac
    bool isPlaying();                                                  // 0x2aa414
    void stop();                                                       // 0x2aa478

private:
    VideoPlayer() = default;
    std::unique_ptr<MoviePlayer> m_movie;
    friend class MovieNode;
};

}  // namespace mt
