#include "as2/MyOpenAL.h"

#include <cstdio>

#include "engine/MediaPlayer.h"
#include "engine/RMSManager.h"
#include "platform/Platform.h"

namespace as2 {

MyOpenAL& MyOpenAL::sharedMyOpenAL() {
    static MyOpenAL* instance = [] {
        MyOpenAL* m = new MyOpenAL;
        m->musicOn = SaveDetails::GetMusicOn();
        m->soundFXOn = SaveDetails::GetSFXOn();
        return m;
    }();
    return *instance;
}

SoundEffectTrack* MyOpenAL::find(const std::string& key) const {
    auto it = soundDictionary.find(key);
    return it == soundDictionary.end() ? nullptr : it->second;
}

bool MyOpenAL::loadSoundWithKey(const std::string& key, const std::string& file, const std::string& ext, bool loop) {
    if (soundDictionary.count(key)) return true;
    SoundEffectTrack* t = SoundEffectTrack::initWithName(ext.empty() ? file : file + "." + ext);
    if (!t) {
        platform::debugTrace("sound: cannot load %s.%s for %s", file.c_str(), ext.c_str(), key.c_str());
        return false;
    }
    t->setReplayTimes(loop ? -1 : 1);
    soundDictionary[key] = t;
    return true;
}

void MyOpenAL::unloadSound(const std::string& key) {
    stopSoundWithKey(key);
    auto it = soundDictionary.find(key);
    if (it == soundDictionary.end()) return;
    releaseRef(it->second);
    soundDictionary.erase(it);
}

void MyOpenAL::forcePlaySFXWithKey(const std::string& key) {
    if (!soundFXOn) return;
    if (SoundEffectTrack* t = find(key)) {
        platform::debugTrace("sound %s", key.c_str());
        t->play();
    }
}

void MyOpenAL::playSFXWithKey(const std::string& key) {
    if (!soundFXOn || isPlayingSoundWithKey(key)) return;
    if (SoundEffectTrack* t = find(key)) {
        platform::debugTrace("sound %s", key.c_str());
        t->play();
    }
}

void MyOpenAL::playMusicWithKey(const std::string& key) {
    if (!musicOn) return;
    if (SoundEffectTrack* t = find(key)) t->play();
}

void MyOpenAL::stopSoundWithKey(const std::string& key) {
    if (SoundEffectTrack* t = find(key)) t->stop();
}

void MyOpenAL::rewindSoundWithKey(const std::string& key) {
    // alSourceRewind: stopped, back at the start.
    if (SoundEffectTrack* t = find(key)) t->stop();
}

bool MyOpenAL::isPlayingSoundWithKey(const std::string& key) const {
    SoundEffectTrack* t = find(key);
    return t && t->getMediaState() == MediaTrack::kPlaying;
}

bool MyOpenAL::changeGain(const std::string& key, float gain) {
    SoundEffectTrack* t = find(key);
    if (t) t->setGain(gain);
    return t != nullptr;
}

bool MyOpenAL::changePitch(const std::string& key, float pitch) {
    SoundEffectTrack* t = find(key);
    if (t) t->setPitch(pitch);
    return t != nullptr;
}

// ---------------------------------------------------------------- SaveDetails

namespace SaveDetails {
namespace {

RMSManager& defaults() {
    static RMSManager* store = [] {
        RMSManager* s = RMSManager::initWithFilename("UserDefaults.txt");
        s->loadFromRMS();
        return s;
    }();
    return *store;
}

void setInt(const std::string& key, int v) {
    defaults().insertInt(v, key);
    defaults().saveIntoRMS();
}

bool has(const std::string& key) { return defaults().getRecordByKey(key).has_value(); }
int getInt(const std::string& key) { return has(key) ? defaults().getIntByKey(key) : 0; }

std::string key(const char* fmt, int n) {
    char buf[32];
    std::snprintf(buf, sizeof buf, fmt, n);
    return buf;
}

}  // namespace

void CreateNewSaveDataFile() {
    if (!has("OFUserIDToUse")) {
        defaults().insertRecord("Default", "OFUserIDToUse");
        defaults().saveIntoRMS();
    }
}

void ClearDownForNewGame() {
    for (int level = 1; level <= 30; ++level) SaveScoreForLevel(0, level);
    for (const char* tool : {"Cutter", "Tongs", "Lighter", "Cream", "Stitch", "Injection", "Battery", "Chainsaw",
                             "Vacuum", "Scanner"})
        SaveToolLevel(tool, 1);
    for (int id = 1; id <= 10; ++id) SaveToolUsed(false, id);
}

void SaveScoreForLevel(int score, int level) { setInt(key("Score%i", level), score); }
int GetScoreForLevel(int level) { return getInt(key("Score%i", level)); }
void SaveTimeForLevel(int time, int level) { setInt(key("Time%i", level), time); }
int GetTimeForLevel(int level) { return getInt(key("Time%i", level)); }
bool GetToolUsedToolID(int id) { return getInt(key("Tool%i", id)) != 0; }
void SaveToolUsed(bool used, int id) { setInt(key("Tool%i", id), used ? 1 : 0); }
void SaveToolLevel(const std::string& tool, int level) { setInt(tool, level); }
int GetToolLevel(const std::string& tool) { return has(tool) ? getInt(tool) : 1; }
void SaveSFXOn(bool on) { setInt("SoundFXOn", on ? 1 : 0); }
bool GetSFXOn() { return has("SoundFXOn") ? getInt("SoundFXOn") != 0 : true; }
void SaveMusicOn(bool on) { setInt("MusicOn", on ? 1 : 0); }
bool GetMusicOn() { return has("MusicOn") ? getInt("MusicOn") != 0 : true; }
bool GetResumeGameAvailable() { return getInt("ResumeGameAvailable") != 0; }
void SaveResumeGameAvailable(bool available) { setInt("ResumeGameAvailable", available ? 1 : 0); }

}  // namespace SaveDetails

}  // namespace as2
