#include "surgeon/Script.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <map>
#include <mutex>

#include "platform/Assets.h"
#include "platform/Platform.h"

namespace script {

// ------------------------------------------------------------------ values

double Value::toNumber() const {
    switch (type) {
    case kUndefined:
    case kNull: return NAN;
    case kBool: return boolean ? 1 : 0;
    case kNumber: return number;
    case kString: {
        const char* s = string.c_str();
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') ++s;
        if (!*s) return 0;
        char* end = nullptr;
        double d = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? static_cast<double>(std::strtol(s, &end, 16))
                                                                 : std::strtod(s, &end);
        while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) ++end;
        return end && !*end ? d : NAN;
    }
    case kObject: return object && object->isArray && object->items.size() == 1 ? object->items[0].toNumber() : NAN;
    case kClip: return NAN;
    }
    return NAN;
}

std::string Value::toString() const {
    switch (type) {
    case kUndefined: return "undefined";
    case kNull: return "null";
    case kBool: return boolean ? "true" : "false";
    case kNumber: {
        if (std::isnan(number)) return "NaN";
        if (std::isinf(number)) return number > 0 ? "Infinity" : "-Infinity";
        char b[32];
        std::snprintf(b, sizeof b, "%.15g", number);
        return b;
    }
    case kString: return string;
    case kObject: {
        if (!object || !object->isArray) return "[object Object]";
        std::string s;
        for (size_t i = 0; i < object->items.size(); ++i) {
            if (i) s += ",";
            if (!object->items[i].isUndefined()) s += object->items[i].toString();
        }
        return s;
    }
    case kClip: return clip ? clip->path() : std::string();
    }
    return std::string();
}

bool Value::toBool() const {
    switch (type) {
    case kUndefined:
    case kNull: return false;
    case kBool: return boolean;
    case kNumber: return number != 0 && !std::isnan(number);
    case kString: return !string.empty();
    case kObject: return object != nullptr;
    case kClip: return clip != nullptr && !clip->gone();
    }
    return false;
}

namespace {
bool deadClip(const Value& v) { return v.type == Value::kClip && (!v.clip || v.clip->gone()); }
}  // namespace

bool strictEquals(const Value& a, const Value& b) {
    // (a removed clip is undefined)
    if (deadClip(a) || deadClip(b))
        return (deadClip(a) || a.type == Value::kUndefined) && (deadClip(b) || b.type == Value::kUndefined);
    if (a.type != b.type) return false;
    switch (a.type) {
    case Value::kUndefined:
    case Value::kNull: return true;
    case Value::kBool: return a.boolean == b.boolean;
    case Value::kNumber: return a.number == b.number;
    case Value::kString: return a.string == b.string;
    case Value::kObject: return a.object == b.object;
    case Value::kClip: return a.clip && b.clip && a.clip->same(*b.clip);
    }
    return false;
}

bool looseEquals(const Value& a, const Value& b) {
    bool aNone = a.type == Value::kUndefined || a.type == Value::kNull || deadClip(a);
    bool bNone = b.type == Value::kUndefined || b.type == Value::kNull || deadClip(b);
    if (a.type == b.type && !aNone && !bNone) return strictEquals(a, b);
    if (aNone || bNone) return aNone && bNone;
    if (a.type == Value::kClip || b.type == Value::kClip) {
        // A clip compares with a string by its path.
        const Value& c = a.type == Value::kClip ? a : b;
        const Value& o = a.type == Value::kClip ? b : a;
        return o.type == Value::kString && c.toString() == o.string;
    }
    if (a.type == Value::kObject || b.type == Value::kObject) return false;
    return a.toNumber() == b.toNumber();
}

// ----------------------------------------------------------------- objects

Value Object::get(const std::string& name) const {
    if (isArray) {
        if (name == "length") return static_cast<double>(items.size());
        char* end = nullptr;
        long i = std::strtol(name.c_str(), &end, 10);
        if (!name.empty() && end && !*end) return i >= 0 && static_cast<size_t>(i) < items.size() ? items[i] : Value();
    }
    for (const auto& [k, v] : members)
        if (k == name) return v;
    return Value();
}

