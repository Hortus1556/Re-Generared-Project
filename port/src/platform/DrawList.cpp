// (GL's own matrices for the replay; recording reads the game's, GL.h)
#define BLEED_GL_MATRICES
#include "platform/DrawList.h"

#include "platform/Platform.h"
#include "platform/TextureAtlas.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace platform {
namespace {

// 2D affine map from game coordinates to screen pixels (y down):
// x' = a x + c y + tx, y' = b x + d y + ty.
struct Affine {
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    void apply(float x, float y, float& ox, float& oy) const {
        ox = a * x + c * y + tx;
        oy = b * x + d * y + ty;
    }
};

// A grid effect in effect (beginWarp), in screen pixels: how far each of
// the grid's corners has moved, and the map from the screen to the grid's
// cells (cell (i, j) spans [i, i + 1] x [j, j + 1]).
struct WarpGrid {
    int cols = 0, rows = 0;
    Affine toCells;
    std::vector<float> d;  // dx, dy per corner, corner (i, j) at (i * (rows + 1) + j) * 2

    // (x, y) moved with the corners of the cell it is in (bilinearly);
    // outside the grid, with its nearest edge
    void displace(float& x, float& y) const {
        float u, v;
        toCells.apply(x, y, u, v);
        u = std::clamp(u, 0.0f, static_cast<float>(cols));
        v = std::clamp(v, 0.0f, static_cast<float>(rows));
        const int i = std::min(static_cast<int>(u), cols - 1), j = std::min(static_cast<int>(v), rows - 1);
        const float fu = u - i, fv = v - j;
        const float* d00 = &d[(i * (rows + 1) + j) * 2];
        const float* d01 = d00 + 2;
        const float* d10 = &d[((i + 1) * (rows + 1) + j) * 2];
        const float* d11 = d10 + 2;
        for (int k = 0; k < 2; ++k) {
            const float bottom = d00[k] + (d10[k] - d00[k]) * fu, top = d01[k] + (d11[k] - d01[k]) * fu;
            (k ? y : x) += bottom + (top - bottom) * fv;
        }
    }
};

struct Command {
    DrawId context = 0;  // the DrawOwner in effect
    DrawId owner = 0;
    DrawId group = 0;  // the KeyframeGroup, 0 for none
    GLuint texture = 0;
    bool textured = false;
    bool mesh = false;
    GLenum srcBlend = GL_SRC_ALPHA;
    GLenum dstBlend = GL_ONE_MINUS_SRC_ALPHA;  // BlendDst in effect
    float rgba[4] = {1, 1, 1, 1};
    bool predicted = false;  // nextStep() was given for it
    bool atOnce = false;     // appearAtOnce() was given for it
    float appearAt = 0;      // its time within the pass (atOnce)
    bool pointer = false;    // pointer art (PointerArt), drawn where it was
    float pointerAt[2] = {0, 0};
    float step[3] = {0, 0, 0};  // dx, dy, dAngle to the next pass
    Affine m;          // the GL matrices it was drawn under
    Affine anchor;     // its KeyframeGroup's transform (only if group != 0)
    Sprite s;          // its own transform and corners (sprites)
    float uv[8] = {};  // sprites
    std::vector<float> world;   // where it landed, x, y per vertex
    std::vector<float> colors;  // meshes, rgba per vertex
    std::vector<unsigned short> indices;
    size_t uvHash = 0;
    // Masks: +1 / -1 writes this draw's shape into the stencil (adds or
    // removes a mask level) instead of the screen; a marker (no geometry)
    // moves the level draws are clipped to by `marker`.
    int stencil = 0;
    int marker = 0;
    std::shared_ptr<const WarpGrid> warp;  // the grid effect it is drawn under, if any
    // A piece of vector art (artPiece): its texture's place, the bounds it
    // shows, its clip's place and instance, its drawing.
    bool art = false;
    Affine local, parent;
    float bounds[4] = {};
    unsigned long long parentId = 0, drawing = 0;
    // Vector art as triangles (drawVector): the mesh `local` places, its
    // colour transform's added part (its multipliers: rgba).
    std::shared_ptr<const VectorMesh> vector;
    float add[4] = {0, 0, 0, 0};
    // Against the previous frame:
    int prev = -1;        // the same draw there, or -1
    bool lerpUv = false;  // a crop of its image region that grew or shrank
};

}  // namespace

struct Frame {
    unsigned long long seq = 0;  // its place among the frames committed
    std::vector<Command> commands;
    float clear[4] = {0, 0, 0, 1};
    // For each draw of the previous frame: its counterpart here, or -1.
    std::vector<int> next;
    // Each keyframe group's matrix here.
    std::unordered_map<DrawId, Affine> groups;
    // The game screen's size it was recorded at (0: not yet committed).
    int width = 0, height = 0;
    // Where the pointer was for the pass that recorded it (game pixels).
    float pointer[2] = {0, 0};
};

namespace {

// The frame being recorded, and the last three committed (the newest is
// the shown frame), as the game's side sees them (debug shots, matching).
Frame gRecording;
std::shared_ptr<Frame> gShown = std::make_shared<Frame>(), gPrevious = std::make_shared<Frame>(),
                       gPrevious2 = std::make_shared<Frame>();
const Frame kNoFrame;

// Frames for the display thread, oldest first, each with when it is shown.
struct Queued {
    std::shared_ptr<const Frame> frame;
    double due, span;
};
bool gDisplayThread = false;
std::mutex gQueueMutex;  // guards gQueue and gDisplayOldest
std::deque<Queued> gQueue;
// The oldest frame the display thread may still draw from (its previous).
unsigned long long gDisplayOldest = 0;
bool gHasDue = false;
double gNextDue = 0;

// Texture work for the display thread (setTexels, deletion), in order, each
// with the frame being recorded when it was asked for.
struct TextureWork {
    GLuint id;
    int width, height;
    bool allocate;
    bool remove;
    std::vector<unsigned char> texels;
    unsigned long long frame;
    int x = 0, y = 0;  // (written over: where in the texture)
};
std::mutex gWorkMutex;  // guards gWork, gWorking
std::condition_variable gWorkCv;  // work queued, or all of it done
std::deque<TextureWork> gWork;
bool gWorking = false;  // (a batch being done)
// The first frame some queued or unfinished texture work is for: frames
// before it have all their textures (no work: the largest number).
std::atomic<unsigned long long> gWorkFrom{~0ull};
// Taken by the display thread while it draws a frame and by the texture
// thread for each small piece of its work: the driver serialises the two
// contexts, so a frame drawn while a big upload ran took as long as it.
std::mutex gDrawTurn;

void writeTexels(GLuint id, int width, int height, bool allocate, const unsigned char* texels, int x = 0, int y = 0) {
    glBindTexture(GL_TEXTURE_2D, id);
    if (allocate) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, kTexelFormat, GL_UNSIGNED_BYTE, texels);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, kTexelFormat, GL_UNSIGNED_BYTE, texels);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}
DrawId gContext = 0, gGroup = 0;
Affine gGroupAnchor;  // the GL matrix as of KeyframeGroup's start, while gGroup != 0
bool gPredicted = false;
bool gAtOnce = false;
float gAppearAt = 0;
bool gPointerArt = false;
float gPointerAt[2] = {0, 0};
int gStencilMode = 0;
std::shared_ptr<const WarpGrid> gWarp;
GLenum gDstBlend = GL_ONE_MINUS_SRC_ALPHA;
float gStep[3] = {0, 0, 0};
bool gArtPending = false;
ArtPiece gArtPiece;
thread_local std::vector<TracedDraw>* gTrace = nullptr;
thread_local int gGlDraws = 0;  // (debug: the draws sent since takeGlDraws)
thread_local int gGlVertices = 0;  // (and their vertices)

void trace(const Command& c, const float* xy, size_t n) {
    if (!gTrace || n == 0) return;
    float x = 0, y = 0;
    for (size_t i = 0; i < n; ++i) {
        x += xy[i * 2];
        y += xy[i * 2 + 1];
    }
    gTrace->push_back({c.context, c.owner, c.group, c.texture, x / n, y / n});
}
// Textures released while recording a frame may still be drawn by it and
// the frames before, never after: they go once no frame that can still be
// shown is that old (commitFrame). A size: recycled (kept in gPool), else
// deleted.
struct Released {
    GLuint id;
    int width, height;
    AtlasSlot slot;  // (a slot of a page instead)
};
std::vector<Released> gReleased;
std::deque<std::pair<unsigned long long, std::vector<Released>>> gReleasedBy;  // by the frame recorded
// Recycled textures, oldest first: kept up to a size and for a few seconds.
struct Pooled {
    GLuint id;
    int width, height;
    unsigned long long since;
};
std::deque<Pooled> gPool;
size_t gPoolBytes = 0;
unsigned long long gCommits = 0;
constexpr size_t kPoolBytes = size_t{16} << 20;
constexpr unsigned long long kPoolFrames = 120;
size_t texelBytes(int w, int h) { return static_cast<size_t>(w) * h * 4; }

// A draw whose centre jumps further than this (game pixels, or its own size
// if larger) between two frames is a different object or a teleport, and is
// not interpolated: 160 pixels on the iPad's 1024-wide screen, the same
// share of a smaller one.
float maxTravel() { return 160.0f * platform::screenWidth() / 1024.0f; }
constexpr float kPi = 3.14159265f;
// How near the pointer new art counts as made at the finger (game pixels).
constexpr float kFingerReach = 30;

