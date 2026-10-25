#include "surgeon/OperationProbe.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "platform/Platform.h"
#include "surgeon/StageScreen.h"

using script::ClipRef;
using script::Value;

namespace {

constexpr int kGameFrame = 3;  // its frame that plays the operation

// A clip's inner clip (`sub`, a path in it), as the node holding it now
// and the path left in that node.
struct At {
    ArtNode* node;
    std::string path;
    flash::Rect bounds() { return node->bounds(path); }
    bool there() { return path.empty() || node->hasPart(path); }
    // A point of its own (its pixels) on the stage.
    void toStage(float px, float py, float& sx, float& sy) {
        sx = px;
        sy = py;
        if (path.empty()) node->localToGlobal(sx, sy);
        else node->partToStage(path, px, py, sx, sy);
    }
};

At at(SceneClip& c, const std::string& sub) {
    std::string p = c.part.empty() ? sub : sub.empty() ? c.part : c.part + "." + sub;
    ArtNode* n = c.node->through(p);
    return {n, p};
}

std::shared_ptr<SceneClip> sceneClip(const Value& v) {
    return v.type == Value::kClip ? std::dynamic_pointer_cast<SceneClip>(v.clip) : nullptr;
}

double number(script::Clip& c, const char* name) {
    double d = c.get(name).toNumber();
    return std::isnan(d) ? 0 : d;
}

bool isCritter(const std::string& make) {
    for (const char* m : {"ant", "queenAnt", "spider", "queenSpider", "scorpion", "queenScorpion", "movingLeech", "centipede",
                          "tapeworm", "evilTapeworm"})
        if (make == m) return true;
    return false;
}

bool isPoison(const std::string& make) {
    for (const char* m : {"poison", "metalPoison", "sugar", "coffee", "advancedPoison"})
        if (make == m) return true;
    return false;
}

}  // namespace

std::shared_ptr<SceneClip> OperationProbe::game() {
    auto g = sceneClip(stage.root->get("game"));
    return g && g->sprite() == operationSprite ? g : nullptr;
}

bool OperationProbe::running() {
    auto g = game();
    return g && g->currentFrame() == kGameFrame;
}

void OperationProbe::update() {
    auto g = game();
    bool onGame = g && g->currentFrame() == kGameFrame;
    // (the won and lost frames' scripts may take the main timeline on in
    // the pass the operation ends: it is told when the game frame is left)
    if (playing && !onGame && platform::traceEnabled()) traceEnd(g.get());
    playing = onGame;
    if (!onGame) {
        known.clear();
        missionStage = bodyLevel = -1;
        return;
    }
    if (g->part.empty() && g->node->serial() != boundNode) {
        // A death is final: once the heart monitor flatlines (and not
        // because the heart was only stopped, which the battery undoes),
        // nothing raises the health again. (The original let Pixie dust
        // put in during the flatline call it off, its monitor put back by
        // CheckDamage: the patient died over and over.)
        boundNode = g->node->serial();
        health = number(*g, "health");
        std::weak_ptr<SceneClip> clip = g;
        auto dying = [clip] {
            auto c = clip.lock();
            if (!c || c->get("paralyzed").toBool()) return false;
            Value display = c->get("display");
            Value watch = display.type == Value::kClip ? display.clip->get("jogwatch") : Value();
            return watch.type == Value::kClip && watch.clip->currentFrame() == 5;
        };
        stage.scene.bind(g->node, "health", [this] { return Value(health); },
                         [this, dying](const Value& v) {
                             double h = v.toNumber();
                             if (h > health && dying()) return;
                             health = h;
                         });
    }
    if (godMode && number(*g, "health") < 50) {
        g->set("health", Value(50));
        g->set("dHealth", Value(50));
    }
    // (and the clock: a minute left at least; timeLeft counts frames, 25 a
    // second, and a patient with no time limit has none)
    if (godMode && g->get("timeLeft").type == Value::kNumber && number(*g, "timeLeft") < 25 * 60)
        g->set("timeLeft", Value(25.0 * 60));
    if (platform::traceEnabled()) trace(*g);
}

void OperationProbe::traceEnd(SceneClip* g) {
    // Its won or lost frame if it is still there; the scripts' result on
    // _root anyway: won, lscore and ltime, or the cause of death.
    SceneClip& root = *stage.root;
    VectorArt& art = stage.scene.art;
    int frame = g ? g->currentFrame() : 0;
    std::string cause = root.get("cause").toString();
    bool won = frame ? frame == art.frameOf(operationSprite, "won") : root.get("won").toBool();
    if (frame && frame != art.frameOf(operationSprite, "won") && frame != art.frameOf(operationSprite, "lost")) return;
    if (won)
        platform::debugTrace("mission won: score %.0f time %.0f", number(root, "lscore"), number(root, "ltime"));
    else if (!cause.empty() && cause != "undefined")
        platform::debugTrace("patient lost (%s)", cause.c_str());
}

