// Locating and reading game data extracted from the IPA.
//
// Game code names files the way the iOS bundle did: a flat file name such as
// "Sound_12.mp3" or "lang/en/stringtable.txt". port/tools/extract_ipa.py
// sorts root-level bundle files into one folder per type; this module maps a
// bundle name back to that location with the same extension table. A file
// in a bundle subfolder keeps its folder; in both cases a converted file
// (a property list or nib as JSON) is found under its new name.
#pragma once

#include <string>
#include <vector>

namespace assets {

// Path of a bundle file, or "" when it does not exist. Mirrors
// -[NSBundle pathForResource:ofType:] returning nil.
std::string path(const std::string& bundleName);
std::string path(const std::string& name, const std::string& ext);

bool exists(const std::string& bundleName);

// Reads a whole bundle file; returns false when it does not exist.
bool read(const std::string& bundleName, std::vector<unsigned char>& out);
bool readText(const std::string& bundleName, std::string& out);

// The assets folder in use, with trailing separator ("" if none was found).
const std::string& root();

}  // namespace assets