Affine currentMatrix() {
    float mv[16], pr[16], m[16];
    glm::getFloatv(GL_MODELVIEW_MATRIX, mv);
    glm::getFloatv(GL_PROJECTION_MATRIX, pr);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += pr[k * 4 + r] * mv[c * 4 + k];
            m[c * 4 + r] = s;
        }
    // Normalised device coordinates to game-screen pixels: x (n + 1) * w / 2,
    // y (1 - n) * h / 2.
    const float hw = platform::screenWidth() * 0.5f, hh = platform::screenHeight() * 0.5f;
    Affine a;
    a.a = m[0] * hw;
    a.c = m[4] * hw;
    a.tx = (m[12] + 1) * hw;
    a.b = -m[1] * hh;
    a.d = -m[5] * hh;
    a.ty = (1 - m[13]) * hh;
    return a;
}

// The sprite's corners on screen.
void corners(const Affine& m, const Sprite& s, float out[8]) {
    float rad = s.angle * kPi / 180;
    float cs = std::cos(rad), sn = std::sin(rad);
    for (int i = 0; i < 4; ++i) {
        float x = s.xy[i * 2] - s.pivotOut[0], y = s.xy[i * 2 + 1] - s.pivotOut[1];
        float rx = x * cs - y * sn + s.pos[0] + s.offset[0] + s.pivotIn[0];
        float ry = x * sn + y * cs + s.pos[1] + s.offset[1] + s.pivotIn[1];
        m.apply(rx, ry, out[i * 2], out[i * 2 + 1]);
    }
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

// From b towards c, along the parabola through a, b and c (a the frame
// before b) while the motion keeps its direction: exact for things thrown or
// falling (steady acceleration), the same as a straight line for steady
// speed. A straight line where it starts, stops or turns back (a shake), and
// never beyond b..c.
float curve(const float* a, float b, float c, float t) {
    if (!a || t > 1 || (b - *a) * (c - b) <= 0) return lerp(b, c, t);
    float v = b + t * (c - *a) / 2 + t * t * (c - 2 * b + *a) / 2;
    return std::clamp(v, std::min(b, c), std::max(b, c));
}

float lerpAngle(float a, float b, float t) {
    float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
    return a + d * t;
}

// Matrices interpolate as translation, turn and scale (so a turn sweeps an
// arc); one with shear falls back to plain component interpolation.
Affine lerpAffine(const Affine& p, const Affine& q, float t) {
    auto decompose = [](const Affine& m, float& angle, float& sx, float& sy) {
        sx = std::hypot(m.a, m.b);
        if (sx < 1e-6f) return false;
        angle = std::atan2(m.b, m.a);
        sy = (m.a * m.d - m.b * m.c) / sx;
        return std::fabs(m.a * m.c + m.b * m.d) <= 1e-3f * sx * std::fabs(sy) + 1e-6f;
    };
    Affine r;
    r.tx = lerp(p.tx, q.tx, t);
    r.ty = lerp(p.ty, q.ty, t);
    float ap, sxp, syp, aq, sxq, syq;
    if (decompose(p, ap, sxp, syp) && decompose(q, aq, sxq, syq)) {
        float d = std::fmod(aq - ap + 3 * kPi, 2 * kPi) - kPi;
        float ang = ap + d * t, sx = lerp(sxp, sxq, t), sy = lerp(syp, syq, t);
        r.a = sx * std::cos(ang);
        r.b = sx * std::sin(ang);
        r.c = -sy * std::sin(ang);
        r.d = sy * std::cos(ang);
    } else {
        r.a = lerp(p.a, q.a, t);
        r.b = lerp(p.b, q.b, t);
        r.c = lerp(p.c, q.c, t);
        r.d = lerp(p.d, q.d, t);
    }
    return r;
}

// `inner` applied first, then `outer` (as nested glPushMatrix/glMultMatrix
// would: the matrix pushed deeper acts on a point first).
Affine compose(const Affine& outer, const Affine& inner) {
    Affine r;
    r.a = outer.a * inner.a + outer.c * inner.b;
    r.b = outer.b * inner.a + outer.d * inner.b;
    r.c = outer.a * inner.c + outer.c * inner.d;
    r.d = outer.b * inner.c + outer.d * inner.d;
    r.tx = outer.a * inner.tx + outer.c * inner.ty + outer.tx;
    r.ty = outer.b * inner.tx + outer.d * inner.ty + outer.ty;
    return r;
}

Affine invert(const Affine& m) {
    float det = m.a * m.d - m.b * m.c;
    if (std::fabs(det) < 1e-9f) return Affine{};
    Affine r;
    r.a = m.d / det;
    r.b = -m.b / det;
    r.c = -m.c / det;
    r.d = m.a / det;
    r.tx = -(r.a * m.tx + r.c * m.ty);
    r.ty = -(r.b * m.tx + r.d * m.ty);
    return r;
}

size_t hashFloats(const float* v, int n) {
    size_t h = 1469598103934665603ull;
    for (int i = 0; i < n; ++i) {
        unsigned int bits;
        std::memcpy(&bits, &v[i], sizeof bits);
        h = (h ^ bits) * 1099511628211ull;
    }
    return h;
}

struct Key {
    DrawId context;
    DrawId owner;
    GLuint texture;
    size_t uv;
    bool operator==(const Key& o) const {
        return context == o.context && owner == o.owner && texture == o.texture && uv == o.uv;
    }
};
struct KeyHash {
    size_t operator()(const Key& k) const {
        return k.context * 1000003u ^ k.owner * 2654435761u ^ k.texture * 40503u ^ k.uv * 131;
    }
};

void centre(const Command& c, float& x, float& y) {
    x = y = 0;
    size_t n = c.world.size() / 2;
    for (size_t i = 0; i < c.world.size(); i += 2) {
        x += c.world[i];
        y += c.world[i + 1];
    }
    x /= n;
    y /= n;
}

// Width and height of where it landed.
void extent(const Command& c, float& w, float& h) {
    float x0 = c.world[0], x1 = x0, y0 = c.world[1], y1 = y0;
    for (size_t i = 2; i < c.world.size(); i += 2) {
        x0 = std::min(x0, c.world[i]);
        x1 = std::max(x1, c.world[i]);
        y0 = std::min(y0, c.world[i + 1]);
        y1 = std::max(y1, c.world[i + 1]);
    }
    w = x1 - x0;
    h = y1 - y0;
}

// Whether `b` can be what `a` became in one pass: about the same width and
// height (at most twice or half, unless tiny) and its centre moved at most
// maxTravel() or its size (a large card swinging in moves further than a
// small sprite could without being a different one).
bool closeEnough(const Command& a, const Command& b) {
    if (a.mesh != b.mesh || a.world.size() != b.world.size() || a.textured != b.textured) return false;
    float ax, ay, bx, by, aw, ah, bw, bh;
    centre(a, ax, ay);
    centre(b, bx, by);
    extent(a, aw, ah);
    extent(b, bw, bh);
    auto similar = [](float u, float v) { return std::max(u, v) <= 24 || (u <= 2 * v && v <= 2 * u); };
    if (!a.mesh && (!similar(aw, bw) || !similar(ah, bh))) return false;
    float size = std::min(std::hypot(aw, ah), std::hypot(bw, bh));
    return std::hypot(ax - bx, ay - by) <= std::max(maxTravel(), size);
}

// Where a quad puts its texture on screen: the screen point of texel (0, 0)
// and the screen steps per unit of u and of v.
bool textureOnScreen(const Command& c, float o[2], float du[2], float dv[2]) {
    float su = c.uv[2] - c.uv[0], sv = c.uv[5] - c.uv[1];
    if (std::fabs(su) < 1e-6f || std::fabs(sv) < 1e-6f) return false;
    for (int k = 0; k < 2; ++k) {
        du[k] = (c.world[2 + k] - c.world[k]) / su;
        dv[k] = (c.world[4 + k] - c.world[k]) / sv;
        o[k] = c.world[k] - c.uv[0] * du[k] - c.uv[1] * dv[k];
    }
    return true;
}

// Whether `c` shows another part of `p`'s image with the image itself where
// it was: a crop whose edge moved (water receding, a bar filling up).
bool sameImagePlacement(const Command& c, const Command& p) {
    float oc[2], uc[2], vc[2], op[2], up[2], vp[2];
    if (!textureOnScreen(c, oc, uc, vc) || !textureOnScreen(p, op, up, vp)) return false;
    for (int k = 0; k < 2; ++k) {
        if (std::fabs(oc[k] - op[k]) > 1) return false;
        if (std::fabs(uc[k] - up[k]) > 0.01f * (std::fabs(up[k]) + 1)) return false;
        if (std::fabs(vc[k] - vp[k]) > 0.01f * (std::fabs(vp[k]) + 1)) return false;
    }
    return true;
}

// Pairs each draw (outside keyframe groups) with its counterpart in `prev`:
// among the draws by the same owner and image of the same image region, the
// one in the same place in drawing order if there are as many as before,
// else the nearest unpaired one; else, if the owner draws that texture just
// once in both frames, that draw (an animation that stepped to another
// cell); else, if the owner draws just once in both frames, that draw
// whatever its image (an animation that stepped to another picture, as
// AS2's, which swap whole textures: a tick flying with its wings beating).
// Owners mostly draw an image once, so this is usually exact.
void match(Frame& cur, const Frame& prev, const Frame& prev2) {
    std::unordered_map<Key, std::vector<int>, KeyHash> exact, loose;
    for (int i = 0; i < static_cast<int>(prev.commands.size()); ++i) {
        const Command& c = prev.commands[i];
        if (c.group || c.marker) continue;
        exact[{c.context, c.owner, c.texture, c.uvHash}].push_back(i);
        loose[{c.context, c.owner, c.texture, 0}].push_back(i);
    }
    std::unordered_map<Key, int, KeyHash> curExact, curLoose;
    for (const Command& c : cur.commands) {
        if (c.group || c.marker) continue;
        ++curExact[{c.context, c.owner, c.texture, c.uvHash}];
        ++curLoose[{c.context, c.owner, c.texture, 0}];
    }
    // Each owner's draws (any image), for its lone draw changing picture.
    std::unordered_map<Key, std::vector<int>, KeyHash> prevOwner;
    std::unordered_map<Key, int, KeyHash> curOwner;
    for (int i = 0; i < static_cast<int>(prev.commands.size()); ++i) {
        const Command& c = prev.commands[i];
        if (c.group || c.marker || !c.owner) continue;
        prevOwner[{c.context, c.owner, 0, 0}].push_back(i);
    }
    for (const Command& c : cur.commands)
        if (!c.group && !c.marker && c.owner) ++curOwner[{c.context, c.owner, 0, 0}];
    // Flash art by owner and character: the same piece whatever texture it
    // is drawn with this pass (made again at another size, a filter or
    // morph step, a stand-in replaced by the art it stood in for).
    std::unordered_map<Key, std::vector<int>, KeyHash> prevArt;
    std::unordered_map<Key, int, KeyHash> curArt;
    auto artKey = [](const Command& c) { return Key{c.context, c.owner, 0, static_cast<size_t>(c.drawing) ^ 0x5a17u}; };
    for (int i = 0; i < static_cast<int>(prev.commands.size()); ++i) {
        const Command& c = prev.commands[i];
        if (!c.group && !c.marker && c.art) prevArt[artKey(c)].push_back(i);
    }
    for (const Command& c : cur.commands)
        if (!c.group && !c.marker && c.art) ++curArt[artKey(c)];
    std::unordered_map<Key, size_t, KeyHash> exactSeen;
    std::vector<char> used(prev.commands.size(), 0);
    auto nearest = [&](const Command& c, const std::vector<int>& candidates) {
        float cx, cy;
        centre(c, cx, cy);
        int best = -1;
        float bestD = 0;
        for (int i : candidates) {
            if (used[i] || !closeEnough(c, prev.commands[i])) continue;
            float px, py;
            centre(prev.commands[i], px, py);
            float d = (px - cx) * (px - cx) + (py - cy) * (py - cy);
            if (best < 0 || d < bestD) {
                best = i;
                bestD = d;
            }
        }
        return best;
    };
    cur.groups.clear();
    for (Command& c : cur.commands) {
        c.prev = -1;
        if (c.marker) continue;
        if (c.group) {
            cur.groups.emplace(c.group, c.anchor);
            continue;
        }
        Key ek{c.context, c.owner, c.texture, c.uvHash};
        size_t k = exactSeen[ek]++;
        auto e = exact.find(ek);
        if (e != exact.end()) {
            if (e->second.size() == static_cast<size_t>(curExact[ek])) {
                int i = e->second[k];
                if (!used[i] && closeEnough(c, prev.commands[i])) c.prev = i;
            } else {
                c.prev = nearest(c, e->second);
            }
        }
        if (c.prev < 0 && c.art) {
            auto a = prevArt.find(artKey(c));
            if (a != prevArt.end() && a->second.size() == static_cast<size_t>(curArt[artKey(c)]))
                c.prev = nearest(c, a->second);
        }
        if (c.prev < 0) {
            Key lk{c.context, c.owner, c.texture, 0};
            auto l = loose.find(lk);
            if (l != loose.end() && l->second.size() == 1 && curLoose[lk] == 1) c.prev = nearest(c, l->second);
        }
        if (c.prev < 0 && c.owner) {
            Key ok{c.context, c.owner, 0, 0};
            auto o = prevOwner.find(ok);
            if (o != prevOwner.end() && o->second.size() == 1 && curOwner[ok] == 1) c.prev = nearest(c, o->second);
            // (Not two different pieces of Flash art, though: another
            // character placed there is another object, as AS1's cursor
            // showing the next tool, whose picture would otherwise slide
            // out of the cursor from where the last one was drawn.)
            if (c.prev >= 0 && c.art && prev.commands[c.prev].art && c.drawing != prev.commands[c.prev].drawing)
                c.prev = -1;
        }
        if (c.prev < 0) continue;
        used[c.prev] = 1;
        const Command& p = prev.commands[c.prev];
        // A region revealed or hidden from one corner (a word written out
        // left to right, a body rising out of a cut) keeps that corner's
        // texture coordinate, and a crop of an image that stays put (the
        // credits' water receding) keeps the image where it was: either is
        // revealed smoothly. An animation stepping to another cell does
        // neither.
        c.lerpUv = false;
        // (Never vector art: it moves by its place, and its texture's region
        // is a slot of a shared page, the next slot's texels past its edge.)
        if (c.textured && !c.art && std::memcmp(c.uv, p.uv, sizeof c.uv) != 0) {
            for (int v = 0; v < 4; ++v)
                if (c.uv[v * 2] == p.uv[v * 2] && c.uv[v * 2 + 1] == p.uv[v * 2 + 1]) c.lerpUv = true;
            if (!c.lerpUv) c.lerpUv = sameImagePlacement(c, p);
        }
    }

    cur.next.assign(prev.commands.size(), -1);
    for (int i = 0; i < static_cast<int>(cur.commands.size()); ++i)
        if (cur.commands[i].prev >= 0) cur.next[cur.commands[i].prev] = i;
}

Command& record(DrawId owner, GLuint texture, const float* uv, const float rgba[4], GLenum srcBlend) {
    gRecording.commands.emplace_back();
    Command& c = gRecording.commands.back();
    c.context = gContext;
    c.group = gGroup;
    c.anchor = gGroupAnchor;
    c.predicted = gPredicted;
    c.atOnce = gAtOnce;
    c.appearAt = gAppearAt;
    gAtOnce = false;
    gAppearAt = 0;
    c.pointer = gPointerArt;
    c.pointerAt[0] = gPointerAt[0];
    c.pointerAt[1] = gPointerAt[1];
    std::copy_n(gStep, 3, c.step);
    gPredicted = false;
    if (gArtPending) {
        const ArtPiece& a = gArtPiece;
        c.art = true;
        c.local = Affine{a.local[0], a.local[1], a.local[2], a.local[3], a.local[4], a.local[5]};
        c.parent = Affine{a.parent[0], a.parent[1], a.parent[2], a.parent[3], a.parent[4], a.parent[5]};
        std::copy_n(a.bounds, 4, c.bounds);
        c.parentId = a.parentId;
        c.drawing = a.drawing;
        gArtPending = false;
    }
    c.owner = owner;
    c.textured = uv != nullptr;
    c.texture = c.textured ? texture : 0;
    c.srcBlend = srcBlend;
    c.dstBlend = gDstBlend;
    c.stencil = gStencilMode;
    c.warp = gWarp;
    std::copy_n(rgba, 4, c.rgba);
    c.m = currentMatrix();
    if (c.textured) {
        std::copy_n(uv, 8, c.uv);
        c.uvHash = hashFloats(uv, 8);
    }
    return c;
}

// ---- drawing (in screen pixels)

// The grid effect a draw between the previous frame's `p` and the shown
// frame's `q` is shown under, `s` of the way: the corners' moves eased
// between the two (from or to none when only one has it). Valid until the
// next call.
const WarpGrid* warpBetween(const Command& p, const Command& q, float s) {
    const WarpGrid* a = p.warp.get();
    const WarpGrid* b = q.warp.get();
    if (!a && !b) return nullptr;
    thread_local WarpGrid mid;
    if (a && b && (a->cols != b->cols || a->rows != b->rows)) return b;
    const WarpGrid* shape = b ? b : a;
    mid.cols = shape->cols;
    mid.rows = shape->rows;
    mid.toCells = shape->toCells;
    mid.d.resize(shape->d.size());
    for (size_t i = 0; i < mid.d.size(); ++i) mid.d[i] = lerp(a ? a->d[i] : 0, b ? b->d[i] : 0, s);
    return &mid;
}

// A quad under a grid effect: cut into pieces small against the grid's cells,
// each corner moved with the grid.
void drawWarpedQuad(const Command& c, const float xy[8], const float uv[8], const WarpGrid& w) {
    float lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
    for (int v = 0; v < 4; ++v) {
        float cu, cv;
        w.toCells.apply(xy[v * 2], xy[v * 2 + 1], cu, cv);
        lo[0] = std::min(lo[0], cu), hi[0] = std::max(hi[0], cu);
        lo[1] = std::min(lo[1], cv), hi[1] = std::max(hi[1], cv);
    }
    const int n = std::clamp(static_cast<int>(std::ceil(std::max(hi[0] - lo[0], hi[1] - lo[1]) * 4)), 1, 32);
    thread_local std::vector<float> pts, tex;
    thread_local std::vector<unsigned short> idx;
    pts.clear();
    tex.clear();
    idx.clear();
    // the strip's corners: 0-1 one edge, 2-3 the opposite one
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i) {
            const float a = static_cast<float>(i) / n, b = static_cast<float>(j) / n;
            for (int k = 0; k < 2; ++k) {
                const float e0 = xy[k] + (xy[2 + k] - xy[k]) * a, e1 = xy[4 + k] + (xy[6 + k] - xy[4 + k]) * a;
                pts.push_back(e0 + (e1 - e0) * b);
                const float t0 = uv[k] + (uv[2 + k] - uv[k]) * a, t1 = uv[4 + k] + (uv[6 + k] - uv[4 + k]) * a;
                tex.push_back(t0 + (t1 - t0) * b);
            }
            w.displace(pts[pts.size() - 2], pts.back());
        }
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            const unsigned short a = static_cast<unsigned short>(j * (n + 1) + i), b = a + 1;
            const unsigned short cc = static_cast<unsigned short>(a + n + 1), d = cc + 1;
            idx.insert(idx.end(), {a, b, cc, b, d, cc});
        }
    glVertexPointer(2, GL_FLOAT, 0, pts.data());
    if (c.textured) glTexCoordPointer(2, GL_FLOAT, 0, tex.data());
    ++gGlDraws;
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(idx.size()), GL_UNSIGNED_SHORT, idx.data());
}

