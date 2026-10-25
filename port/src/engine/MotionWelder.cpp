#include "engine/MotionWelder.h"

#include <map>

#include "platform/Assets.h"

// The parsed .anu, shared by every animation made from the same file (the
// original's resource manager cached it the same way).
struct MotionWelder::Data {
    std::vector<int> animFirst, animLast;  // per animation
    std::vector<int> framePool, frameDelay;  // per frame
    std::vector<std::vector<int>> pools;   // per pool: clip, x, y, flag, clip, x, y, flag...
    std::vector<Rect> imageClips;          // regions of the sheet
    std::vector<Size> positioners;         // touch area sizes
};

namespace {

class Reader {
public:
    explicit Reader(const std::vector<unsigned char>& b) : bytes(b) {}
    bool ok() const { return good; }
    void skip(size_t n) { take(n); }
    int byte() {
        const unsigned char* p = take(1);
        return p ? static_cast<signed char>(p[0]) : 0;
    }
    int shortInt() {
        const unsigned char* p = take(2);
        return p ? static_cast<short>(p[0] << 8 | p[1]) : 0;
    }
    int intVal() {
        const unsigned char* p = take(4);
        return p ? static_cast<int>(static_cast<unsigned>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]) : 0;
    }

private:
    const unsigned char* take(size_t n) {
        if (!good || pos + n > bytes.size()) {
            good = false;
            return nullptr;
        }
        const unsigned char* p = bytes.data() + pos;
        pos += n;
        return p;
    }
    const std::vector<unsigned char>& bytes;
    size_t pos = 0;
    bool good = true;
};

std::shared_ptr<const MotionWelder::Data> load(const std::string& name);

}  // namespace

namespace {

std::shared_ptr<const MotionWelder::Data> load(const std::string& name) {
    static std::map<std::string, std::shared_ptr<const MotionWelder::Data>> cache;
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;

    std::vector<unsigned char> bytes;
    if (!assets::read(name, bytes)) return nullptr;
    auto d = std::make_shared<MotionWelder::Data>();
    Reader r(bytes);
    r.skip(7);
    int animations = r.byte();
    for (int i = 0; i < animations; ++i) {
        d->animFirst.push_back(r.shortInt());
        d->animLast.push_back(r.shortInt());
    }
    int frames = r.shortInt();
    for (int i = 0; i < frames; ++i) {
        d->framePool.push_back(r.shortInt());
        d->frameDelay.push_back(r.byte());
        r.shortInt();
        r.shortInt();
    }
    r.shortInt();
    int pools = r.shortInt();
    for (int i = 0; i < pools && r.ok(); ++i) {
        std::vector<int> clips;
        int n = r.shortInt();
        for (int k = 0; k < n; ++k) {
            clips.push_back(r.shortInt());
            clips.push_back(r.shortInt());
            clips.push_back(r.shortInt());
            clips.push_back(r.byte());
        }
        d->pools.push_back(std::move(clips));
    }
    r.shortInt();
    int images = r.byte();
    for (int i = 0; i < images && r.ok(); ++i) {
        int n = r.shortInt();
        for (int k = 0; k < n; ++k) {
            float x = static_cast<float>(r.shortInt()), y = static_cast<float>(r.shortInt());
            float w = static_cast<float>(r.shortInt()), h = static_cast<float>(r.shortInt());
            d->imageClips.push_back({{x, y}, {w, h}});
        }
    }
    // Ellipses (4 shorts + colour), lines (2 + colour), rectangles (2 +
    // colour), round rectangles (4 + colour): read past.
    for (int shorts : {4, 2, 2, 4}) {
        int n = r.shortInt();
        for (int k = 0; k < n; ++k) {
            for (int s = 0; s < shorts; ++s) r.shortInt();
            r.intVal();
        }
    }
    int positioners = r.shortInt();
    for (int i = 0; i < positioners; ++i) {
        float w = static_cast<float>(r.shortInt()), h = static_cast<float>(r.shortInt());
        d->positioners.push_back({w, h});
    }
    if (!r.ok()) return nullptr;
    cache[name] = d;
    return d;
}

const std::vector<int> kNoClips;

}  // namespace

