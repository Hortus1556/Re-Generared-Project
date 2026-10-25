// A Flash movie as a game package: its content extracted into ordinary,
// categorized files (docs/engine.md), and read back into the same model.
//
//   library.json            stage size, frame rate, background, the linkage
//                           names, and every character id by kind
//   vector/shapes/<id>.svg  each shape (flash/SvgShape.h)
//   vector/morphs/<id>_start.svg, <id>_end.svg
//   vector/symbols/<id>.json  each sprite's timeline (0: the main one):
//                           frames of place/move/remove, names, masks,
//                           colour transforms, labels, frame actions
//   vector/buttons.json, vector/texts.json, vector/edittexts.json
//   vector/fonts/<id>.json  metrics and glyphs (each glyph an SVG)
//   audio/<name>.mp3|.wav   each sound (MP3 as stored: the decoder skips
//                           `seek` samples and plays `samples`), audio/index.json
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "flash/Swf.h"

namespace flash {

// Writes the package under `dir` (its library.json, vector/ and audio/ are
// replaced). `report` gets what was written.
bool writePackage(const Swf& movie, const std::string& dir, std::string* report = nullptr);

// Reads a package through `read` (a path relative to the package's folder
// -> its bytes). nullptr (and `error`) if it is not one.
using FileReader = std::function<bool(const std::string& path, std::vector<unsigned char>& bytes)>;
std::unique_ptr<Swf> loadPackage(const FileReader& read, std::string* error = nullptr);

}  // namespace flash