// Quads drawn one after another with the same texture and blending go to
// the GPU together, as one draw with their colours per vertex: with small
// art packed in shared pages (TextureAtlas) a frame is a few dozen draws
// instead of one, with its state set anew, per piece. What changes the
// state they are drawn under (a mask, a grid effect, the pointer's shift, a
// mesh) sends what was gathered first (flushBatch).
struct Batch {
    bool textured = false;
    GLuint texture = 0;
    GLenum src = GL_ONE, dst = GL_ONE_MINUS_SRC_ALPHA;
    std::vector<float> xy, uv, rgba;
    std::vector<unsigned short> indices;
    // A texture's alpha tinted by the vertices' colour (rgb: colour times
    // texel alpha), not the texture itself: an added colour (drawVectorAt).
    bool tint = false;
    // Room for `vertices` more in this state (sent first if the state
    // differs or the indices would run out): the first new vertex's index.
    unsigned short begin(bool tex, GLuint id, GLenum s, GLenum d, int vertices, bool tinted = false);
};
thread_local Batch gBatch;
const float kNoAdd[4] = {0, 0, 0, 0};
// The game screen's size the frame being replayed was recorded at.
thread_local float gReplayWidth = 0, gReplayHeight = 0;

void flushBatch();

unsigned short Batch::begin(bool tex, GLuint id, GLenum s, GLenum d, int vertices, bool tinted) {
    if (!xy.empty() && (textured != tex || texture != id || src != s || dst != d || tint != tinted ||
                        xy.size() / 2 + static_cast<size_t>(vertices) > 65535))
        flushBatch();
    tint = tinted;
    textured = tex;
    texture = id;
    src = s;
    dst = d;
    return static_cast<unsigned short>(xy.size() / 2);
}