void Object::set(const std::string& name, const Value& v) {
    if (isArray) {
        char* end = nullptr;
        long i = std::strtol(name.c_str(), &end, 10);
        if (!name.empty() && end && !*end && i >= 0) {
            if (static_cast<size_t>(i) >= items.size()) items.resize(i + 1);
            items[i] = v;
            return;
        }
        if (name == "length") {
            items.resize(static_cast<size_t>(std::max(0, v.toInt())));
            return;
        }
    }
    for (auto& [k, old] : members)
        if (k == name) {
            old = v;
            return;
        }
    members.emplace_back(name, v);
}

bool Object::remove(const std::string& name) {
    for (auto it = members.begin(); it != members.end(); ++it)
        if (it->first == name) {
            members.erase(it);
            return true;
        }
    return false;
}

std::vector<std::string> Object::keys() const {
    std::vector<std::string> out;
    for (auto it = members.rbegin(); it != members.rend(); ++it) out.push_back(it->first);
    if (isArray)
        for (size_t i = items.size(); i-- > 0;) out.push_back(std::to_string(i));
    return out;
}

// ------------------------------------------------------------------- index

namespace {

long long frameKey(int sprite, int frame) { return (static_cast<long long>(sprite) << 20) | frame; }

}  // namespace

namespace {

struct ParsedIndex {
    bool found = false, ok = false;
    std::string error;
    json::Value root;
};

ParsedIndex readIndex(const std::string& path) {
    ParsedIndex r;
    std::string text;
    if (!assets::readText(path, text)) return r;
    r.found = true;
    r.ok = json::parse(text, r.root, &r.error);
    return r;
}

std::mutex gIndexMutex;
std::map<std::string, std::shared_future<std::shared_ptr<ParsedIndex>>>& indexPreloads() {
    static std::map<std::string, std::shared_future<std::shared_ptr<ParsedIndex>>> m;
    return m;
}

}  // namespace

void ScriptIndex::preload(const std::string& path) {
    std::lock_guard<std::mutex> lock(gIndexMutex);
    auto& f = indexPreloads()[path];
    if (!f.valid())
        f = std::async(std::launch::async, [path] { return std::make_shared<ParsedIndex>(readIndex(path)); }).share();
}