void OperationProbe::traceInjury(SceneClip& c, Known& k, const char* what) {
    // Where it and its hit pieces are, in stage pixels, for automated tests
    // to aim at.
    At glass = at(c, "holder.glass");
    flash::Rect g = glass.there() ? glass.bounds() : flash::Rect{1, 1, 0, 0};
    if (g.empty()) g = {0, 0, 0, 0};
    platform::debugTrace(
        "injury %s %s '%s' stage %d at %.0f,%.0f rot %.0f glass %.0f,%.0f level %d glassbox %.0f,%.0f,%.0f,%.0f id %d", what,
        k.make.c_str(), k.frame.c_str(), k.stage, k.x, k.y, number(c, "_rotation"), (g.xMin + g.xMax) / 2,
        (g.yMin + g.yMax) / 2, k.level, g.xMin, g.yMin, g.xMax, g.yMax, k.id);
    At holder = at(c, "holder");
    if (holder.there())
        platform::debugTrace("  holder frame %d", holder.path.empty() ? holder.node->frame() : holder.node->partFrame(holder.path));
    At hit = at(c, "holder.hit");
    if (!hit.there()) return;
    // (a tool reaches it where its cursor's hit meets this box)
    flash::Rect box = hit.bounds();
    if (!box.empty()) platform::debugTrace("  hitbox %.0f,%.0f,%.0f,%.0f", box.xMin, box.yMin, box.xMax, box.yMax);
    std::vector<std::string> pieces = hit.node->partChildren(hit.path);
    for (const std::string& h : pieces) {
        flash::Rect r = at(c, "holder.hit." + h).bounds();
        if (!r.empty()) platform::debugTrace("  hit %s %.0f,%.0f", h.c_str(), (r.xMin + r.xMax) / 2, (r.yMin + r.yMax) / 2);
    }
    if (pieces.empty()) {  // one drawn hit area: its middle
        flash::Rect r = hit.bounds();
        if (!r.empty()) platform::debugTrace("  hit hit %.0f,%.0f", (r.xMin + r.xMax) / 2, (r.yMin + r.yMax) / 2);
    }
    // Loose pieces to carry to the tray (those still out).
    for (int n = 1; n <= 10; ++n) {
        std::string name = "holder.loosePiece" + std::to_string(n);
        At piece = at(c, name);
        if (piece.path.empty() || !piece.there()) continue;
        Value v = c.get("holder");
        auto clip = v.type == Value::kClip ? v.clip : nullptr;
        Value p = clip ? clip->get("loosePiece" + std::to_string(n)) : Value();
        if (p.type == Value::kClip && (!p.clip->get("_visible").toBool() || p.clip->get("finished").toBool())) continue;
        flash::Rect b = piece.bounds();
        if (!b.empty()) platform::debugTrace("  loose %d %.0f,%.0f", n, (b.xMin + b.xMax) / 2, (b.yMin + b.yMax) / 2);
    }
    // A puzzle's pieces (ribs) and the slots they go to.
    for (int n = 1; n <= 11; ++n) {
        At slot = at(c, "holder.r" + std::to_string(n)), rib = at(c, "holder.rib" + std::to_string(n));
        if (slot.path.empty() || !slot.there() || !rib.there()) continue;
        float sx, sy, rx, ry;
        slot.toStage(0, 0, sx, sy);
        flash::Rect b = rib.bounds();
        // (its middle, as the tongs carry it by its registration point)
        float w = rib.path.empty() ? 0 : rib.node->partWidth(rib.path), h = rib.path.empty() ? 0 : rib.node->partHeight(rib.path);
        At holder = at(c, "holder");
        float px = rib.path.empty() ? 0 : rib.node->partX(rib.path), py = rib.path.empty() ? 0 : rib.node->partY(rib.path);
        holder.toStage(px + w / 2, py + h / 2, rx, ry);
        platform::debugTrace("  piece %d at %.0f,%.0f slot %.0f,%.0f box %.0f,%.0f,%.0f,%.0f", n, rx, ry, sx, sy, b.xMin,
                             b.yMin, b.xMax, b.yMax);
    }
}