void flushBatch() {
    Batch& b = gBatch;
    if (b.xy.empty()) return;
    glBlendFunc(b.src, b.dst);
    glVertexPointer(2, GL_FLOAT, 0, b.xy.data());
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, b.rgba.data());
    if (b.textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, b.texture);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, b.uv.data());
        if (b.tint) {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        }
    }
    ++gGlDraws;
    gGlVertices += static_cast<int>(b.xy.size() / 2);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(b.indices.size()), GL_UNSIGNED_SHORT, b.indices.data());
    if (b.tint) glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (b.textured) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_2D);
    }
    glDisableClientState(GL_COLOR_ARRAY);
    b.xy.clear();
    b.uv.clear();
    b.rgba.clear();
    b.indices.clear();
}

void drawQuadAt(const Command& c, const float xy[8], const float uv[8], const float rgba[4], const WarpGrid* warp) {
    trace(c, xy, 4);
    if (!warp && !c.stencil) {
        Batch& b = gBatch;
        const unsigned short at = b.begin(c.textured, c.textured ? c.texture : 0, c.srcBlend, c.dstBlend, 4);
        b.xy.insert(b.xy.end(), xy, xy + 8);
        if (c.textured) b.uv.insert(b.uv.end(), uv, uv + 8);
        for (int k = 0; k < 4; ++k) b.rgba.insert(b.rgba.end(), rgba, rgba + 4);
        // (the strip's two triangles, 0 1 2 and 2 1 3)
        for (int k : {0, 1, 2, 2, 1, 3}) b.indices.push_back(static_cast<unsigned short>(at + k));
        return;
    }
    flushBatch();
    glVertexPointer(2, GL_FLOAT, 0, xy);
    glBlendFunc(c.srcBlend, c.dstBlend);
    // A mask's shape goes into the stencil by its own texels' alpha, not
    // its fade: a mask on a clip fading in (a layer coming up, alpha 0 to
    // 100) is the whole shape from the start, as in Flash. (Tested with the
    // fade, every mask was empty below 50%: the ice shards masked in their
    // holders missing through the first half of the crossfade.)
    glColor4f(rgba[0], rgba[1], rgba[2], c.stencil ? 1.0f : rgba[3]);
    if (c.textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, c.texture);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, uv);
    }
    if (warp) drawWarpedQuad(c, xy, uv, *warp);
    else {
        ++gGlDraws;
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    if (c.textured) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_2D);
    }
}

void drawQuadAt(const Command& c, const float xy[8], const float uv[8], const float rgba[4]) {
    drawQuadAt(c, xy, uv, rgba, c.warp.get());
}

void drawMeshAt(const Command& c, const float* xy, const float* colors, const WarpGrid* warp) {
    flushBatch();
    trace(c, xy, c.world.size() / 2);
    thread_local std::vector<float> moved;
    if (warp) {
        moved.assign(xy, xy + c.world.size());
        for (size_t i = 0; i + 1 < moved.size(); i += 2) warp->displace(moved[i], moved[i + 1]);
        xy = moved.data();
    }
    glVertexPointer(2, GL_FLOAT, 0, xy);
    glBlendFunc(c.srcBlend, c.dstBlend);
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, colors);
    ++gGlDraws;
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(c.indices.size()), GL_UNSIGNED_SHORT, c.indices.data());
    glDisableClientState(GL_COLOR_ARRAY);
}

void drawMeshAt(const Command& c, const float* xy, const float* colors) { drawMeshAt(c, xy, colors, c.warp.get()); }

// A piece of vector art's texture `local` places, its corners as recorded.
void artCorners(const Affine& local, const float b[4], float xy[8]) {
    local.apply(b[0], b[1], xy[0], xy[1]);
    local.apply(b[2], b[1], xy[2], xy[3]);
    local.apply(b[0], b[3], xy[4], xy[5]);
    local.apply(b[2], b[3], xy[6], xy[7]);
}

// Vector art's triangles where `to` maps the mesh's units on screen, its
// colours by (mult, add), into the batch (a mask's straight to the stencil).
// Nothing when all of it is off the game screen.
void drawVectorAt(const Command& c, const Affine& to, const float mult[4], const float add[4], const WarpGrid* warp) {
    const VectorMesh& v = *c.vector;
    float corner[8];
    artCorners(to, v.bounds, corner);
    trace(c, corner, 4);
    if (!warp && !c.pointer) {
        float x0 = corner[0], x1 = x0, y0 = corner[1], y1 = y0;
        for (int i = 2; i < 8; i += 2) {
            x0 = std::min(x0, corner[i]);
            x1 = std::max(x1, corner[i]);
            y0 = std::min(y0, corner[i + 1]);
            y1 = std::max(y1, corner[i + 1]);
        }
        if (x1 < 0 || y1 < 0 || x0 > gReplayWidth || y0 > gReplayHeight) return;
    }
    Batch& b = gBatch;
    for (const VectorMesh::Part& part : v.parts) {
        const bool textured = part.texture != 0;
        const unsigned short at = b.begin(textured, part.texture, GL_ONE, c.dstBlend, part.vertexCount);
        const size_t from = b.xy.size();
        b.xy.resize(from + static_cast<size_t>(part.vertexCount) * 2);
        b.rgba.resize(from * 2 + static_cast<size_t>(part.vertexCount) * 4);
        float* xy = &b.xy[from];
        float* rgba = &b.rgba[from * 2];
        const float* in = &v.xy[static_cast<size_t>(part.firstVertex) * 2];
        const unsigned char* colour = &v.rgba[static_cast<size_t>(part.firstVertex) * 4];
        // (colours: worked out once for each run of one colour)
        unsigned char last[4] = {0, 0, 0, 0};
        float out[4] = {0, 0, 0, 0};
        bool known = false;
        const float* plus = textured ? kNoAdd : add;
        for (int i = 0; i < part.vertexCount; ++i, in += 2, xy += 2, colour += 4, rgba += 4) {
            xy[0] = to.a * in[0] + to.c * in[1] + to.tx;
            xy[1] = to.b * in[0] + to.d * in[1] + to.ty;
            if (warp) warp->displace(xy[0], xy[1]);
            if (!known || std::memcmp(last, colour, 4) != 0) {
                known = true;
                std::memcpy(last, colour, 4);
                // Premultiplied (blended GL_ONE); a mask's shape is whole.
                const float a = c.stencil ? 1.0f : std::clamp(colour[3] / 255.0f * mult[3] + plus[3], 0.0f, 1.0f);
                for (int k = 0; k < 3; ++k) out[k] = std::clamp(colour[k] / 255.0f * mult[k] + plus[k], 0.0f, 1.0f) * a;
                out[3] = a;
            }
            std::memcpy(rgba, out, sizeof out);
        }
        if (textured)
            b.uv.insert(b.uv.end(), v.uv.begin() + static_cast<long>(part.firstVertex) * 2,
                        v.uv.begin() + static_cast<long>(part.firstVertex + part.vertexCount) * 2);
        const unsigned* idx = &v.indices[static_cast<size_t>(part.firstIndex)];
        const unsigned base = static_cast<unsigned>(part.firstVertex);
        auto indices = [&](unsigned short first) {
            const size_t n = b.indices.size();
            b.indices.resize(n + static_cast<size_t>(part.indexCount));
            unsigned short* o = &b.indices[n];
            for (int i = 0; i < part.indexCount; ++i) o[i] = static_cast<unsigned short>(first + (idx[i] - base));
        };
        indices(at);
        // A gradient's or a bitmap's added colour: drawn again over it,
        // added where its texels are (c * mult + add: the texture times
        // the multipliers above, then the added colour times its alpha).
        if (textured && !c.stencil && (add[0] > 0 || add[1] > 0 || add[2] > 0)) {
            thread_local std::vector<float> placed;
            placed.assign(b.xy.begin() + static_cast<long>(from), b.xy.end());  // (begin() may send the batch)
            const unsigned short again = b.begin(true, part.texture, GL_ONE, GL_ONE, part.vertexCount, true);
            b.xy.insert(b.xy.end(), placed.begin(), placed.end());
            b.uv.insert(b.uv.end(), v.uv.begin() + static_cast<long>(part.firstVertex) * 2,
                        v.uv.begin() + static_cast<long>(part.firstVertex + part.vertexCount) * 2);
            const unsigned char* vc = &v.rgba[static_cast<size_t>(part.firstVertex) * 4];
            for (int i = 0; i < part.vertexCount; ++i, vc += 4) {
                const float a = std::clamp(vc[3] / 255.0f * mult[3], 0.0f, 1.0f);
                b.rgba.insert(b.rgba.end(), {std::min(add[0], 1.0f) * a, std::min(add[1], 1.0f) * a, std::min(add[2], 1.0f) * a, 0.0f});
            }
            indices(again);
        }
    }
    if (c.stencil) flushBatch();  // (under the mask's stencil state)
}

