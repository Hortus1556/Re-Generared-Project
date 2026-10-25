// The game's translated scripts (docs/engine.md, "The script index"): the
// index of a Flash game's package (scripts/index.json) run as data. Its
// actions are the mechanical part of the original ActionScript: timeline
// control, variable sets, ifs, for..in over clips, handlers whose body is
// all of these, and calls. A call reaches a native function the game's
// C++ modules register (Runner::define), or a built-in of the clip or
// the language (random, Math, ...).
//
// Values follow ActionScript 2: undefined, null, booleans, numbers,
// strings, objects (Arrays included) and movie clips, with its conversions
// and its loose ==.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/Json.h"

namespace script {

class Clip;
struct Object;
using ObjectRef = std::shared_ptr<Object>;
using ClipRef = std::shared_ptr<Clip>;

struct Value {
    enum Type { kUndefined, kNull, kBool, kNumber, kString, kObject, kClip } type = kUndefined;
    bool boolean = false;
    double number = 0;
    std::string string;
    ObjectRef object;
    ClipRef clip;

    Value() = default;
    Value(bool b) : type(kBool), boolean(b) {}
    Value(double n) : type(kNumber), number(n) {}
    Value(int n) : type(kNumber), number(n) {}
    Value(const char* s) : type(kString), string(s) {}
    Value(std::string s) : type(kString), string(std::move(s)) {}
    Value(ObjectRef o) : type(o ? kObject : kNull), object(std::move(o)) {}
    Value(ClipRef c) : type(c ? kClip : kUndefined), clip(std::move(c)) {}
    static Value null() {
        Value v;
        v.type = kNull;
        return v;
    }

    bool isUndefined() const { return type == kUndefined; }
    // ActionScript's conversions (Number(), String(), Boolean()).
    double toNumber() const;
    std::string toString() const;
    bool toBool() const;
    int toInt() const { return static_cast<int>(toNumber()); }
};

bool looseEquals(const Value& a, const Value& b);   // ==
bool strictEquals(const Value& a, const Value& b);  // ===

class Runner;

// What an engine class (Sound, XML...) does for its objects: properties it
// computes and methods it runs; false: not its own (a plain member).
struct Host {
    virtual ~Host() = default;
    virtual bool get(const std::string& /*name*/, Value& /*out*/) { return false; }
    virtual bool set(const std::string& /*name*/, const Value& /*v*/) { return false; }
    virtual bool call(Runner& /*runner*/, Object& /*self*/, const std::string& /*name*/,
                      const std::vector<Value>& /*args*/, Value& /*result*/) {
        return false;
    }
};

// An object or an Array: named members in the order they were made (for..in
// meets the newest first), an Array's items as members "0", "1"...
struct Object {
    std::shared_ptr<Host> host;  // an engine class's object
    bool isArray = false;
    // A function the scripts defined (a translated one: its actions), run
    // with `this` = what it is called on, its names looked up in `home`
    // (the timeline it was defined on).
    const json::Value* body = nullptr;
    std::vector<std::string> params;
    std::weak_ptr<Clip> home;
    std::vector<Value> items;  // an Array's
    std::vector<std::pair<std::string, Value>> members;

