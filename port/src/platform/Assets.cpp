#include "platform/Assets.h"

#include "platform/Platform.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>

namespace assets {
namespace {

// Must match CATEGORY and CONVERTED in port/tools/extract_ipa.py.
const char* categoryFor(const std::string& name) {
    std::string::size_type dot = name.rfind('.');
    if (dot == std::string::npos) return nullptr;
    std::string ext = name.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if (ext == "png" || ext == "jpg") return "images";
    if (ext == "mp3" || ext == "wav" || ext == "m4a" || ext == "caf") return "audio";
    if (ext == "json" || ext == "plist") return "data";
    if (ext == "nib") return "ui";
    if (ext == "mp4" || ext == "m4v") return "video";
    if (ext == "txt" || ext == "anu") return "anim";
    if (ext == "xml") return "dialogue";
    if (ext == "pod") return "models";
    if (ext == "swf") return "flash";
    return nullptr;
}

// The name extract_ipa.py gives a file it converts (its CONVERTED): compiled
// nibs and property lists are kept as JSON. (CAF sounds stay CAF:
// platform/AudioFile decodes them.)
std::string converted(const std::string& name) {
    std::string::size_type dot = name.rfind('.');
    if (dot == std::string::npos) return name;
    std::string ext = name.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if (ext == "nib" || ext == "plist") return name.substr(0, dot) + ".json";
    return name;
}

bool fileExists(const std::string& p) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(p.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

}  // namespace

const std::string& root() {
    static std::string r = [] {
#if defined(__ANDROID__)
        // Inside the APK: its assets' `game/` folder (SDL reads APK assets by
        // a relative path).
        return std::string("game/");
#endif
        std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
        // Next to the exe (installed layout), else the game's folder in the
        // source tree when running from port/build.
        for (std::string rel : {std::string("assets/"), "../" + std::string(platform::game().assetsDir) + "/"}) {
            std::string candidate = base + rel;
            SDL_PathInfo info;
            if (SDL_GetPathInfo(candidate.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY)
                return candidate;
        }
        return std::string();
    }();
    return r;
}

std::string path(const std::string& bundleName) {
    if (root().empty() || bundleName.empty()) return "";
    std::string p;
    if (bundleName.find('/') != std::string::npos) {
        p = root() + converted(bundleName);  // kept its bundle subfolder (lang/..., AS3's Sounds/, 4.0x/)
    } else {
        // Bundle files by type (AS2's catalogue is data/library.json); else a
        // game package's own files at its root (a Flash package's
        // library.json: Christmas's).
        if (const char* cat = categoryFor(bundleName)) {
            p = root() + cat + "/" + converted(bundleName);
            if (fileExists(p)) return p;
        }
        p = root() + bundleName;
    }
    return fileExists(p) ? p : "";
}

std::string path(const std::string& name, const std::string& ext) {
    return path(ext.empty() ? name : name + "." + ext);
}

bool exists(const std::string& bundleName) { return !path(bundleName).empty(); }

bool read(const std::string& bundleName, std::vector<unsigned char>& out) {
    std::string p = path(bundleName);
    if (p.empty()) return false;
    size_t n = 0;
    void* data = SDL_LoadFile(p.c_str(), &n);  // UTF-8 path safe, unlike fopen on Windows
    if (!data) return false;
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    out.assign(bytes, bytes + n);
    SDL_free(data);
    return true;
}

bool readText(const std::string& bundleName, std::string& out) {
    std::vector<unsigned char> bytes;
    if (!read(bundleName, bytes)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

}  // namespace assets
