// cocos2d-x 2.1.1's CCFileUtils.cpp (the search), ported to the engine's
// package lookup. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCFileUtils.h"

#include <cstring>

#include "platform/Assets.h"
#include "platform/Platform.h"

namespace cocos2d {

CCFileUtils* CCFileUtils::sharedFileUtils() {
    static CCFileUtils u;
    return &u;
}

static std::string withSlash(std::string p) {
    if (!p.empty() && p.back() != '/') p += '/';
    return p;
}

void CCFileUtils::setSearchPaths(const std::vector<std::string>& paths) {
    m_fullPathCache.clear();
    m_searchPathArray.clear();
    for (const std::string& p : paths) m_searchPathArray.push_back(withSlash(p));
    // the bundle's root is always searched last (2.1.1 adds the default root path)
    bool hasRoot = false;
    for (const std::string& p : m_searchPathArray) hasRoot = hasRoot || p.empty();
    if (!hasRoot) m_searchPathArray.push_back("");
}

void CCFileUtils::addSearchPath(const char* path) {
    m_fullPathCache.clear();
    m_searchPathArray.push_back(withSlash(path));
}

void CCFileUtils::setSearchResolutionsOrder(const std::vector<std::string>& orders) {
    m_fullPathCache.clear();
    m_searchResolutionsOrderArray.clear();
    for (const std::string& o : orders) m_searchResolutionsOrderArray.push_back(withSlash(o));
    bool hasRoot = false;
    for (const std::string& o : m_searchResolutionsOrderArray) hasRoot = hasRoot || o.empty();
    if (!hasRoot) m_searchResolutionsOrderArray.push_back("");
}

void CCFileUtils::addSearchResolutionsOrder(const char* order) {
    m_fullPathCache.clear();
    m_searchResolutionsOrderArray.push_back(withSlash(order));
}

bool CCFileUtils::isFileExist(const std::string& path) { return !path.empty() && assets::exists(path); }

std::string CCFileUtils::fullPathForFilename(const char* filename) {
    if (!filename || !*filename) return "";
    auto cached = m_fullPathCache.find(filename);
    if (cached != m_fullPathCache.end()) return cached->second;
    std::string name = filename;
    // split into directory and file, as 2.1.1 does
    std::string dir, file = name;
    size_t slash = name.find_last_of('/');
    if (slash != std::string::npos) {
        dir = name.substr(0, slash + 1);
        file = name.substr(slash + 1);
    }
    for (const std::string& search : m_searchPathArray)
        for (const std::string& resolution : m_searchResolutionsOrderArray) {
            std::string p = search + dir + resolution + file;
            if (assets::exists(p)) {
                m_fullPathCache[name] = p;
                return p;
            }
        }
    // not found: the name itself, as 2.1.1 returns it
    return name;
}

std::string CCFileUtils::fullPathFromRelativeFile(const char* filename, const char* relativeFile) {
    std::string rel = relativeFile;
    size_t slash = rel.find_last_of('/');
    std::string dir = slash == std::string::npos ? "" : rel.substr(0, slash + 1);
    // relative to the file's own directory, where the plist was found
    std::string found = fullPathForFilename(relativeFile);
    size_t s2 = found.find_last_of('/');
    if (s2 != std::string::npos) dir = found.substr(0, s2 + 1);
    return dir + filename;
}

bool CCFileUtils::readFile(const std::string& filename, std::vector<unsigned char>& out) {
    return assets::read(fullPathForFilename(filename.c_str()), out);
}

bool CCFileUtils::readText(const std::string& filename, std::string& out) {
    return assets::readText(fullPathForFilename(filename.c_str()), out);
}

unsigned char* CCFileUtils::getFileData(const char* filename, const char*, unsigned long* size) {
    std::vector<unsigned char> bytes;
    if (size) *size = 0;
    if (!readFile(filename, bytes)) return nullptr;
    unsigned char* data = new unsigned char[bytes.size() + 1];
    std::memcpy(data, bytes.data(), bytes.size());
    data[bytes.size()] = 0;
    if (size) *size = static_cast<unsigned long>(bytes.size());
    return data;
}

std::string CCFileUtils::getWritablePath() { return platform::prefPath(); }

}  // namespace cocos2d