// Replay state of the masks (see Command::stencil).
thread_local int gLevel = 0;

// Sets the stencil state for `c`; true for a marker (nothing to draw).
bool stencilFor(const Command& c) {
    if (c.marker || c.stencil) flushBatch();
    if (c.marker) {
        gLevel += c.marker;
        glStencilFunc(GL_EQUAL, gLevel, 0xFF);
        return true;
    }
    if (c.stencil) {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.5f);
        glStencilFunc(GL_EQUAL, gLevel, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, c.stencil > 0 ? GL_INCR : GL_DECR);
    }
    return false;
}

void stencilDone(const Command& c) {
    if (!c.stencil) return;
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_ALPHA_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

bool usesStencil(const Frame& f) {
    for (const Command& c : f.commands)
        if (c.marker || c.stencil) return true;
    return false;
}

void drawAsRecorded(const Command& c) {
    if (c.vector) drawVectorAt(c, compose(c.m, c.local), c.rgba, c.add, c.warp.get());
    else if (c.mesh) drawMeshAt(c, c.world.data(), c.colors.data());
    else drawQuadAt(c, c.world.data(), c.uv, c.rgba);
}


// Whether a placement going from `p` to `q` is motion: not mirrored on the
// way (its determinant changing sign: a clip flipped) nor turned by more
// than 90 degrees, which are cuts, as a sprite's flip (drawBetween).
// Eased, a flip turns the piece half round in the middle of its pass (the
// kidney swapped to the other side as mission 3's new one goes in swung
// across the middle).
bool smoothMove(const Affine& p, const Affine& q) {
    float dp = p.a * p.d - p.b * p.c, dq = q.a * q.d - q.b * q.c;
    if ((dp < 0) != (dq < 0)) return false;
    float turn = std::fabs(std::atan2(q.b, q.a) - std::atan2(p.b, p.a)) * 180 / kPi;
    if (turn > 180) turn = 360 - turn;
    return turn <= 90;
}

// A piece of vector art `p` riding with its clip `s` of the way from where
// the clip was to `to` (that clip's place in the shown frame): false if the
// clip is another instance there, or was squashed flat.
bool riding(const Command& p, unsigned long long parentId, const Affine& to, float s, Affine& local) {
    if (!p.art || p.parentId != parentId) return false;
    if (std::fabs(p.parent.a * p.parent.d - p.parent.b * p.parent.c) < 1e-12f) return false;
    if (!smoothMove(p.parent, to)) {  // (its clip flipped or turned over: held till its pass)
        local = p.local;
        return true;
    }
    local = compose(lerpAffine(p.parent, to, s), compose(invert(p.parent), p.local));
    return true;
}

// Where a piece of vector art `p` is `s` of the way to its counterpart `q`:
// by its own place while it shows the same drawing (whatever texture shows
// it), riding with its clip once it changes drawing; false if neither.
bool artPlace(const Command& p, const Command& q, float s, Affine& local) {
    if (!p.art || !q.art) return false;
    if (p.drawing == q.drawing) {
        // (flipped or turned over: held where it was until its pass)
        local = smoothMove(p.local, q.local) ? lerpAffine(p.local, q.local, s) : p.local;
        return true;
    }
    return riding(p, q.parentId, q.parent, s, local);
}

// The previous frame's draw `p` with what the game animates `s` of the way to
// its counterpart `q` in the shown frame; `pp`: p's own counterpart in the
// frame before, if any. A touch target (q.predicted) is shown from where it
// is now on towards its next step.
void drawBetween(const Command& p, const Command& q, const Command* pp, float s) {
    if (p.vector) {
        // (as a quad of vector art below: by its place, its fade eased)
        bool fade = true;
        for (int i = 0; i < 4; ++i)
            fade = fade && std::fabs(q.rgba[i] - p.rgba[i]) <= 0.5f && std::fabs(q.add[i] - p.add[i]) <= 0.5f;
        float mult[4], add[4];
        for (int i = 0; i < 4; ++i) {
            mult[i] = fade ? lerp(p.rgba[i], q.rgba[i], s) : p.rgba[i];
            add[i] = fade ? lerp(p.add[i], q.add[i], s) : p.add[i];
        }
        Affine local;
        if (!artPlace(p, q, s, local)) local = p.local;
        Affine m = lerpAffine(p.m, q.m, s);
        if (std::fabs(local.tx - p.local.tx) <= 1e-3f && std::fabs(local.ty - p.local.ty) <= 1e-3f) {
            m.tx = curve(pp ? &pp->m.tx : nullptr, p.m.tx, q.m.tx, s);
            m.ty = curve(pp ? &pp->m.ty : nullptr, p.m.ty, q.m.ty, s);
        }
        drawVectorAt(p, compose(m, local), mult, add, warpBetween(p, q, s));
        return;
    }
    if (p.mesh) {
        if (q.world.size() != p.world.size()) {
            drawAsRecorded(p);
            return;
        }
        thread_local std::vector<float> xy, colors;
        xy.resize(p.world.size());
        for (size_t i = 0; i < xy.size(); ++i) xy[i] = lerp(p.world[i], q.world[i], s);
        colors = p.colors;
        if (q.colors.size() == colors.size())
            for (size_t i = 0; i < colors.size(); ++i) colors[i] = lerp(p.colors[i], q.colors[i], s);
        drawMeshAt(p, xy.data(), colors.data(), warpBetween(p, q, s));
        return;
    }
    Sprite sp = p.s;
    float uv[8], rgba[4], out[8];
    std::copy_n(p.uv, 8, uv);
    // A fade moves in small steps; a change of more than half in one pass is
    // a blink or a switch, and stays a cut.
    bool fade = true;
    for (int i = 0; i < 4; ++i) fade = fade && std::fabs(q.rgba[i] - p.rgba[i]) <= 0.5f;
    for (int i = 0; i < 4; ++i) rgba[i] = fade ? lerp(p.rgba[i], q.rgba[i], s) : p.rgba[i];
    if (q.predicted) {
        sp.pos[0] = q.s.pos[0] + q.step[0] * s;
        sp.pos[1] = q.s.pos[1] + q.step[1] * s;
        sp.angle = q.s.angle + q.step[2] * s;
    } else {
        sp.pos[0] = curve(pp ? &pp->s.pos[0] : nullptr, p.s.pos[0], q.s.pos[0], s);
        sp.pos[1] = curve(pp ? &pp->s.pos[1] : nullptr, p.s.pos[1], q.s.pos[1], s);
        // Likewise a turn of more than 90 degrees in one pass is a flip.
        if (std::fabs(lerpAngle(p.s.angle, q.s.angle, 1) - p.s.angle) <= 90)
            sp.angle = lerpAngle(p.s.angle, q.s.angle, s);
    }
    // Same image region (a sprite scaled, a rectangle resized) or a crop of
    // it: its corners move too; another animation cell or picture: as it was.
    bool sameRegion = p.textured == q.textured && (!p.textured || (p.uvHash == q.uvHash && p.texture == q.texture));
    // Whether its corners themselves slid (their centre moved: a quad drawn
    // at a place it works out, as a particle: free particles are written
    // each pass where they are less how far their emitter moved, and the
    // transform takes them back by the same).
    bool slid = false;
    Affine artLocal;
    if (artPlace(p, q, s, artLocal)) {
        // Vector art: by its place, not its corners (its texture may be
        // another one now, of another size).
        artCorners(artLocal, p.bounds, sp.xy);
        float dx = 0, dy = 0;
        for (int i = 0; i < 8; ++i) (i % 2 ? dy : dx) += sp.xy[i] - p.s.xy[i];
        slid = std::fabs(dx) > 1e-3f || std::fabs(dy) > 1e-3f;
    } else if (sameRegion || q.lerpUv) {
        float dx = 0, dy = 0;
        for (int i = 0; i < 8; ++i) {
            sp.xy[i] = lerp(p.s.xy[i], q.s.xy[i], s);
            (i % 2 ? dy : dx) += q.s.xy[i] - p.s.xy[i];
        }
        slid = std::fabs(dx) > 1e-3f || std::fabs(dy) > 1e-3f;
    }
    if (q.lerpUv)
        for (int i = 0; i < 8; ++i) uv[i] = lerp(p.uv[i], q.uv[i], s);
    Affine m = lerpAffine(p.m, q.m, s);
    // The transform's move follows its curve, unless the corners slid too:
    // the two moves may cancel (a free particle under its moving emitter,
    // where the curve on one half and a straight line on the other made
    // the particle sway with each uneven step of the emitter); both go
    // straight then, so their sum does.
    if (!slid) {
        m.tx = curve(pp ? &pp->m.tx : nullptr, p.m.tx, q.m.tx, s);
        m.ty = curve(pp ? &pp->m.ty : nullptr, p.m.ty, q.m.ty, s);
    }
    corners(m, sp, out);
    drawQuadAt(p, out, uv, rgba, warpBetween(p, q, s));
}

// A keyframed piece as the shown frame recorded it (its own look: never the
// previous frame's, so an authored frame change is never held stale), under
// `delta` eased in from its group's anchor as it was.
void drawGroupPiece(const Command& q, const Affine& delta) {
    if (q.vector) {
        drawVectorAt(q, compose(compose(delta, q.m), q.local), q.rgba, q.add, q.warp.get());
        return;
    }
    if (q.mesh) {  // no local vertices kept to retransform: show it exactly
        drawAsRecorded(q);
        return;
    }
    float out[8];
    corners(compose(delta, q.m), q.s, out);
    drawQuadAt(q, out, q.uv, q.rgba);
}

// The shown frame's draw `q` as recorded, moved by (dx, dy) on screen.
void drawShifted(const Command& q, float dx, float dy) {
    if (q.vector) {
        Affine to = compose(q.m, q.local);
        to.tx += dx;
        to.ty += dy;
        drawVectorAt(q, to, q.rgba, q.add, q.warp.get());
        return;
    }
    thread_local std::vector<float> xy;
    xy = q.world;
    for (size_t i = 0; i < xy.size(); i += 2) {
        xy[i] += dx;
        xy[i + 1] += dy;
    }
    if (q.mesh) drawMeshAt(q, xy.data(), q.colors.data());
    else drawQuadAt(q, xy.data(), q.uv, q.rgba);
}

}  // namespace

