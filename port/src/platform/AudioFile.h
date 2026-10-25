// Decoding Apple's Core Audio files (CAF) natively, on every platform.
//
// The iOS games' sounds are CAF: AS2's few are linear PCM, AS3's are Apple
// IMA4 (ADPCM, 4:1). They stay in the packages as the IPAs had them and are
// decoded here, whole, when a sound is loaded (engine/MediaPlayer).
#pragma once

#include <cstddef>
#include <vector>

namespace platform {

// A CAF file's sound as interleaved float samples (-1..1): linear PCM
// (integers of 8 to 32 bits or floats, either byte order) or IMA4, trimmed
// to its packet table's valid frames. False if it is not a CAF file of
// those formats.
bool decodeCaf(const unsigned char* bytes, size_t size, std::vector<float>& pcm, int& channels,
               int& sampleRate);

}  // namespace platform
