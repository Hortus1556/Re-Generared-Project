#include "engine/RMSManager.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "platform/Platform.h"

namespace {

// Keys and values are written on one line each; escape the separators.
std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            out += n == 't' ? '\t' : n == 'n' ? '\n' : n;
        } else {
            out += s[i];
        }
    }
    return out;
}

}  // namespace

std::string RMSManager::filePath() const { return platform::prefPath() + filename; }

void RMSManager::loadFromRMS() {
    size_t n = 0;
    void* raw = SDL_LoadFile(filePath().c_str(), &n);
    if (!raw) return;  // no save yet
    std::istringstream in(std::string(static_cast<const char*>(raw), n));
    SDL_free(raw);
    recordStore.clear();
    std::string line;
    while (std::getline(in, line)) {
        std::string::size_type tab = line.find('\t');
        if (tab != std::string::npos) recordStore[unescape(line.substr(0, tab))] = unescape(line.substr(tab + 1));
    }
}

bool RMSManager::saveIntoRMS() const {
    std::string text;
    for (const auto& [k, v] : recordStore) text += escape(k) + "\t" + escape(v) + "\n";
    // Write to a temporary file and rename over the save, so a crash mid-write
    // cannot leave a truncated save behind (the original wrote atomically too).
    std::string tmp = filePath() + ".tmp";
    if (!SDL_SaveFile(tmp.c_str(), text.data(), text.size())) {
        SDL_Log("Could not write %s: %s", tmp.c_str(), SDL_GetError());
        return false;
    }
    if (!SDL_RenamePath(tmp.c_str(), filePath().c_str())) {
        SDL_Log("Could not replace %s: %s", filePath().c_str(), SDL_GetError());
        return false;
    }
    return true;
}

std::optional<std::string> RMSManager::getRecordByKey(const std::string& key) const {
    auto it = recordStore.find(key);
    if (it == recordStore.end()) return std::nullopt;
    return it->second;
}

int RMSManager::getIntByKey(const std::string& key) const {
    auto v = getRecordByKey(key);
    return v ? std::atoi(v->c_str()) : kMissing;  // -intValue
}

void RMSManager::insertFloat(float value, const std::string& key) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", value);
    insertRecord(buf, key);
}
