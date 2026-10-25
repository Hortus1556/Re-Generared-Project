// Vector art of a Flash movie rasterised for the screen (plutovg, the
// renderer lunasvg is built on): a shape, a morph shape at a ratio, a static
// text or a text field's glyphs, drawn at the resolution it appears at,
// into a premultiplied texture. Textures are cached per character and scale
// step (scales sqrt(2) apart, so a growing or shrinking clip re-rasterises
// rarely) and dropped when unused for a while, or the least recently drawn
// first when they outgrow a memory budget.
//
// Art seen for the first time is rasterised at once (screens rasterise what
// they show while loading). A new resolution of art already cached (a zoom,
// a window resize) is made on worker threads while the nearest cached one
// is drawn scaled, so nothing stalls; pump() (called by endFrame) swaps the
// finished ones in.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "flash/Swf.h"
#include "platform/GL.h"
#include "platform/TextureAtlas.h"

namespace flash {

struct Texture {
    GLuint id = 0;
    int width = 0, height = 0;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // where it is drawn: twips of the character
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;  // the texels drawn there (a tile's without its overlap)
    int tile = 0;                          // a tile's number in its art (0: whole art)
    platform::AtlasSlot slot;              // art up to 256 texels a side: its place in a shared page (id: the page's)
};

// Flash's filters (blur, glow, drop shadow) on the clips above a piece,
// innermost first, baked into its texture: each piece of a filtered clip is
// filtered on its own (Flash filtered the clip's whole picture; for the
// games' glowing and blurred texts and lights it is the same). Their sizes
// are in stage pixels, made screen pixels with `screenPerStagePixel`.
struct Effects {
    const std::vector<Filter>* filters = nullptr;
    float screenPerStagePixel = 1;
};

// Rasterised texels before they become a texture (premultiplied BGRA).
struct Pixels {
    int width = 0, height = 0;
    float x0 = 0, y0 = 0, twipsPerTexel = 20;  // twips at texel (0, 0)
    float drawn[4] = {};                       // Texture's x0, y0, x1, y1
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    int tile = 0;
    std::vector<unsigned char> bgra;
};

// A tile of big art: texels [x0, x1) x [y0, y1) of the art's whole grid at
// `scale`, numbered `index` (Raster::place).
struct TileArea {
    int index, x0, y0, x1, y1;
    float scale;
};
// Not a tile: the whole art without its filters (a filtered piece's stand-in).
inline constexpr TileArea kPlain{-1, 0, 0, 0, 0, 0};
// (Index -2: the whole art at the area's scale, as tiled art's coarse stand-in.)
inline constexpr int kWholeAt = -2;

// A text field's text laid out as Flash did (lines at '\r' and '\n', a
// word-wrapping field wrapped at its last space that fits, each line aligned
// in the field): each glyph (its index in the field's font) and where its
// origin goes, in the field's twips, the glyphs scaled by `glyphScale`;
// `drawn`: where the lines are (within the field).
struct PlacedGlyph {
    int glyph;
    float x, y;
};
std::vector<PlacedGlyph> layoutEditText(const EditText& field, const Font& font, const std::string& content, float& glyphScale,
                                        Rect& drawn);

class Raster {
public:
    explicit Raster(const Swf& swf);
    ~Raster();
    Raster(const Raster&) = delete;
    Raster& operator=(const Raster&) = delete;

