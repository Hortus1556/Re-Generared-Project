// A Flash movie (SWF) file, parsed into what the native Flash runtime plays
// (port of the Flash games: the original Flash versions of Amateur Surgeon,
// from the Flashpoint archive, re/flash/).
//
// Characters by id: vector shapes (fills and strokes as closed paths, in
// twips: 1/20 pixel), morph shapes, sprites (movie clips: a timeline of
// display-list operations per frame, with frame labels), buttons, static
// texts, dynamic text fields, fonts and sounds. Id 0 is the main timeline.
// Exported linkage names (attachMovie) map to ids.
//
// Scripts (DoAction, button and clip actions) are not interpreted: the game
// logic is ported by hand to C++ and hooks into the timeline by sprite id
// and frame (see MovieClip). Bitmaps are JPEG ones (DefineBitsJPEG2: the
// AS1's title crowd and its last page), kept as their JPEG files and drawn
// as shape fills; video and streamed sound are not supported.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace flash {

struct Matrix {  // x' = a x + c y + tx, y' = b x + d y + ty (Flash: a sx, b r0, c r1, d sy)
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    Matrix operator*(const Matrix& n) const;  // this applied after n
    Matrix inverse() const;
    void apply(float x, float y, float& ox, float& oy) const {
        ox = a * x + c * y + tx;
        oy = b * x + d * y + ty;
    }
};

struct ColorTransform {  // c' = c * mult + add, add in 0..255 units
    float mult[4] = {1, 1, 1, 1};
    float add[4] = {0, 0, 0, 0};
    ColorTransform operator*(const ColorTransform& inner) const;  // inner applied first
    bool isIdentity() const;
};

struct Rect {
    float xMin = 0, yMin = 0, xMax = 0, yMax = 0;  // twips
    bool empty() const { return xMax <= xMin || yMax <= yMin; }
};

// ------------------------------------------------------------------ shapes

struct GradientStop {
    float ratio;  // 0..1
    uint8_t rgba[4];
};

struct FillStyle {
    enum Type { kSolid, kLinear, kRadial, kFocal, kBitmap } type = kSolid;
    uint8_t rgba[4] = {0, 0, 0, 255};
    Matrix matrix;  // gradient square (-16384..16384) -> shape twips
    std::vector<GradientStop> stops;
    int spread = 0;  // 0 pad, 1 reflect, 2 repeat
    float focal = 0;
    // kBitmap: the bitmap (character id), its pixels -> shape twips by
    // `matrix` (20 twips a pixel at its own size); clipped (its edge
    // pixels go on past it) or repeated; smoothed or not.
    int bitmap = -1;
    bool clipped = true, smoothed = true;
};

struct LineStyle {
    float width = 20;  // twips
    uint8_t rgba[4] = {0, 0, 0, 255};
    int startCap = 0, endCap = 0, join = 0;  // caps 0 round 1 none 2 square; join 0 round 1 bevel 2 miter
    float miterLimit = 3;
    bool hasFill = false;
    FillStyle fill;
};

// One piece of path: a move, a line or a quadratic curve, in twips.
struct PathOp {
    enum Kind : uint8_t { kMove, kLine, kCurve } kind;
    int32_t cx, cy, x, y;  // control point (curves), end point
};

struct StyledPath {
    int style;  // index into the layer's fills or lines
    std::vector<PathOp> ops;
};

// Styles change mid-shape (DefineShape2+ "new styles"): each set is a layer
// drawn over the ones before, its fills first, then its strokes.
struct ShapeLayer {
    std::vector<FillStyle> fills;
    std::vector<LineStyle> lines;
    std::vector<StyledPath> fillPaths;  // closed contours per fill (even-odd)
    std::vector<StyledPath> linePaths;
};

struct Shape {
    Rect bounds;
    std::vector<ShapeLayer> layers;
};
// A morph shape `t` of the way from its start `a` to its end `b` (the same
// structure): points, colours, gradients and line widths in between.
Shape lerpShape(const Shape& a, const Shape& b, float t);

// ------------------------------------------------------------- timelines

struct Filter {
    enum Type { kDropShadow, kBlur, kGlow, kOther } type = kOther;
    uint8_t rgba[4] = {0, 0, 0, 255};
    float blurX = 0, blurY = 0, strength = 1, angle = 0, distance = 0;
    bool inner = false, knockout = false;
    int passes = 1;
};

struct PlaceOp {
    int depth = 0;
    bool move = false;     // modify what is at depth
    int character = -1;    // -1: unchanged
    bool hasMatrix = false, hasColor = false, hasRatio = false, hasName = false, hasClip = false;
    Matrix matrix;
    ColorTransform color;
    float ratio = 0;       // morph shapes, 0..1
    std::string name;      // instance name
    int clipDepth = 0;     // a mask over depths up to clipDepth
    int blend = 0;         // 0/1 normal, 2 layer, 3 multiply, ...
    std::vector<Filter> filters;
};

struct FrameOp {
    enum Kind { kPlace, kRemove } kind;
    PlaceOp place;  // kPlace
    int depth = 0;  // kRemove
};

