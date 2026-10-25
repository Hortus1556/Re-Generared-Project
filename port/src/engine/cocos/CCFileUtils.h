// cocos2d-x 2.1.1's CCFileUtils, ported to Bleed Engine: files are the
// game package's (platform/Assets), found as 2.1.1 found them in its bundle:
// each search path, and in each the resolution directories in order (the
// game sets them: AS3's "4.0x/"), then the name itself. The writable path
// is the game's save folder. MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace cocos2d {

class CCFileUtils {
public:
    static CCFileUtils* sharedFileUtils();
    // a name -> the package path it is found at ("" when nowhere)
    std::string fullPathForFilename(const char* filename);
    // a file named relative to another (a sheet's texture beside its plist)
    std::string fullPathFromRelativeFile(const char* filename, const char* relativeFile);
    bool isFileExist(const std::string& path);
    // the whole file (empty when missing); `size` its length
    unsigned char* getFileData(const char* filename, const char* mode, unsigned long* size);  // new[]
    bool readFile(const std::string& filename, std::vector<unsigned char>& out);
    bool readText(const std::string& filename, std::string& out);

    void setSearchPaths(const std::vector<std::string>& paths);
    void addSearchPath(const char* path);
    const std::vector<std::string>& getSearchPaths() { return m_searchPathArray; }
    void setSearchResolutionsOrder(const std::vector<std::string>& orders);
    void addSearchResolutionsOrder(const char* order);
    const std::vector<std::string>& getSearchResolutionsOrder() { return m_searchResolutionsOrderArray; }
    void purgeCachedEntries() { m_fullPathCache.clear(); }

    // the save folder (AS_SAVE_DIR in debug runs), with a trailing separator
    std::string getWritablePath();

private:
    std::vector<std::string> m_searchPathArray{""};
    std::vector<std::string> m_searchResolutionsOrderArray{""};
    std::map<std::string, std::string> m_fullPathCache;
};

}  // namespace cocos2d
