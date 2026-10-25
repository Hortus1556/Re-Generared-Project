#include "as3/SoundMgr.h"

#include <cstdlib>

#include "as3/Log.h"
#include "engine/cocos/CCFileUtils.h"
#include "engine/cocos/SimpleAudioEngine.h"
#include "platform/Platform.h"

namespace mt {

using CocosDenshion::SimpleAudioEngine;

namespace {
const char* kDefinitions = "Data/Sounds/ScenesDefinition_ios.json";
const std::string kPrefix = "Sounds/";
// a frame's steps (the original's at 60 frames a second)
constexpr float kFadeOutStep = 0.04f + 0.01f;  // effects fading out
constexpr float kChannelStep = 0.04f;          // a channel's sound toward its volume
constexpr float kMusicStep = 0.01f;

SimpleAudioEngine* audio() { return SimpleAudioEngine::sharedEngine(); }

// a random whole number in [lo, hi] (0x644b0)
int randomInt(int lo, int hi) { return lo + std::rand() % (hi - lo + 1); }

// (debug: AS_TRACE_SOUNDS=1) the channels' sounds as they start and stop,
// and the looping one-shots: "sound <what> ch <channel> <path>[ loop]", so a
// test can tell which sound is left playing
bool tracing() {
    static const bool on = platform::debugMode() && std::getenv("AS_TRACE_SOUNDS") != nullptr;
    return on;
}
void trace(const char* what, int channel, const std::string& path, bool loop) {
    if (tracing()) log("sound %s ch %d %s%s", what, channel, path.c_str(), loop ? " loop" : "");
}
}  // namespace

// A sound's file by its name: Sounds/<name>.caf, as the original's, else a
// restored track put in its place under another extension (extract_ipa.py's
// restored music: AAC .m4a, WAV); the original's path if none is there.
std::string SoundMgr::path(const std::string& name) {
    auto known = m_paths.find(name);
    if (known != m_paths.end()) return known->second;
    std::string found = kPrefix + name + ".caf";
    if (!cocos2d::CCFileUtils::sharedFileUtils()->isFileExist(found))
        for (const char* ext : {".m4a", ".wav"})
            if (cocos2d::CCFileUtils::sharedFileUtils()->isFileExist(kPrefix + name + ext)) {
                found = kPrefix + name + ext;
                break;
            }
    return m_paths[name] = found;
}

SoundMgr* SoundMgr::sharedSoundMgr() {
    static SoundMgr* s = new SoundMgr;
    return s;
}

SoundMgr::SoundMgr() {
    std::string text;
    json::Value root;
    if (cocos2d::CCFileUtils::sharedFileUtils()->readText(kDefinitions, text) && json::parse(text, root)) {
        m_groups[kSurgeryCommon] = root["surgery_common"];
        m_groups[kGameCommon] = root["game_common"];
    }
}

bool SoundMgr::finished(const Entry& e) { return !audio()->isEffectPlaying(e.id); }  // 0x3bb0

void SoundMgr::setGain(unsigned int id, float gain) { audio()->setEffectVolume(id, gain); }  // 0x3b9c

void SoundMgr::preloadGroup(int group) {
    if (group != kSurgeryCommon && group != kGameCommon) return;
    for (const json::Value& n : m_groups[group].items) {
        const std::string& file = n.str();
        audio()->preloadEffect(path(file.substr(0, file.rfind('.'))).c_str());
        if (group != kGameCommon) m_preloaded.push_back(n.str());
    }
}

void SoundMgr::preloadSound(const std::string& name) {
    log("%s name: %s", "preloadSound", name.c_str());
    audio()->preloadEffect(path(name).c_str());
}

void SoundMgr::playMusic(const std::string& name, bool fadeIn) {
    if (name == m_music) return;
    // (the port's) the track a screen faded out, asked for again while it
    // still plays (silent): faded back in where it is, not started over (the
    // original restarted the title tune at every menu screen it came back to)
    if (!name.empty() && name == m_fadedMusic && audio()->isBackgroundMusicPlaying()) {
        m_music = name;
        m_fadedMusic.clear();
        m_musicTarget = m_musicOn ? 1 : 0;
        if (tracing()) log("sound music %s resumed", path(name).c_str());
        return;
    }
    m_fadedMusic.clear();
    m_music = name;
    audio()->playBackgroundMusic(path(name).c_str(), true);
    if (tracing())
        log("sound music %s%s", path(name).c_str(),
            cocos2d::CCFileUtils::sharedFileUtils()->isFileExist(path(name)) ? "" : " MISSING");
    m_musicVolume = fadeIn ? 0 : 1;
    m_musicTarget = 1;
    if (!m_musicOn) m_musicVolume = m_musicTarget = 0;
    audio()->setBackgroundMusicVolume(m_musicVolume);
}

void SoundMgr::stopMusic(bool fade) {
    // (faded: the track plays on silent, so asked for again it resumes)
    if (fade && !m_music.empty()) m_fadedMusic = m_music;
    if (!fade) m_fadedMusic.clear();
    m_music = "";
    if (!fade) audio()->stopBackgroundMusic();
    else m_musicTarget = 0;
}

void SoundMgr::musicOff() {
    m_musicOn = false;
    m_musicVolume = m_musicTarget = 0;
    audio()->setBackgroundMusicVolume(0);
}

void SoundMgr::musicOn() {
    m_musicOn = true;
    m_musicVolume = m_musicTarget = 1;
    audio()->setBackgroundMusicVolume(1);
}

void SoundMgr::playSound(const std::string& name, bool loop) {
    Entry e;
    e.path = path(name);
    e.loop = loop;
    e.id = audio()->playEffect(e.path.c_str(), loop);
    if (!m_soundEffectsOn) setGain(e.id, 0);
    if (loop) trace("start", 0, e.path, true);
    else trace("once", 0, e.path, false);  // (AS_TRACE_SOUNDS: a one-shot played)
    m_oneShots.push_back(e);
}

void SoundMgr::playRandomSound(const std::string& name, int variants, bool loop) {
    playSound(name + "_" + std::to_string(randomInt(1, variants)), loop);
}

bool SoundMgr::channelPlays(const std::string& path) {
    for (auto& c : m_channels)
        if (c.second.index < static_cast<int>(c.second.queue.size()) && c.second.queue[c.second.index].path == path)
            return true;
    return false;
}

void SoundMgr::playPathOnChannel(const std::string& path, int channel, bool loop, bool replace, bool crossFade,
                                 bool fadeIn) {
    if (channelPlays(path)) return;
    Entry e;
    e.path = path;
    e.loop = loop;
    e.volume = fadeIn ? 0 : 1;
    e.target = 1;
    if (!m_soundEffectsOn) e.volume = e.target = 0;
    auto it = m_channels.find(channel);
    if (it == m_channels.end()) {
        e.id = audio()->playEffect(path.c_str(), loop);
        setGain(e.id, e.volume);
        trace("start", channel, path, loop);
        Channel c;
        c.queue.push_back(e);
        m_channels[channel] = c;
        return;
    }
    Channel& c = it->second;
    if (replace) {
        audio()->stopEffect(c.queue[c.index].id);
        trace("stop", channel, c.queue[c.index].path, c.queue[c.index].loop);
        e.id = audio()->playEffect(path.c_str(), loop);
        setGain(e.id, e.volume);
        trace("start", channel, path, loop);
        c.queue.push_back(e);
    } else if (crossFade) {
        c.queue[c.index].target = 0;
        m_fading.push_back(c.queue[c.index]);
        trace("stop", channel, c.queue[c.index].path, c.queue[c.index].loop);
        e.id = audio()->playEffect(path.c_str(), loop);
        setGain(e.id, e.volume);
        trace("start", channel, path, loop);
        c.queue.push_back(e);
    } else {
        c.queue.push_back(e);  // it waits its turn
        trace("queued", channel, path, loop);
        return;
    }
    c.index = static_cast<int>(c.queue.size()) - 1;
}

void SoundMgr::playOnChannel(const std::string& name, int variants, int channel, bool loop, bool replace,
                             bool crossFade, bool fadeIn) {
    playPathOnChannel(path(name + "_" + std::to_string(randomInt(1, variants))), channel, loop, replace, crossFade,
                      fadeIn);
}

void SoundMgr::playOnChannel(const std::string& name, int channel, bool loop, bool replace, bool crossFade,
                             bool fadeIn) {
    playPathOnChannel(path(name), channel, loop, replace, crossFade, fadeIn);
}

bool SoundMgr::ambiencePlays(const std::string& name) {
    auto it = m_channels.find(kAmbienceChannel);
    if (it == m_channels.end()) return false;
    const Channel& c = it->second;
    return c.index < static_cast<int>(c.queue.size()) && c.queue[c.index].path == path(name);
}

void SoundMgr::playAmbience(const std::string& name) {
    if (ambiencePlays(name)) return;
    stopChannel(kAmbienceChannel, false);
    playOnChannel(name, kAmbienceChannel, true, false, false, false);
}

void SoundMgr::stopChannel(int channel, bool fade) {
    auto it = m_channels.find(channel);
    if (it == m_channels.end()) return;
    Entry& e = it->second.queue[it->second.index];
    trace("stop", channel, e.path, e.loop);
    if (!fade) {
        audio()->stopEffect(e.id);
    } else {
        e.target = 0;
        m_fading.push_back(e);
    }
    m_channels.erase(it);
}

void SoundMgr::stopLoops(int channel, bool fade) {
    auto it = m_channels.find(channel);
    if (it == m_channels.end()) return;
    Channel& c = it->second;
    if (c.queue[c.index].loop) {
        stopChannel(channel, fade);
        return;
    }
    for (size_t k = c.queue.size(); k-- > static_cast<size_t>(c.index) + 1;)
        if (c.queue[k].loop) {
            trace("dropped", channel, c.queue[k].path, true);
            c.queue.erase(c.queue.begin() + static_cast<long>(k));
        }
}

void SoundMgr::stopAllSounds() {
    trace("stop all", 0, "", false);
    for (auto& c : m_channels) {
        Entry& e = c.second.queue[c.second.index];
        if (!finished(e)) audio()->stopEffect(e.id);
    }
    m_channels.clear();
    for (Entry& e : m_oneShots) audio()->stopEffect(e.id);
    m_oneShots.clear();
}

void SoundMgr::soundEffectsOff() {
    m_soundEffectsOn = false;
    for (auto& c : m_channels) {
        Entry& e = c.second.queue[c.second.index];
        e.volume = e.target = 0;
        setGain(e.id, 0);
    }
    for (Entry& e : m_oneShots) {
        e.volume = e.target = 0;
        setGain(e.id, 0);
    }
}

void SoundMgr::soundEffectsOn() {
    m_soundEffectsOn = true;
    for (auto& c : m_channels) {
        Entry& e = c.second.queue[c.second.index];
        e.volume = e.target = 1;
        setGain(e.id, 1);
    }
    for (Entry& e : m_oneShots) {
        e.volume = e.target = 1;
        setGain(e.id, 1);
    }
}

void SoundMgr::pauseSounds() {
    for (auto& c : m_channels) {
        Entry& e = c.second.queue[c.second.index];
        if (!finished(e)) audio()->pauseEffect(e.id);
    }
    for (Entry& e : m_oneShots) audio()->pauseEffect(e.id);
}

void SoundMgr::resumeSounds() {
    for (auto& c : m_channels) audio()->resumeEffect(c.second.queue[c.second.index].id);
    for (Entry& e : m_oneShots) audio()->resumeEffect(e.id);
}

void SoundMgr::update(float) {
    // The ones fading out: down a step each, and the first gone quiet stopped
    // (one a frame, as the original).
    for (size_t i = 0; i < m_fading.size(); ++i) {
        Entry& e = m_fading[i];
        e.volume -= kFadeOutStep;
        setGain(e.id, e.volume);
        if (e.volume <= 0) {
            audio()->stopEffect(e.id);
            m_fading.erase(m_fading.begin() + i);
            break;
        }
    }
    // The channels: one whose sound ended goes on to the next in its queue (or
    // is done); a playing one eases toward its volume. One change a frame.
    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        Channel& c = it->second;
        Entry& e = c.queue[c.index];
        if (finished(e) && !e.loop) {
            if (c.index == static_cast<int>(c.queue.size()) - 1) {
                trace("ended", it->first, e.path, false);
                m_channels.erase(it);
            } else {
                trace("ended", it->first, e.path, false);
                c.queue.erase(c.queue.begin() + c.index);
                Entry& next = c.queue[c.index];
                if (!m_soundEffectsOn) next.volume = next.target = 0;
                next.id = audio()->playEffect(next.path.c_str(), next.loop);
                setGain(next.id, next.volume);
                trace("start", it->first, next.path, next.loop);
            }
            break;
        }
        if (e.target <= e.volume) {
            e.volume = e.target;
        } else if (m_soundEffectsOn) {
            e.volume += kChannelStep;
            setGain(e.id, e.volume);
        }
    }
    // The one-shots: the first that ended let go (one a frame).
    for (size_t i = 0; i < m_oneShots.size(); ++i)
        if (finished(m_oneShots[i]) && !m_oneShots[i].loop) {
            m_oneShots.erase(m_oneShots.begin() + i);
            break;
        }
    // The music toward its target.
    if (!m_musicOn) return;
    if (m_musicVolume <= m_musicTarget) {
        m_musicVolume += kMusicStep;
        if (m_musicTarget < m_musicVolume) m_musicVolume = m_musicTarget;
    } else {
        m_musicVolume -= kMusicStep;
        if (m_musicVolume < m_musicTarget) m_musicVolume = m_musicTarget;
    }
    audio()->setBackgroundMusicVolume(m_musicVolume);
}

}  // namespace mt