// How a timeline plays on from a frame: the plain frame actions of its
// script (stop(), play(), gotoAndStop/gotoAndPlay with a fixed frame or
// label, or a random one of N frames from a base (a clip showing one of
// its variants), nextFrame(), prevFrame() on the clip itself). They are authored
// animation control, kept as data; the rest of a script is game logic,
// ported by hand. A script that branches or retargets keeps none.
struct FrameAction {
    // kRepeat (set by a game, not parsed): back to `frame` and on, `count`
    // times in all, then on to the next frame and stop (a script counting
    // its loops).
    enum Kind { kStop, kPlay, kGoto, kGotoLabel, kGotoRandom, kNext, kPrev, kRepeat } kind;
    int frame = 0;      // kGoto: 1-based; kGotoRandom: the first of `count`
    int count = 0;      // kGotoRandom
    bool play = false;  // kGotoRandom: gotoAndPlay
    std::vector<int> choices;  // kGotoRandom: the frames picked from, if not `count` from `frame`
    std::string label;  // kGotoLabel
};

struct Timeline {
    int frameCount = 1;
    std::vector<std::vector<FrameOp>> frames;
    std::map<std::string, int> labels;  // label (lower-case: Flash ignores case) -> frame (1-based)
    std::vector<bool> hasScript;        // frames with a DoAction
    std::vector<std::vector<FrameAction>> actions;  // per frame, in order
};

// ----------------------------------------------------------- other chars

struct ButtonRecord {
    int states;  // 1 up, 2 over, 4 down, 8 hit
    int character, depth;
    Matrix matrix;
    ColorTransform color;
};

struct Button {
    std::vector<ButtonRecord> records;
};

struct Font {
    std::string name;
    bool bold = false, italic = false;
    float unitsPerEm = 1024;  // glyph shapes are in 1/20 of these for DefineFont3
    std::vector<Shape> glyphs;
    std::vector<uint16_t> codes;
    std::vector<float> advances;  // twips at unitsPerEm
    float ascent = 0, descent = 0, leading = 0;
};

struct TextGlyph {
    int index;
    float advance;  // twips
};

struct TextRecord {
    int font = -1;
    uint8_t rgba[4] = {0, 0, 0, 255};
    float x = 0, y = 0, height = 240;  // twips
    std::vector<TextGlyph> glyphs;
};

struct StaticText {
    Rect bounds;
    Matrix matrix;
    std::vector<TextRecord> records;
};

struct EditText {
    Rect bounds;
    int font = -1;
    float height = 240;
    uint8_t rgba[4] = {0, 0, 0, 255};
    int align = 0;  // 0 left 1 right 2 center 3 justify
    float leftMargin = 0, rightMargin = 0, indent = 0, leading = 0;
    bool wordWrap = false, multiline = false, html = false, border = false, noSelect = false;
    std::string variable, text;
};

// A bitmap: its image file as stored (a JPEG).
struct Bitmap {
    std::vector<uint8_t> data;
};

struct Sound {
    int format = 0;   // 0 raw, 1 ADPCM, 2 MP3, 3 raw little-endian, 6 Nellymoser
    int rate = 44100; // 5512, 11025, 22050, 44100
    bool is16 = true, stereo = false;
    uint32_t samples = 0;  // sample frames played (MP3: after the seek samples)
    int seekSamples = 0;   // MP3: decoded frames before the sound starts (encoder delay)
    std::vector<uint8_t> data;  // MP3: without the seek-samples word
};

// ---------------------------------------------------------------- the file

class Swf {
public:
    // nullptr when the bytes are not a supported SWF.
    static std::unique_ptr<Swf> load(const std::vector<unsigned char>& bytes);

    int version = 0;
    float width = 550, height = 400;  // stage, pixels
    float fps = 12;
    uint8_t background[3] = {255, 255, 255};

    enum class Kind { kNone, kShape, kMorph, kSprite, kButton, kText, kEditText, kFont, kSound };
    Kind kind(int id) const;

private:
    struct ShapeState {
        bool read = false;
        uint64_t used = 0;
    };
    mutable std::unordered_map<int, ShapeState> shapeStates;

public:

    // A package's shapes are read when first asked for (shape()) and let go
    // again while unused (unloadShapes): `shapes` holds every id, a Shape
    // with no layers until read. A SWF's are all read at once.
    mutable std::unordered_map<int, Shape> shapes;
    const Shape* shape(int id) const;  // null: no such shape
    std::function<bool(int id, Shape& out)> readShape;  // (a package's)
    mutable uint64_t shapeClock = 0;                    // advanced by the caller (Raster: frames)
    // Shapes not asked for in `unused` ticks of shapeClock forgotten (read
    // again when asked for); how many.
    size_t unloadShapes(uint64_t unused) const;
    std::unordered_map<int, std::pair<Shape, Shape>> morphs;  // start, end (same structure)
    std::unordered_map<int, Timeline> sprites;               // 0: the main timeline
    std::unordered_map<int, Button> buttons;
    std::unordered_map<int, StaticText> texts;
    std::unordered_map<int, EditText> editTexts;
    std::unordered_map<int, Font> fonts;
    std::unordered_map<int, Sound> sounds;
    std::unordered_map<int, Bitmap> bitmaps;
    std::unordered_map<std::string, int> exports;  // linkage name -> id

    struct Reader;  // little-endian byte and bit reader (Swf.cpp)

private:
    void parseTags(Reader& r, size_t end, Timeline& timeline, bool main);
    void parseShape(Reader& r, int tag, int id);
    void parseMorph(Reader& r, int tag, size_t end);
    void parseButton2(Reader& r, size_t end);
    void parseText(Reader& r, int tag);
    void parseEditText(Reader& r);
    void parseFont(Reader& r, int tag, size_t end);
    void parseSound(Reader& r, size_t end);
    PlaceOp parsePlace(Reader& r, int tag, size_t end);
};

}  // namespace flash
