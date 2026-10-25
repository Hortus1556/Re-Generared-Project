// CocosDenshion's SimpleAudioEngine, ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/SimpleAudioEngine.h"

#include "engine/MediaPlayer.h"
#include "engine/cocos/CCFileUtils.h"

namespace CocosDenshion {

static SimpleAudioEngine* s_engine = nullptr;

SimpleAudioEngine* SimpleAudioEngine::sharedEngine() {
    if (!s_engine) s_engine = new SimpleAudioEngine;
    return s_engine;
}

void SimpleAudioEngine::end() {
    if (!s_engine) return;
    s_engine->stopAllEffects();
    s_engine->stopBackgroundMusic(true);
    delete s_engine;
    s_engine = nullptr;
}

static std::string fullPath(const char* path) {
    return cocos2d::CCFileUtils::sharedFileUtils()->fullPathForFilename(path);
}

// ---- music

void SimpleAudioEngine::playBackgroundMusic(const char* path, bool loop) {
    stopBackgroundMusic(true);
    m_musicPath = fullPath(path);
    BackgroundMusicTrack* t = BackgroundMusicTrack::initWithName(m_musicPath);
    t->setReplayTimes(loop ? -1 : 1);
    t->setGain(m_musicVolume);
    t->play();
    m_music = t;
}

void SimpleAudioEngine::stopBackgroundMusic(bool) {
    if (!m_music) return;
    m_music->stop();
    m_music->release();
    m_music = nullptr;
}

void SimpleAudioEngine::pauseBackgroundMusic() {
    if (m_music) m_music->pause();
}

void SimpleAudioEngine::resumeBackgroundMusic() {
    if (m_music) m_music->resume();
}

void SimpleAudioEngine::rewindBackgroundMusic() {
    if (m_music) m_music->play();
}

bool SimpleAudioEngine::isBackgroundMusicPlaying() {
    return m_music && m_music->getMediaState() == MediaTrack::kPlaying;
}

void SimpleAudioEngine::setBackgroundMusicVolume(float volume) {
    m_musicVolume = volume < 0 ? 0 : volume > 1 ? 1 : volume;
    if (m_music) m_music->setGain(m_musicVolume);
}

// ---- effects

void SimpleAudioEngine::applyEffectGain(Effect& e) { e.track->setGain(e.volume * m_effectsVolume); }

void SimpleAudioEngine::setEffectsVolume(float volume) {
    m_effectsVolume = volume < 0 ? 0 : volume > 1 ? 1 : volume;
    for (auto& e : m_effects) applyEffectGain(e.second);
}

void SimpleAudioEngine::preloadEffect(const char* path) {
    std::string p = fullPath(path);
    if (m_pcm.count(p)) return;
    std::vector<float> pcm;
    int channels = 0, rate = 0;
    if (!decodeSound(p, pcm, channels, rate)) return;
    m_pcm[p] = std::move(pcm);
    m_format[p] = {channels, rate};
}

void SimpleAudioEngine::unloadEffect(const char* path) {
    std::string p = fullPath(path);
    m_pcm.erase(p);
    m_format.erase(p);
}

unsigned int SimpleAudioEngine::playEffect(const char* path, bool loop) {
    preloadEffect(path);
    std::string p = fullPath(path);
    auto it = m_pcm.find(p);
    if (it == m_pcm.end()) return 0;
    std::pair<int, int> f = m_format[p];
    SoundEffectTrack* t = SoundEffectTrack::initWithPcm(it->second, f.first, f.second);
    if (!t) return 0;
    unsigned int id = m_nextId++;
    Effect& e = m_effects[id];
    e.track = t;
    t->setReplayTimes(loop ? -1 : 1);
    applyEffectGain(e);
    t->play();
    return id;
}

void SimpleAudioEngine::pauseEffect(unsigned int id) {
    auto it = m_effects.find(id);
    if (it != m_effects.end()) it->second.track->pause();
}

void SimpleAudioEngine::pauseAllEffects() {
    for (auto& e : m_effects) e.second.track->pause();
}

void SimpleAudioEngine::resumeEffect(unsigned int id) {
    auto it = m_effects.find(id);
    if (it != m_effects.end()) it->second.track->resume();
}

void SimpleAudioEngine::resumeAllEffects() {
    for (auto& e : m_effects) e.second.track->resume();
}

void SimpleAudioEngine::stopEffect(unsigned int id) {
    auto it = m_effects.find(id);
    if (it == m_effects.end()) return;
    it->second.track->stop();
    it->second.track->release();
    m_effects.erase(it);
}

void SimpleAudioEngine::stopAllEffects() {
    for (auto& e : m_effects) {
        e.second.track->stop();
        e.second.track->release();
    }
    m_effects.clear();
}

void SimpleAudioEngine::setEffectVolume(unsigned int id, float volume) {
    auto it = m_effects.find(id);
    if (it == m_effects.end()) return;
    it->second.volume = volume < 0 ? 0 : volume;
    applyEffectGain(it->second);
}

bool SimpleAudioEngine::isEffectPlaying(unsigned int id) {
    auto it = m_effects.find(id);
    return it != m_effects.end() && it->second.track->getMediaState() == MediaTrack::kPlaying;
}

void SimpleAudioEngine::updateShared() {
    if (s_engine) s_engine->update();
}

void SimpleAudioEngine::update() {
    if (m_music) m_music->update();
    for (auto it = m_effects.begin(); it != m_effects.end();) {
        it->second.track->update();
        if (it->second.track->getMediaState() != MediaTrack::kPlaying &&
            it->second.track->getMediaState() != MediaTrack::kPaused) {
            it->second.track->release();
            it = m_effects.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace CocosDenshion
