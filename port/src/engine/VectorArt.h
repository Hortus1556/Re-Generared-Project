// A Flash movie (SWF) used as the game's art library: every symbol drawn as
// vector art at the resolution it appears on screen, never from bitmaps.
//
// The art is an asset, not a player. Drawing a symbol takes the frame to
// show and how long it has been on screen (`age`, in movie frames). Its
// nested animations (a blinking eye in a head, the flicker of a flame)
// follow from that age by their authored timelines, including the stop and
// goto markers the artist put on them (Swf's FrameAction). Nothing here
// keeps state between draws or runs game logic: the game's objects own
// their state and decide which symbol, frame and age to draw.
//
// Coordinates are the movie's pixels under the current GL matrices (a game
// whose screen is the movie's stage draws at stage coordinates).
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "flash/Raster.h"
#include "flash/Swf.h"
#include "flash/VectorMesh.h"

// Where and how a symbol is drawn: position (pixels), scale (1 = authored
// size), rotation (degrees), opacity and an optional tint.
struct VectorTransform {
    float x = 0, y = 0;
    float scaleX = 1, scaleY = 1;
    float rotation = 0;
    float alpha = 1;
    flash::ColorTransform color;  // applied before alpha
    // Applied first (twips): where the art is placed in the movie, as
    // placement() gives it.
    flash::Matrix base;
};

// Per-draw overrides of a symbol's named inner pieces: the text a named
// text field shows, the frame a named inner clip shows (instead of playing).
// Keys are instance names or paths of them ("head.mouth"), matching the
// end of a piece's path inside the drawn symbol, or "#<character id>" for
// an unnamed inner clip; text fields also match by the variable they show.
struct VectorLook {
    std::unordered_map<std::string, std::string> texts;
    std::unordered_map<std::string, int> frames;
    // For some of those: how long the clip has been on that frame (a
    // playhead the game moves), the clock its newly placed pieces start by
    // (else they age with the clip's own placement).
    std::unordered_map<std::string, long> inFrames;
    // The same by instance (VectorArt's instance ids), before any path: an
    // unnamed clip of which several copies share a path ("a.#id"), each
    // its own (AS1's bandage strips, each its own stain picture).
    std::unordered_map<unsigned long long, int> instanceFrames;
    std::unordered_map<unsigned long long, long> instanceInFrames;
    // Buttons by character id: the state shown (1 up, 2 over, 4 down).
    std::unordered_map<int, int> buttons;
    // Named pieces not drawn (by path, as `frames`, or "#<character id>").
    std::vector<std::string> hidden;
    // Named pieces drawn at that opacity (Flash's alpha), by path.
    std::unordered_map<std::string, float> alphas;
    // Named pieces moved to a position in their parent (pixels).
    struct Position {
        float x, y;
    };
    std::unordered_map<std::string, Position> positions;
    // Named pieces raised above the rest of their parent, drawn in the
    // order of these numbers (Flash's swapDepths to a script depth).
    std::unordered_map<std::string, int> raised;
    // Only the symbol's own pieces at these depths (inclusive): its
    // timeline drawn in slices around what a game draws in place of one
    // of them (a native module's clip).
    int fromDepth = -0x7fffffff, toDepth = 0x7fffffff;
    // A text field showing a variable no `texts` key gives: its value, as
    // the scripts have it (the field is in the clip at `clip`, instance
    // names); false: the field's own text.
    std::function<bool(const std::string& clip, const std::string& variable, std::string& text)> fieldText;
};

// Whether the instance path `names` ("a.b.c") ends with `key` ("b.c"),
// or, for a key starting with '/' ("/a.b.c"), is exactly that path.
bool vectorPathMatches(const std::string& key, const std::string& names);

class VectorPlayhead;

class VectorArt {
public:
    // The game's movie, loaded once: a game package by its library.json
    // ("library.json", "<folder>/library.json"; flash/Package.h), or a SWF
    // in the bundle (flash/<name>.swf). Fatal error if missing.
    static VectorArt& library(const std::string& name);
    // Starts reading that movie on a worker thread (a big package takes a
    // few hundred milliseconds), for library() to take when it is first
    // asked for (waiting only if it is not read yet).
    static void preload(const std::string& name);
    // Whether `name` can be had without waiting: read already, or its
    // preload done (or never begun).
    static bool preloaded(const std::string& name);

