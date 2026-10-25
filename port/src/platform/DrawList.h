// Recorded drawing. Game code never rasterises directly: each logic pass
// records one frame of draws, and presentation replays frames at the
// window's native resolution, as often as the display refreshes.
//
// Smooth motion without invented frames: the display runs one pass behind
// the logic. Between two passes it shows the previous frame's content (which
// animation frame, which story keyframe, which letters: everything authored
// steps exactly at its pass, as in the original) while the parameters the
// game changes from pass to pass (positions, rotation angles around their
// pivots, the parent transform such as zoom or a slide, sprite scale,
// colour and fade) move evenly towards the current frame's values. What a
// pass adds appears, and what it removes vanishes, when the display reaches
// that pass. So programmatic motion is smooth at any refresh rate while the
// logic keeps the original's 20 passes per second, and nothing is rendered
// at the original screen size and then stretched.
#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "platform/GL.h"

namespace platform {

// Identifies the object behind a draw: an Object::serial(), or for the few
// drawers that are not Objects, their address.
using DrawId = unsigned long long;

// ---- recording (game side)

// Starts the frame being recorded over, cleared to (r, g, b, a) (the game's
// glClearColor + glClear).
void clearScreen(float r, float g, float b, float a);

// An image draw in AdvancedImage's form: under the current GL matrices,
//   translate(pos + offset + pivotIn) * rotate(angle) * translate(-pivotOut)
// applied to the corners xy (order (x0,y0) (x1,y0) (x0,y1) (x1,y1)).
// pos and angle are what the game animates; offset (a frame's offset),
// pivots, corners and texture coordinates are the drawn content. `uv`
// nullptr: untextured. Blending is (srcBlend, GL_ONE_MINUS_SRC_ALPHA).
// `owner` is the object drawing it, used to find the same draw in the
// previous frame.
struct Sprite {
    float pos[2] = {0, 0};
    float offset[2] = {0, 0};
    float pivotIn[2] = {0, 0};
    float pivotOut[2] = {0, 0};
    float angle = 0;  // degrees
    float xy[8] = {};
};
void drawSprite(DrawId owner, GLuint texture, const Sprite& s, const float uv[8], const float rgba[4],
                GLenum srcBlend);
// The same with no transform of its own: just corners.
void drawQuad(DrawId owner, GLuint texture, const float xy[8], const float uv[8], const float rgba[4],
              GLenum srcBlend);

// An untextured indexed triangle mesh with per-vertex colours, `components`
// (2 or 3) floats per vertex, blended (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA).
// Its vertices move evenly from frame to frame (a morphing blob).
void drawMesh(DrawId owner, const float* vertices, int components, int vertexCount, const float* rgba,
              const unsigned short* indices, int indexCount);

// While it lives, draws are attributed to `object` (a wound, an effect): the
// game shares images between many objects (every blood drop draws the same
// image), and this is what tells their draws apart from frame to frame.
// Scopes nest; the innermost wins.
class DrawOwner {
public:
    explicit DrawOwner(DrawId object);
    ~DrawOwner();
    DrawOwner(const DrawOwner&) = delete;
    DrawOwner& operator=(const DrawOwner&) = delete;

private:
    DrawId saved;
};

// The next sprite drawn is a marker the player follows with the finger (the
// corkscrew's, the vacuum gauge's), which the logic tests touches against
// where it is now. Its logic says it moves by (dx, dy) and turns by dAngle
// by the next pass: it is shown from where it is on towards there, never
// behind the logic, instead of on its way from the previous frame.
void nextStep(float dx, float dy, float dAngle);

// The next quad is a piece of vector art (VectorArt): `local` (a, b, c, d,
// tx, ty) places its texture's twips, of which it shows `bounds` (x0, y0,
// x1, y1), in the coordinates of the GL matrices; `parent` places the clip
// holding it, `parentId` that clip's instance; `drawing` is what it shows
// (its character). Between frames it moves by these, not by its corners:
// showing the same drawing it glides by its own place, whatever texture
// shows it (art rasterised anew at another size during a zoom, or once a
// sharper one is made); changing drawing (a mouth stepping to its next
// drawing) or about to go, it rides with its clip, so a face's pieces never
// come apart. (Without them such a piece stood still until the pass while
// the pieces around it moved on.)
struct ArtPiece {
    float local[6];
    float bounds[4];
    float parent[6];
    unsigned long long parentId;
    unsigned long long drawing;
};
void artPiece(const ArtPiece& piece);

// A piece of vector art as triangles (flash/VectorMesh: a Flash shape cut
// into triangles, drawn by the GPU at any size instead of from a texture),
// in its own units (twips), which the pending artPiece() places: its
// `local` maps them to the GL matrices' coordinates, its `bounds` are the
// mesh's. Parts are runs of triangles over vertices of their own, each with
// its texture (0: untextured; a gradient's or a bitmap's, by `uv`), at most
// kMeshPartVertices vertices. Colours are straight (not premultiplied), one
// per vertex.
constexpr int kMeshPartVertices = 32768;
struct VectorMesh {
    struct Part {
        GLuint texture;
        int firstVertex, vertexCount;
        int firstIndex, indexCount;  // (indices: of the mesh's vertices)
    };
    std::vector<float> xy;            // per vertex
    std::vector<float> uv;            // per vertex (empty when no part is textured)
    std::vector<unsigned char> rgba;  // per vertex
    std::vector<unsigned> indices;    // three a triangle
    std::vector<Part> parts;
    float bounds[4] = {};   // x0, y0, x1, y1
    bool textured = false;  // (a part is)
    // Its textures, deleted with it (releaseTexture).
    std::vector<GLuint> textures;
    size_t bytes() const { return xy.size() * 4 + uv.size() * 4 + rgba.size() + indices.size() * 4 + sizeof *this; }
};
// Draws it after artPiece(), its colours by Flash's colour transform:
// c * mult + add (add in 0..1). (Textured parts take `mult` only.)
void drawVector(DrawId owner, std::shared_ptr<const VectorMesh> mesh, const float mult[4], const float add[4]);

// While it lives, draws are the game's pointer (its cursor art), drawn
// where the pointer was at this pass, (x, y) in game coordinates: every
// display step shows them where the pointer is then (livePointer), above
// everything else drawn, so the cursor never trails the mouse.
struct PointerArt {
    PointerArt(float x, float y);
    ~PointerArt();
};

// The next draw is made at the finger (a tool's flame, cream or stitch
// thread where the pointer is): new in a pass, it is shown as soon as it is
// made instead of when the display reaches that pass, so it keeps up with
// the pointer (on a PC the system's, which moves at once) rather than
// trailing it by a pass. What it looks like is untouched. `at` (0..1): when
// within the pass it shows, for a trail laid along the finger's stroke since
// the last pass (each piece when the finger was there).
void appearAtOnce(float at = 0);

// While it lives, draws belong to one piece of keyframed art (a Spritex):
// what its keyframes place where is content, so only the transform the art
// is drawn under (its position, scale and turn) moves smoothly; the pieces
// themselves change exactly at the pass their keyframe does.
class KeyframeGroup {
public:
    explicit KeyframeGroup(DrawId art);
    ~KeyframeGroup();
    KeyframeGroup(const KeyframeGroup&) = delete;
    KeyframeGroup& operator=(const KeyframeGroup&) = delete;

private:
    DrawId saved;
    float savedAnchor[6];
};

// While it lives, draws blend with `dst` as their destination factor
// instead of GL_ONE_MINUS_SRC_ALPHA (glBlendFunc(src, dst)): cocos2d's blend
// functions, as additive glows and particles (GL_SRC_ALPHA, GL_ONE). Scopes
// nest; the innermost wins.
class BlendDst {
public:
    explicit BlendDst(GLenum dst);
    ~BlendDst();
    BlendDst(const BlendDst&) = delete;
    BlendDst& operator=(const BlendDst&) = delete;

private:
    GLenum saved;
};

// Masks (the Flash games). Draws made while stencilMode(+1) is on add their
// shape (texels over half opaque) to the mask instead of drawing; with -1
// they take it away again; 0 draws normally. maskLevel(+1) then clips what
// follows to the shapes just added (within any outer mask), maskLevel(-1)
// after they are taken away returns to the outer mask. So a mask is:
//   stencilMode(1); draw mask; stencilMode(0); maskLevel(1);
//   draw the masked content;
//   stencilMode(-1); draw mask; stencilMode(0); maskLevel(-1);
void stencilMode(int mode);
void maskLevel(int delta);

// Grid effects (cocos2d's CCGrid3D: a liquid's wobble). Between beginWarp and
// endWarp everything drawn is shown displaced by a grid laid over a w x h
// area at the origin of the coordinates in effect at beginWarp (cocos: the
// window, in points, under the warped node's parent): `cols` x `rows` cells
// whose (cols + 1) x (rows + 1) corners, column by column (x, y each, the
// corner (i, j) at index (i * (rows + 1) + j) * 2), have moved to `vertices`;
// a point inside a cell moves with its corners (bilinearly). The original
// drew the node into a texture and the texture on the moved grid; drawing
// each piece on the moved grid looks the same. The displacement eases from
// frame to frame like other code-driven motion. Warps don't nest: an inner
// one replaces the outer until its end.
void beginWarp(int cols, int rows, float w, float h, const float* vertices);
void endWarp();

// The game screen widened by `margin` of its size on each side, in the
// current drawing coordinates (the GL matrices): its bounding box. False
// when the matrices flatten everything.
bool screenInDrawing(float margin, float& x0, float& y0, float& x1, float& y1);

// Deletes a texture once no recorded frame can still draw it.
void releaseTexture(GLuint texture);
// The same for a texture of width x height RGBA8 texels whose storage is
// kept a while for reuseTexture: art made and dropped all the time (a
// swarm of tweening morphs) reuses textures instead of the driver
// allocating new ones (its memory grew with every one made).
void recycleTexture(GLuint texture, int width, int height);
// A recycled texture of exactly that size (its texels stale), or 0.
GLuint reuseTexture(int width, int height);

// A texture's texels (width x height, premultiplied, in kTexelFormat): its
// storage made anew (`allocate`: RGBA8, clamped, linear) or written over.
// With a display thread this is queued and done by that thread, in order,
// before it shows the frame being recorded (a context uploading while
// another draws made every upload several times slower); else at once. The
// texture can be drawn from now on either way. `drawnNow` false: art made
// ahead, not drawn before the next frame (the display thread then has a
// pass longer to do it).
void setTexels(GLuint texture, int width, int height, bool allocate, std::vector<unsigned char> texels,
               bool drawnNow = true);
// The same for a part of a texture (written over, width x height texels at
// x, y).
void setTexelsAt(GLuint texture, int x, int y, int width, int height, std::vector<unsigned char> texels,
                 bool drawnNow = true);
// (TextureAtlas: a page's slot let go, as releaseTexture)
struct AtlasSlot;
void releaseAtlasSlot(const AtlasSlot& slot);
// A new texture's name, the texture made at once in this thread's context
// (so the texture thread and the display thread find it made, and never
// each make their own). For setTexels and setTexelsAt.
GLuint newTexture();
// Whether setTexels queues (a display thread): the texels' cost is then not
// this thread's.
bool texelsQueued();
// Debug: waits until the queued texture work is done (a screenshot drawn by
// this thread's context then has every texture).
void finishTextureWork();

// ---- replay (platform side)

// Debug (AS_FILM): while set, renderFrame appends where each draw landed.
struct TracedDraw {
    DrawId context, owner, group;
    unsigned texture;
    float x, y;  // centre, screen pixels
};
void traceDraws(std::vector<TracedDraw>* out);
// (debug) the GL draws this thread sent since the last call (and the
// vertices of its batched ones)
int takeGlDraws(int* vertices = nullptr);

// The frame committed next is shown from `due` (now()'s clock) on, eased in
// from the one before over the pass length then. Without it a frame is
// shown from when it is committed.
void setFrameDue(double due);

// Where the pointer is for the pass being recorded (game coordinates): what
// moves with it from frame to frame is shown where the pointer is live, and
// new art made near it appears at once (renderFrames).
void setFramePointer(float x, float y);

// Ends the frame being recorded: it becomes the shown frame and the shown one
// the previous frame.
void commitFrame();

// Draws into the current framebuffer and viewport what the display shows `t`
// (0..1) of the way from the previous frame to the shown one; t = 1 is the
// shown frame exactly as recorded.
void renderFrame(float t);

// ---- the display thread (Platform.cpp's presenter)
//
// With a display thread, frames are shown on their own clock: each is shown
// from its due time on, whatever the logic does meanwhile, so a pass that
// takes long to compute never holds the display. The textures a frame draws
// are kept until the display has moved past it.
struct Frame;
struct DueFrames {
    std::shared_ptr<const Frame> shown, previous, beforeThat;
    float t = 1;             // how far from the previous frame to the shown one
    int width = 0, height = 0;  // the game screen's size the shown frame was recorded at
    unsigned long long seq = 0;  // the shown frame's number among those committed
    size_t draws = 0;            // (debug) its draws and the previous frame's
};
// Whether frames go to a display thread (before the first commit).
void setDisplayThread(bool on);
// The display thread: what to show at `now` (the latest frame due by then,
// eased from the one before); false before the first frame.
bool dueFrames(double now, DueFrames& out);
// Draws them as renderFrame does.
void renderFrames(const DueFrames& f, float t);
// The latest committed frame is shown exactly from now on (a loop outside the
// main one presenting what it drew: the splash, a forced show).
void showLatestNow();
// The texture thread (Platform.cpp: its own context, sharing the
// textures): does the queued texture work as soon as it comes, until `stop`.
// The display thread shows a frame only once its textures are in, so an
// upload (a new texture's storage made by the driver took up to 15 ms)
// never costs a display frame.
void textureWorkLoop(const std::atomic<bool>& stop);
// Held by the display thread while it draws a frame: the texture thread's
// work goes in small pieces between (two contexts working at once made the
// frame wait for a whole upload).
struct DrawTurn {
    DrawTurn();
    ~DrawTurn();
    DrawTurn(const DrawTurn&) = delete;
    DrawTurn& operator=(const DrawTurn&) = delete;
};

// The game screen's size the shown frame was recorded at (screenWidth/Height
// then: a game may change it between frames, setScreenSize); before any
// frame, the current one. A frame is shown at its own size, and not eased
// from a frame of another size.
void shownFrameSize(int& w, int& h);

}  // namespace platform
