#include "surgeon/FlashClasses.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>

#include "engine/MediaPlayer.h"
#include "engine/VectorArt.h"
#include "engine/XmlReader.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

using SObject = script::Object;
using script::ObjectRef;
using script::Value;

// ------------------------------------------------------------------- Sound

struct FlashClasses::SoundHost : script::Host {
    explicit SoundHost(FlashClasses& owner) : owner(owner) {}
    ~SoundHost() override {
        if (track) {
            track->stop();
            releaseRef(track);
        }
    }
    FlashClasses& owner;
    std::string linkage;
    int volume = 100;
    bool wasPlaying = false;
    // This Sound object's own channel: Flash's Sound is per object (two
    // clips that each attachSound the same effect play independently, not
    // one swallowing the other's start()), so it is not shared by linkage.
    MediaTrack* track = nullptr;
    std::string trackLinkage;  // the linkage `track` was made for

    bool playing() const { return track && track->getMediaState() == MediaTrack::kPlaying; }

    // Every Sound call, which object (its address, short and stable for the
    // run), its linkage, and the channel's state before and after — so a
    // stuck or silent sound is read off the trace instead of re-derived by
    // hand each time. On whenever -d's own trace is (see traceEnabled()):
    // sound is reported often enough, and rare enough, to always be in it.
    static bool tracing() { return platform::traceEnabled(); }
    const char* state() const { return !track ? "none" : playing() ? "playing" : "stopped"; }

    bool call(script::Runner&, SObject&, const std::string& name, const std::vector<Value>& args, Value& result) override {
        bool traced = tracing();
        const char* before = traced ? state() : "";
        if (name == "attachSound") {
            linkage = args.empty() ? std::string() : args[0].toString();
        } else if (name == "start") {
            // start(secondOffset, loops): a new play always starts (Flash
            // does not dedupe a Sound object's own start() calls either).
            if (linkage.empty()) {
                if (traced) platform::debugTrace("sound %p start(): no attachSound yet", static_cast<void*>(this));
                return true;
            }
            if (!track || trackLinkage != linkage) {
                if (track) releaseRef(track);
                track = owner.makeTrack(linkage);
                trackLinkage = linkage;
            }
            if (!track) {
                if (traced)
                    platform::debugTrace("sound %p start('%s'): no such sound in the movie", static_cast<void*>(this),
                                         linkage.c_str());
                return true;
            }
            track->setVolume(volume);
            track->setReplayTimes(std::max(1, args.size() > 1 ? args[1].toInt() : 1));
            track->play();
            wasPlaying = true;
        } else if (name == "stop") {
            if (track) track->stop();
            else if (linkage.empty()) owner.stopAllSounds();
        } else if (name == "setVolume") {
            volume = args.empty() ? 100 : args[0].toInt();
            if (playing()) track->setVolume(volume);
        } else if (name == "getVolume") {
            result = Value(volume);
        } else if (name == "getBytesLoaded" || name == "getBytesTotal") {
            result = Value(100);
        } else {
            return false;
        }
        if (traced)
            platform::debugTrace("sound %p %s('%s'): %s -> %s", static_cast<void*>(this), name.c_str(), linkage.c_str(),
                                 before, state());
        return true;
    }
};

MediaTrack* FlashClasses::makeTrack(const std::string& linkage) {
    if (linkage.empty()) return nullptr;
    const VectorArt::Pcm* pcm = art.sound(linkage);
    return pcm ? SoundEffectTrack::initShared(pcm->samples, pcm->channels, pcm->rate) : nullptr;
}

void FlashClasses::stopAllSounds() {
    for (auto& w : sounds)
        if (ObjectRef o = w.lock()) {
            if (MediaTrack* t = static_cast<SoundHost*>(o->host.get())->track) t->stop();
        }
}

// --------------------------------------------------------------------- XML

namespace {

ObjectRef node(const std::string& name, const std::string& value, int type) {
    auto n = std::make_shared<SObject>();
    n->set("nodeType", Value(type));
    n->set("nodeName", type == 1 ? Value(name) : Value::null());
    n->set("nodeValue", type == 3 ? Value(value) : Value::null());
    n->set("attributes", Value(std::make_shared<SObject>()));
    auto kids = std::make_shared<SObject>();
    kids->isArray = true;
    n->set("childNodes", Value(kids));
    return n;
}

void append(SObject& parent, const ObjectRef& child) {
    SObject& kids = *parent.get("childNodes").object;
    if (!kids.items.empty()) {
        SObject& last = *kids.items.back().object;
        last.set("nextSibling", Value(child));
        child->set("previousSibling", kids.items.back());
    } else {
        parent.set("firstChild", Value(child));
    }
    kids.items.push_back(Value(child));
    parent.set("lastChild", Value(child));
    child->set("parentNode", Value(std::shared_ptr<SObject>(std::shared_ptr<SObject>(), &parent)));
}

// Fills `doc` with the nodes of `text`.
bool parseInto(SObject& doc, const std::string& text) {
    bool ignoreWhite = doc.get("ignoreWhite").toBool();
    auto kids = std::make_shared<SObject>();
    kids->isArray = true;
    doc.set("childNodes", Value(kids));
    doc.remove("firstChild");
    std::vector<SObject*> open{&doc};
    XmlHandlers h;
    h.startElement = [&](const std::string& name) {
        ObjectRef n = node(name, std::string(), 1);
        append(*open.back(), n);
        open.push_back(n.get());
    };
    h.endElement = [&](const std::string&) {
        if (open.size() > 1) open.pop_back();
    };
    h.characters = [&](const std::string& s) {
        if (ignoreWhite && s.find_first_not_of(" \t\r\n") == std::string::npos) return;
        append(*open.back(), node(std::string(), s, 3));
    };
    bool ok = parseXml(text, h);
    doc.set("loaded", Value(ok));
    return ok;
}

}  // namespace