    const flash::Swf& movie() const { return *swf; }

    // An exported symbol by linkage name, or a character id as text ("5303");
    // -1 if unknown.
    int symbol(const std::string& name) const;
    int frameCount(int symbol) const;
    int frameOf(int symbol, const std::string& label) const;  // 0 if none
    // The one label of `symbol` that begins with `prefix` (lower-case; a
    // name another version shortened), "" if none or several.
    std::string labelStartingWith(int symbol, const std::string& prefix) const;
    // Where `sprite`'s timeline (0: the movie's main timeline) shows the
    // piece named `name` (or with character id "#<id>") on `frame`: its
    // transform, tint and character. False if it is not there.
    bool placement(int sprite, int frame, const std::string& name, VectorTransform& t, int* character = nullptr);
    // The frame a sprite shows `age` frames after it appeared, following its
    // authored stops and gotos (1-based). `seed` tells instances apart where
    // the art picks a random variant (each instance keeps its pick).
    int playhead(int symbol, long age, unsigned long long seed = 0);
    // Whether the sprite stops by itself (an authored stop), and after how
    // many frames; false for one that loops forever.
    bool stopsAfter(int symbol, long& frames);

    // Draws `symbol` showing `frame` (1-based; 0: by its own timeline from
    // `age`), `age` frames after it appeared. `owner` identifies the drawer
    // from frame to frame (smooth motion); `look` overrides named pieces.
    void draw(int symbol, int frame, long age, const VectorTransform& t, unsigned long long owner,
              const VectorLook* look = nullptr);
    // The same with how long the frame shown has been on screen (`inFrame`,
    // for inner animations placed on it), e.g. a VectorPlayhead's.
    void draw(int symbol, int frame, long age, long inFrame, const VectorTransform& t, unsigned long long owner,
              const VectorLook* look = nullptr);
    void draw(const VectorPlayhead& p, const VectorTransform& t, unsigned long long owner,
              const VectorLook* look = nullptr);
    // Drawn under `world` (movie pixels, any transform; the part smooth
    // motion moves) with `color`.
    void drawUnder(const flash::Matrix& world, const flash::ColorTransform& color, int symbol, int frame, long age,
                   long inFrame, unsigned long long owner, const VectorLook* look = nullptr);
    // The button (character id) whose hit area is topmost at the movie point
    // (x, y) when the symbol is drawn so; -1 if none.
    int buttonAt(int symbol, int frame, long age, long inFrame, const VectorTransform& t, float x, float y,
                 const VectorLook* look = nullptr);
    int buttonAt(const VectorPlayhead& p, const VectorTransform& t, float x, float y);
    // The same, with the path of the clip the button is in (instance
    // names; "#<id>" last for an unnamed one; "" the symbol itself): the
    // `this` of its handlers.
    int buttonAt(int symbol, int frame, long age, long inFrame, const VectorTransform& t, float x, float y,
                 const VectorLook* look, std::string& clipPath, int* topDepth = nullptr);
    // Draws the text field `editText` (a character id) showing `text`, its
    // box centred on x and its top at y, scaled (debug labels; a name
    // written in). `boxWidth` (pixels of the field, 0: its own) widens its
    // box, its text aligned as the field's (a longer text in its style).
    void drawText(int editText, const std::string& text, float x, float y, float scale, unsigned long long owner,
                  float boxWidth = 0);
    // Has what draw() would draw with these arguments made in the background
    // (call it a second or so before the frame is shown, so drawing never
    // waits for rasterising).
    // `keepFrames`: kept that long even if not drawn (art for later).
    void prefetch(int symbol, int frame, long age, const VectorTransform& t, unsigned long long owner,
                  const VectorLook* look = nullptr, long keepFrames = 0);
    // The same with how long the frame shown has been on screen (`inFrame`).
    void prefetch(int symbol, int frame, long age, long inFrame, const VectorTransform& t, unsigned long long owner,
                  const VectorLook* look, long keepFrames = 0);
    bool busy() { return raster->busy(); }
    // While on, draw(), drawUnder() and drawText() draw nothing: they have
    // what they would draw made in the background (a screen drawn so while
    // a transition still hides it).
    void setPrefetchOnly(bool on) { prefetchOnly = on; }
    // Smooth motion of the art's own tweens: each drawn piece is matched
    // with itself in the pass before (by its place in the art and its
    // drawing) and moves evenly at the display's rate; a piece that changes
    // drawing (a mouth, a pose) or jumps still steps at its pass. Off: only
    // the transform the art is drawn under moves smoothly.
    static bool smoothTweens;
    // Whether the movie point (x, y) is on the symbol's art drawn so; with
    // `only`, on that inner character's art (a button: its hit area).
    bool hitTest(int symbol, int frame, long age, const VectorTransform& t, float x, float y, int only = -1);
    // The same for the named inner piece at `part` (a path, as VectorLook's
    // keys; empty: the whole symbol), drawn with `look`: on its shapes
    // (`shape`), or within its bounding box.
    bool hitTestPart(int symbol, int frame, long age, long inFrame, const VectorTransform& t, const VectorLook* look,
                     const std::string& part, float x, float y, bool shape);
    // That piece's bounding box in movie pixels (empty if not there). As
    // Flash's getBounds, it counts hidden pieces too.
    flash::Rect partBounds(int symbol, int frame, long age, long inFrame, const VectorTransform& t,
                           const VectorLook* look, const std::string& part);
    // The same for each movie clip placed directly in the piece at `part`
    // (empty: in the symbol itself),
    // one box a placement (unnamed copies of one clip told apart), in depth
    // order: Flash's numChildren / getChildAt over its clips (a creature's
    // hit areas).
    std::vector<flash::Rect> childBounds(int symbol, int frame, long age, long inFrame, const VectorTransform& t,
                                         const VectorLook* look, const std::string& part);
    // The frame the inner sprite at instance path `path` shows when the
    // symbol is drawn so (0 if it is not on that frame); `shownAge`,
    // `shownInFrame`: how long it has been on the stage and on that frame.
    int innerFrame(int symbol, int frame, long age, const std::string& path, const VectorLook* look = nullptr,
                   long inFrame = -1, long* shownAge = nullptr, long* shownInFrame = nullptr);
    // The sprite (character id) at that path, -1 if none there.
    int innerSymbol(int symbol, int frame, long age, const std::string& path);
    // The named inner piece at `path` drawn with `look`: its transform in
    // its parent clip (`local`) and in the symbol (`inSymbol`), in twips,
    // and its character. False if it is not there. `instance` identifies
    // the Flash instance: it changes when that clip or one above it is
    // placed anew (its depth removed or placed again), not otherwise.
    bool partInfo(int symbol, int frame, long age, long inFrame, const VectorLook* look, const std::string& path,
                  flash::Matrix& local, flash::Matrix& inSymbol, int& character,
                  unsigned long long* instance = nullptr);
    // The inner clips placed directly in the piece at `path` (empty: the
    // symbol itself): their instance names, "#<character id>" for unnamed
    // ones, in the order Flash's for..in over a clip meets them (the last
    // created first).
    std::vector<std::string> childClips(int symbol, int frame, long age, long inFrame, const VectorLook* look,
                                        const std::string& path);
    // Every inner sprite instance of the symbol drawn so, hidden ones too,
    // as the art plays them: its path (instance names; an unnamed one as
    // "#<id>" after its parent's path, as VectorLook keys), character, the
    // frame it shows and its instance (see partInfo). The scripts of a
    // scene run by these (Scene.h).
    struct ClipVisit {
        std::string path;
        int character;
        int frame;
        unsigned long long instance;
        unsigned long long parent = 0;  // the instance of the clip it is in (0: the symbol)
    };
    void clips(int symbol, int frame, long age, long inFrame, const VectorLook* look, std::vector<ClipVisit>& out);
    // Only the sprites placed on the symbol's own frame (no walk; their
    // frames not known: 0), with their depths and placements.
    struct TopClip {
        std::string path;  // instance name, or "#<id>"
        int character;
        int depth;
        unsigned long long instance;
        flash::Matrix matrix;  // twips
        flash::ColorTransform color;
    };
    std::vector<TopClip> topClips(int symbol, int frame);
    // Every instance of the sprite `character` shows `frame` (0: its own
    // timeline again) where no look says otherwise: art whose frame script
    // picks a frame from the game's state when it appears.
    void setFrameOverride(int character, int frame);
    // Art whose scripts pick at random, told by the game (the scripts are
    // not run): frame `frame` of `sprite` goes to one of `choices` (and
    // plays on, or stays), the pick fixed per instance...
    void setRandomStart(int sprite, int frame, std::vector<int> choices, bool play);
    // ...and a clip whose parent plays its inner clip `child` (stopped on
    // its frame 1) now and then: each frame, one chance in `chance`
    // (`onEnterFrame: if(random(chance) == 1) child.play()`).
    void addRandomTrigger(int parent, const std::string& child, int chance);
    // An ActionScript 3 frame script's stop() (in the movie's classes, not
    // its art): `sprite` stops on `frame` when it plays there.
    void addStop(int sprite, int frame);
    // ...and frame `frame` of `sprite` going back to `from` until it has
    // played `times` times in all, then on one frame and stopping.
    void setRepeat(int sprite, int frame, int from, int times);
    // Debug: prints the inner sprites drawn so, with their frames and ages.
    void dumpTree(int symbol, int frame, long age);
    // The art's bounding box in movie pixels, drawn so (empty: xMin > xMax).
    flash::Rect bounds(int symbol, int frame, long age, const VectorTransform& t);

