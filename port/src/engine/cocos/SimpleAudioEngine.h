// CocosDenshion's SimpleAudioEngine (cocos2d-x 2.1.1), ported to Bleed
// Engine's audio (engine/MediaPlayer's tracks, miniaudio). MIT:
// LICENSE-cocos2d-x.txt.
//
// One background music (streamed, looped or not) and any number of effects:
// each playEffect is a new instance with its own id. A sound is decoded once
// (preloadEffect, or on its first play) and kept until unloaded. Besides
// 2.1.1's calls: an effect's own volume and whether it still plays, which the
// iOS CocosDenshion gave per sound source (CDSoundSource) and games used.
// Volumes are 0..1.
#pragma once

#include <map>
#include <string>
#include <vector>

class MediaTrack;

namespace CocosDenshion {

class SimpleAudioEngine {
public:
    static SimpleAudioEngine* sharedEngine();
    static void end();

    void preloadBackgroundMusic(const char* path) {}
    void playBackgroundMusic(const char* path, bool loop = false);
    void stopBackgroundMusic(bool releaseData = false);
    void pauseBackgroundMusic();
    void resumeBackgroundMusic();
    void rewindBackgroundMusic();
    bool willPlayBackgroundMusic() { return true; }
    bool isBackgroundMusicPlaying();
    float getBackgroundMusicVolume() { return m_musicVolume; }
    void setBackgroundMusicVolume(float volume);

    float getEffectsVolume() { return m_effectsVolume; }
    void setEffectsVolume(float volume);
    unsigned int playEffect(const char* path, bool loop = false);
    void pauseEffect(unsigned int id);
    void pauseAllEffects();
    void resumeEffect(unsigned int id);
    void resumeAllEffects();
    void stopEffect(unsigned int id);
    void stopAllEffects();
    void preloadEffect(const char* path);
    void unloadEffect(const char* path);
    // the iOS CocosDenshion's per-source gain and state
    void setEffectVolume(unsigned int id, float volume);
    bool isEffectPlaying(unsigned int id);

    // Once a frame (the director's): finished effects let go. The engine's
    // tracks need it (iOS's OpenAL did not); a no-op until the engine is made.
    static void updateShared();
    void update();

private:
    SimpleAudioEngine() = default;
    struct Effect {
        MediaTrack* track;
        float volume = 1;
    };
    void applyEffectGain(Effect& e);
    std::map<std::string, std::vector<float>> m_pcm;  // decoded sounds by path
    std::map<std::string, std::pair<int, int>> m_format;  // channels, rate
    std::map<unsigned int, Effect> m_effects;
    unsigned int m_nextId = 1;
    MediaTrack* m_music = nullptr;
    std::string m_musicPath;
    float m_musicVolume = 1, m_effectsVolume = 1;
};

}  // namespace CocosDenshion