bool ScriptIndex::preloaded(const std::string& path) {
    std::lock_guard<std::mutex> lock(gIndexMutex);
    auto it = indexPreloads().find(path);
    return it == indexPreloads().end() || it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

ScriptIndex::ScriptIndex(const std::string& path) {
    std::shared_future<std::shared_ptr<ParsedIndex>> pending;
    {
        std::lock_guard<std::mutex> lock(gIndexMutex);
        auto it = indexPreloads().find(path);
        if (it != indexPreloads().end()) {
            pending = it->second;
            indexPreloads().erase(it);
        }
    }
    std::shared_ptr<ParsedIndex> parsed = pending.valid() ? pending.get() : std::make_shared<ParsedIndex>(readIndex(path));
    if (!parsed->found) return;
    if (!parsed->ok) {
        platform::fatal("The game's script index (" + path + ") is unreadable: " + parsed->error);
        return;
    }
    root = std::move(parsed->root);
    for (const json::Value& s : root["scripts"].items) {
        const std::string& kind = s["kind"].str();
        int owner = s["owner"].integer();
        if (kind == "frame") {
            scripts[frameKey(owner, s["frame"].integer())] = &s["actions"];
            scriptedSprites[owner] = true;
        } else if (kind == "button") {
            for (const json::Value& e : s["events"].items)
                buttons[std::to_string(owner) + ":" + e.str()] = &s["actions"];
        }
    }
}

const json::Value* ScriptIndex::frame(int sprite, int f) const {
    auto it = scripts.find(frameKey(sprite, f));
    return it == scripts.end() ? nullptr : it->second;
}

const json::Value* ScriptIndex::button(int b, const std::string& event) const {
    auto it = buttons.find(std::to_string(b) + ":" + event);
    return it == buttons.end() ? nullptr : it->second;
}

// ------------------------------------------------------------------ runner

namespace {

bool isTimeline(const std::string& d) {
    return d == "stop" || d == "play" || d == "goto" || d == "nextFrame" || d == "prevFrame";
}

Value fromLiteral(const json::Value& v) {
    switch (v.type) {
    case json::Value::kBool: return Value(v.boolean);
    case json::Value::kNumber: return Value(v.number);
    case json::Value::kString: return Value(v.string);
    default: return Value();
    }
}

}  // namespace

void Runner::runFrame(const ClipRef& clip, int frame, bool skipMarkers) {
    const json::Value* actions = clip ? index.frame(clip->sprite(), frame) : nullptr;
    if (!actions) return;
    Scope scope{clip};
    for (const json::Value& a : actions->items) {
        if (skipMarkers && isTimeline(a["do"].str()) && (a["on"][0].str() == "this" || a["on"][0].str() == "scope"))
            continue;
        runOne(a, scope);
    }
}

bool Runner::runButton(const ClipRef& timeline, int button, const std::string& event) {
    const json::Value* actions = index.button(button, event);
    if (!actions) return false;
    run(*actions, Scope{timeline});
    return true;
}

void Runner::run(const json::Value& actions, const Scope& scope) {
    for (const json::Value& a : actions.items) {
        runOne(a, scope);
        if (scope.ret && scope.ret->done) return;
        if (scope.loop && (scope.loop->broke || scope.loop->continued)) return;
    }
}

namespace {

// A loop that has not ended after this many rounds is a script waiting on
// something that never comes (it would hang Flash): given up.
constexpr int kMaxRounds = 100000;

}  // namespace

Value Runner::callMethod(const Value& self, const std::string& name, const std::vector<Value>& args) {
    Value f = member(self, name);
    if (f.type == Value::kObject && f.object->body) return invoke(*f.object, self, args);
    Value r;
    if (self.type == Value::kClip && self.clip->call(name, args, r)) return r;
    if (self.type == Value::kObject && self.object->host && self.object->host->call(*this, *self.object, name, args, r))
        return r;
    auto it = natives.find(name);
    if (it != natives.end()) return it->second(*this, Scope{self.type == Value::kClip ? self.clip : nullptr}, self, args);
    return Value();
}

Value Runner::invoke(const Object& fn, const Value& self, const std::vector<Value>& args) {
    std::unordered_map<std::string, Value> locals;
    for (size_t i = 0; i < fn.params.size(); ++i) locals[fn.params[i]] = i < args.size() ? args[i] : Value();
    Scope::Return ret;
    ClipRef home = fn.home.lock();
    Scope scope{home ? home : (self.type == Value::kClip ? self.clip : nullptr), &locals, &ret, self};
    if (fn.body) run(*fn.body, scope);
    return ret.value;
}

std::string Runner::key(const json::Value& name, const Scope& scope) {
    return name.type == json::Value::kString ? name.str() : eval(name, scope).toString();
}

void Runner::runOne(const json::Value& a, const Scope& scope) {
    const std::string& d = a["do"].str();
    if (d == "function") {
        auto f = std::make_shared<Object>();
        f->body = &a["body"];
        for (const json::Value& p : a["params"].items) f->params.push_back(p.str());
        f->home = scope.self;
        if (scope.self) scope.self->set(a["name"].str(), Value(f));
    } else if (d == "return") {
        if (scope.ret) {
            scope.ret->value = eval(a["value"], scope);
            scope.ret->done = true;
        }
    } else if (d == "set") {
        std::string name = key(a["name"], scope);
        Value v = eval(a["value"], scope);
        const std::string& op = a.has("op") ? a["op"].str() : std::string("=");
        // (a bare name: the function's local if it has one)
        bool local = scope.locals && (a["local"].flag() ||
                                      (a["on"][0].str() == "scope" && scope.locals->count(name) != 0));
        Value target = local ? Value() : eval(a["on"], scope);
        if (op != "=") {
            Value old = local ? (*scope.locals)[name] : member(target, name);
            if (op == "+=") {
                v = old.type == Value::kString || v.type == Value::kString ? Value(old.toString() + v.toString())
                                                                           : Value(old.toNumber() + v.toNumber());
            } else if (op == "-=") {
                v = old.toNumber() - v.toNumber();
            } else if (op == "*=") {
                v = old.toNumber() * v.toNumber();
            } else if (op == "/=") {
                v = old.toNumber() / v.toNumber();
            } else if (op == "%=") {
                v = std::fmod(old.toNumber(), v.toNumber());
            }
        }
        if (!traceVars.empty() && ("," + traceVars + ",").find("," + name + ",") != std::string::npos)
            std::printf("script: set %s%s = %s\n", local ? "local " : "", name.c_str(), v.toString().c_str());
        if (local) (*scope.locals)[name] = v;
        else setMember(target, name, v, scope);
    } else if (d == "goto" || d == "stop" || d == "play" || d == "nextFrame" || d == "prevFrame") {
        Value target = eval(a["on"], scope);
        if (target.type == Value::kObject && target.object->host) {
            // A host object (Sound, XML...) whose own method has the same
            // no-argument shape as a timeline action (Sound.stop(), with no
            // arguments, parses the same as MovieClip.stop()): call it
            // there instead of silently doing nothing to a non-clip.
            const char* name = d == "goto"      ? (a["play"].flag() ? "gotoAndPlay" : "gotoAndStop")
                                : d == "stop"     ? "stop"
                                : d == "play"     ? "play"
                                : d == "nextFrame" ? "nextFrame"
                                                   : "prevFrame";
            std::vector<Value> args;
            if (d == "goto") args.push_back(eval(a["frame"], scope));
            Value result;
            target.object->host->call(*this, *target.object, name, args, result);
            return;
        }
        if (target.type != Value::kClip) return;
        Clip& c = *target.clip;
        if (d == "goto") c.gotoFrame(eval(a["frame"], scope), a["play"].flag());
        else if (d == "stop") c.stop();
        else if (d == "play") c.play();
        else if (d == "nextFrame") c.nextFrame();
        else c.prevFrame();
    } else if (d == "while" || d == "for") {
        if (d == "for") run(a["init"], scope);
        Scope inner = scope;
        Scope::Loop loop;
        inner.loop = &loop;
        bool first = a["first"].flag();  // do..while
        for (int round = 0; round < kMaxRounds; ++round) {
            if (!first && !eval(a["cond"], scope).toBool()) break;
            first = false;
            loop.continued = false;
            run(a["body"], inner);
            if (loop.broke || (scope.ret && scope.ret->done)) break;
            if (d == "for") run(a["step"], scope);
        }
    } else if (d == "break" || d == "continue") {
        if (scope.loop) (d == "break" ? scope.loop->broke : scope.loop->continued) = true;
    } else if (d == "if") {
        run(eval(a["cond"], scope).toBool() ? a["then"] : a["else"], scope);
    } else if (d == "call") {
        std::vector<Value> args;
        for (const json::Value& x : a["args"].items) args.push_back(eval(x, scope));
        call(a["fn"], args, scope);
    } else if (d == "forin") {
        Value obj = eval(a["in"], scope);
        std::vector<std::string> keys;
        if (obj.type == Value::kClip) keys = obj.clip->keys();
        else if (obj.type == Value::kObject) keys = obj.object->keys();
        const std::string& name = a["name"].str();
        Scope inner = scope;
        Scope::Loop loop;
        inner.loop = &loop;
        for (const std::string& k : keys) {
            if (scope.locals) (*scope.locals)[name] = Value(k);
            else if (scope.self) scope.self->set(name, Value(k));
            loop.continued = false;
            run(a["body"], inner);
            if (loop.broke || (scope.ret && scope.ret->done)) break;
        }
    } else if (d == "handler") {
        Value target = eval(a["on"], scope);
        if (target.type == Value::kClip) {
            setHandler(target.clip, key(a["name"], scope), &a["body"]);
        } else if (target.type == Value::kObject) {
            // An object's handler (a Sound's onSoundComplete, an XML's
            // onLoad): a function kept on it, its names looked up in the
            // timeline that set it, as a function defined there.
            auto f = std::make_shared<Object>();
            f->body = &a["body"];
            for (const json::Value& p : a["params"].items) f->params.push_back(p.str());
            f->home = scope.self;
            target.object->set(key(a["name"], scope), Value(f));
        }
    } else if (d == "delete") {
        Value target = eval(a["on"], scope);
        std::string name = key(a["name"], scope);
        if (target.type == Value::kClip) {
            if (handler(*target.clip, name)) setHandler(target.clip, name, nullptr);
            else target.clip->remove(name);
        } else if (target.type == Value::kObject) {
            target.object->remove(name);
        }
    }
    // "native": done by the game's module, in its own way.
}

Value Runner::lookup(const std::string& name, const Scope& scope) {
    if (scope.locals) {
        auto it = scope.locals->find(name);
        if (it != scope.locals->end()) return it->second;
    }
    if (name == "_root" || name == "_level0") return Value(root);
    if (name == "_parent") return scope.self ? Value(scope.self->parent()) : Value();
    if (name == "this") return scope.thisValue.isUndefined() ? Value(scope.self) : scope.thisValue;
    return scope.self ? scope.self->get(name) : Value();
}

Value Runner::member(const Value& obj, const std::string& name) {
    switch (obj.type) {
    case Value::kClip: return obj.clip->get(name);
    case Value::kObject: {
        Value v;
        if (obj.object->host && obj.object->host->get(name, v)) return v;
        return obj.object->get(name);
    }
    case Value::kString:
        if (name == "length") return static_cast<double>(obj.string.size());
        return Value();
    default: return Value();
    }
}

void Runner::setMember(const Value& obj, const std::string& name, const Value& v, const Scope&) {
    if (obj.type == Value::kClip) obj.clip->set(name, v);
    else if (obj.type == Value::kObject && !(obj.object->host && obj.object->host->set(name, v))) obj.object->set(name, v);
}

Value Runner::eval(const json::Value& e, const Scope& scope) {
    const std::string& k = e[0].str();
    if (k == "lit") return fromLiteral(e[1]);
    if (k == "undef") return Value();
    if (k == "this") return scope.thisValue.isUndefined() ? Value(scope.self) : scope.thisValue;
    if (k == "scope") return Value(scope.self);
    if (k == "var") return lookup(e[1].str(), scope);
    if (k == "get") return member(eval(e[1], scope), e[2].str());
    if (k == "index") return member(eval(e[1], scope), eval(e[2], scope).toString());
    if (k == "call") {
        std::vector<Value> args;
        for (const json::Value& x : e[2].items) args.push_back(eval(x, scope));
        return call(e[1], args, scope);
    }
    if (k == "op") {
        const std::string& op = e[1].str();
        if (op == "&&") {
            Value a = eval(e[2], scope);
            return a.toBool() ? eval(e[3], scope) : a;
        }
        if (op == "||") {
            Value a = eval(e[2], scope);
            return a.toBool() ? a : eval(e[3], scope);
        }
        Value a = eval(e[2], scope), b = eval(e[3], scope);
        if (op == "+") {
            if (a.type == Value::kString || b.type == Value::kString) return Value(a.toString() + b.toString());
            return Value(a.toNumber() + b.toNumber());
        }
        if (op == "-") return Value(a.toNumber() - b.toNumber());
        if (op == "*") return Value(a.toNumber() * b.toNumber());
        if (op == "/") return Value(a.toNumber() / b.toNumber());
        if (op == "%") return Value(std::fmod(a.toNumber(), b.toNumber()));
        if (op == "==") return Value(looseEquals(a, b));
        if (op == "!=") return Value(!looseEquals(a, b));
        if (op == "===") return Value(strictEquals(a, b));
        if (op == "!==") return Value(!strictEquals(a, b));
        if (op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (a.type == Value::kString && b.type == Value::kString) {
                int c = a.string.compare(b.string);
                return Value(op == "<" ? c < 0 : op == ">" ? c > 0 : op == "<=" ? c <= 0 : c >= 0);
            }
            double x = a.toNumber(), y = b.toNumber();
            // AVM1: `<` and `>` with NaN are undefined, and `a <= b` is
            // `!(a > b)`, `a >= b` is `!(a < b)`: true then (a hit piece's
            // health first taken from undefined is NaN, and done).
            if (std::isnan(x) || std::isnan(y)) return op == "<=" || op == ">=" ? Value(true) : Value();
            return Value(op == "<" ? x < y : op == ">" ? x > y : op == "<=" ? x <= y : x >= y);
        }
        auto i32 = [](const Value& v) {
            double d = v.toNumber();
            return std::isfinite(d) ? static_cast<int>(static_cast<long long>(d)) : 0;
        };
        if (op == "&") return Value(i32(a) & i32(b));
        if (op == "|") return Value(i32(a) | i32(b));
        if (op == "^") return Value(i32(a) ^ i32(b));
        if (op == "<<") return Value(i32(a) << (i32(b) & 31));
        if (op == ">>") return Value(i32(a) >> (i32(b) & 31));
        if (op == ">>>") return Value(static_cast<double>(static_cast<unsigned>(i32(a)) >> (i32(b) & 31)));
        return Value();
    }
    if (k == "unary") {
        const std::string& op = e[1].str();
        Value a = eval(e[2], scope);
        if (op == "!") return Value(!a.toBool());
        if (op == "-") return Value(-a.toNumber());
        if (op == "+") return Value(a.toNumber());
        if (op == "~") return Value(~static_cast<int>(a.toNumber()));
        if (op == "typeof") {
            switch (a.type) {
            case Value::kUndefined: return Value("undefined");
            case Value::kNull: return Value("null");
            case Value::kBool: return Value("boolean");
            case Value::kNumber: return Value("number");
            case Value::kString: return Value("string");
            case Value::kObject: return Value("object");
            case Value::kClip: return Value("movieclip");
            }
        }
        return Value();
    }
    if (k == "cond") return eval(e[1], scope).toBool() ? eval(e[2], scope) : eval(e[3], scope);
    if (k == "array") {
        auto o = std::make_shared<Object>();
        o->isArray = true;
        for (const json::Value& x : e[1].items) o->items.push_back(eval(x, scope));
        return Value(o);
    }
    if (k == "object") {
        auto o = std::make_shared<Object>();
        for (const json::Value& m : e[1].items) o->set(m[0].str(), eval(m[1], scope));
        return Value(o);
    }
    if (k == "new") {
        std::vector<Value> args;
        for (const json::Value& x : e[2].items) args.push_back(eval(x, scope));
        auto c = classes.find(e[1].str());
        if (c != classes.end()) return c->second(*this, scope, Value(), args);
        if (e[1].str() == "Array") {
            auto o = std::make_shared<Object>();
            o->isArray = true;
            if (args.size() == 1 && args[0].type == Value::kNumber) o->items.resize(static_cast<size_t>(std::max(0, args[0].toInt())));
            else o->items = args;
            return Value(o);
        }
        if ((e[1].str() == "String" || e[1].str() == "Number" || e[1].str() == "Boolean") && !args.empty()) return args[0];
        return Value(std::make_shared<Object>());
    }
    return Value();
}

Value Runner::call(const json::Value& fn, const std::vector<Value>& args, const Scope& scope) {
    const std::string& k = fn[0].str();
    std::string name;
    Value self;
    if (k == "var") {
        name = fn[1].str();
        Value f = lookup(name, scope);
        // A plain call's `this` is the timeline the call is made on (AVM1's
        // target clip), not the caller's own `this`: a listener's method
        // calling its timeline's functions.
        if (f.type == Value::kObject && f.object->body) return invoke(*f.object, Value(scope.self), args);
        bool found = false;
        Value r = builtin(name, args, found);
        if (found) return r;
        self = Value(scope.self);
        // A method of the timeline the script is on (attachMovie(...)).
        if (scope.self && !natives.count(name) && scope.self->call(name, args, r)) return r;
    } else if (k == "get" || k == "index") {
        name = k == "get" ? fn[2].str() : eval(fn[2], scope).toString();
        if (fn[1][0].str() == "var") {
            // SharedObject.getLocal, Math.floor...: the engine's classes and
            // the language's built-in objects.
            auto n = natives.find(fn[1][1].str() + "." + name);
            if (n != natives.end()) return n->second(*this, scope, Value(), args);
            bool found = false;
            Value r = builtin(fn[1][1].str() + "." + name, args, found);
            if (found) return r;
        }
        self = eval(fn[1], scope);
        Value f = member(self, name);
        if (f.type == Value::kObject && f.object->body) return invoke(*f.object, self, args);
        if (self.type == Value::kClip) {
            Value r;
            if (self.clip->call(name, args, r)) return r;
        } else if (self.type == Value::kObject && self.object->host) {
            Value r;
            if (self.object->host->call(*this, *self.object, name, args, r)) return r;
        }
        if (self.type == Value::kObject && self.object->isArray) {
            Object& o = *self.object;
            if (name == "push") {
                for (const Value& a : args) o.items.push_back(a);
                return Value(static_cast<double>(o.items.size()));
            }
            if (name == "pop") {
                if (o.items.empty()) return Value();
                Value v = o.items.back();
                o.items.pop_back();
                return v;
            }
            if (name == "join") {
                std::string sep = args.empty() ? "," : args[0].toString(), s;
                for (size_t i = 0; i < o.items.size(); ++i) s += (i ? sep : "") + o.items[i].toString();
                return Value(s);
            }
        } else if (self.type == Value::kString) {
            const std::string& s = self.string;
            if (name == "toUpperCase" || name == "toLowerCase") {
                std::string r = s;
                for (char& c : r) c = name == "toUpperCase" ? static_cast<char>(std::toupper(c)) : static_cast<char>(std::tolower(c));
                return Value(r);
            }
            if (name == "charAt") {
                int i = args.empty() ? 0 : args[0].toInt();
                return Value(i >= 0 && i < static_cast<int>(s.size()) ? s.substr(i, 1) : std::string());
            }
            if (name == "substr" || name == "substring" || name == "slice") {
                int n = static_cast<int>(s.size());
                int a = args.empty() ? 0 : args[0].toInt();
                if (name == "substr") {
                    if (a < 0) a = std::max(0, n + a);
                    int len = args.size() > 1 ? args[1].toInt() : n;
                    return Value(a < n ? s.substr(a, std::max(0, len)) : std::string());
                }
                int b = args.size() > 1 ? args[1].toInt() : n;
                if (name == "slice") {
                    if (a < 0) a += n;
                    if (b < 0) b += n;
                }
                a = std::clamp(a, 0, n);
                b = std::clamp(b, 0, n);
                if (a > b) std::swap(a, b);
                return Value(s.substr(a, b - a));
            }
            if (name == "indexOf" || name == "lastIndexOf") {
                std::string what = args.empty() ? std::string("undefined") : args[0].toString();
                size_t at = name == "indexOf" ? s.find(what) : s.rfind(what);
                return Value(at == std::string::npos ? -1.0 : static_cast<double>(at));
            }
            if (name == "split") {
                auto o = std::make_shared<Object>();
                o->isArray = true;
                std::string sep = args.empty() ? std::string() : args[0].toString();
                if (args.empty()) {
                    o->items.push_back(Value(s));
                } else if (sep.empty()) {
                    for (char c : s) o->items.push_back(Value(std::string(1, c)));
                } else {
                    size_t from = 0, at;
                    while ((at = s.find(sep, from)) != std::string::npos) {
                        o->items.push_back(Value(s.substr(from, at - from)));
                        from = at + sep.size();
                    }
                    o->items.push_back(Value(s.substr(from)));
                }
                return Value(o);
            }
            if (name == "charCodeAt") {
                size_t i = args.empty() ? 0 : static_cast<size_t>(std::max(0, args[0].toInt()));
                return Value(i < s.size() ? static_cast<double>(static_cast<unsigned char>(s[i])) : NAN);
            }
            if (name == "toString") return Value(s);
        }
        if (self.type == Value::kObject && self.object->isArray) {
            Object& o = *self.object;
            auto clampIndex = [&](size_t i, int def) {
                int n = static_cast<int>(o.items.size());
                int v = i < args.size() ? args[i].toInt() : def;
                if (v < 0) v += n;
                return std::clamp(v, 0, n);
            };
            if (name == "shift") {
                if (o.items.empty()) return Value();
                Value v = o.items.front();
                o.items.erase(o.items.begin());
                return v;
            }
            if (name == "unshift") {
                o.items.insert(o.items.begin(), args.begin(), args.end());
                return Value(static_cast<double>(o.items.size()));
            }
            if (name == "slice") {
                auto r = std::make_shared<Object>();
                r->isArray = true;
                int a = clampIndex(0, 0), b = clampIndex(1, static_cast<int>(o.items.size()));
                if (a < b) r->items.assign(o.items.begin() + a, o.items.begin() + b);
                return Value(r);
            }
            if (name == "splice") {
                auto r = std::make_shared<Object>();
                r->isArray = true;
                int a = clampIndex(0, 0);
                int count = args.size() > 1 ? std::clamp(args[1].toInt(), 0, static_cast<int>(o.items.size()) - a)
                                            : static_cast<int>(o.items.size()) - a;
                r->items.assign(o.items.begin() + a, o.items.begin() + a + count);
                o.items.erase(o.items.begin() + a, o.items.begin() + a + count);
                if (args.size() > 2) o.items.insert(o.items.begin() + a, args.begin() + 2, args.end());
                return Value(r);
            }
            if (name == "concat") {
                auto r = std::make_shared<Object>();
                r->isArray = true;
                r->items = o.items;
                for (const Value& a : args) {
                    if (a.type == Value::kObject && a.object->isArray) r->items.insert(r->items.end(), a.object->items.begin(), a.object->items.end());
                    else r->items.push_back(a);
                }
                return Value(r);
            }
            if (name == "reverse") {
                std::reverse(o.items.begin(), o.items.end());
                return self;
            }
            if (name == "toString") return Value(self.toString());
        }
    }
    auto it = natives.find(name);
    if (it != natives.end()) return it->second(*this, scope, self, args);
    if (traceUnknown && !unknownSaid[name]) {
        unknownSaid[name] = true;
        std::printf("script: nothing answers %s()\n", name.c_str());
    }
    return Value();
}

Value Runner::builtin(const std::string& name, const std::vector<Value>& args, bool& found) {
    found = true;
    auto arg = [&](size_t i) { return i < args.size() ? args[i].toNumber() : NAN; };
    if (name == "random") {
        int n = static_cast<int>(arg(0));
        return Value(n > 0 ? std::rand() % n : 0);
    }
    if (name == "int") {
        double d = arg(0);
        return Value(std::isfinite(d) ? std::trunc(d) : 0.0);
    }
    if (name == "Number") return Value(arg(0));
    if (name == "String") return Value(args.empty() ? std::string() : args[0].toString());
    if (name == "Boolean") return Value(!args.empty() && args[0].toBool());
    if (name == "isNaN") return Value(std::isnan(arg(0)));
    if (name == "parseInt") return Value(args.empty() ? NAN : static_cast<double>(std::strtol(args[0].toString().c_str(), nullptr, args.size() > 1 ? args[1].toInt() : 10)));
    if (name == "parseFloat") return Value(args.empty() ? NAN : std::strtod(args[0].toString().c_str(), nullptr));
    if (name == "trace") {
        if (traceUnknown && !args.empty()) std::printf("trace: %s\n", args[0].toString().c_str());
        return Value();
    }
    if (name == "getTimer") return Value(static_cast<double>(platform::now() * 1000));
    if (name.rfind("Math.", 0) == 0) {
        std::string f = name.substr(5);
        double x = arg(0), y = arg(1);
        if (f == "floor") return Value(std::floor(x));
        if (f == "ceil") return Value(std::ceil(x));
        if (f == "round") return Value(std::floor(x + 0.5));
        if (f == "abs") return Value(std::fabs(x));
        if (f == "sqrt") return Value(std::sqrt(x));
        if (f == "sin") return Value(std::sin(x));
        if (f == "cos") return Value(std::cos(x));
        if (f == "atan2") return Value(std::atan2(x, y));
        if (f == "pow") return Value(std::pow(x, y));
        if (f == "min") return Value(std::fmin(x, y));
        if (f == "max") return Value(std::fmax(x, y));
        if (f == "random") return Value(std::rand() / (RAND_MAX + 1.0));
    }
    found = false;
    return Value();
}

const json::Value* Runner::handler(const Clip& clip, const std::string& name) {
    for (auto& [n, h] : handlers) {
        ClipRef c = h.clip.lock();
        if (n == name && c && c->same(clip)) return h.body;
    }
    return nullptr;
}

void Runner::setHandler(const ClipRef& clip, const std::string& name, const json::Value* body) {
    handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                  [&](const auto& p) {
                                      ClipRef c = p.second.clip.lock();
                                      return !c || (p.first == name && c->same(*clip));
                                  }),
                   handlers.end());
    if (body) handlers.push_back({name, Handler{body, clip}});
}

void Runner::dropHandlers(const Clip& clip) {
    handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                  [&](const auto& p) {
                                      ClipRef c = p.second.clip.lock();
                                      return !c || c->same(clip);
                                  }),
                   handlers.end());
}

void Runner::runEnterFrames() {
    std::vector<std::pair<ClipRef, const json::Value*>> due;
    for (auto& [n, h] : handlers)
        if (n == "onEnterFrame")
            if (ClipRef c = h.clip.lock()) due.emplace_back(c, h.body);
    for (auto& [c, body] : due) {
        if (handler(*c, "onEnterFrame") != body) continue;  // (dropped by one run before)
        std::unordered_map<std::string, Value> locals;
        run(*body, Scope{c, &locals});
    }
}

void Runner::runHandler(const ClipRef& clip, const std::string& name) {
    const json::Value* body = clip ? handler(*clip, name) : nullptr;
    if (!body) return;
    std::unordered_map<std::string, Value> locals;
    run(*body, Scope{clip, &locals});
}

}  // namespace script