MotionWelder* MotionWelder::initWithImageNameData(const std::string& image, const std::string& anu) {
    std::shared_ptr<const Data> data = load(anu);
    AdvancedImage* sheet = AdvancedImage::initWithPathRowColData(image, 1, 1, nullptr);
    if (!data || !sheet) {
        releaseRef(sheet);
        return nullptr;
    }
    MotionWelder* m = new MotionWelder;
    m->data = std::move(data);
    m->sheet = sheet;
    m->setAnimationWithIndex(0);
    return m;
}

MotionWelder::~MotionWelder() { releaseRef(sheet); }

int MotionWelder::getAnimationCount() const { return static_cast<int>(data->animFirst.size()); }

void MotionWelder::setAnimationWithIndex(int a) {
    if (a < 0 || a >= getAnimationCount()) return;
    animation = a;
    frameCount = data->animLast[a] - data->animFirst[a] + 1;
    endFrame = frameCount;
    setFrameWithIndex(0);
    isPlaying = true;
}

void MotionWelder::setAnimationOffsetWithIndex(int offset) {
    loopOffset = offset > frameCount ? frameCount : offset;
    cycleCount = 0;
    isLoopBack = false;
}

void MotionWelder::setFrameWithIndex(int f) {
    if (f + data->animFirst[animation] <= data->animLast[animation]) frame = f;
}

void MotionWelder::cycle() {
    ++cycleCount;
    isLoopBack = false;
    int f = data->animFirst[animation] + frame;
    int delay = f >= 0 && f < static_cast<int>(data->frameDelay.size()) ? data->frameDelay[f] : 1;
    if (durationRatio * delay > cycleCount) return;
    if (frame == frameCount - 1 || frame == endFrame) {
        if (loopOffset == -1) {
            isPlaying = false;
            return;
        }
        frame = loopOffset - 1;
        if (frame < -1) frame = 0;
        isLoopBack = true;
    }
    cycleCount = 0;
    setFrameWithIndex(frame + 1);
}

const std::vector<int>& MotionWelder::currentClips() const {
    int f = data->animFirst[animation] + frame;
    if (f < 0 || f >= static_cast<int>(data->framePool.size())) return kNoClips;
    int pool = data->framePool[f];
    if (pool < 0 || pool >= static_cast<int>(data->pools.size())) return kNoClips;
    return data->pools[pool];
}

void MotionWelder::draw() {
    const std::vector<int>& clips = currentClips();
    for (size_t i = 0; i + 3 < clips.size(); i += 4) {
        int index = clips[i], flag = clips[i + 3];
        if (flag % 8 != 0 && flag % 8 != 2 && flag % 8 != 4) continue;  // not an image clip
        if (index < 0 || index >= static_cast<int>(data->imageClips.size())) continue;
        const Rect& c = data->imageClips[index];
        sheet->setMirrorWith(0, flag % 8 == 2);
        sheet->setMirrorWith(1, flag % 8 == 4);
        sheet->setPositionWithXY(static_cast<int>(posX + clips[i + 1]), static_cast<int>(posY + clips[i + 2]));
        sheet->drawWithImageRegionWithXYWidthHeight(static_cast<int>(c.origin.x), static_cast<int>(c.origin.y),
                                                    static_cast<int>(c.size.width), static_cast<int>(c.size.height));
    }
}

std::vector<Rect> MotionWelder::getCollisionRects() const {
    std::vector<Rect> rects;
    const std::vector<int>& clips = currentClips();
    for (size_t i = 0; i + 3 < clips.size(); i += 4) {
        if (clips[i + 3] != 15) continue;
        int index = clips[i];
        Size size = index >= 0 && index < static_cast<int>(data->positioners.size()) ? data->positioners[index]
                                                                                       : Size{0, 0};
        rects.push_back({{posX + clips[i + 1], posY + clips[i + 2]}, size});
    }
    return rects;
}

int MotionWelder::checkCollision(float x, float y) const {
    std::vector<Rect> rects = getCollisionRects();
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].contains({x, y})) return static_cast<int>(i);
    return -1;
}
