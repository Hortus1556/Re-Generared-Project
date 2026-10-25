// Persistent key/value record store (port of RMSManager).
//
// The original kept an NSMutableDictionary of NSNumber/NSString values in
// Documents/RMS.txt (a plist). The port keeps every value as text in
// <prefPath>/RMS.txt, one "key<TAB>value" line per record, and parses numbers
// on read exactly where the original called intValue/floatValue.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/Object.h"

class RMSManager : public Object {
public:
    static constexpr int kMissing = -1000;  // getIntByKey for an absent key

    static RMSManager* init() { return new RMSManager("RMS.txt"); }
    // The same store in another file of the save folder (AS2's
    // NSUserDefaults: UserDefaults.txt).
    static RMSManager* initWithFilename(const std::string& name) { return new RMSManager(name); }

    void loadFromRMS();
    bool saveIntoRMS() const;

    std::optional<std::string> getRecordByKey(const std::string& key) const;
    int getIntByKey(const std::string& key) const;
    void insertRecord(const std::string& value, const std::string& key) { recordStore[key] = value; }
    void insertInt(int value, const std::string& key) { insertRecord(std::to_string(value), key); }
    void insertFloat(float value, const std::string& key);
    void removeRecordByKey(const std::string& key) { recordStore.erase(key); }
private:
    explicit RMSManager(std::string filename) : filename(std::move(filename)) {}
    std::string filePath() const;

    std::string filename;
    std::map<std::string, std::string> recordStore;
};
