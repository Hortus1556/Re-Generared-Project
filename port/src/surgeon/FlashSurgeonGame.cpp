#include "surgeon/FlashSurgeonGame.h"

#include <cstdlib>

#include "platform/Platform.h"
#include "surgeon/OperationProbe.h"
#include "surgeon/StageScreen.h"
#include "surgeon/SymbolViewer.h"

using script::ClipRef;
using script::Value;

namespace {
double gLaunched = 0, gReady = 0;  // when preload() began, when it was done (debug: start-up times)
}  // namespace

void FlashSurgeonGame::preload() {
    gLaunched = platform::now();
    VectorArt::preload("library.json");
    script::ScriptIndex::preload("scripts/index.json");
}

bool FlashSurgeonGame::preloaded() {
    bool done = VectorArt::preloaded("library.json") && script::ScriptIndex::preloaded("scripts/index.json");
    if (done && !gReady) gReady = platform::now();
    return done;
}

FlashSurgeonGame::FlashSurgeonGame(Setup s) : SurgeonGame("library.json", {}), setup(std::move(s)) {
    debugMenu().labelText = setup.labelText;
    noHandCursor = setup.noHandCursor;
    for (const std::string& f : setup.flags) flags.emplace_back(f, false);  // (all made before they are bound)
    if (SymbolViewer* v = SymbolViewer::fromEnvironment(*this)) {
        show(v);
        return;
    }
    double built = platform::now();
    startStage();
    if (platform::traceEnabled())
        platform::debugTrace("startup: content read in %.0f ms (during the splash), game set up %.0f ms after launch, "
                             "first screen built in %.0f ms",
                             ((gReady ? gReady : built) - gLaunched) * 1000, (built - gLaunched) * 1000,
                             (platform::now() - built) * 1000);
}

FlashSurgeonGame::~FlashSurgeonGame() = default;

void FlashSurgeonGame::startStage() {
    stage_ = new StageScreen(*this);
    StageScreen& s = *stage_;
    probe = std::make_unique<OperationProbe>(s, setup.operationSprite);
    s.afterUpdate = [this] { probe->update(); };
    s.classes.setSaveFormat(setup.sharedObject, setup.saveFormat);
    s.transitionTarget = "fTarg";   // (ftb and bandage)
    s.pointerClip = "game.cursor";  // (the operation's: the tool in hand)
    s.pointerHit = "hit";
    for (int c : setup.hiddenCharacters) s.stage->hideCharacter(c);
    if (setup.extend) setup.extend(s);

    // What the scripts keep on _root that the port reads or sets (the debug
    // menu's patient, the save): one place, these fields.
    auto bindBool = [&](const char* name, bool& field) {
        s.scene.bind(s.stage, name, [&field] { return Value(field); }, [&field](const Value& v) { field = v.toBool(); });
    };
    auto bindNumber = [&](const char* name, auto& field) {
        s.scene.bind(s.stage, name, [&field] { return Value(static_cast<double>(field)); },
                     [&field](const Value& v) { field = static_cast<std::decay_t<decltype(field)>>(v.toNumber()); });
    };
    bindNumber("level", level);
    bindNumber("curSpeech", curSpeech);
    bindNumber("scene", scene);
    bindNumber("lscore", lscore);
    bindNumber("ltime", ltime);
    bindBool("won", won);
    bindBool("story", story);
    bindBool("organPlaced", organPlaced);
    bindBool("dontAllowInjury", dontAllowInjury);
    bindBool("ktg", ktg);
    bindBool("hitMenu", hitMenu);
    for (auto& [name, value] : flags) bindBool(name.c_str(), value);
    s.scene.bind(s.stage, "cause", [this] { return Value(cause); },
                 [this](const Value& v) { cause = v.isUndefined() ? std::string() : v.toString(); });

    // The operation's progress: the scripts' _root.level<n>, score<n>,
    // timer<n>, and their SharedObject (_root.user), as the Flash
    // operation's won and lost frames kept them.
    save.loader = [this](SaveData& d) {
        for (int i = 1; i <= SaveData::kLevels; ++i) {
            std::string n = std::to_string(i);
            Value g = stage_->root->get("level" + n);
            d.grade[i] = g.isUndefined() ? 0 : g.toInt();
            d.score[i] = stage_->root->get("score" + n).toNumber();
            d.timer[i] = stage_->root->get("timer" + n).toNumber();
            if (d.score[i] != d.score[i]) d.score[i] = 0;  // (NaN: unset)
            if (d.timer[i] != d.timer[i]) d.timer[i] = 0;
        }
    };
    save.saver = [this](const SaveData& d) {
        SceneClip& root = *stage_->root;
        Value user = root.get("user");
        Value data = user.type == Value::kObject ? user.object->get("data") : Value();
        for (int i = 1; i <= SaveData::kLevels; ++i) {
            std::string n = std::to_string(i);
            Value g = d.grade[i] ? Value(d.grade[i]) : Value();
            root.set("level" + n, g);
            root.set("score" + n, Value(d.score[i]));
            root.set("timer" + n, Value(d.timer[i]));
            if (data.type == Value::kObject) {
                data.object->set("level" + n, g);
                if (d.score[i]) data.object->set("score" + n, Value(d.score[i]));
                if (d.timer[i]) data.object->set("timer" + n, Value(d.timer[i]));
            }
        }
        root.set("score", Value(d.totalScore()));
        root.set("timer", Value(d.totalTimer()));
        if (user.type == Value::kObject) stage_->runner.callMethod(user, "flush", {});
        if (platform::traceEnabled())
            platform::debugTrace("progress saved: level %d grade %d score %.0f%s", level, d.grade[level], d.score[level],
                                 user.type == Value::kObject ? "" : " (no SharedObject yet)");
    };

    // The port's sound settings go through the scripts' sound manager.
    SoundBank::Scripts calls;
    calls.play = [this](const std::string& name, int times) {
        stage_->runner.callMethod(Value(ClipRef(stage_->root)), "playSound", {Value(times), Value(name)});
    };
    calls.stop = [this](const std::string& name) {
        stage_->runner.callMethod(Value(ClipRef(stage_->root)), "stopSound", {Value(name)});
    };
    calls.musicOn = [this] { return stage_->root->get("musicOn").toBool(); };
    calls.effectsOn = [this] { return stage_->root->get("soundOn").toBool(); };
    calls.enable = [this](bool music, bool effects) {
        auto press = [this](int button) { stage_->runner.runButton(std::make_shared<VarClip>(), button, "release"); };
        if (music != sounds().musicOn()) press(music ? setup.musicOn : setup.musicOff);
        if (effects != sounds().effectsOn()) press(effects ? setup.effectsOn : setup.effectsOff);
    };
    sounds().setScripts(std::move(calls));
    show(stage_);
}

