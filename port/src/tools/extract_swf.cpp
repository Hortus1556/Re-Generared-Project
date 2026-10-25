// extract_swf <movie.swf> <package dir>: a Flash movie's content as a game
// package (flash/Package.h, docs/engine.md): SVG shapes, JSON symbols,
// fonts and texts, the sounds as audio files. It then reads the package back
// and writes it again beside it: the two must be byte-identical, so nothing
// is lost or changed by the round trip. The build runs it for Christmas
// (CMakeLists.txt).
//
// extract_swf <movie.swf> <package dir> --only <export>[,<export>...]: only
// those exported symbols and what they draw (their inner clips, shapes,
// morphs, buttons, texts and fonts), the main timeline empty: one piece of
// another game's art for a game's own package (AS3's pointer, AS2's Flash
// `cursor`), without the rest of the movie.
//
// --sound <export>=<file.mp3|file.wav> (any number of times): that exported
// sound is the given MP3 (or 16-bit PCM WAV, kept as the movie's raw sound)
// instead of the movie's own (the user's restored music, as
// AS3's: docs/as1.md). An MP3's own encoder delay and padding are in its
// LAME tag, so it is played whole: no seek samples, no sample count. A name the
// movie does not export is added as a new sound exported by that name (a
// sound the original lacks: AS1's female screams, taken from AS2).
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <vector>

#include "flash/Package.h"
#include "flash/Swf.h"

namespace fs = std::filesystem;