    // `scale`: screen pixels per twip-unit pixel (the drawn size's scale).
    // `baked`: a colour transform applied to the texels (one with an added
    // colour, which drawing cannot apply; null for none).
    //
    // The textures that draw the art where `visible` (twips, the art's own
    // space; null: anywhere) is on screen are appended to `out` (none when
    // there is nothing to draw). Art more than kTileAbove texels wide or
    // tall at its scale (and without filters) comes in tiles of kTile
    // texels, and only the tiles `visible` touches are made and kept: art
    // zoomed far in or reaching far off screen costs no more than the part
    // of the screen it covers. Each texture draws its own part (u0..v1).
    void shape(int id, float scale, const ColorTransform* baked, const Effects* effects, const Rect* visible,
               std::vector<const Texture*>& out);
    void morph(int id, float ratio, float scale, const ColorTransform* baked, const Effects* effects,
               const Rect* visible, std::vector<const Texture*>& out);
    void text(int id, float scale, const ColorTransform* baked, const Effects* effects, const Rect* visible,
              std::vector<const Texture*>& out);
    // A text field's text in its font, colour and layout (cached by content).
    // Null when there is nothing to draw.
    const Texture* editText(int id, const std::string& text, float scale, const ColorTransform* baked = nullptr,
                         const Effects* effects = nullptr);
    // Shapes and morph shapes, each placed in a clip, rasterised together as
    // one picture with the filters applied to the whole (Flash's filtered
    // clip: its picture filtered, then faded as one). Its twips are the
    // clip's (`clip`, its character). Cached by what is drawn where.
    struct GroupPiece {
        Swf::Kind kind;  // kShape or kMorph
        int id;
        float ratio;     // morphs
        Matrix matrix;   // the piece's twips -> the clip's
        float alpha = 1; // its fade (drawn alone, then faded as one)
    };
    // Its textures (tiled as above when big and unfiltered) into `out`.
    // With `complete`: what is missing is only queued, to be made in the
    // background, and *complete says whether it was all there.
    void group(int clip, const std::vector<GroupPiece>& pieces, float scale, const Effects* effects,
               const Rect* visible, std::vector<const Texture*>& out, bool* complete = nullptr);

    // Once per drawn frame: swaps in finished textures, drops textures not
    // used for `frames` calls.
    // `uploadMs`: how long this frame may spend uploading art made in the
    // background (more while a transition hides the screen).
    void endFrame(int frames = 120, int uploadMs = 4);
    size_t cachedCount() const { return cache.size(); }  // (debug: memory)
    // (debug: memory) art waiting to be made, made and waiting to go up, its bytes
    void backlog(size_t& waiting, size_t& made, size_t& bytes);
    // (debug: memory) the cached textures' bytes, and the largest one's size
    size_t textureBytes(int& largestW, int& largestH) const;
    // (debug: memory) the art with the most cached textures: "kind:id count MB"
    std::string topArt(int n) const;
    // (debug: memory) texture storage ever allocated, art made a frame, morph detail
    std::string churn() const;
    // Whether art is still being made in the background.
    bool busy();
    // While on, the calls above only queue what is not cached yet, to be
    // made in the background (and return null): art asked for ahead of
    // when it is drawn.
    bool prefetching = false;
    // With prefetching: frames the art asked for is kept even if not drawn
    // (0: the usual eviction), for art needed a while from now.
    long prefetchKeep = 0;
    // While on, art that would be made at once (seen for the first time, no
    // other size to stand in) is queued ahead of everything else and not
    // drawn: a pass's new art is then made by all the workers side by side
    // (finishDeferred) instead of one piece after another on this thread.
    bool deferring = false;
    // Whether art was put off since the last finishDeferred.
    bool deferred() const { return !deferredKeys.empty(); }
    // Waits for the art put off (this thread making some of it too) and
    // uploads it, so drawing again finds it all.
    void finishDeferred();
    // Art made at once (not in the background) since the last call: how
    // many, and how long it took (debug profiling).
    void takeSyncStats(int& count, double& ms) {
        count = syncCount;
        ms = syncMs;
        syncCount = 0;
        syncMs = 0;
    }
    // Art made in the background but uploaded at once because it was drawn
    // before its turn (its upload stalls that frame): how many, how long.
    void takeUploadStats(int& count, double& ms) {
        count = uploadCount;
        ms = uploadMs;
        uploadCount = 0;
        uploadMs = 0;
    }