struct FlashClasses::XmlHost : script::Host {
    explicit XmlHost(FlashClasses& owner) : owner(owner) {}
    FlashClasses& owner;
    std::weak_ptr<SObject> self;
    bool call(script::Runner&, SObject& doc, const std::string& name, const std::vector<Value>& args, Value&) override {
        if (name == "parseXML") {
            parseInto(doc, args.empty() ? std::string() : args[0].toString());
        } else if (name == "load") {
            // A file of the game's package (data/ first, as the games keep
            // what they loaded from their server there).
            std::string url = args.empty() ? std::string() : args[0].toString(), text;
            bool ok = assets::readText("data/" + url, text) || assets::readText(url, text);
            if (ok) ok = parseInto(doc, text);
            owner.loaded.emplace_back(self, ok);
        } else {
            return false;
        }
        return true;
    }
};

// ------------------------------------------------------------ SharedObject

struct FlashClasses::SharedHost : script::Host {
    SharedHost(FlashClasses& owner, std::string name) : owner(owner), name(std::move(name)) {}
    FlashClasses& owner;
    std::string name;
    bool call(script::Runner&, SObject& self, const std::string& method, const std::vector<Value>&, Value& result) override {
        Value data = self.get("data");
        if (method == "flush") {
            if (data.type == Value::kObject) owner.saveShared(name, *data.object);
            result = Value(true);
        } else if (method == "clear") {
            self.set("data", Value(std::make_shared<SObject>()));
            owner.saveShared(name, *self.get("data").object);
        } else {
            return false;
        }
        return true;
    }
};

namespace {

std::string fileOf(const std::map<std::string, FlashClasses::SaveFormat>& formats, const std::string& name) {
    auto f = formats.find(name);
    return platform::prefPath() + (f != formats.end() && !f->second.file.empty() ? f->second.file : name + ".txt");
}

}  // namespace

void FlashClasses::loadShared(const std::string& name, SObject& data) {
    SDL_IOStream* f = SDL_IOFromFile(fileOf(formats, name).c_str(), "rb");
    if (!f) return;
    std::string text;
    char buf[4096];
    size_t got;
    while ((got = SDL_ReadIO(f, buf, sizeof buf)) > 0) text.append(buf, got);
    SDL_CloseIO(f);
    auto fmt = formats.find(name);
    if (fmt != formats.end() && !fmt->second.marker.empty()) data.set(fmt->second.marker, Value(0));
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), raw = line.substr(eq + 1);
        if (fmt != formats.end())
            for (const auto& [dataKey, fileKey] : fmt->second.renames)
                if (key.compare(0, fileKey.size(), fileKey) == 0) {
                    key = dataKey + key.substr(fileKey.size());
                    break;
                }
        Value v(raw);
        char* e = nullptr;
        double d = std::strtod(raw.c_str(), &e);
        if (raw == "true" || raw == "false") v = Value(raw == "true");
        else if (!raw.empty() && e && !*e) v = Value(d);
        data.set(key, v);
    }
}

void FlashClasses::saveShared(const std::string& name, const SObject& data) {
    auto fmt = formats.find(name);
    std::string out;
    for (const auto& [k, v] : data.members) {
        if (v.isUndefined() || v.type == Value::kNull || v.type == Value::kObject || v.type == Value::kClip) continue;
        if (fmt != formats.end() && k == fmt->second.marker) continue;
        std::string key = k;
        if (fmt != formats.end())
            for (const auto& [dataKey, fileKey] : fmt->second.renames)
                if (key.compare(0, dataKey.size(), dataKey) == 0) {
                    key = fileKey + key.substr(dataKey.size());
                    break;
                }
        out += key + "=" + v.toString() + "\n";
    }
    if (SDL_IOStream* f = SDL_IOFromFile(fileOf(formats, name).c_str(), "wb")) {
        SDL_WriteIO(f, out.data(), out.size());
        SDL_CloseIO(f);
    }
    if (platform::traceEnabled()) {  // Debug (AS_TRACE): what the scripts saved, for automated tests
        std::string line = out;
        std::replace(line.begin(), line.end(), '\n', ' ');
        platform::debugTrace("saved %s: %s", fileOf(formats, name).c_str(), line.c_str());
    }
}

