// A node of the game's scene: one symbol of the movie's art with its own
// playhead (frame, playing), its place (position, rotation, scale, opacity,
// visibility, tint) and the nodes the game attached to it (drawn above its
// art, in depth order). The surgeon games build their screens from these:
// an operation's body layers, the injuries on them, the blood, the tools,
// the HUD. What a node does is up to the game objects that own it.
//
// A node's symbol may have named inner parts (a "holder" inside an injury,
// "hit1" inside a background): the game can play or hold them (part()),
// hide them (hidePart()), and test the mouse or another node against them
// (hitTest, bounds, with a part path).
//
// A named inner clip of its timeline can be made a node of its own
// (promote): what a script does to it that only a node can (attach clips
// under it, turn it, swap its depth) then works on it, the timeline still
// placing it as before until the script takes it over.
//
// Positions are in the parent node's pixels; a root node's parent is the
// stage.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/Object.h"
#include "engine/VectorArt.h"

class ArtNode : public Object {
public:
    // `symbol` -1: an empty node (a container, as Flash's "empty" clip).
    ArtNode(VectorArt& art, int symbol);
    ~ArtNode() override;

    // ---- the tree
    // Attaches `child` (+1, taken over) under `name` at `depth`, replacing
    // what was at that depth; returns it.
    ArtNode* attach(ArtNode* child, const std::string& name, int depth);
    // A new node for the exported symbol `linkage` (or "<id>"); null if
    // the movie has no such symbol.
    ArtNode* attach(const std::string& linkage, const std::string& name, int depth);
    void remove();  // from its parent (it is released)
    ArtNode* child(const std::string& name) const;
    // Flash's swapDepths: a clip its parent's timeline placed is the
    // script's from then on.
    void swapDepths(int depth);
    ArtNode* parent = nullptr;
    bool removed = false;  // taken off its parent (remove, or replaced at its depth)
    std::string name;
    int depth = 0;
    std::map<int, ArtNode*> children;  // by depth, retained

    // ---- its timeline
    VectorPlayhead clip;
    int symbol() const { return clip.symbol; }
    int frame() const { return clip.frame; }
    int frameCount() const;
    void gotoAndStop(int f);
    void gotoAndPlay(int f);
    // A label the symbol does not have: nothing happens, as in Flash.
    void gotoAndStop(const std::string& label) {
        if (int f = clip.labelFrame(label)) gotoAndStop(f);
    }
    void gotoAndPlay(const std::string& label) {
        if (int f = clip.labelFrame(label)) gotoAndPlay(f);
    }
    void play() { clip.play(); }
    void stop() { clip.stop(); }

    // ---- inner parts (paths of instance names in its symbol)
    // A playhead for the inner clip at `path` (created on its current
    // frame, stopped); null if there is none. When the clip at that path is
    // replaced (the node or a part above it went to another frame), its
    // playhead and its look (hidden, moved) go, as a new instance's would.
    // A part with its `instance` given has a playhead of its own instance:
    // where several copies share its path (unnamed clips, "a.#id", or named
    // ones in unnamed copies: AS1's bandage strips, its pause screen's
    // three `myCol`s), each goes to its own frames.
    VectorPlayhead* part(const std::string& path, unsigned long long instance = 0);
    // Moves the part's playhead (made if needed); false if there is none.
    bool partGotoAndStop(const std::string& path, int frame, unsigned long long instance = 0);
    bool partGotoAndStop(const std::string& path, const std::string& label, unsigned long long instance = 0);
    bool partGotoAndPlay(const std::string& path, int frame, unsigned long long instance = 0);
    // The instances of unnamed parts still shown (Scene::enter's walk):
    // the playheads of the others go.
    void keepInstanceParts(const std::unordered_set<unsigned long long>& shown);
    // The inner clips directly in a part (empty: in the node's symbol), as
    // Flash's for..in over a clip meets them.
    std::vector<std::string> partChildren(const std::string& path);
    // Every inner sprite instance shown now (VectorArt::clips).
    void innerClips(std::vector<VectorArt::ClipVisit>& out);
    // The character placed at the path now (-1: none).
    int partSymbol(const std::string& path);
    // The Flash instance at the path now (0: none): the same until that
    // clip, or one above it, is placed anew by its timeline.
    unsigned long long partInstance(const std::string& path);
    // Every unnamed instance of the sprite `character` in it held on `frame`
    // (0: its own timeline again), or hidden.
    void setCharacterFrame(int character, int frame);
    int characterFrame(int character) const;
    void hideCharacter(int character, bool hidden = true) { hidePart("#" + std::to_string(character), hidden); }
    void hidePart(const std::string& path, bool hidden = true);
    bool partHidden(const std::string& path) const;
    void setPartText(const std::string& path, const std::string& text) { look.texts[path] = text; }
    bool hasPart(const std::string& path);
    // Whether this path already has a part playhead of its own (made by a
    // gotoAndStop/Play on it): once it does, its frame advances one at a
    // time or jumps by an explicit goto, same as any clip's own; before
    // that, it is found fresh from its parent's age each time, which can
    // jump several frames between two sightings instead of advancing.
    bool hasPartPlayhead(const std::string& path, unsigned long long instance = 0) const {
        return byInstance(path, instance) ? instanceParts.count(instance) != 0 : parts.count(path) != 0;
    }
    int partFrame(const std::string& path, unsigned long long instance = 0);
    // A part's position in its parent clip (as authored until moved).
    float partX(const std::string& path);
    float partY(const std::string& path);
    void setPartPosition(const std::string& path, float px, float py);
    // The stage point (sx, sy) in the part's own pixels (Flash's
    // part._xmouse/_ymouse for the mouse).
    void stageToPart(const std::string& path, float sx, float sy, float& px, float& py);
    // And back: a point in the part's own pixels on the stage.
    void partToStage(const std::string& path, float px, float py, float& sx, float& sy);
    // The part's size in its parent (Flash's _width, _height).
    float partWidth(const std::string& path);
    float partHeight(const std::string& path);
    // Its transform in its parent clip (pixels); identity if not there.
    flash::Matrix partMatrix(const std::string& path);