void OperationProbe::trace(SceneClip& g) {
    int s = static_cast<int>(number(g, "stage")), b = static_cast<int>(number(g, "bodyLevel"));
    if (s != missionStage || b != bodyLevel) {
        missionStage = s;
        bodyLevel = b;
        platform::debugTrace("mission stage %d body level %d", s, b);
    }
    bool p = g.get("paralyzed").toBool();
    if (p != paralyzed) {
        paralyzed = p;
        platform::debugTrace(p ? "heart stopped" : "heart started");
    }

    // The injuries: new ones, a stage on, gone.
    for (auto& [c, k] : known) k.seen = false;
    for (int level = 1; level <= 3; ++level) {
        Value array = g.get("injury" + std::to_string(level) + "Array");
        if (array.type != Value::kObject || !array.object->isArray) continue;
        for (const Value& item : array.object->items) {
            auto c = sceneClip(item);
            if (!c) continue;
            Known& k = known[c.get()];
            if (k.id && k.clip.lock() != c) k = Known{};  // (another clip where a gone one was)
            bool fresh = k.id == 0;
            if (fresh) {
                k.id = nextId++;
                k.clip = c;
                k.make = c->get("make").toString();
            }
            k.seen = true;
            int st = static_cast<int>(number(*c, "currentStage"));
            Value fr = c->get("frame");
            std::string frameName = fr.isUndefined() ? std::to_string(c->currentFrame()) : fr.toString();
            float x, y;
            at(*c, "").toStage(0, 0, x, y);
            bool moved = std::fabs(x - k.x) >= 1 || std::fabs(y - k.y) >= 1;
            k.x = x;
            k.y = y;
            k.level = level;
            k.frame = frameName;
            double health = number(*c, "health");
            if (!fresh && isPoison(k.make) && health < k.health)
                platform::debugTrace("cut through %s to %.0f id %d", k.make.c_str(), health, k.id);
            k.health = health;
            At holder = at(*c, "holder");
            int hf = holder.path.empty() ? holder.node->frame() : holder.node->partFrame(holder.path);
            if (!fresh && hf != k.holderFrame && st == k.stage)
                platform::debugTrace("injury %d holder on frame %d (stage %d)", k.id, hf, st);
            k.holderFrame = hf;
            if (fresh || st != k.stage) {
                k.stage = st;
                traceInjury(*c, k, fresh ? "created" : "stage");
            } else if (moved && isCritter(k.make)) {
                platform::debugTrace("critter %d at %.0f,%.0f", k.id, x, y);
            }
        }
    }
    for (auto it = known.begin(); it != known.end();) {
        if (it->second.seen && !it->second.clip.expired()) {
            ++it;
            continue;
        }
        const Known& k = it->second;
        platform::debugTrace("injury removed %s '%s' stage %d at %.0f,%.0f rot 0 glass 0,0 level %d glassbox 0,0,0,0 id %d",
                             k.make.c_str(), k.frame.c_str(), k.stage, k.x, k.y, k.level, k.id);
        it = known.erase(it);
    }

    // A tool pressed on the patient (the scripts' mousePressed).
    bool down = g.get("mousePressed").toBool();
    if (down && !pressed) {
        platform::debugTrace("tool %s down at %.0f,%.0f", g.get("selectedTool").toString().c_str(), stage.scene.mouseX,
                             stage.scene.mouseY);
        platform::debugTrace("    health %.1f", number(g, "health"));
        platform::debugTrace("    vacuum blood %.0f", number(g, "vacuumBlood"));
        if (auto cursor = sceneClip(g.get("cursor"))) {  // (what the tools test: the cursor's hit)
            flash::Rect r = at(*cursor, "hit").bounds();
            platform::debugTrace("    cursor hit %.0f,%.0f,%.0f,%.0f", r.xMin, r.yMin, r.xMax, r.yMax);
        }
        // Where the blood is (the vacuum takes it up within 150 pixels of
        // where it starts): the middle of the drops, and how many.
        Value blood = g.get("bloodArray");
        float sx = 0, sy = 0;
        double px = 0, py = 0;
        int count = 0;
        if (blood.type == Value::kObject)
            for (const Value& drop : blood.object->items)
                if (auto d = sceneClip(drop); d && !d->gone()) {
                    float x, y;
                    at(*d, "").toStage(0, 0, x, y);
                    sx += x;
                    sy += y;
                    px += number(*d, "_x");
                    py += number(*d, "_y");
                    ++count;
                }
        if (count)
            platform::debugTrace("    blood at %.0f,%.0f count %d (scripts' %.0f,%.0f; mouse %.0f,%.0f)", sx / count, sy / count,
                                 count, px / count, py / count, number(g, "_xmouse"), number(g, "_ymouse"));
    }
    pressed = down;

    // The corkscrew's or vacuum's moving hit.
    if (auto follow = sceneClip(g.get("followHolder")))
        if (auto mc = sceneClip(follow->get("followMc"))) {
            flash::Rect r = at(*mc, "hit").bounds();
            if (!r.empty())
                platform::debugTrace("follow hit %.0f,%.0f left %.0f", (r.xMin + r.xMax) / 2, (r.yMin + r.yMax) / 2,
                                     number(*mc, "health"));
        }

    // An organ put on the tray, for the injury it goes in; and one carried
    // from there let go of: on its place's hit area (`in`: put in, as the
    // pies one by one, which move no stage on) or not.
    std::shared_ptr<SceneClip> now;
    if (auto holder = sceneClip(g.get("glassHolder"))) now = sceneClip(holder->get("glass"));
    if (!organMake.empty() && organ.lock() != now) {
        float mx = stage.scene.mouseX, my = stage.scene.mouseY;
        bool in = mx >= organHit.xMin - 5 && mx <= organHit.xMax + 5 && my >= organHit.yMin - 5 && my <= organHit.yMax + 5;
        platform::debugTrace("dropped %s at %.0f,%.0f on its hit %.0f,%.0f-%.0f,%.0f: %s", organMake.c_str(), mx, my,
                             organHit.xMin, organHit.yMin, organHit.xMax, organHit.yMax, in ? "in" : "missed");
        organMake.clear();
    }
    if (now) {
        if (auto ref = sceneClip(now->get("ref"))) {
            flash::Rect h = at(*ref, "holder.hit").bounds();
            if (!h.empty()) {
                organMake = ref->get("make").toString();
                organHit = h;
            }
        }
    }
    if (auto holder = sceneClip(g.get("glassHolder")))
        if (auto glass = sceneClip(holder->get("glass"))) {
            if (organ.lock() != glass) {
                organ = glass;
                auto ref = sceneClip(glass->get("ref"));
                flash::Rect r = at(*glass, "").bounds();
                float rx = 0, ry = 0;
                if (ref) at(*ref, "").toStage(0, 0, rx, ry);
                platform::debugTrace("organ %d on the tray at %.0f,%.0f for %s at %.0f,%.0f", glass->currentFrame(),
                                     (r.xMin + r.xMax) / 2, (r.yMin + r.yMax) / 2,
                                     ref ? ref->get("make").toString().c_str() : "nothing", rx, ry);
            }
        }
}