void FlashSurgeonGame::stageGoto(const char* label, bool bandage) {
    stage_->runner.callMethod(Value(ClipRef(stage_->root)), bandage ? "bandage" : "ftb", {Value(label)});
}

void FlashSurgeonGame::startIntro() { stageGoto("intro", true); }

void FlashSurgeonGame::startDialogue(int speech) {
    curSpeech = speech;
    stageGoto("dialog");
}

void FlashSurgeonGame::startLevels(bool bandage) { stageGoto("levels", bandage); }
void FlashSurgeonGame::startOpReady() { stageGoto("opready"); }
void FlashSurgeonGame::startOperation() { stageGoto("game"); }
void FlashSurgeonGame::startTitleScreen() { stageGoto("start"); }
void FlashSurgeonGame::startComplete3() { stageGoto("complete3"); }

// A patient that goes on from the one before (AS1's 24 after 23) starts
// with the time that one left, as the scripts would have stored it.
void FlashSurgeonGame::seedStoredTime(int patient) {
    auto item = [](const Value& v, size_t i) {
        return v.type == Value::kObject && i < v.object->items.size() ? v.object->items[i] : Value();
    };
    SceneClip& root = *stage_->root;
    Value first = item(root.get("missionArray" + std::to_string(patient)), 0);
    if (item(first, 3).toString() != "stored") return;
    Value time = item(item(root.get("missionArray" + std::to_string(patient - 1)), 0), 3);
    root.set("storedTimeLeft", time);
    root.set("storedOTimeLeft", time);
    root.set("storedScore", Value(0));
}

void FlashSurgeonGame::openHelp() {
    Value help = stage_->root->get("help");
    if (help.type == Value::kClip) help.clip->play();
}

std::vector<DebugMenu::Item> FlashSurgeonGame::debugItems() {
    std::vector<DebugMenu::Item> items;
    items.push_back({"Title", [this] { startTitleScreen(); }});
    items.push_back({"Intro", [this] { startIntro(); }});
    items.push_back({"Levels", [this] { startLevels(false); }});
    for (int m = 1; m <= setup.patients; ++m) {
        DebugMenu::Item i{"Patient " + std::to_string(m), [this, m] {
                              level = m;
                              // The screens a player comes through (the levels,
                              // a story scene, the patient) stop the title music
                              // and the intro's van as they open; the
                              // operation's frame does not.
                              for (const char* music : {"theme", "vandriving", "invan"}) sounds().stop(music);
                              seedStoredTime(m);
                              startOperation();
                          }};
        i.newRow = m % 4 == 1;  // (four fit a row)
        items.push_back(i);
    }
    bool op = probe && probe->running();
    auto opItem = [&](const char* label, std::function<void(OperationProbe&)> act, bool newRow = false) {
        DebugMenu::Item i{label, [this, act] {
                              if (probe && probe->running()) act(*probe);
                          }};
        i.enabled = op;
        i.newRow = newRow;
        items.push_back(i);
    };
    opItem("Skip stage", [](OperationProbe& o) { o.skipStage(); }, true);
    opItem("Win now", [](OperationProbe& o) { o.win(); });
    opItem("Heal", [](OperationProbe& o) { o.heal(); });
    opItem("+60 s", [](OperationProbe& o) { o.addTime(60); });
    opItem("Kill patient", [](OperationProbe& o) { o.kill(); });
    bool god = probe && probe->godMode;
    DebugMenu::Item godItem{std::string("God mode: ") + (god ? "ON" : "OFF"), [this] {
                                if (probe) probe->godMode = !probe->godMode;
                            }};
    godItem.on = god;
    godItem.keepOpen = true;
    godItem.newRow = true;
    items.push_back(godItem);
    DebugMenu::Item smooth{std::string("Smooth: ") + (platform::smoothMotion() ? "ON" : "OFF"),
                           [] { platform::setSmoothMotion(!platform::smoothMotion()); }};
    smooth.on = platform::smoothMotion();
    smooth.keepOpen = true;
    items.push_back(smooth);
    items.push_back({"Unlock all", [this] {
                         save.load();
                         for (int l = 1; l <= setup.patients; ++l)
                             if (save.grade[l] == 0) save.grade[l] = 1;
                         save.save();
                     }});
    return items;
}
