// The translated scripts (Script.h) running on a scene of ArtNodes: every
// clip the scripts can reach is a node, or an inner clip of a node's
// symbol (a "part"), with its variables kept as long as that Flash
// instance lives. Each pass, after the nodes have moved on, the scene runs
// the script of each frame a scripted clip has entered (the clip's own, its
// parts' at any depth), then the clips' onEnterFrame handlers. A game's
// native module shows its own state to the scripts as variables of its
// clips (bind).
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "surgeon/ArtNode.h"
#include "surgeon/Script.h"

class Scene;

// A clip of the scene as the scripts see it: `node`'s own clip (path "") or
// the inner clip at `path` in its symbol. What only a node can do (attach
// clips, turn, fade, swap depths) makes an inner clip a node of its own
// (ArtNode::promote): the handle then points at it, with its variables.
class SceneClip : public script::Clip {
public:
    SceneClip(Scene& scene, ArtNode* node, std::string path, unsigned long long instance);
    ~SceneClip() override;

    script::Value get(const std::string& name) override;
    void set(const std::string& name, const script::Value& v) override;
    bool remove(const std::string& name) override;
    script::ClipRef parent() override;
    void gotoFrame(const script::Value& frame, bool play) override;
    void play() override;
    void stop() override;
    void nextFrame() override;
    void prevFrame() override;
    bool call(const std::string& method, const std::vector<script::Value>& args, script::Value& result) override;
    std::vector<std::string> keys() override;
    bool same(const script::Clip& other) const override;
    int sprite() const override;
    int currentFrame() const override;
    std::string path() const override;
    bool gone() const override;

    // Its variables (what the scripts and the game's modules set on it).
    script::Value var(const std::string& name) const;
    void setVar(const std::string& name, const script::Value& v);
    bool hasVar(const std::string& name) const;

    ArtNode* node;
    std::string part;  // "" for the node's own clip
    unsigned long long instance;
    bool shared = false;  // other instances have the same path (Scene::clipWithin)
    // The instance to address it by, where its path is not enough (an
    // unnamed clip, or a path several copies share); else 0.
    unsigned long long own() const;
    void rebind(ArtNode* to, std::string path, unsigned long long id);

private:
    Scene& scene;
    ArtNode* asNode();  // its own node (an inner clip promoted); null if it cannot be
    std::vector<std::pair<std::string, script::Value>> vars;
    std::string child(const std::string& name) const;  // a part path under it
};

// A clip with no art of its own, as the scripts see it: its variables,
// values bound to a module's state, and named clips under it. A game's
// _root while its main timeline is still hand-made (an operation's).
class VarClip : public script::Clip {
public:
    script::Value get(const std::string& name) override;
    void set(const std::string& name, const script::Value& v) override;
    bool remove(const std::string& name) override;
    script::ClipRef parent() override { return nullptr; }
    void gotoFrame(const script::Value&, bool) override {}
    void play() override {}
    void stop() override {}
    void nextFrame() override {}
    void prevFrame() override {}
    std::vector<std::string> keys() override;
    bool same(const script::Clip& other) const override { return &other == this; }
    int sprite() const override { return -1; }
    int currentFrame() const override { return 1; }
    std::string path() const override { return "_level0"; }

    void bind(const std::string& name, std::function<script::Value()> get,
              std::function<void(const script::Value&)> set = nullptr) {
        bindings[name] = {std::move(get), std::move(set)};
    }
    std::unordered_map<std::string, script::ClipRef> children;

private:
    struct Binding {
        std::function<script::Value()> get;
        std::function<void(const script::Value&)> set;
    };
    std::unordered_map<std::string, Binding> bindings;
    std::vector<std::pair<std::string, script::Value>> vars;
};

class Scene {
public:
    Scene(script::Runner& runner, VectorArt& art) : runner(runner), art(art) {}

    // The scripts' handle on a clip: `node` itself (path "") or its inner
    // clip at `path`; the same handle while that instance lives, null if
    // there is no clip there.
    std::shared_ptr<SceneClip> clip(ArtNode* node, const std::string& path = std::string());
    // The same for a path several instances share (named clips in unnamed
    // copies of one clip): the one inside the instance `within`.
    std::shared_ptr<SceneClip> clipWithin(ArtNode* node, const std::string& path, unsigned long long within);
    // The clip a root node's own clip has as _parent (null: none).
    void setParentOf(ArtNode* root, script::ClipRef parent) { parents[root] = std::move(parent); }
    script::ClipRef parentOf(ArtNode* root) const;
    // A module's own state as a variable of `node`'s clip: read and
    // written through these (a null `set`: the scripts cannot change it).
    void bind(ArtNode* node, const std::string& name, std::function<script::Value()> get,
              std::function<void(const script::Value&)> set = nullptr);

    // After the nodes under `root` moved on this pass: the frame scripts
    // of what entered a frame, then onEnterFrame. `mouseX/Y`: the stage
    // point the scripts read as _xmouse/_ymouse.
    void update(ArtNode* root);
    float mouseX = 0, mouseY = 0;
    // At once: the scripts of the frames `node`'s clips (its own, its
    // parts') entered since they last ran (Flash runs those of a clip a
    // goto moves before the code that moved it goes on).
    void enter(ArtNode* node);
    // Lets go of the clips of nodes no longer under `root` (with their
    // variables and handlers).
    void prune(ArtNode* root);
    // The inner clip of `c` made a node of its own (with the clips above
    // it in the path), every handle on it and below it following; false if
    // it cannot be (an unnamed clip).
    bool promote(SceneClip& c);
    // The inner clip at `path` of `node` is a native module's (its scripts
    // are not run; the scripts reaching it get `to`, the module's own
    // clip); null `to`: no longer.
    void hand(ArtNode* node, const std::string& path, script::ClipRef to);

    script::Runner& runner;
    VectorArt& art;
    size_t entryCount() const { return entries.size(); }  // (debug: memory)

private:
    struct Entry {
        std::shared_ptr<SceneClip> clip;
        int frame = -1;  // the frame its script last ran for (-1: none yet)
        bool seen = false;
    };
    using Key = std::pair<unsigned long long, unsigned long long>;  // node serial, instance (0: the node's own)
    std::map<Key, Entry> entries;
    // The inner clips met by this update (prune: the others have left the
    // stage, as Flash destroyed them; placed again, they start anew).
    std::set<Key> met;
    // The nodes entered this update (enterNew: those made since run then).
    std::unordered_set<unsigned long long> entered;
    void enterNew(ArtNode* root);
    std::unordered_map<ArtNode*, script::ClipRef> parents;
    struct Binding {
        std::function<script::Value()> get;
        std::function<void(const script::Value&)> set;
    };
    std::map<std::pair<unsigned long long, std::string>, Binding> bindings;  // node serial, name
    friend class SceneClip;
    std::unordered_map<int, bool> subtreeScripted;
    std::map<std::pair<unsigned long long, std::string>, script::ClipRef> handed;  // node serial, path
    bool isHanded(ArtNode* node, const std::string& path) const;
    bool scriptedBelow(int sprite);
    void enter(Entry& e, int frame, bool allowCatchUp);
    void runFrame(Entry& e, int frame);
    void rebindUnder(ArtNode* owner, const std::string& name, ArtNode* to);
    std::vector<VectorArt::ClipVisit> visits;
    long scriptsRun = 0;  // frame scripts run so far
};