void setFramePointer(float x, float y) {
    gRecording.pointer[0] = x;
    gRecording.pointer[1] = y;
}

void stencilMode(int mode) { gStencilMode = mode; }

void beginWarp(int cols, int rows, float w, float h, const float* vertices) {
    if (cols < 1 || rows < 1) return;
    // the grid's cells (u, v) to the screen, and back
    const Affine m = currentMatrix();
    const float sx = w / cols, sy = h / rows;
    Affine g = m;
    g.a = m.a * sx, g.b = m.b * sx, g.c = m.c * sy, g.d = m.d * sy;
    const float det = g.a * g.d - g.b * g.c;
    if (std::fabs(det) < 1e-12f) return;
    auto grid = std::make_shared<WarpGrid>();
    grid->cols = cols;
    grid->rows = rows;
    Affine& inv = grid->toCells;
    inv.a = g.d / det, inv.b = -g.b / det, inv.c = -g.c / det, inv.d = g.a / det;
    inv.tx = -(inv.a * g.tx + inv.c * g.ty);
    inv.ty = -(inv.b * g.tx + inv.d * g.ty);
    grid->d.resize((cols + 1) * (rows + 1) * 2);
    for (int i = 0; i <= cols; ++i)
        for (int j = 0; j <= rows; ++j) {
            const int k = (i * (rows + 1) + j) * 2;
            float x0, y0, x1, y1;
            m.apply(i * sx, j * sy, x0, y0);
            m.apply(vertices[k], vertices[k + 1], x1, y1);
            grid->d[k] = x1 - x0;
            grid->d[k + 1] = y1 - y0;
        }
    gWarp = std::move(grid);
}

void endWarp() { gWarp.reset(); }

BlendDst::BlendDst(GLenum dst) : saved(gDstBlend) { gDstBlend = dst; }
BlendDst::~BlendDst() { gDstBlend = saved; }

void maskLevel(int delta) {
    gRecording.commands.emplace_back();
    Command& c = gRecording.commands.back();
    c.marker = delta;
    c.owner = 0;
    c.context = gContext;
}

DrawOwner::DrawOwner(DrawId object) : saved(gContext) { gContext = object; }
DrawOwner::~DrawOwner() { gContext = saved; }

void appearAtOnce(float at) {
    gAtOnce = true;
    gAppearAt = at;
}

PointerArt::PointerArt(float x, float y) {
    gPointerArt = true;
    gPointerAt[0] = x;
    gPointerAt[1] = y;
}
PointerArt::~PointerArt() { gPointerArt = false; }

void artPiece(const ArtPiece& piece) {
    gArtPiece = piece;
    gArtPending = true;
}

void nextStep(float dx, float dy, float dAngle) {
    gPredicted = true;
    gStep[0] = dx;
    gStep[1] = dy;
    gStep[2] = dAngle;
}

KeyframeGroup::KeyframeGroup(DrawId art) : saved(gGroup) {
    std::memcpy(savedAnchor, &gGroupAnchor, sizeof savedAnchor);
    gGroup = art;
    gGroupAnchor = currentMatrix();
}
KeyframeGroup::~KeyframeGroup() {
    gGroup = saved;
    std::memcpy(&gGroupAnchor, savedAnchor, sizeof savedAnchor);
}

void traceDraws(std::vector<TracedDraw>* out) { gTrace = out; }

int takeGlDraws(int* vertices) {
    int n = gGlDraws;
    gGlDraws = 0;
    if (vertices) *vertices = gGlVertices;
    gGlVertices = 0;
    return n;
}

void clearScreen(float r, float g, float b, float a) {
    gRecording.commands.clear();
    // (what a draw that did not happen left marked: a piece of art whose
    // texture was not ready, a recording begun over)
    gArtPending = false;
    gPredicted = false;
    gAtOnce = false;
    gRecording.clear[0] = r;
    gRecording.clear[1] = g;
    gRecording.clear[2] = b;
    gRecording.clear[3] = a;
}

void drawSprite(DrawId owner, GLuint texture, const Sprite& s, const float uv[8], const float rgba[4],
                GLenum srcBlend) {
    Command& c = record(owner, texture, uv, rgba, srcBlend);
    c.s = s;
    c.world.resize(8);
    corners(c.m, s, c.world.data());
}

void drawQuad(DrawId owner, GLuint texture, const float xy[8], const float uv[8], const float rgba[4],
              GLenum srcBlend) {
    Sprite s;
    std::copy_n(xy, 8, s.xy);
    drawSprite(owner, texture, s, uv, rgba, srcBlend);
}

void drawMesh(DrawId owner, const float* vertices, int components, int vertexCount, const float* rgba,
              const unsigned short* indices, int indexCount) {
    const float white[4] = {1, 1, 1, 1};
    Command& c = record(owner, 0, nullptr, white, GL_SRC_ALPHA);
    c.mesh = true;
    c.world.resize(static_cast<size_t>(vertexCount) * 2);
    for (int i = 0; i < vertexCount; ++i) {
        const float* v = vertices + static_cast<size_t>(i) * components;
        c.m.apply(v[0], v[1], c.world[i * 2], c.world[i * 2 + 1]);
    }
    c.colors.assign(rgba, rgba + static_cast<size_t>(vertexCount) * 4);
    c.indices.assign(indices, indices + indexCount);
}

void drawVector(DrawId owner, std::shared_ptr<const VectorMesh> mesh, const float mult[4], const float add[4]) {
    Command& c = record(owner, 0, nullptr, mult, GL_ONE);
    if (!c.art) {  // (placed by the GL matrices alone)
        c.art = true;
        c.local = Affine{};
        c.parent = Affine{};
        std::copy_n(mesh->bounds, 4, c.bounds);
    }
    std::copy_n(add, 4, c.add);
    // (matched as the same drawing from pass to pass, as a quad by its image)
    c.uvHash = static_cast<size_t>(c.drawing * 0x9E3779B97F4A7C15ull) ^ 0x7e57u;
    c.world.resize(8);
    artCorners(compose(c.m, c.local), c.bounds, c.world.data());
    c.vector = std::move(mesh);
}

void releaseTexture(GLuint texture) {
    if (texture) gReleased.push_back({texture, 0, 0, {}});
}

void releaseAtlasSlot(const AtlasSlot& slot) {
    if (slot.page) gReleased.push_back({0, 0, 0, slot});
}