// ----------------------------------------------------------------- set up

FlashClasses::FlashClasses(script::Runner& r, VectorArt& a) : runner(r), art(a) {
    runner.defineClass("Sound", [this](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>&) {
        auto o = std::make_shared<SObject>();
        o->host = std::make_shared<SoundHost>(*this);
        sounds.push_back(o);
        return Value(o);
    });
    runner.defineClass("XML", [this](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>& args) {
        auto o = std::make_shared<SObject>();
        auto host = std::make_shared<XmlHost>(*this);
        host->self = o;
        o->host = host;
        if (!args.empty()) parseInto(*o, args[0].toString());
        return Value(o);
    });
    runner.define("SharedObject.getLocal",
                  [this](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>& args) {
                      std::string name = args.empty() ? std::string() : args[0].toString();
                      ObjectRef& so = shared[name];
                      if (!so) {
                          so = std::make_shared<SObject>();
                          so->host = std::make_shared<SharedHost>(*this, name);
                          auto data = std::make_shared<SObject>();
                          loadShared(name, *data);
                          so->set("data", Value(data));
                      }
                      return Value(so);
                  });
    runner.define("stopAllSounds", [this](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>&) {
        stopAllSounds();
        return Value();
    });
    // (the browser, the player's own pointer and right-click menu: none here)
    for (const char* noop : {"getURL", "fscommand", "Mouse.hide", "Mouse.show", "hideBuiltInItems"})
        runner.define(noop, [](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>&) { return Value(); });
    // Mouse and Key: their listeners.
    for (auto [cls, list] : {std::pair{"Mouse", &mouseListeners}, std::pair{"Key", &keyListeners}}) {
        runner.define(std::string(cls) + ".addListener",
                      [list](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>& args) {
                          if (!args.empty() && std::none_of(list->begin(), list->end(), [&](const Value& l) {
                                  return script::strictEquals(l, args[0]);
                              }))
                              list->push_back(args[0]);
                          return Value();
                      });
        runner.define(std::string(cls) + ".removeListener",
                      [list](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>& args) {
                          if (args.empty()) return Value(false);
                          size_t before = list->size();
                          list->erase(std::remove_if(list->begin(), list->end(),
                                                     [&](const Value& l) { return script::strictEquals(l, args[0]); }),
                                      list->end());
                          return Value(list->size() != before);
                      });
    }
    runner.define("Key.getCode", [this](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>&) {
        return Value(lastKey);
    });
    runner.define("Key.isDown", [this](script::Runner&, const script::Scope&, const Value&, const std::vector<Value>& args) {
        return Value(!args.empty() && keysDown.count(args[0].toInt()) != 0);
    });
}

void FlashClasses::broadcast(const std::vector<Value>& listeners, const char* name) {
    std::vector<Value> now = listeners;  // (a handler may add or remove some)
    for (const Value& l : now) {
        Value f = l.type == Value::kObject ? l.object->get(name) : l.type == Value::kClip ? l.clip->get(name) : Value();
        if (f.type == Value::kObject && f.object->body) runner.invoke(*f.object, l, {});
    }
}

void FlashClasses::mouseEvent(const char* name) { broadcast(mouseListeners, name); }

void FlashClasses::keyEvent(int code, bool down) {
    lastKey = code;
    if (down) keysDown.insert(code);
    else keysDown.erase(code);
    broadcast(keyListeners, down ? "onKeyDown" : "onKeyUp");
}

FlashClasses::~FlashClasses() = default;

void FlashClasses::update() {
    // Sounds that finished: their onSoundComplete.
    sounds.erase(std::remove_if(sounds.begin(), sounds.end(), [](const auto& w) { return w.expired(); }), sounds.end());
    std::vector<ObjectRef> done;
    for (auto& w : sounds) {
        ObjectRef o = w.lock();
        auto* h = static_cast<SoundHost*>(o->host.get());
        if (h->track) h->track->update();
        if (h->wasPlaying && !h->playing()) {
            h->wasPlaying = false;
            done.push_back(o);
            if (h->tracing())
                platform::debugTrace("sound %p ('%s') completed, has onSoundComplete: %d (%s)", static_cast<void*>(h),
                                     h->linkage.c_str(), o->get("onSoundComplete").type == Value::kObject,
                                     h->track ? h->track->debugState().c_str() : "no track");
        }
    }
    for (const ObjectRef& o : done) {
        Value f = o->get("onSoundComplete");
        if (f.type == Value::kObject && f.object->body) runner.invoke(*f.object, Value(o), {});
    }
    // XML loads answered.
    auto answered = std::move(loaded);
    loaded.clear();
    for (auto& [w, ok] : answered)
        if (ObjectRef o = w.lock()) {
            Value f = o->get("onLoad");
            if (f.type == Value::kObject && f.object->body) runner.invoke(*f.object, Value(o), {Value(ok)});
        }
}