    Value get(const std::string& name) const;
    void set(const std::string& name, const Value& v);
    bool remove(const std::string& name);
    std::vector<std::string> keys() const;  // for..in order
};

// A movie clip the scripts can see: its variables, its properties (_x,
// _visible, _currentframe...), its child clips by instance name, its
// timeline. What a game's scene is made of decides how (SceneClip.h).
class Clip : public std::enable_shared_from_this<Clip> {
public:
    virtual ~Clip() = default;
    // A variable, property or child clip (undefined if none).
    virtual Value get(const std::string& name) = 0;
    virtual void set(const std::string& name, const Value& v) = 0;
    virtual bool remove(const std::string& name) = 0;  // delete
    virtual ClipRef parent() = 0;                       // null at the root
    virtual void gotoFrame(const Value& frame, bool play) = 0;  // a number or a label
    virtual void play() = 0;
    virtual void stop() = 0;
    virtual void nextFrame() = 0;
    virtual void prevFrame() = 0;
    // The clip's own methods (removeMovieClip, swapDepths, hitTest...):
    // false if it has no such method.
    virtual bool call(const std::string& method, const std::vector<Value>& args, Value& result) {
        return false;
    }
    // for..in: its child clips and variables, the newest first.
    virtual std::vector<std::string> keys() = 0;
    // The same clip (two handles can point at one).
    virtual bool same(const Clip& other) const = 0;
    // The sprite (character id) it shows, -1 if none; its frame (1-based).
    virtual int sprite() const = 0;
    virtual int currentFrame() const = 0;
    virtual std::string path() const { return std::string(); }  // for traces
    // Removed from the stage (removeMovieClip, or its parent removed): a
    // reference to it then reads as undefined, as AVM1 resolved one (a
    // script's `item != undefined` skips it).
    virtual bool gone() const { return false; }
};

// The package's scripts/index.json: each frame script by (sprite, frame),
// each button handler by (button, event).
class ScriptIndex {
public:
    // Reads `path` through the game's assets; empty if missing. A preload
    // of it begun earlier is waited for and taken.
    explicit ScriptIndex(const std::string& path);
    // Reads and parses `path` on a worker thread (at start, during the
    // engine's splash: Christmas's index is 3.5 MB of JSON); preloaded():
    // done (or never begun).
    static void preload(const std::string& path);
    static bool preloaded(const std::string& path);
    bool loaded() const { return !scripts.empty(); }
    // The actions of the script on that frame of that sprite (0: the main
    // timeline); null if none.
    const json::Value* frame(int sprite, int frame) const;
    // The actions run by a button's event ("release", "press",
    // "rollOver"...); null if none.
    const json::Value* button(int button, const std::string& event) const;
    // Whether the sprite has any frame script (scenes walk only those).
    bool hasScripts(int sprite) const { return scriptedSprites.count(sprite) != 0; }

private:
    json::Value root;
    std::unordered_map<long long, const json::Value*> scripts;  // sprite << 20 | frame
    std::unordered_map<std::string, const json::Value*> buttons;  // "<id>:<event>"
    std::unordered_map<int, bool> scriptedSprites;
};

class Runner;

// Where a script runs: its clip (names are looked up on it), `this` when
// a function was called on something else, and a function's locals and
// return.
struct Scope {
    ClipRef self;
    std::unordered_map<std::string, Value>* locals = nullptr;
    struct Return {
        bool done = false;
        Value value;
    };
    Return* ret = nullptr;
    Value thisValue;  // undefined: `self`
    // The innermost loop's break / continue.
    struct Loop {
        bool broke = false, continued = false;
    };
    Loop* loop = nullptr;
};

// A native function: what a game's module does for a script's call.
// `self` is the object it was called on (a clip, or the scope's clip for a
// plain call).
using Native = std::function<Value(Runner& runner, const Scope& scope, const Value& self, const std::vector<Value>& args)>;

class Runner {
public:
    explicit Runner(const ScriptIndex& index) : index(index) {}

    // Registers a function the scripts call by name, on any clip or plainly
    // (_root.ftb(...), _parent.SelectTool(...), formatScore(...)).
    void define(const std::string& name, Native fn) { natives[name] = std::move(fn); }
    bool defines(const std::string& name) const { return natives.count(name) != 0; }
    // An engine class: `new name(...)` makes its object (FlashClasses.h).
    void defineClass(const std::string& name, Native make) { classes[name] = std::move(make); }

    // The script of `clip`'s frame, entered now. `skipMarkers`: the movie
    // already plays the frame's plain stop/play/goto on the clip itself
    // (Swf's FrameAction markers), so they are not run again.
    void runFrame(const ClipRef& clip, int frame, bool skipMarkers);
    // A button's handler; `timeline` is the clip the button is in.
    bool runButton(const ClipRef& timeline, int button, const std::string& event);
    // Actions (a JSON array of the index's) with `this` = clip.
    void run(const json::Value& actions, const Scope& scope);

    Value eval(const json::Value& expr, const Scope& scope);
    // A handler set by a script (this.onEnterFrame = function () {...}),
    // run by the scene each frame (onEnterFrame) or on its event.
    void runHandler(const ClipRef& clip, const std::string& name);
    // Every live clip's onEnterFrame (once per pass, by the scene).
    void runEnterFrames();
    // A function the scripts defined, called on `self`.
    Value invoke(const Object& fn, const Value& self, const std::vector<Value>& args);
    // `self.name(args)` as a script would call it (the game's code calling
    // into the scripts: _root.ftb("levels")).
    Value callMethod(const Value& self, const std::string& name, const std::vector<Value>& args);

    ClipRef root;  // _root
    const ScriptIndex& index;
    // Calls nobody answers, said once each when tracing (AS_TRACE).
    bool traceUnknown = false;
    // Debug: the variables whose every set is printed ("score,dScore").
    std::string traceVars;

private:
    struct Handler {
        const json::Value* body;
        std::weak_ptr<Clip> clip;
    };
    Value member(const Value& obj, const std::string& name);
    void setMember(const Value& obj, const std::string& name, const Value& v, const Scope& scope);
    Value lookup(const std::string& name, const Scope& scope);
    Value call(const json::Value& fn, const std::vector<Value>& args, const Scope& scope);
    Value builtin(const std::string& name, const std::vector<Value>& args, bool& found);
    std::string key(const json::Value& nameOrExpr, const Scope& scope);
    void runOne(const json::Value& a, const Scope& scope);

    std::unordered_map<std::string, Native> natives;
    std::unordered_map<std::string, Native> classes;
    // Handlers by the clip they are on and their name.
    std::vector<std::pair<std::string, Handler>> handlers;
    std::unordered_map<std::string, bool> unknownSaid;

public:
    // The handler a clip has under `name` (null if none).
    const json::Value* handler(const Clip& clip, const std::string& name);
    void setHandler(const ClipRef& clip, const std::string& name, const json::Value* body);
    void dropHandlers(const Clip& clip);
    size_t handlerCount() const { return handlers.size(); }  // (debug: memory)
};

}  // namespace script