bool screenInDrawing(float margin, float& x0, float& y0, float& x1, float& y1) {
    Affine m = currentMatrix();
    float det = m.a * m.d - m.b * m.c;
    if (std::fabs(det) < 1e-12f) return false;
    Affine inv{m.d / det, -m.b / det, -m.c / det, m.a / det, (m.c * m.ty - m.d * m.tx) / det,
               (m.b * m.tx - m.a * m.ty) / det};
    float w = platform::screenWidth(), h = platform::screenHeight();
    x0 = y0 = 1e30f;
    x1 = y1 = -1e30f;
    for (float sx : {-margin * w, w + margin * w})
        for (float sy : {-margin * h, h + margin * h}) {
            float x, y;
            inv.apply(sx, sy, x, y);
            x0 = std::min(x0, x);
            y0 = std::min(y0, y);
            x1 = std::max(x1, x);
            y1 = std::max(y1, y);
        }
    return true;
}

void recycleTexture(GLuint texture, int width, int height) {
    if (texture) gReleased.push_back({texture, width, height, {}});
}

void setTexels(GLuint texture, int width, int height, bool allocate, std::vector<unsigned char> texels,
               bool drawnNow) {
    if (!texture || width <= 0 || height <= 0) return;
    if (!gDisplayThread) {
        writeTexels(texture, width, height, allocate, texels.empty() ? nullptr : texels.data());
        return;
    }
    {
        std::lock_guard<std::mutex> lock(gWorkMutex);
        const unsigned long long frame = gCommits + (drawnNow ? 1 : 2);
        gWork.push_back({texture, width, height, allocate, false, std::move(texels), frame});
        gWorkFrom = std::min(gWorkFrom.load(), frame);
    }
    gWorkCv.notify_all();
}

void setTexelsAt(GLuint texture, int x, int y, int width, int height, std::vector<unsigned char> texels, bool drawnNow) {
    if (!texture || width <= 0 || height <= 0 || texels.empty()) return;
    if (!gDisplayThread) {
        writeTexels(texture, width, height, false, texels.data(), x, y);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(gWorkMutex);
        const unsigned long long frame = gCommits + (drawnNow ? 1 : 2);
        TextureWork w{texture, width, height, false, false, std::move(texels), frame};
        w.x = x;
        w.y = y;
        gWork.push_back(std::move(w));
        gWorkFrom = std::min(gWorkFrom.load(), frame);
    }
    gWorkCv.notify_all();
}

bool texelsQueued() { return gDisplayThread; }

// Textures the texture thread made ahead (named and made in its context),
// for newTexture: the game's thread then names a texture with no GL call
// (with Android's GL every call there waited for the texture thread's
// uploads, a hand-over of art 14 ms a pass).
std::vector<GLuint> gNames;  // (guarded by gWorkMutex)
constexpr size_t kNamesLow = 128, kNamesMade = 256;

void makeNames() {
    {
        std::lock_guard<std::mutex> lock(gWorkMutex);
        if (gNames.size() >= kNamesLow) return;
    }
    std::vector<GLuint> ids(kNamesMade);
    glGenTextures(static_cast<GLsizei>(ids.size()), ids.data());
    for (GLuint id : ids) glBindTexture(GL_TEXTURE_2D, id);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFinish();
    std::lock_guard<std::mutex> lock(gWorkMutex);
    gNames.insert(gNames.end(), ids.begin(), ids.end());
}

