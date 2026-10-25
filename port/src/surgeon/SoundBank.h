// The game's sounds by name (port of the Flash games' sound manager clip,
// `mysounds`, and its playSound/stopSound): each name plays one sound
// exported by the movie at its own volume. A name already playing is not
// started again. Names are music, effects, or neither (heard only while
// both are on), so music and effects can be switched off apart.
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class SoundEffectTrack;
class VectorArt;

class SoundBank {
public:
    enum Group { kEffect, kMusic, kOther };
    struct Sound {
        std::string name;     // what the game asks for
        std::string linkage;  // the movie's exported sound
        int volume;           // percent; above 100 amplifies
        Group group;
    };

    SoundBank(VectorArt& art, std::vector<Sound> sounds);
    ~SoundBank();

    // Plays `name` `times` times over (Flash's start(0, times): 0 is once);
    // nothing if it is already playing or its group is off.
    void play(const std::string& name, int times = 1);
    void stop(const std::string& name);
    void stopAll();
    bool playing(const std::string& name) const;
    // Music and effects on or off (what is switched off stops).
    void setEnabled(bool music, bool effects);
    bool musicOn() const { return scripts.musicOn ? scripts.musicOn() : music; }
    bool effectsOn() const { return scripts.effectsOn ? scripts.effectsOn() : effects; }

    void update();  // once per pass

    // A game whose scripts run its sounds (StageScreen): the calls above go
    // to them instead (play: name and times; enable: music, effects).
    struct Scripts {
        std::function<void(const std::string&, int)> play;
        std::function<void(const std::string&)> stop;
        std::function<void(bool, bool)> enable;
        std::function<bool()> musicOn, effectsOn;
    };
    void setScripts(Scripts s) { scripts = std::move(s); }

private:
    struct Entry {
        Sound sound;
        SoundEffectTrack* track = nullptr;
    };
    Entry* find(const std::string& name);
    const Entry* find(const std::string& name) const;
    bool allowed(const Entry& e) const;

    VectorArt& art;
    std::vector<Entry> entries;  // the first of equal names is the one heard
    bool music = true, effects = true;
    Scripts scripts;
};
