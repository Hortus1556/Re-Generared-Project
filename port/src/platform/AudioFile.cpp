#include "platform/AudioFile.h"

#include <cstdint>
#include <cstring>

namespace platform {
namespace {

uint32_t be32(const unsigned char* p) { return static_cast<uint32_t>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
uint64_t be64(const unsigned char* p) { return static_cast<uint64_t>(be32(p)) << 32 | be32(p + 4); }
double beDouble(const unsigned char* p) {
    uint64_t v = be64(p);
    double d;
    std::memcpy(&d, &v, 8);
    return d;
}

const int kSteps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
const int kIndexStep[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

// Apple IMA4: per channel, packets of 34 bytes: a 2-byte header (the
// predictor's top 9 bits and the step index), then 64 samples as nibbles,
// low nibble first; a stereo file alternates the channels' packets.
// The header only keeps the predictor to 9 bits, so a decoder that started
// every packet from it would jump by up to 127 at each packet (64 samples):
// a buzz of 689 Hz under every sound. As QuickTime's decoder (and ffmpeg's
// adpcm_ima_qt) does, the running state is kept while the header agrees with
// it (same step index, predictor within 127) and taken from the header only
// when it does not (the first packet, or after a cut in the stream).
void decodeIma4(const unsigned char* data, size_t size, int channels, std::vector<float>& out) {
    size_t packets = size / (34 * static_cast<size_t>(channels));
    out.resize(packets * 64 * channels);
    std::vector<int> pred(channels, 0), index(channels, -1);
    for (size_t p = 0; p < packets; ++p) {
        for (int c = 0; c < channels; ++c) {
            const unsigned char* pk = data + (p * channels + c) * 34;
            int header = pk[0] << 8 | pk[1];
            int hpred = static_cast<int16_t>(header & 0xFF80);
            int hindex = header & 0x7F;
            if (hindex > 88) hindex = 88;
            int d = hpred - pred[c];
            if (index[c] != hindex || d > 0x7F || d < -0x7F) {
                pred[c] = hpred;
                index[c] = hindex;
            }
            float* o = out.data() + p * 64 * channels + c;
            for (int i = 0; i < 64; ++i) {
                int n = (pk[2 + i / 2] >> ((i & 1) * 4)) & 15;
                int step = kSteps[index[c]];
                int diff = step >> 3;
                if (n & 4) diff += step;
                if (n & 2) diff += step >> 1;
                if (n & 1) diff += step >> 2;
                pred[c] += (n & 8) ? -diff : diff;
                if (pred[c] > 32767) pred[c] = 32767;
                if (pred[c] < -32768) pred[c] = -32768;
                index[c] += kIndexStep[n];
                if (index[c] < 0) index[c] = 0;
                if (index[c] > 88) index[c] = 88;
                o[i * channels] = pred[c] / 32768.0f;
            }
        }
    }
}

bool decodeLpcm(const unsigned char* data, size_t size, int bits, bool isFloat, bool little, int channels,
                std::vector<float>& out) {
    int bytes = bits / 8;
    if (bytes < 1 || bytes > 8) return false;
    size_t n = size / bytes;
    n -= n % channels;
    out.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const unsigned char* s = data + i * bytes;
        uint64_t v = 0;
        for (int b = 0; b < bytes; ++b) v |= static_cast<uint64_t>(s[little ? b : bytes - 1 - b]) << (8 * b);
        if (isFloat && bits == 32) {
            uint32_t w = static_cast<uint32_t>(v);
            float f;
            std::memcpy(&f, &w, 4);
            out[i] = f;
        } else if (isFloat && bits == 64) {
            double f;
            std::memcpy(&f, &v, 8);
            out[i] = static_cast<float>(f);
        } else if (bits == 8) {
            out[i] = (static_cast<int>(v) - 128) / 128.0f;  // 8-bit PCM is unsigned
        } else {
            int64_t sv = static_cast<int64_t>(v << (64 - bits)) >> (64 - bits);
            out[i] = static_cast<float>(sv / static_cast<double>(1LL << (bits - 1)));
        }
    }
    return true;
}

}  // namespace

bool decodeCaf(const unsigned char* bytes, size_t size, std::vector<float>& pcm, int& channels,
               int& sampleRate) {
    if (size < 8 || std::memcmp(bytes, "caff", 4) != 0) return false;
    // chunks: type, 64-bit size (-1: the data chunk runs to the end), body
    bool haveDesc = false, isIma = false, isFloat = false, little = false;
    int bits = 0;
    int64_t validFrames = -1, priming = 0;
    const unsigned char* data = nullptr;
    size_t dataSize = 0;
    size_t p = 8;
    while (p + 12 <= size) {
        const unsigned char* kind = bytes + p;
        int64_t len = static_cast<int64_t>(be64(bytes + p + 4));
        const unsigned char* body = bytes + p + 12;
        size_t avail = size - (p + 12);
        size_t n = len < 0 || static_cast<uint64_t>(len) > avail ? avail : static_cast<size_t>(len);
        if (std::memcmp(kind, "desc", 4) == 0 && n >= 32) {
            sampleRate = static_cast<int>(beDouble(body));
            uint32_t flags = be32(body + 12);
            channels = static_cast<int>(be32(body + 24));
            bits = static_cast<int>(be32(body + 28));
            if (std::memcmp(body + 8, "ima4", 4) == 0) isIma = true;
            else if (std::memcmp(body + 8, "lpcm", 4) != 0) return false;
            isFloat = flags & 1;
            little = flags & 2;
            haveDesc = true;
        } else if (std::memcmp(kind, "pakt", 4) == 0 && n >= 24) {
            validFrames = static_cast<int64_t>(be64(body + 8));
            priming = static_cast<int32_t>(be32(body + 16));
        } else if (std::memcmp(kind, "data", 4) == 0 && n >= 4) {
            data = body + 4;  // after the edit count
            dataSize = n - 4;
        }
        if (len < 0) break;
        p += 12 + n;
    }
    if (!haveDesc || !data || channels <= 0 || sampleRate <= 0) return false;
    if (isIma) {
        decodeIma4(data, dataSize, channels, pcm);
        size_t frames = pcm.size() / channels;
        if (validFrames >= 0 && priming >= 0 && static_cast<size_t>(priming + validFrames) <= frames) {
            pcm.erase(pcm.begin(), pcm.begin() + priming * channels);
            pcm.resize(static_cast<size_t>(validFrames) * channels);
        }
        return true;
    }
    return decodeLpcm(data, dataSize, bits, isFloat, little, channels, pcm);
}

}  // namespace platform
