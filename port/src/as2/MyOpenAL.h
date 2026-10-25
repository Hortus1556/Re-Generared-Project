// MyOpenAL: every sound by key, one OpenAL source each; effects and music
// switched on and off by the player (SaveDetails). On the engine's tracks.
#pragma once

#include <map>
#include <string>

class SoundEffectTrack;

namespace as2 {

class MyOpenAL {
public:
    static MyOpenAL& sharedMyOpenAL();

    bool musicOn = true;
    bool soundFXOn = true;

    // File.Ext from the bundle under key, looped or not; does nothing if the
    // key is loaded already.
    bool loadSoundWithKey(const std::string& key, const std::string& file, const std::string& ext, bool loop);
    void unloadSound(const std::string& key);
    // alSourcePlay: starts it from the beginning (also when playing).
    void forcePlaySFXWithKey(const std::string& key);
    // Starts it unless it is playing.
    void playSFXWithKey(const std::string& key);
    void playMusicWithKey(const std::string& key);
    void stopSoundWithKey(const std::string& key);
    void rewindSoundWithKey(const std::string& key);
    bool isPlayingSoundWithKey(const std::string& key) const;
    bool changeGain(const std::string& key, float gain);
    bool changePitch(const std::string& key, float pitch);

private:
    SoundEffectTrack* find(const std::string& key) const;
    std::map<std::string, SoundEffectTrack*> soundDictionary;  // retained
};

// SaveDetails: progress in NSUserDefaults, kept as UserDefaults.txt in the
// save folder. The original also kept per-OpenFeint-user copies of the keys
// ("Score_<user>_<level>"); with OpenFeint removed the user is always
// "Default", whose keys are the plain ones.
namespace SaveDetails {
void CreateNewSaveDataFile();
void ClearDownForNewGame();
void SaveScoreForLevel(int score, int level);
int GetScoreForLevel(int level);
void SaveTimeForLevel(int time, int level);
int GetTimeForLevel(int level);
bool GetToolUsedToolID(int toolId);
void SaveToolUsed(bool used, int toolId);
void SaveToolLevel(const std::string& tool, int level);
int GetToolLevel(const std::string& tool);
void SaveSFXOn(bool on);
bool GetSFXOn();
void SaveMusicOn(bool on);
bool GetMusicOn();
bool GetResumeGameAvailable();
void SaveResumeGameAvailable(bool available);
}  // namespace SaveDetails

}  // namespace as2