    // A sound exported by linkage name, decoded (interleaved 16-bit, shared
    // by the tracks playing it); null if unknown.
    struct Pcm {
        std::shared_ptr<const std::vector<int16_t>> samples;
        int channels = 1, rate = 44100;
    };
    const Pcm* sound(const std::string& linkage);

    // Once per drawn frame (swaps in art rasterised in the background).
    void endFrame(int uploadMs = 4) {
        raster->endFrame(120, uploadMs);
        if (meshes) meshes->endFrame();
        if (++passes % 100 == 0)  // (stage pieces no longer drawn forgotten)
            for (auto it = stills.begin(); it != stills.end();) it = it->second.seen + 100 < passes ? stills.erase(it) : std::next(it);
    }
    flash::Raster& rasterCache() { return *raster; }
    const flash::MeshCache* meshCache() const { return meshes.get(); }  // (null: none)

private:
    struct Piece {  // one object on a sprite's frame
        int depth, character, placedFrame;
        flash::Matrix matrix;
        flash::ColorTransform color;
        float ratio = 0;
        int clipDepth = 0;
        std::string name;
        bool persistent = false;  // placed on frame 1 and never replaced
        std::vector<flash::Filter> filters;  // blur, glow, drop shadow (drawn on its pieces)
    };
    // The playhead from some start, age by age, until it stops, repeats or
    // reaches a random goto (then a Run per possible pick follows).
    struct Run {
        std::vector<int> heads;    // frame per age
        std::vector<int> since;    // the age the playhead came to that frame
        int cycleFrom = -1;        // heads repeat from this age
        int randomCount = 0;       // > 0: then a random one of these frames...
        int randomBase = 0;        // ...from this one
        std::vector<int> randomChoices;  // (or one of these)
        bool randomPlay = false;   // ...played on, or held
    };
    struct Timeline {
        std::vector<std::vector<Piece>> frames;  // the pieces shown on each frame
        Run main;                                // from the start
        std::unordered_map<int, Run> picked;     // after a random goto, by frame picked
    };
    struct Head {
        int frame;
        long inFrame;  // ages on that frame so far
    };
    Run simulate(int sprite, int start, bool playing, bool followRandom);
    Head head(int sprite, long age, unsigned long long seed);

