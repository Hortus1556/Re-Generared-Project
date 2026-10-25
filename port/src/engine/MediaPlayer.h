// Music and sound effects (port of MediaPlayer, MediaTrack,
// SoundEffectTrack, BackgroundMusicTrack and OALSoundEngine), on miniaudio.
//
// Tracks are keyed by bundle file name plus an optional sub-key, so several
// instances of one sound can play at once. Volumes are 0..100. A sound
// effect's replayTimes is how often it plays (-1 forever); music always
// loops. Effective effect gain = volume / 100 * soundEffectMasterVolume,
// where the master volume is set as 0..100 and reported as 0..1.
// Without an audio device every call is a silent no-op.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/Object.h"
#include "platform/MovieDecoder.h"

// A package sound decoded whole into interleaved float samples (what a
// sound effect is made of: CAF, MP3, WAV, AAC); false if it cannot be read.
bool decodeSound(const std::string& name, std::vector<float>& pcm, int& channels, int& sampleRate);

class MediaTrack : public Object {
public:
    enum State { kIdle = 0, kPlaying = 1, kPaused = 2, kStopped = 3 };

    ~MediaTrack() override;

    virtual void play();
    void stop();
    void pause();
    void resume();
    // (debug) its mixer state: at its end, its cursor and length (frames), seconds since play()
    std::string debugState() const;
    int getMediaState() const;
    void setVolume(int v);
    int getVolume() const { return volume; }
    // A gain on top of the volume (OpenAL's AL_GAIN, AS2), 1 by default.
    void setGain(float g);
    // Playback speed and pitch together (AL_PITCH), 1 by default.
    void setPitch(float p);
    void setReplayTimes(int n);
    void setIsEnable(bool e);
    void setIsMute(bool m);

    // Restarts finite repeats; called once per game pass.
    void update();

protected:
    struct Audio;  // miniaudio objects, defined in MediaPlayer.cpp
    explicit MediaTrack(bool isSoundEffect);
    void applyGain();

    std::unique_ptr<Audio> audio;  // null when the file could not be played
    bool isSoundEffect;
    int mediaState = kIdle;
    int volume = 100;
    float gain = 1;
    int replayTimes = 1;  // -1: forever
    int playsLeft = 0;
    double startMs = 0, pausedMs = 0;  // engine time it started / was paused (repeats)
    bool enabled = true;
    bool muted = false;
};

class SoundEffectTrack : public MediaTrack {
public:
    static SoundEffectTrack* initWithName(const std::string& name);
    // A sound from already decoded interleaved float samples.
    static SoundEffectTrack* initWithPcm(std::vector<float> pcm, int channels, int sampleRate);
    // A sound from interleaved 16-bit samples shared with every other track
    // playing them (a Flash movie's sound, each Sound object its own track).
    static SoundEffectTrack* initShared(std::shared_ptr<const std::vector<int16_t>> pcm, int channels, int sampleRate);
    // A sound played as it is decoded (a movie's soundtrack): `produce` runs
    // on a worker thread, giving the format and then the samples
    // (platform::decodeAudioFileStreamed's way); the track is made once the
    // format is known. Null if it gives none.
    using Producer = std::function<bool(const platform::AudioFormatFn&, const platform::AudioSamplesFn&)>;
    static SoundEffectTrack* initStreamed(Producer produce);

private:
    SoundEffectTrack() : MediaTrack(true) {}
};

class BackgroundMusicTrack : public MediaTrack {
public:
    static BackgroundMusicTrack* initWithName(const std::string& name);
    void play() override;

private:
    BackgroundMusicTrack() : MediaTrack(false) {}
};

class MediaPlayer : public Object {
public:
    MediaPlayer() = default;
    ~MediaPlayer() override { releaseAllMediaTrack(); }

    // Returns the existing track for the key, or loads it (null if the file
    // cannot be decoded or there is no audio device).
    SoundEffectTrack* allocSoundEffectTrack(const std::string& name, const std::string& subKey = "");
    SoundEffectTrack* allocSoundEffectTrackWithReplayTimes(const std::string& name, const std::string& subKey,
                                                           int replayTimes);
    SoundEffectTrack* allocSoundEffectTrackWithVolumeReplayTimes(const std::string& name, const std::string& subKey,
                                                                 int volume, int replayTimes);
    // Replaces the current music.
    BackgroundMusicTrack* allocBackgroundMusicTrack(const std::string& name, const std::string& subKey = "");

    MediaTrack* getTrack(const std::string& name, const std::string& subKey = "") const;
    void playTrack(const std::string& name, const std::string& subKey = "");
    void stopTrack(const std::string& name, const std::string& subKey = "");
    void releaseBackgroundMusicTrack();
    void releaseAllMediaTrack();


    float getSoundEffectMasterVolume() const;
    void setSoundEffectMasterVolume(int v);
    void setBackgroundMusicMasterVolume(int v);

    bool isSoundEffectTrackEnabled() const { return soundEffectsEnabled; }
    bool isBackgroundMusicTrackEnabled() const { return musicEnabled; }
    void setIsAllSoundEffectTrackEnable(bool e);
    // Christmas's music switch: the music track (now and later ones) on or off.
    void setIsBackgroundMusicTrackEnable(bool e);

    // Called once per game pass.
    void update();

private:
    static std::string key(const std::string& name, const std::string& subKey) { return name + subKey; }

    std::map<std::string, MediaTrack*> mediaTrackList;  // retained
    BackgroundMusicTrack* backgroundMusicTrack = nullptr;  // retained, also in the list
    std::string backgroundMusicKey;
    bool soundEffectsEnabled = true;
    bool musicEnabled = true;
    bool soundEffectsMuted = false;
    bool musicMuted = false;
};