namespace {

bool readFile(const fs::path& p, std::vector<unsigned char>& bytes) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// The package's files (relative paths) that differ between two folders.
std::vector<std::string> differences(const fs::path& a, const fs::path& b) {
    std::vector<std::string> out;
    for (const fs::path& root : {a, b}) {
        const fs::path& other = root == a ? b : a;
        for (const auto& e : fs::recursive_directory_iterator(root)) {
            if (!e.is_regular_file()) continue;
            fs::path rel = fs::relative(e.path(), root);
            if (rel.begin()->string() == "data" || rel.begin()->string() == "scripts" || rel.filename() == "game.json") continue;
            std::vector<unsigned char> x, y;
            readFile(e.path(), x);
            if (!readFile(other / rel, y) || x != y) out.push_back(rel.generic_string());
        }
    }
    return out;
}

// Keeps only the exports named in `names` (comma-separated) and every
// character they use; false if one is not exported.
bool keepOnly(flash::Swf& movie, const std::string& names) {
    std::set<int> keep;
    std::vector<int> todo;
    std::unordered_map<std::string, int> exports;
    std::istringstream list(names);
    std::string name;
    while (std::getline(list, name, ',')) {
        auto it = movie.exports.find(name);
        if (it == movie.exports.end()) {
            std::fprintf(stderr, "extract_swf: no symbol exported as %s\n", name.c_str());
            return false;
        }
        exports[name] = it->second;
        todo.push_back(it->second);
    }
    while (!todo.empty()) {
        int id = todo.back();
        todo.pop_back();
        if (id < 0 || !keep.insert(id).second) continue;
        if (auto s = movie.sprites.find(id); s != movie.sprites.end())
            for (const auto& frame : s->second.frames)
                for (const flash::FrameOp& op : frame)
                    if (op.kind == flash::FrameOp::kPlace) todo.push_back(op.place.character);
        if (auto b = movie.buttons.find(id); b != movie.buttons.end())
            for (const flash::ButtonRecord& r : b->second.records) todo.push_back(r.character);
        if (auto t = movie.texts.find(id); t != movie.texts.end())
            for (const flash::TextRecord& r : t->second.records) todo.push_back(r.font);
        if (auto e = movie.editTexts.find(id); e != movie.editTexts.end()) todo.push_back(e->second.font);
    }
    // (The bitmaps the kept shapes fill with.)
    std::set<int> bitmaps;
    for (int id : keep)
        if (auto s = movie.shapes.find(id); s != movie.shapes.end())
            for (const flash::ShapeLayer& l : s->second.layers)
                for (const flash::FillStyle& f : l.fills)
                    if (f.type == flash::FillStyle::kBitmap) bitmaps.insert(f.bitmap);
    for (auto it = movie.bitmaps.begin(); it != movie.bitmaps.end();)
        it = bitmaps.count(it->first) ? std::next(it) : movie.bitmaps.erase(it);
    auto prune = [&](auto& table) {
        for (auto it = table.begin(); it != table.end();)
            it = keep.count(it->first) ? std::next(it) : table.erase(it);
    };
    prune(movie.shapes);
    prune(movie.morphs);
    prune(movie.buttons);
    prune(movie.texts);
    prune(movie.editTexts);
    prune(movie.fonts);
    prune(movie.sounds);
    keep.insert(0);  // (the main timeline, emptied)
    prune(movie.sprites);
    movie.sprites[0] = flash::Timeline();
    movie.sprites[0].frames.resize(1);
    movie.sprites[0].hasScript.resize(1);
    movie.sprites[0].actions.resize(1);
    movie.exports = exports;
    return true;
}

// An MP3 file's sample rate and channels, from its first frame header
// (after an ID3v2 tag, if any).
bool mp3Format(const std::vector<unsigned char>& b, int& rate, bool& stereo) {
    size_t at = 0;
    if (b.size() > 10 && b[0] == 'I' && b[1] == 'D' && b[2] == '3')
        at = 10 + ((b[6] & 0x7f) << 21 | (b[7] & 0x7f) << 14 | (b[8] & 0x7f) << 7 | (b[9] & 0x7f));
    for (; at + 4 <= b.size(); ++at) {
        if (b[at] != 0xff || (b[at + 1] & 0xe0) != 0xe0) continue;
        int version = (b[at + 1] >> 3) & 3;  // 3 MPEG-1, 2 MPEG-2, 0 MPEG-2.5
        int index = (b[at + 2] >> 2) & 3;
        if (version == 1 || index == 3) continue;
        static const int base[3] = {44100, 48000, 32000};
        rate = base[index] >> (version == 3 ? 0 : version == 2 ? 1 : 2);
        stereo = (b[at + 3] >> 6) != 3;
        return true;
    }
    return false;
}

// A WAV file's 16-bit PCM as the movie's raw little-endian sound (Flash's
// format 3): its rate, channels and samples; false if it is not that.
bool wavSound(const std::vector<unsigned char>& b, flash::Sound& out) {
    auto u16 = [&](size_t at) { return static_cast<unsigned>(b[at] | b[at + 1] << 8); };
    auto u32 = [&](size_t at) { return u16(at) | u16(at + 2) << 16; };
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0) return false;
    unsigned channels = 0, bits = 0;
    for (size_t at = 12; at + 8 <= b.size();) {
        size_t size = u32(at + 4);
        if (std::memcmp(b.data() + at, "fmt ", 4) == 0 && size >= 16) {
            if (u16(at + 8) != 1) return false;  // (PCM only)
            channels = u16(at + 10);
            out.rate = static_cast<int>(u32(at + 12));
            bits = u16(at + 22);
        } else if (std::memcmp(b.data() + at, "data", 4) == 0) {
            if (bits != 16 || (channels != 1 && channels != 2)) return false;
            size = std::min(size, b.size() - at - 8);
            out.format = 3;
            out.is16 = true;
            out.stereo = channels == 2;
            out.data.assign(b.begin() + static_cast<long>(at + 8), b.begin() + static_cast<long>(at + 8 + size));
            out.samples = static_cast<uint32_t>(size / (channels * 2));
            return true;
        }
        at += 8 + size + (size & 1);
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string only;
    std::vector<std::pair<std::string, std::string>> sounds;  // export -> file
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (args[i] == "--only") {
            only = args[i + 1];
        } else if (args[i] == "--sound" && args[i + 1].find('=') != std::string::npos) {
            size_t eq = args[i + 1].find('=');
            sounds.emplace_back(args[i + 1].substr(0, eq), args[i + 1].substr(eq + 1));
        } else {
            args.clear();
            break;
        }
    }
    if (args.size() < 2 || args.size() % 2) {
        std::fprintf(stderr, "usage: extract_swf <movie.swf> <package dir> [--only <export>[,<export>...]] "
                             "[--sound <export>=<file.mp3|file.wav>]...\n");
        return 2;
    }
    std::vector<unsigned char> bytes;
    std::unique_ptr<flash::Swf> movie;
    if (readFile(argv[1], bytes)) movie = flash::Swf::load(bytes);
    if (!movie) {
        std::fprintf(stderr, "extract_swf: %s is not a movie it can read\n", argv[1]);
        return 1;
    }
    if (!only.empty() && !keepOnly(*movie, only)) return 1;
    for (const auto& [name, file] : sounds) {
        auto e = movie->exports.find(name);
        if (e == movie->exports.end()) {
            int id = 65535;  // (a character id nothing uses, from the top)
            while (id > 0 && movie->kind(id) != flash::Swf::Kind::kNone) --id;
            movie->sounds[id] = flash::Sound{};
            e = movie->exports.emplace(name, id).first;
        }
        std::vector<unsigned char> bytes;
        flash::Sound replaced;
        replaced.format = 2;
        bool wav = file.size() > 4 && (file.compare(file.size() - 4, 4, ".wav") == 0 || file.compare(file.size() - 4, 4, ".WAV") == 0);
        bool read = movie->sounds.count(e->second) && readFile(file, bytes) &&
                    (wav ? wavSound(bytes, replaced) : mp3Format(bytes, replaced.rate, replaced.stereo));
        if (!read) {
            std::fprintf(stderr, "extract_swf: cannot put %s in place of the sound %s\n", file.c_str(), name.c_str());
            return 1;
        }
        if (!wav) replaced.data = std::move(bytes);
        movie->sounds[e->second] = std::move(replaced);
    }
    fs::path dir = fs::absolute(argv[2]);
    std::string report;
    if (!flash::writePackage(*movie, dir.string(), &report)) {
        std::fprintf(stderr, "extract_swf: cannot write %s\n", argv[2]);
        return 1;
    }

    // The round trip.
    auto start = std::chrono::steady_clock::now();
    std::string error;
    std::unique_ptr<flash::Swf> back = flash::loadPackage(
        [&](const std::string& path, std::vector<unsigned char>& out) { return readFile(dir / path, out); }, &error);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (!back) {
        std::fprintf(stderr, "extract_swf: the package does not read back: %s\n", error.c_str());
        return 1;
    }
    fs::path again = dir.parent_path() / (dir.filename().string() + ".roundtrip");
    std::error_code ec;
    fs::remove_all(again, ec);
    flash::writePackage(*back, again.string());
    std::vector<std::string> diff = differences(dir, again);
    fs::remove_all(again, ec);
    if (!diff.empty()) {
        std::fprintf(stderr, "extract_swf: %zu files change in the round trip, e.g. %s\n", diff.size(), diff.front().c_str());
        return 1;
    }
    std::printf("extract_swf: %s: %s (reads back in %.0f ms, round trip exact)\n", argv[2], report.c_str(), ms);
    return 0;
}