// ------------------------------------------------------------ debug acts

void OperationProbe::skipStage() {
    auto g = game();
    if (!g || g->currentFrame() != kGameFrame) return;
    Value array = g->get("injury" + std::to_string(static_cast<int>(number(*g, "bodyLevel"))) + "Array");
    if (array.type != Value::kObject) return;
    std::vector<Value> now = array.object->items;  // (RemoveInjury takes them out)
    for (const Value& inj : now) stage.runner.callMethod(Value(ClipRef(g)), "RemoveInjury", {inj, Value(true)});
}

void OperationProbe::win() {
    auto g = game();
    if (!g || g->currentFrame() != kGameFrame) return;
    // On the last stage with nothing left: the scripts' own end (CheckEnd).
    Value mission = g->get("missionArray" + std::to_string(static_cast<int>(number(*g, "level"))));
    if (mission.type == Value::kObject) g->set("stage", Value(static_cast<double>(mission.object->items.size())));
    for (int level = 1; level <= 3; ++level) {
        Value array = g->get("injury" + std::to_string(level) + "Array");
        if (array.type != Value::kObject) continue;
        std::vector<Value> now = array.object->items;
        for (const Value& inj : now) stage.runner.callMethod(Value(ClipRef(g)), "RemoveInjury", {inj, Value(true)});
    }
    if (g->currentFrame() == kGameFrame) stage.runner.callMethod(Value(ClipRef(g)), "CheckEnd", {});
}

void OperationProbe::heal() {
    if (auto g = game()) {
        g->set("health", Value(100));
        g->set("dHealth", Value(100));
    }
}

void OperationProbe::kill() {
    if (auto g = game()) {
        g->set("health", Value(0));
        g->set("dHealth", Value(0));
        // (as damage does: the scripts' own check starts the death)
        stage.runner.callMethod(Value(ClipRef(g)), "CheckDamage", {});
    }
}

void OperationProbe::addTime(int seconds) {
    if (auto g = game()) g->set("timeLeft", Value(number(*g, "timeLeft") + seconds * 25.0));  // (frames)
}