    // ---- inner clips made nodes
    // The clip its timeline places as `name` (directly in its symbol), from
    // now on a child node at its depth (Flash's timeline depths: the
    // symbol's depth - 16384, below what is attached): its playhead, its
    // parts' and their looks go with it, its frame goes on as it was. The
    // timeline keeps placing it (its matrix and colour) until a script
    // moves it (`scripted`) or swaps its depth, and removes it when it no
    // longer places that instance. Null if there is no such clip.
    ArtNode* promote(const std::string& name);
    // The node a part path leads into, through the clips made nodes and
    // the nodes attached on the way; `path` is left as the rest of it.
    ArtNode* through(std::string& path);
    static constexpr int kTimelineDepth = -16384;
    bool timeline = false;             // placed by its parent's timeline
    bool scripted = false;             // its place set by a script (no longer the timeline's)
    unsigned long long placement = 0;  // the instance it was made from (0: attached)

    // ---- its place
    // Sets x, y, rotation and scales from a placement matrix (pixels).
    void setMatrix(const flash::Matrix& m);
    float x = 0, y = 0, rotation = 0, xscale = 100, yscale = 100, alpha = 100;
    // A script set its _alpha: that is its opacity, in place of the one its
    // timeline's colour gives it (Flash's _alpha is the colour transform's
    // alpha multiplier: AS1's brain cut line, placed transparent, shows once
    // the skull is off by `hit._alpha = 100`).
    bool alphaSet = false;
    float opacity() const { return (alphaSet ? 1 : tint.mult[3]) * alpha / 100; }
    bool visible = true;
    // The game's pointer (the cursor clip, the tool in hand): drawn as
    // platform::PointerArt, where the mouse is at every display step and
    // above everything, as AS2's and AS3's cursors; the system's is hidden
    // while it shows (StageScreen::drawsPointer).
    bool pointer = false;
    bool shown() const;  // visible, as is every node it is in
    flash::ColorTransform tint;
    flash::Matrix matrix() const;  // in the parent, pixels
    flash::Matrix world() const;   // to the stage, pixels
    void localToGlobal(float& px, float& py) const;
    void globalToLocal(float& px, float& py) const;

    // ---- per pass
    virtual void update();  // its playhead, its parts' and its children's
    void draw();

    // ---- tests, in stage pixels
    bool hitTest(float sx, float sy, bool shape, const std::string& part = std::string());
    flash::Rect bounds(const std::string& part = std::string());  // empty if nothing drawn
    // Bounding boxes (Flash's hitTest(target)).
    bool hitTest(ArtNode& other, const std::string& part = std::string(),
                 const std::string& otherPart = std::string());
    // The button (character id) under the stage point, -1 if none.
    int buttonAt(float sx, float sy);
    // The same, with the path of the clip it is in (its handlers' `this`)
    // and the depth of the symbol's own piece it is in.
    int buttonAt(float sx, float sy, std::string& clipPath, int& topDepth);
    // The same in it and the nodes under it: `in` the node the clip path is
    // in.
    int buttonAt(float sx, float sy, ArtNode*& in, std::string& clipPath, int& topDepth);
    // Draws only the symbol's own pieces at these depths, with the nodes
    // made of its clips there (promote), and no attached nodes: its
    // timeline in slices around a clip the game draws itself.
    void drawSlice(int fromDepth, int toDepth);
    void drawAttached();  // the nodes attached to it (by depth)
    // Has its art `frames` passes from now made in the background (as it
    // plays on by itself: its inner clips' own timelines).
    void prefetchAhead(long frames);
    // Text fields showing a variable get its value from here (VectorLook).
    // Its parts' playheads give their clocks to the art too (VectorLook's
    // inFrames): what a goto places in a part starts then.
    bool partClocks = false;
    void setFieldText(std::function<bool(const std::string&, const std::string&, std::string&)> f) {
        look.fieldText = std::move(f);
    }
    // Button states shown (character id -> 1 up, 2 over, 4 down), in it
    // and the nodes under it.
    void setButtonState(int character, int state);
    void clearButtonStates();

    VectorArt& art;

private:
    VectorTransform stageTransform() const;
    bool partMatrices(const std::string& path, flash::Matrix& local, flash::Matrix& inSymbol);
    flash::Rect partLocalBounds(const std::string& path);
    void applyParts();
    void dropReplacedParts();
    void followTimeline();  // the nodes made of its clips: placed, or gone
    void drawOwn(int fromDepth, int toDepth);
    void noteInstance(const std::string& path);
    std::unordered_map<std::string, unsigned long long> lookInstances;  // hidden/moved paths -> the instance they were set on

    VectorLook look;
    std::unordered_map<std::string, VectorPlayhead> parts;
    std::unordered_map<std::string, unsigned long long> partIds;  // the instance each playhead was made for
    std::unordered_map<unsigned long long, VectorPlayhead> instanceParts;  // unnamed parts', by instance
    // (A caller gives an instance where the path alone is not enough:
    // SceneClip::own.)
    static bool byInstance(const std::string&, unsigned long long instance) { return instance != 0; }
};
