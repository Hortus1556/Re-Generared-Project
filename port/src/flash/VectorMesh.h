// Vector art of a Flash movie as triangles for the GPU (platform::VectorMesh):
// a shape, a morph shape at a ratio or a static text cut into triangles once
// and drawn at any size, position, turn and colour by the GPU, as the Flash
// player drew its vectors, instead of rasterised into a texture per size.
//
// Fills are their outlines (curves flattened finely enough for the size
// they are drawn at) cut into triangles by libtess2, even-odd as Flash fills;
// strokes are built from their segments, joins and caps; solid colours are
// per vertex, gradients and bitmap fills small textures the triangles map by
// the fill's matrix. Edges are smoothed by the window's multisampling.
//
// Meshes are cached per character, size step (a factor sqrt(2): only how
// finely curves are flattened and how wide hairlines are depend on it) and
// morph step, and dropped when not drawn for a while; a text field's from
// its glyphs' meshes. What drawing needs beyond them (filters) stays with
// flash::Raster.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "flash/Swf.h"
#include "platform/DrawList.h"

namespace flash {

class MeshCache {
public:
    explicit MeshCache(const Swf& swf);
    ~MeshCache();
    MeshCache(const MeshCache&) = delete;
    MeshCache& operator=(const MeshCache&) = delete;

    // `scale`: screen pixels per pixel of the art (its drawn size). Null when
    // there is nothing to draw.
    std::shared_ptr<const platform::VectorMesh> shape(int id, float scale);
    std::shared_ptr<const platform::VectorMesh> morph(int id, float ratio, float scale);
    std::shared_ptr<const platform::VectorMesh> text(int id, float scale);
    // A text field showing `content` (laid out as flash::layoutEditText),
    // from its font's glyphs, each cut into triangles once.
    std::shared_ptr<const platform::VectorMesh> editText(int id, const std::string& content, float scale);

    // Once per drawn frame: drops meshes not drawn for `frames` frames.
    void endFrame(int frames = 300);
    size_t bytes() const { return total; }
    size_t count() const { return cache.size(); }

private:
    struct Key {
        int kind, id, ratio, level;
        uint64_t text = 0;  // (a text field's: its content's hash)
        bool operator==(const Key& o) const {
            return kind == o.kind && id == o.id && ratio == o.ratio && level == o.level && text == o.text;
        }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return (static_cast<size_t>(k.kind) * 31 + static_cast<size_t>(k.id)) * 1000003u ^
                   static_cast<size_t>(k.ratio) * 7919u ^ static_cast<size_t>(k.level + 64) * 104729u ^
                   static_cast<size_t>(k.text);
        }
    };
    struct Entry {
        std::shared_ptr<const platform::VectorMesh> mesh;  // (null: nothing to draw)
        uint64_t lastUsed = 0;
    };
    template <class Build>
    std::shared_ptr<const platform::VectorMesh> get(const Key& key, Build build);
    void drop(const Entry& e);

    const Swf& swf;
    std::unordered_map<Key, Entry, KeyHash> cache;
    uint64_t frame = 0;
    size_t total = 0;
};

}  // namespace flash