    struct Leaf;  // a drawn piece met by walk()
    // The clip holding the faded, filtered clip drawn as one (drewAsOne).
    flash::Matrix groupParentWorld;
    unsigned long long groupParentPath = 0;
    struct Draw;  // the visitor drawing it
    explicit VectorArt(std::unique_ptr<flash::Swf> movie);
    flash::Rect localBounds(const Leaf& l) const;
    Timeline& timeline(int sprite);
    template <class Visit>
    void walk(int character, int frame, long age, const flash::Matrix& world, const flash::ColorTransform& color,
              const VectorLook* look, unsigned long long path, const std::string& names, Visit& visit,
              long inFrameGiven = -1, unsigned long long instance = 0);
    // A faded, filtered clip `p` handed to the visitor as one picture (its
    // pieces, from a walk of its own); false if it holds what that cannot
    // draw (texts, masks, inner filters or colours): walked as usual then.
    struct Collect;
    template <class Visit>
    bool drewAsOne(const Piece& p, int frame, long age, long inFrame, const flash::Matrix& world,
                   const flash::ColorTransform& color, const VectorLook* look, unsigned long long path,
                   const std::string& names, unsigned long long instance, Visit& visit);

    std::unique_ptr<flash::Swf> swf;
    bool prefetchOnly = false;
    // Stage pieces drawn as one picture once still (drewStill), by owner.
    struct Still {
        uint64_t hash = 0;
        long since = 0, seen = 0, notPlainUntil = 0;
    };
    std::unordered_map<unsigned long long, Still> stills;
    long passes = 0;
    // A clip that stayed the same for a while drawn as one picture
    // (Draw::flat); false: walked as usual.
    template <class Visit>
    // (`world`: the walk's matrix for it; `placed`: where that puts it on
    // the stage, the GL matrix included; `key`: its picture's; `instance`:
    // its Flash instance, 0 for a node's own symbol, whose slice of depths
    // the look gives.)
    bool drewStill(int character, int frame, long age, long inFrame, const flash::Matrix& world,
                   const flash::Matrix& placed, const flash::ColorTransform& color, const VectorLook* look,
                   unsigned long long path, const std::string& names, unsigned long long key,
                   unsigned long long instance, const flash::Matrix& parentWorld, unsigned long long parentPath,
                   Visit& visit);
    std::unique_ptr<flash::Raster> raster;
    std::unique_ptr<flash::MeshCache> meshes;  // (null: all art rasterised)
    std::unordered_map<int, Timeline> timelines;
    // The filters of the clips a walk is inside, outermost first.
    std::vector<const std::vector<flash::Filter>*> filterStack;
    std::unordered_map<std::string, Pcm> sounds;
    std::unordered_map<int, int> frameOverrides;
    struct Trigger {
        int parent;
        std::string child;
        int chance;
    };
    std::vector<Trigger> triggers;
    struct TriggerState {
        long simulated = -1, started = -1, busyUntil = 0;
    };
    std::unordered_map<unsigned long long, TriggerState> triggerStates;
    // The frame shown `age` frames into a clip played now and then (-1: at
    // rest on frame 1).
    int triggeredFrame(int sprite, int chance, unsigned long long seed, long age, long& since);
};

