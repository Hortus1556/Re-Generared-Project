#include "engine/VectorMovie.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "lunasvg.h"
#include "miniaudio.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

namespace {

// The end of the <g> element starting at `start` (nested groups matched).
size_t groupEnd(const std::string& s, size_t start) {
    int depth = 0;
    size_t k = start;
    for (;;) {
        size_t open = s.find("<g", k), close = s.find("</g>", k);
        if (close == std::string::npos) return std::string::npos;
        if (open != std::string::npos && open < close) {
            ++depth;
            k = open + 2;
        } else {
            --depth;
            k = close + 4;
            if (depth == 0) return k;
        }
    }
}

std::string attribute(const std::string& tag, const char* name) {
    std::string key = std::string(" ") + name + "=\"";
    size_t p = tag.find(key);
    if (p == std::string::npos) return "";
    p += key.size();
    return tag.substr(p, tag.find('"', p) - p);
}

// How many frames are rendered ahead of the one shown.
constexpr int kAhead = 12;

}  // namespace

std::unique_ptr<VectorMovie> VectorMovie::open(const std::string& bundleName) {
    std::vector<unsigned char> file;
    if (!assets::read(bundleName, file)) return nullptr;
    std::unique_ptr<VectorMovie> m(new VectorMovie);
    m->text.assign(file.begin(), file.end());
    const std::string& s = m->text;
    size_t root = s.find("<svg"), defs = s.find("<defs>");
    if (root == std::string::npos || defs == std::string::npos) return nullptr;
    std::string rootTag = s.substr(root, s.find('>', root) - root);
    int count = std::atoi(attribute(rootTag, "data-frames").c_str());
    m->fps = std::atof(attribute(rootTag, "data-fps").c_str());
    float stageW = static_cast<float>(std::atof(attribute(rootTag, "width").c_str()));
    float stageH = static_cast<float>(std::atof(attribute(rootTag, "height").c_str()));
    if (count <= 0 || m->fps <= 0 || stageW <= 0 || stageH <= 0) return nullptr;
    m->head = s.substr(0, defs);

    for (size_t p = s.find("<g id=\"u_", defs); p != std::string::npos; p = s.find("<g id=\"u_", p + 1)) {
        size_t idStart = p + 9;
        std::string key = s.substr(idStart, s.find('"', idStart) - idStart);
        size_t end = groupEnd(s, p);
        if (end == std::string::npos) return nullptr;
        m->units[key] = {p, end - p};
        p = end - 1;
    }
    m->frames.resize(count);
    for (int n = 1; n <= count; ++n) {
        std::string id = "<g id=\"f" + std::to_string(n) + "\"";
        size_t p = s.find(id, defs);
        if (p == std::string::npos) return nullptr;
        std::string tag = s.substr(p, s.find('>', p) - p);
        Frame& f = m->frames[n - 1];
        f.begin = p;
        f.end = groupEnd(s, p);
        std::string keys = attribute(tag, "data-defs");
        for (size_t a = 0; a < keys.size();) {
            size_t b = keys.find(' ', a);
            if (b == std::string::npos) b = keys.size();
            if (b > a) f.defs.push_back(keys.substr(a, b - a));
            a = b + 1;
        }
    }

    // Rasterised to cover the game screen at the display's resolution (the
    // player shows it "aspect fill").
    float scale = std::max(platform::screenWidth() / stageW, platform::screenHeight() / stageH) *
                  std::max(1.0f, platform::displayScale());
    m->width = static_cast<int>(std::ceil(stageW * scale)) & ~1;
    m->height = static_cast<int>(std::ceil(stageH * scale)) & ~1;

    std::string base = bundleName.substr(0, bundleName.rfind('.'));
    m->audioName = base + ".mp3";

    unsigned threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency() / 2));
    for (unsigned i = 0; i < threads; ++i) m->workers.emplace_back(&VectorMovie::work, m.get());
    return m;
}

VectorMovie::~VectorMovie() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    changed.notify_all();
    for (std::thread& t : workers) t.join();
}

std::string VectorMovie::frameDocument(int index) const {
    const Frame& f = frames[index];
    std::string doc = head;
    doc += "<defs>";
    for (const std::string& key : f.defs) {
        auto it = units.find(key);
        if (it != units.end()) doc.append(text, it->second.first, it->second.second);
    }
    doc += "</defs>";
    std::string body = text.substr(f.begin, f.end - f.begin);
    size_t hidden = body.find(" display=\"none\"");
    if (hidden != std::string::npos) body.erase(hidden, 15);
    doc += body;
    doc += "</svg>";
    return doc;
}

void VectorMovie::work() {
    for (;;) {
        int index;
        {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait(lock, [&] {
                return stopping || (nextToRender < static_cast<int>(frames.size()) && nextToRender < nextToShow + kAhead);
            });
            if (stopping) return;
            index = nextToRender++;
        }
        std::vector<unsigned char> bgra(static_cast<size_t>(width) * height * 4, 0);
        if (auto doc = lunasvg::Document::loadFromData(frameDocument(index))) {
            // lunasvg's ARGB32 is B, G, R, A in memory: the frame's BGRA
            // (opaque, so premultiplied is plain).
            lunasvg::Bitmap bitmap(bgra.data(), width, height, width * 4);
            bitmap.clear(0x000000ff);
            doc->render(bitmap, lunasvg::Matrix::scaled(width / doc->width(), height / doc->height()));
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            done[index] = std::move(bgra);
        }
        changed.notify_all();
    }
}

bool VectorMovie::readAudio(std::vector<float>& pcm, int& channels, int& sampleRate) {
    std::vector<unsigned char> file;
    if (!assets::read(audioName, file)) return false;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_uint64 count = 0;
    void* data = nullptr;
    if (ma_decode_memory(file.data(), file.size(), &cfg, &count, &data) != MA_SUCCESS) return false;
    const float* samples = static_cast<const float*>(data);
    pcm.assign(samples, samples + count * cfg.channels);
    ma_free(data, nullptr);
    channels = static_cast<int>(cfg.channels);
    sampleRate = static_cast<int>(cfg.sampleRate);
    return true;
}

bool VectorMovie::nextFrame(std::vector<unsigned char>& bgra, int& w, int& h, double& seconds) {
    std::unique_lock<std::mutex> lock(mutex);
    if (nextToShow >= static_cast<int>(frames.size())) return false;
    int index = nextToShow;
    changed.wait(lock, [&] { return done.count(index) != 0; });
    bgra = std::move(done[index]);
    done.erase(index);
    ++nextToShow;
    lock.unlock();
    changed.notify_all();
    w = width;
    h = height;
    seconds = index / fps;
    return true;
}
