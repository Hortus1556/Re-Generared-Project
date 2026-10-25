// mt::SoundMgr (a singleton, 0x3045a8; 0x74 bytes): the game's sounds, on
// CocosDenshion's SimpleAudioEngine (the engine's cocos layer). Names are
// files in Sounds/ ("Sounds/<name>.caf", or a restored track's own
// extension: path()).
//
// - Music: one track by name (0x304698), faded in if asked; its volume eases
//   to a target 0.01 a frame (60 a second), so stopMusic(fade) fades it out.
//   A faded track plays on silent: asked for again, it fades back in where
//   it is (the port's: menu screens no longer restart the title tune).
// - Effects: one-shots (playSound 0x304d80, a random numbered variant
//   0x304f58), and channels (0x305144, 0x305780): a channel plays one sound
//   at a time from a queue; a new one replaces the current, fades it out
//   (0.05 a frame) or waits behind it.
// - The groups of sounds to preload are Data/Sounds/ScenesDefinition_ios.json's.
// - Sound effects and music each have their switch (the settings').
// The method names are the port's (the binary has none).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Json.h"

namespace mt {

class SoundMgr {
public:
    static SoundMgr* sharedSoundMgr();  // 0x3045a8

    enum Group { kSurgeryCommon = 0, kGameCommon = 1 };
    void preloadGroup(int group);                                  // 0x30496c
    void preloadSound(const std::string& name);                    // 0x304bac
    void playMusic(const std::string& name, bool fadeIn);          // 0x304698
    void stopMusic(bool fade);                                     // 0x304768
    void musicOff();                                               // 0x304880
    void musicOn();                                                // 0x3048a0
    void playSound(const std::string& name, bool loop);            // 0x304d80
    void playRandomSound(const std::string& name, int variants, bool loop);  // 0x304f58
    // on a channel: a numbered variant (0x305144), or the name itself (0x305780)
    void playOnChannel(const std::string& name, int variants, int channel, bool loop, bool replace,
                       bool crossFade, bool fadeIn);
    void playOnChannel(const std::string& name, int channel, bool loop, bool replace, bool crossFade, bool fadeIn);
    void stopChannel(int channel, bool fade);                      // 0x3047ec
    // the port's: a channel's looping sounds stopped (its current one if it
    // loops, faded or not; the loops waiting behind it dropped), a one-shot
    // playing left to end (a finished injury's: SurgeryScreen::injuryRemoved)
    void stopLoops(int channel, bool fade);
    void stopAllSounds();                                          // 0x3048c8
    void soundEffectsOff();                                        // 0x305bc0
    void soundEffectsOn();                                         // 0x305c28
    void pauseSounds();                                            // 0x305c9c
    void resumeSounds();                                           // 0x305cfc
    void update(float dt);                                         // 0x305574

    bool areSoundEffectsOn() const { return m_soundEffectsOn; }
    bool isMusicOn() const { return m_musicOn; }
    // the channel the game keeps its ambience on (the original used an
    // address of its own, +0x70, as the channel's key)
    static constexpr int kAmbienceChannel = -1;
    // (the port's) a screen's ambience loop on that channel: kept going if
    // it is the one playing there (a screen coming back, or another with the
    // same loop, no longer starts it over), else the channel's sound stopped
    // and this one started (the original stopped and started it every time)
    void playAmbience(const std::string& name);
    bool ambiencePlays(const std::string& name);

private:
    SoundMgr();
    struct Entry {  // 0x14 bytes
        std::string path;
        float volume = 1, target = 1;
        unsigned int id = 0;
        bool loop = false;
    };
    struct Channel {
        int index = 0;  // the playing one
        std::vector<Entry> queue;
    };
    static bool finished(const Entry& e);
    std::string path(const std::string& name);
    std::map<std::string, std::string> m_paths;  // path()'s, by name
    void setGain(unsigned int id, float gain);
    bool channelPlays(const std::string& path);  // 0x3047b0
    void playPathOnChannel(const std::string& path, int channel, bool loop, bool replace, bool crossFade,
                           bool fadeIn);

    json::Value m_groups[2];               // +0, +0x10: the preload groups
    std::map<int, Channel> m_channels;     // +0x20
    std::vector<Entry> m_fading;           // +0x38
    std::vector<Entry> m_oneShots;         // +0x48
    std::vector<std::string> m_preloaded;  // +0x54
    bool m_soundEffectsOn = true;          // +0x60
    bool m_musicOn = true;                 // +0x61
    float m_musicVolume = 1, m_musicTarget = 1;  // +0x64, +0x68
    std::string m_music;                   // +0x6c
    std::string m_fadedMusic;              // (the port's) the one stopMusic faded out, still playing silent
};

}  // namespace mt