    // Whether the shape covers (x, y) in its own twips (hitTest shapeFlag).
    bool shapeContains(int id, float x, float y);

private:
    struct Key {
        int kind, id, ratio, level;
        uint64_t text;
        uint64_t effects = 0;  // the filters baked in (0: none)
        int tile = 0;          // a tile of it (TileArea::index; 0: the whole art)
        bool operator==(const Key& o) const {
            return kind == o.kind && id == o.id && ratio == o.ratio && level == o.level && text == o.text &&
                   effects == o.effects && tile == o.tile;
        }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return (static_cast<size_t>(k.kind) * 31 + k.id) * 1000003u ^ static_cast<size_t>(k.ratio) * 7919u ^
                   static_cast<size_t>(k.level) * 104729u ^ static_cast<size_t>(k.text) ^
                   static_cast<size_t>(k.effects * 0x9E3779B97F4A7C15ull) ^ static_cast<size_t>(k.tile) * 2654435761u;
        }
    };
    struct Entry {
        Texture tex;
        uint64_t lastUsed = 0;
    };

    struct Job {
        Key key;
        std::function<Pixels()> make;
    };
    const Texture* get(const Key& key, std::function<Pixels()> make);
    // The art's texture, or its tiles that `visible` touches (`grid`: its
    // bounds, which its tiles are cut from), into `out`.
    static constexpr int kTileAbove = 256, kTile = 254;
    // The whole of tiled art at most this many texels a side: drawn while
    // the tiles on screen are made (Raster::place).
    static constexpr int kCoarse = 1024;
    void place(Key key, const Rect& grid, const Rect* visible, const std::function<Pixels(const TileArea*)>& make,
               std::vector<const Texture*>& out, bool* complete = nullptr);
    void enqueue(const Key& key, std::function<Pixels()> make);
    // (deferring) the art queued first, or moved to the front if queued.
    void defer(const Key& key, std::function<Pixels()> make);
    std::vector<Key> deferredKeys;
    std::condition_variable made;  // a worker finished a piece (guarded by mutex)
    void keep(const Key& key);  // (prefetchKeep)
    std::unordered_map<Key, uint64_t, KeyHash> keepUntil;
    // Per art (its key without filters): the filtered texture last drawn.
    std::unordered_map<Key, Key, KeyHash> lastEffects;
    void noteEffects(const Key& key);
    static Key artOf(const Key& key);
    const Texture* takeDone(const Key& key);
    void pump(int ms);
    void work();

    const Swf& swf;
    std::unordered_map<Key, Entry, KeyHash> cache;
    std::unordered_set<Key, KeyHash> pending;  // being made (GL thread's view)
    uint64_t frame = 0;
    std::mutex mutex;  // guards jobs, done, quitting
    std::condition_variable wake;
    std::deque<Job> jobs;
    std::vector<std::pair<Key, Pixels>> done;
    // Made art being uploaded a strip of rows at a time (a big texture
    // took some 30 ms in one go), finished at once if drawn before.
    struct Uploading {
        bool active = false;
        Key key{};
        Pixels pixels;
        Texture tex;
        int rows = 0;
    } uploading;
    const Texture* install(const Key& key, const Texture& tex);
    // The cache's texels are kept under a budget (the driver keeps a copy
    // too): beyond it the least recently drawn art goes (endFrame).
    static constexpr size_t kBudget = size_t{80} << 20;
    size_t bytes = 0;
    // Morphs' detail (endFrame): while more than kBusyBytes of art is made a
    // frame (on average), 1-4 scale steps coarser, then 5-7 also with 32,
    // 16 or 8 tween steps instead of 64.
    static constexpr double kBusyBytes = 8 << 20;
    int morphDrop = 0;
    size_t madeThisFrame = 0;
    double madeAverage = 0;
    uint64_t detailChanged = 0;
    static size_t bytesOf(const Texture& t) { return static_cast<size_t>(t.width) * t.height * 4; }
    Entry& put(const Key& key, const Texture& tex);  // (the old texture released)
    void drop(std::unordered_map<Key, Entry, KeyHash>::iterator it);
    bool quitting = false;
    int syncCount = 0, uploadCount = 0;
    double syncMs = 0, uploadMs = 0;
    std::vector<std::thread> workers;
};

}  // namespace flash
