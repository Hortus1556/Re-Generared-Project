#include "as3/VideoPlayer.h"

#include "engine/MoviePlayer.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCNode.h"

namespace mt {

// The node that shows the movie: updated and drawn as the director draws.
class MovieNode : public cocos2d::CCNode {
public:
    void draw() override {
        VideoPlayer* v = VideoPlayer::sharedVideoPlayer();
        if (!v->m_movie) return;
        if (!v->m_movie->update()) {  // over
            v->m_movie.reset();
            return;
        }
        v->m_movie->draw();
    }
};

VideoPlayer* VideoPlayer::sharedVideoPlayer() {
    static VideoPlayer* v = new VideoPlayer;
    return v;
}

void VideoPlayer::playVideo(const std::string& name, const std::string& ext) {
    m_movie = MoviePlayer::open(name + "." + ext);
    if (!m_movie) return;
    m_movie->aspectFill = true;
    m_movie->start();
    cocos2d::CCNode* node = new MovieNode;
    cocos2d::CCDirector::sharedDirector()->setNotificationNode(node);
    node->release();
}

bool VideoPlayer::isPlaying() { return m_movie != nullptr; }

void VideoPlayer::stop() {
    m_movie.reset();
    cocos2d::CCDirector::sharedDirector()->setNotificationNode(nullptr);
}

}  // namespace mt
