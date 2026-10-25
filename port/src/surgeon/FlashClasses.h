// Flash's built-in classes as the games' translated scripts use them
// (Script.h): Sound (a movie's exported sounds, played with their volume
// and loops; onSoundComplete), XML (a file of the game's package read into
// nodes: firstChild, childNodes, nodeName, nodeValue...; onLoad),
// SharedObject (the player's saved data: getLocal, data, flush), and the
// global stopAllSounds().
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "surgeon/Script.h"

class MediaTrack;
class VectorArt;

class FlashClasses {
public:
    FlashClasses(script::Runner& runner, VectorArt& art);
    ~FlashClasses();

    // Once per pass: sounds that finished (onSoundComplete), XML files
    // loaded (onLoad: Flash answers a load on a later frame).
    void update();
    void stopAllSounds();

    // How a SharedObject is kept in the save folder: as `file`, key=value
    // lines (numbers, true/false, text), its keys renamed on the way
    // (`renames`: data key prefix -> file key prefix, e.g. "level" ->
    // "grade"), and `marker` a key the data has (as 0) whenever the file
    // exists, never written. Without one: <name>.txt, keys as they are.
    struct SaveFormat {
        std::string file;
        std::vector<std::pair<std::string, std::string>> renames;
        std::string marker;
    };
    void setSaveFormat(const std::string& name, SaveFormat format) { formats[name] = std::move(format); }

    // The pointer's and the keys' events to the Mouse and Key listeners
    // (addListener), as Flash broadcast them: onMouseDown, onMouseUp,
    // onMouseMove; onKeyDown, onKeyUp, the key's code then Key.getCode().
    void mouseEvent(const char* name);
    void keyEvent(int code, bool down);
    size_t soundCount() const { return sounds.size(); }  // (debug: memory)

private:
    struct SoundHost;
    struct XmlHost;
    struct SharedHost;
    script::Runner& runner;
    VectorArt& art;
    std::vector<std::weak_ptr<script::Object>> sounds;    // the Sound objects
    std::vector<std::pair<std::weak_ptr<script::Object>, bool>> loaded;  // XMLs to answer onLoad
    std::map<std::string, script::ObjectRef> shared;      // SharedObjects by name
    std::map<std::string, SaveFormat> formats;
    std::vector<script::Value> mouseListeners, keyListeners;
    int lastKey = 0;
    std::set<int> keysDown;
    void broadcast(const std::vector<script::Value>& listeners, const char* name);

    // A fresh channel for `linkage` (every Sound object is its own channel,
    // as Flash's is: two of them attached to the same clip play apart).
    MediaTrack* makeTrack(const std::string& linkage);
    void loadShared(const std::string& name, script::Object& data);
    void saveShared(const std::string& name, const script::Object& data);
};