// A game object's running animation of one symbol.
class VectorAnimation {
public:
    VectorAnimation() = default;
    VectorAnimation(VectorArt& art, int symbol) : art(&art), symbol(symbol) {}

    // One movie frame later.
    void update() { ++age; }
    void restart() { age = 0; }
    // The symbol's frame now (its own timeline from the start).
    int frame() const { return art ? art->playhead(symbol, age) : 1; }
    bool finished() const {
        long n;
        return art && art->stopsAfter(symbol, n) && age >= n;
    }
    void draw(const VectorTransform& t, unsigned long long owner, const VectorLook* look = nullptr) const {
        if (art) art->draw(symbol, 0, age, t, owner, look);
    }

    VectorArt* art = nullptr;
    int symbol = -1;
    long age = 0;
};

// The game's playhead over a sprite's timeline, for art that plays and stops
// on cue (a menu panel sliding open when asked, a banner): the game moves it
// (play, stop, gotoAndStop...) and it follows the timeline's own stop and
// goto markers as it plays, one movie frame per update().
class VectorPlayhead {
public:
    VectorPlayhead() = default;
    VectorPlayhead(VectorArt& art, int symbol);  // on frame 1 (its markers applied)

    void update();
    void play() { playing = true; }
    void stop() { playing = false; }
    void gotoAndStop(int frame);
    void gotoAndPlay(int frame);
    // A label the sprite does not have: nothing happens, as in Flash.
    void gotoAndStop(const std::string& label) {
        if (int f = labelFrame(label)) gotoAndStop(f);
    }
    void gotoAndPlay(const std::string& label) {
        if (int f = labelFrame(label)) gotoAndPlay(f);
    }
    int labelFrame(const std::string& label) const;

    VectorArt* art = nullptr;
    int symbol = -1;
    int frame = 1;
    bool playing = true;
    long age = 0;      // movie frames since it started
    long inFrame = 0;  // movie frames on the current frame
    int repeats = 0;   // loops counted by a repeat marker (FrameAction::kRepeat)

private:
    void enter(int frame);
};

