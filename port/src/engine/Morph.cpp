#include "engine/Morph.h"

#include <cstring>
#include <map>

#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/GL.h"

namespace {

// The parts of a POD mesh the game draws.
struct PodMesh {
    int numVertex = 0, numFaces = 0, components = 3;
    std::vector<float> vertices;
    std::vector<unsigned short> indices;
};

// PowerVR POD block tags (PVRTModelPOD.h).
enum : unsigned {
    kScene = 1001,
    kMesh = 2012,
    kMeshNumVtx = 6000,
    kMeshNumFaces = 6001,
    kMeshFaces = 6003,
    kMeshVtx = 6006,
    kDataType = 9000,
    kDataN = 9001,
    kDataStride = 9002,
    kDataData = 9003,
    kEnd = 0x80000000u,
};

class PodReader {
public:
    explicit PodReader(const std::vector<unsigned char>& d) : data(d) {}

    std::vector<PodMesh> meshes() {
        std::vector<PodMesh> out;
        size_t pos = 0;
        unsigned tag, len;
        while (next(pos, tag, len)) {
            if (tag == kMesh) {
                out.push_back(mesh(pos));
                continue;
            }
            // Containers (scene, ...) have no payload of their own: descend.
            if (tag != kScene) pos += len;
        }
        return out;
    }

private:
    bool next(size_t& pos, unsigned& tag, unsigned& len) {
        if (pos + 8 > data.size()) return false;
        std::memcpy(&tag, &data[pos], 4);
        std::memcpy(&len, &data[pos + 4], 4);
        pos += 8;
        return true;
    }

    // A data block (faces, positions): type, components, stride, bytes.
    void block(size_t& pos, unsigned endTag, int& components, std::vector<unsigned char>& bytes) {
        unsigned tag, len;
        while (next(pos, tag, len) && tag != (endTag | kEnd)) {
            if (tag == kDataN) std::memcpy(&components, &data[pos], 4);
            else if (tag == kDataData) bytes.assign(data.begin() + pos, data.begin() + pos + len);
            pos += len;
        }
    }

    PodMesh mesh(size_t& pos) {
        PodMesh m;
        unsigned tag, len;
        while (next(pos, tag, len) && tag != (kMesh | kEnd)) {
            if (tag == kMeshNumVtx) {
                std::memcpy(&m.numVertex, &data[pos], 4);
            } else if (tag == kMeshNumFaces) {
                std::memcpy(&m.numFaces, &data[pos], 4);
            } else if (tag == kMeshFaces || tag == kMeshVtx) {
                int n = 0;
                std::vector<unsigned char> bytes;
                block(pos, tag, n, bytes);
                if (tag == kMeshFaces) {
                    m.indices.resize(bytes.size() / 2);
                    std::memcpy(m.indices.data(), bytes.data(), m.indices.size() * 2);
                } else {
                    m.components = n;
                    m.vertices.resize(bytes.size() / 4);
                    std::memcpy(m.vertices.data(), bytes.data(), m.vertices.size() * 4);
                }
                continue;
            } else if (len == 0 && tag >= 6000 && tag < 7000) {
                // Other data containers: skip their contents.
                std::vector<unsigned char> ignored;
                int n = 0;
                block(pos, tag, n, ignored);
                continue;
            }
            pos += len;
        }
        return m;
    }

    const std::vector<unsigned char>& data;
};

const PodMesh* loadMesh(const std::string& file, int index) {
    // Files are shared by every blob (the original cached them in
    // ResourceManager).
    static std::map<std::string, std::vector<PodMesh>> cache;
    auto it = cache.find(file);
    if (it == cache.end()) {
        std::vector<unsigned char> bytes;
        assets::read(file, bytes);
        it = cache.emplace(file, PodReader(bytes).meshes()).first;
    }
    const std::vector<PodMesh>& meshes = it->second;
    return index >= 0 && index < static_cast<int>(meshes.size()) ? &meshes[index] : nullptr;
}

}  // namespace

// -------------------------------------------------------------------- Shape

Shape::Shape(const std::string& podFile, int meshIndex) {
    if (const PodMesh* m = loadMesh(podFile, meshIndex)) {
        numVertex = m->numVertex;
        components = m->components;
        vertices = m->vertices;
        indices = m->indices;
    }
}

void Shape::generateAABB() {
    float lowX = 1e9f, highX = -1e9f, lowY = 1e9f, highY = -1e9f;
    for (size_t i = 0; i < vertices.size(); ++i) {
        float v = vertices[i];
        if (i % 3 == 0) {
            if (v < lowX) lowX = v;
            else if (v > highX) highX = v;
        } else if (i % 3 == 1) {
            if (v < lowY) lowY = v;
            else if (v > highY) highY = v;
        }
    }
    maxX = static_cast<int>(highX);
    minX = static_cast<int>(lowX);
    maxY = static_cast<int>(highY);
    minY = static_cast<int>(lowY);
}

// -------------------------------------------------------------------- Morph

Morph::Morph(const std::string& name1, int mesh1, int t1, const std::string& name2, int mesh2, int t2, Color4 c)
    : color(c) {
    add(name1, mesh1, t1);
    add(name2, mesh2, t2);
}

Morph::~Morph() {
    for (Shape* s : shapes) delete s;
}

void Morph::add(const std::string& name, int mesh, int time) {
    shapes.push_back(new Shape(name, mesh));
    times.push_back(time);
}

void Morph::nextFrame() {
    if (finished && loopingState != 0) return;
    ++frame;
    int size = static_cast<int>(shapes.size());
    auto advance = [&] {
        if (times[current + 1] - times[current] < frame) {
            frame = 0;
            ++current;
        }
    };
    if (loopingState == 0) {
        advance();
        if (current + 1 >= size) current = 0;
    } else if (loopingState == 1) {
        advance();
        if (current + 1 >= size) {  // hold on the last shape
            finished = true;
            current = size - 2;
            frame = times[size - 1] - times[size - 2];
        }
    } else if (loopingState == 2) {
        advance();
        if (current + 1 >= size) {
            current = 0;
            finished = true;
            frame = 0;
        }
    }
}

void Morph::draw() {
    if (shapes.size() < 2) return;
    const Shape& a = *shapes[current];
    const Shape& b = *shapes[current + 1];
    shapes[current]->generateAABB();
    maxX = a.maxX;
    maxY = a.maxY;
    minX = a.minX;
    minY = a.minY;
    if (a.vertices.empty() || a.vertices.size() != b.vertices.size()) return;

    int span = times[current + 1] - times[current];
    float t = span != 0 ? static_cast<float>(frame) / static_cast<float>(span) : 0.0f;
    std::vector<float> v(a.vertices.size());
    for (size_t i = 0; i < v.size(); ++i) v[i] = a.vertices[i] + (b.vertices[i] - a.vertices[i]) * t;
    std::vector<float> colors(static_cast<size_t>(a.numVertex) * 4);
    for (int i = 0; i < a.numVertex; ++i) {
        colors[i * 4 + 0] = color.r;
        colors[i * 4 + 1] = color.g;
        colors[i * 4 + 2] = color.b;
        colors[i * 4 + 3] = color.a;
    }

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(posX, posY, 0);
    glRotatef(orientation, 0, 0, 1);
    glScalef(scaleX, scaleY, scaleZ);
    // With a colour array the glColor alpha the original also set has no
    // effect; the per-vertex colours carry it.
    platform::drawMesh(reinterpret_cast<platform::DrawId>(this), v.data(), a.components, a.numVertex, colors.data(),
                       a.indices.data(), static_cast<int>(a.indices.size()));
    glPopMatrix();
}
