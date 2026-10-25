// The game's data files: JSON read from the package (the original's
// Json::Reader over a file, 0xf42dc), and the content its ContentManager
// (0x2714b8) kept up to date from the developer's server.
//
// The original copied its content into Documents/Downloaded on the first
// launch of each build: the arrays of Data/default_data.json each to a file
// (upgrades.json, partners.json, upgrade_unlocks.json, area_unlocks.json,
// settings.json), and Data/areas.json, Data/Surgeries/ and Data/Tutorials/
// as they are (copyData, 0x272458); then it asked the server for newer ones.
// The port is offline: a "Downloaded/" file is read from the package
// directly, as the first launch would have written it.
#pragma once

#include <string>

#include "core/Json.h"

namespace mt {

// A JSON file of the package; false (logged) if missing or not JSON.
bool readJson(const std::string& path, json::Value& out);
// A file of the downloaded content, by its name in Downloaded/
// ("areas.json", "partners.json", "Surgeries/<id>.json"...).
bool readContent(const std::string& name, json::Value& out);

}  // namespace mt