GLuint newTexture() {
    if (gDisplayThread) {
        std::lock_guard<std::mutex> lock(gWorkMutex);
        if (!gNames.empty()) {
            const GLuint id = gNames.back();
            gNames.pop_back();
            if (gNames.size() < kNamesLow) gWorkCv.notify_all();
            return id;
        }
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

void finishTextureWork() {
    if (!gDisplayThread) return;
    std::unique_lock<std::mutex> lock(gWorkMutex);
    gWorkCv.wait_for(lock, std::chrono::seconds(2), [] { return gWork.empty() && !gWorking; });
}

void textureWorkLoop(const std::atomic<bool>& stop) {
    // Uploads go in strips of rows, each at most this many bytes, so the
    // display thread waits for one strip at most.
    constexpr size_t kStrip = size_t{512} << 10;
    for (;;) {
        makeNames();
        std::deque<TextureWork> batch;
        {
            std::unique_lock<std::mutex> lock(gWorkMutex);
            gWorkCv.wait_for(lock, std::chrono::milliseconds(50),
                             [&] { return stop || !gWork.empty() || gNames.size() < kNamesLow; });
            if (stop) return;
            if (gWork.empty()) continue;
            batch.swap(gWork);
            gWorking = true;
        }
        std::vector<GLuint> gone;
        for (const TextureWork& w : batch) {
            if (w.remove) {
                gone.push_back(w.id);
                continue;
            }
            if (w.allocate) {
                std::lock_guard<std::mutex> turn(gDrawTurn);
                writeTexels(w.id, w.width, w.height, true, nullptr);
            }
            if (w.texels.empty()) continue;
            const int rows = std::max(1, static_cast<int>(kStrip / (static_cast<size_t>(w.width) * 4)));
            for (int y = 0; y < w.height; y += rows) {
                std::lock_guard<std::mutex> turn(gDrawTurn);
                glBindTexture(GL_TEXTURE_2D, w.id);
                glTexSubImage2D(GL_TEXTURE_2D, 0, w.x, w.y + y, w.width, std::min(rows, w.height - y), kTexelFormat,
                                GL_UNSIGNED_BYTE, w.texels.data() + static_cast<size_t>(y) * w.width * 4);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        if (!gone.empty()) {
            std::lock_guard<std::mutex> turn(gDrawTurn);
            glDeleteTextures(static_cast<GLsizei>(gone.size()), gone.data());
        }
        // (complete before another context draws them)
        glFinish();
        {
            std::lock_guard<std::mutex> lock(gWorkMutex);
            gWorking = false;
            unsigned long long from = ~0ull;
            for (const TextureWork& w : gWork) from = std::min(from, w.frame);
            gWorkFrom = from;
        }
        gWorkCv.notify_all();
    }
}

DrawTurn::DrawTurn() { gDrawTurn.lock(); }
DrawTurn::~DrawTurn() { gDrawTurn.unlock(); }

GLuint reuseTexture(int width, int height) {
    // (Not one freed in the last few frames: the display thread's last
    // draws of it may still be on the GPU when its new texels land.)
    for (auto it = gPool.rbegin(); it != gPool.rend(); ++it)
        if (it->width == width && it->height == height && it->since + 4 <= gCommits) {
            GLuint id = it->id;
            gPoolBytes -= texelBytes(width, height);
            gPool.erase(std::next(it).base());
            return id;
        }
    return 0;
}

void setFrameDue(double due) {
    gHasDue = true;
    gNextDue = due;
}

void setDisplayThread(bool on) { gDisplayThread = on; }

void commitFrame() {
    ++gCommits;
    gRecording.seq = gCommits;
    // The game's side draws from the newest two frames (debug shots); the
    // display thread from its shown frame and the one before.
    unsigned long long oldest = gCommits - 1;
    if (gDisplayThread) {
        std::lock_guard<std::mutex> lock(gQueueMutex);
        oldest = std::min(oldest, gDisplayOldest);
    }
    gReleasedBy.emplace_back(gCommits, std::move(gReleased));
    gReleased.clear();
    std::vector<GLuint> gone;
    while (!gReleasedBy.empty() && gReleasedBy.front().first < oldest) {
        for (const Released& r : gReleasedBy.front().second) {
            if (r.slot.page) {
                atlasFree(r.slot);
                continue;
            }
            if (!r.width) {
                gone.push_back(r.id);
                continue;
            }
            gPool.push_back({r.id, r.width, r.height, gCommits});
            gPoolBytes += texelBytes(r.width, r.height);
        }
        gReleasedBy.pop_front();
    }
    while (!gPool.empty() && (gPoolBytes > kPoolBytes || gPool.front().since + kPoolFrames < gCommits)) {
        gone.push_back(gPool.front().id);
        gPoolBytes -= texelBytes(gPool.front().width, gPool.front().height);
        gPool.pop_front();
    }
    if (!gone.empty()) {
        if (gDisplayThread) {  // (in order with the uploads: a name is not reused before)
            {
                std::lock_guard<std::mutex> lock(gWorkMutex);
                for (GLuint id : gone) gWork.push_back({id, 0, 0, false, true, {}, ~0ull});
            }
            gWorkCv.notify_all();
        } else {
            glDeleteTextures(static_cast<GLsizei>(gone.size()), gone.data());
        }
    }

    gRecording.width = platform::screenWidth();
    gRecording.height = platform::screenHeight();
    match(gRecording, *gShown, *gPrevious);
    gPrevious2 = std::move(gPrevious);
    gPrevious = std::move(gShown);
    gShown = std::make_shared<Frame>(std::move(gRecording));
    gRecording = Frame();
    std::copy_n(gShown->clear, 4, gRecording.clear);
    const double due = gHasDue ? gNextDue : platform::now();
    gHasDue = false;
    if (gDisplayThread) {
        // What this thread's context uploaded itself (the games that make
        // their own textures) is complete before another context draws it.
        glFinish();
        std::lock_guard<std::mutex> lock(gQueueMutex);
        gQueue.push_back({gShown, due, platform::passSeconds()});
        // (a display thread not keeping up: the oldest are never shown)
        while (gQueue.size() > 64) gQueue.pop_front();
    }
}

bool dueFrames(double now, DueFrames& out) {
    std::lock_guard<std::mutex> lock(gQueueMutex);
    if (gQueue.empty()) return false;
    // The latest frame due by now (none yet: the oldest, as it is); the
    // frames before the one before it are past.
    // (Only frames whose textures are all in: one whose uploads are not done
    // yet waits, the one before held meanwhile.)
    const unsigned long long ready = gWorkFrom;
    size_t k = 0;
    bool due = false;
    for (size_t i = 0; i < gQueue.size(); ++i)
        if (gQueue[i].due <= now && gQueue[i].frame->seq < ready) {
            k = i;
            due = true;
        }
    // (none ready yet: nothing new shown. Drawn before its textures are in,
    // a texture's name is used in two contexts before either made it, which
    // can leave each its own texture: the first page of art stayed empty.)
    if (gQueue[k].frame->seq >= ready) return false;
    while (k > 2) {
        gQueue.pop_front();
        --k;
    }
    const Queued& q = gQueue[k];
    out.shown = q.frame;
    out.previous = k >= 1 ? gQueue[k - 1].frame : nullptr;
    out.beforeThat = k >= 2 ? gQueue[k - 2].frame : nullptr;
    out.t = due && q.span > 0 ? static_cast<float>(std::min(1.0, (now - q.due) / q.span)) : 1.0f;
    out.width = q.frame->width > 0 ? q.frame->width : platform::screenWidth();
    out.height = q.frame->height > 0 ? q.frame->height : platform::screenHeight();
    out.seq = q.frame->seq;
    out.draws = q.frame->commands.size() + (out.previous ? out.previous->commands.size() : 0);
    gDisplayOldest = (out.previous ? out.previous : out.shown)->seq;
    return true;
}

void showLatestNow() {
    std::lock_guard<std::mutex> lock(gQueueMutex);
    if (!gQueue.empty()) gQueue.back().due = platform::now() - gQueue.back().span;
}

void shownFrameSize(int& w, int& h) {
    w = gShown->width > 0 ? gShown->width : platform::screenWidth();
    h = gShown->height > 0 ? gShown->height : platform::screenHeight();
}

void renderFrame(float t) {
    DueFrames f;
    f.shown = gShown;
    f.previous = gPrevious;
    f.beforeThat = gPrevious2;
    renderFrames(f, t);
}

void renderFrames(const DueFrames& f, float t) {
    const Frame& shown = *f.shown;
    const Frame& previous = f.previous ? *f.previous : kNoFrame;
    const Frame& beforeThat = f.beforeThat ? *f.beforeThat : kNoFrame;
    t = std::clamp(t, 0.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const int sw = shown.width > 0 ? shown.width : platform::screenWidth();
    const int sh = shown.height > 0 ? shown.height : platform::screenHeight();
    gReplayWidth = static_cast<float>(sw);
    gReplayHeight = static_cast<float>(sh);
    glOrtho(0, sw, sh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glClearColor(shown.clear[0], shown.clear[1], shown.clear[2], shown.clear[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glEnableClientState(GL_VERTEX_ARRAY);
    // (a frame of another size, the game's screen changed between them: not
    // eased from it)
    const bool exact = t >= 1 || !f.previous || shown.next.size() != previous.commands.size() || shown.width != previous.width ||
                       shown.height != previous.height;
    const bool masks = usesStencil(exact ? shown : previous);
    if (masks) {
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glEnable(GL_STENCIL_TEST);
        gLevel = 0;
        glStencilFunc(GL_EQUAL, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }

    // The pointer's art: the shown frame's, where the pointer is now.
    float liveX, liveY;
    livePointer(liveX, liveY);
    auto drawPointer = [&]() {
        flushBatch();
        for (const Command& c : shown.commands) {
            if (!c.pointer || c.stencil || c.marker) continue;
            glPushMatrix();
            glTranslatef(liveX - c.pointerAt[0], liveY - c.pointerAt[1], 0);
            drawAsRecorded(c);
            flushBatch();
            glPopMatrix();
        }
    };
    if (exact) {
        for (const Command& c : shown.commands) {
            if (c.pointer || stencilFor(c)) continue;
            drawAsRecorded(c);
            stencilDone(c);
        }
    } else {
        // The previous frame's content on its way to the shown frame (what
        // the shown frame adds appears when the display reaches it). A
        // keyframe group's own pieces are always its own current ones (an
        // authored frame is never shown stale): only its anchor eases from
        // where the group was; once the group itself is gone, it holds what
        // it last looked like until then.
        std::vector<DrawId> doneGroups;
        // Each clip of vector art in the shown frame (its place and the GL
        // matrices), for the previous frame's pieces that go at it to ride with.
        struct ShownClip {
            Affine parent, m;
        };
        std::unordered_map<unsigned long long, ShownClip> shownClips;
        for (const Command& q : shown.commands)
            if (q.art) shownClips.emplace(q.parentId, ShownClip{q.parent, q.m});
        auto drawGoing = [&](const Command& p) {
            auto clip = p.art && !p.mesh ? shownClips.find(p.parentId) : shownClips.end();
            Affine local;
            if (clip == shownClips.end() || !riding(p, p.parentId, clip->second.parent, t, local)) {
                drawAsRecorded(p);
                return;
            }
            if (p.vector) {
                drawVectorAt(p, compose(lerpAffine(p.m, clip->second.m, t), local), p.rgba, p.add, p.warp.get());
                return;
            }
            Sprite sp = p.s;
            artCorners(local, p.bounds, sp.xy);
            float out[8];
            corners(lerpAffine(p.m, clip->second.m, t), sp, out);
            drawQuadAt(p, out, p.uv, p.rgba);
        };
        // What the shown frame adds at the finger (appearAtOnce) is there at
        // once, in its place in the drawing order: before the shown draw it
        // preceded.
        int nextNew = 0;
        // So is new vector art made at the finger (a cut's trail, blood
        // where a tool is): a clip new in this pass (no piece of it in the
        // frame before) near where the pointer was for it.
        std::unordered_set<unsigned long long> previousClips;
        for (const Command& p : previous.commands)
            if (p.art) previousClips.insert(p.parentId);
        auto atFinger = [&](const Command& q) {
            if (!q.art || previousClips.count(q.parentId)) return false;
            float x, y;
            centre(q, x, y);
            return std::hypot(x - shown.pointer[0], y - shown.pointer[1]) <= kFingerReach;
        };
        auto drawNewBefore = [&](int limit) {
            for (; nextNew < limit; ++nextNew) {
                const Command& q = shown.commands[nextNew];
                if (!q.pointer && q.prev < 0 && !q.group && !q.marker && !q.stencil &&
                    ((q.atOnce && t >= q.appearAt) || atFinger(q)))
                    drawAsRecorded(q);
            }
        };
        // What moved with the pointer from the frame before to the shown
        // one (a rib held in the tongs, whatever the scripts keep at the
        // mouse) is drawn where the pointer is now, as the pointer's own art
        // is: eased from the frame before, it trailed the finger by a pass.
        const float pdx = shown.pointer[0] - previous.pointer[0], pdy = shown.pointer[1] - previous.pointer[1];
        const float pointerMove = std::hypot(pdx, pdy);
        auto followsPointer = [&](const Command& p, const Command& q) {
            if (pointerMove < 1.5f || q.world.size() != p.world.size() || q.world.empty()) return false;
            float px, py, qx, qy;
            centre(p, px, py);
            centre(q, qx, qy);
            return std::hypot(qx - px - pdx, qy - py - pdy) <= std::max(1.0f, 0.1f * pointerMove);
        };
        for (int i = 0; i < static_cast<int>(previous.commands.size()); ++i) {
            const Command& p = previous.commands[i];
            if (p.group) {
                if (std::find(doneGroups.begin(), doneGroups.end(), p.group) != doneGroups.end()) continue;
                doneGroups.push_back(p.group);
                auto g = shown.groups.find(p.group);
                if (g == shown.groups.end()) {
                    for (int j = i; j < static_cast<int>(previous.commands.size()); ++j) {
                        const Command& pj = previous.commands[j];
                        if (pj.group != p.group || stencilFor(pj)) continue;
                        drawAsRecorded(pj);
                        stencilDone(pj);
                    }
                } else {
                    const Affine& curAnchor = g->second;
                    auto pg = previous.groups.find(p.group);
                    Affine delta = compose(lerpAffine(pg != previous.groups.end() ? pg->second : curAnchor, curAnchor, t),
                                           invert(curAnchor));
                    for (const Command& q : shown.commands) {
                        if (q.group != p.group || stencilFor(q)) continue;
                        drawGroupPiece(q, delta);
                        stencilDone(q);
                    }
                }
                continue;
            }
            if (p.pointer || stencilFor(p)) continue;
            if (int j = shown.next[i]; j >= 0) {
                drawNewBefore(j);
                const Command& q = shown.commands[j];
                const Command* pp = p.prev >= 0 && p.prev < static_cast<int>(beforeThat.commands.size())
                                        ? &beforeThat.commands[p.prev]
                                        : nullptr;
                if (pp && (pp->mesh || pp->group)) pp = nullptr;
                if (followsPointer(p, q)) drawShifted(q, liveX - shown.pointer[0], liveY - shown.pointer[1]);
                else drawBetween(p, q, pp, t);
            } else {
                drawGoing(p);
            }
            stencilDone(p);
        }
        drawNewBefore(static_cast<int>(shown.commands.size()));
    }
    drawPointer();
    flushBatch();
    if (masks) glDisable(GL_STENCIL_TEST);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_BLEND);
    glColor4f(1, 1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

}  // namespace platform
