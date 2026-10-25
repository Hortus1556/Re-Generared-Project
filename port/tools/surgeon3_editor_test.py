#!/usr/bin/env python3
"""The surgery editor of Amateur Surgeon 3, tested by real play (docs/as3.md,
"Surgery editor").

Driven live like surgeon3_playtest.py (whose Live driver and Surgery player
it uses), on scratch saves, with the clipboard kept in a file
(AS_CLIPBOARD_FILE) so the user's is never touched:

 1. editor: a slot that has finished the game opens the editor from the
    Bunker's map (EDITOR), makes a new surgery through the editor's own
    buttons: a large cut tapped in from the palette and dragged, a small cut
    dragged out of the palette, a creature (the first of the creatures'
    group) turned by its handle, a second stage (past the last) with an
    extraction and a poison going back to stage 1 when done, an incision on
    stage 1 that opens stage 2 when cut; the clock set; saved; tested to the
    win (the stage jump and back); a new version saved; its share code copied;
 2. shared: a slot that has not finished the game (SHARED: no editor) pastes
    the code, plays it to the win; its slot is the same before and after;
 3. refused: malformed and hostile codes pasted, each refused with a message;
 4. import: files dropped in editedLevels/import/ (a level, a code, a broken
    file) imported; a level file broken by hand listed as one that can't be read.
Screenshots of each step are kept in the saves (printed at the end).

Usage: surgeon3_editor_test.py [--build build-dev] [step...]   (steps: editor shared refused import)
"""
import base64
import copy
import json
import os
import shutil
import sys
import tempfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import surgeon3_playtest as pt  # noqa: E402

# the menus (screen points, y down)
TITLE_PLAY, SLOT, WORLD_BUNKER, MAP_ENTRY = (300, 270), (284, 160), (284, 160), (516, 299)
# the list of levels
HUB_NEW, HUB_PASTE_UNLOCKED, HUB_PASTE_LOCKED, HUB_IMPORT_UNLOCKED, HUB_IMPORT_LOCKED = (
    (72, 299), (350, 299), (70, 299), (484, 299), (204, 299))
HUB_ROW0_PLAY_UNLOCKED, HUB_ROW0_PLAY_LOCKED, HUB_ROW0_EDIT = (345, 66), (407, 66), (409, 66)
# the editor's bars
MENU, PREV, STAGE, NEXT, MODE, ADD, LIST, TEST = (33, 17), (79, 17), (142, 17), (205, 17), (268, 17), (353, 17), (419, 17), (532, 17)
ROT15L, ROT1L, ROT1R, ROT15R, COPY, EDIT, DEL = (274, 303), (312, 303), (346, 303), (384, 303), (431, 303), (483, 303), (537, 303)
# the palette: its cells, its groups' tabs
CELL = [(87, 137), (217, 137), (347, 137), (477, 137), (87, 227), (217, 227), (347, 227), (477, 227)]
TABS = [(72 + 104 * k, 60) for k in range(5)]
# the popups: yes/no, OK; a property popup's rows' < and >, its DONE
YES, NO, OK = (344, 215), (214, 215), (284, 220)
ROW_LESS_X, ROW_MORE_X, DONE = 266, 434, (419, 281)
STAGE_CLOCK_Y, STAGE_THEN_Y, INJ_OPENS_Y, INJ_STEP_Y = 161, 225, 101, 133
MENU_SAVE, MENU_SAVE_NEW, MENU_TEST, MENU_CODE = (188, 79), (380, 79), (188, 117), (380, 117)
RESULT_AGAIN, RESULT_BACK = (209, 228), (359, 228)

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def seed(save, finished):
    """A slot past the second patient (the world map open), every surgery
    won (three stars) but the last unless `finished`."""
    with open(os.path.join(pt.package(), "Data", "areas.json"), encoding="utf-8") as f:
        order = [s for a in json.load(f)["areas"] for s in a["surgeries"]]
    won = [s for s in order if finished or s != "surgery_bunker_hubris2"]
    slot = {"uniqueId": 1, "lastSaveTimestamp": "0", "starsMain": {s: 3 for s in won}, "upgradesMain": {},
            "partnersMain": {}, "starsChallenge1": {}, "starsChallenge2": {}, "completedTutorials": [],
            "statProgress": {}, "highScores": {}, "transactions": [], "selectedTagPartner": "giblets",
            "lastSelectedArea": "area_bunker", "sfxOn": True, "musicOn": True, "numBloodPacks": 0,
            "lastBloodPackTimestamp": "0", "tag_cooldowns": {}, "lastFailedSurgery": "",
            "lastFailedSurgeryMode": 0, "lastFailedSurgeryStage": 0}
    state = {"formatVersion": 1, "lastLoaded": 1, "last1DayLocalNotificationTimestamp": "0",
             "last3DayLocalNotificationTimestamp": "0", "nextAppReviewPopupTimestamp": "0", "monetized": False,
             "savedGames": [slot]}
    with open(os.path.join(save, "state.json"), "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)


def slot_state(save):
    """What the campaign keeps of a slot."""
    with open(os.path.join(save, "state.json"), encoding="utf-8") as f:
        s = json.load(f)["savedGames"][0]
    return {k: s.get(k) for k in ("starsMain", "starsChallenge1", "starsChallenge2", "transactions", "statProgress",
                                  "highScores", "selectedTagPartner", "tag_cooldowns", "lastFailedSurgery",
                                  "completedTutorials", "upgradesMain", "partnersMain")}


class Run:
    def __init__(self, exe, name, finished, clip):
        self.save = tempfile.mkdtemp(prefix="as3edit_%s_" % name)
        seed(self.save, finished)
        self.g = pt.Live(exe, self.save, {"AS_CLIPBOARD_FILE": clip, "AS_SEED": "7"})

    def to_map(self):
        g = self.g
        g.run(320)
        g.click(*TITLE_PLAY)
        g.run(300)
        g.click(*SLOT)
        g.run(330)
        g.click(*WORLD_BUNKER)
        g.run(330)

    def after(self, pattern, since):
        return [l for l in self.g.log[since:] if pattern in l]


def fresh(g):
    """The trace from now on only (the player reads all of it: it starts
    afresh before each surgery is started)."""
    g.log, g.frames, g.starts = [], [], []


def play_to_end(g):
    g.run(200)
    result = pt.Surgery(g).play(limit=60000)
    g.run(240)
    return result


def step_editor(exe, clip):
    r = Run(exe, "editor", True, clip)
    g = r.g
    try:
        r.to_map()
        g.shot("map")
        g.click(*MAP_ENTRY)
        g.run(120)
        g.shot("hub")
        g.click(*HUB_NEW)
        g.run(150)
        # stage 1: a large cut tapped in (at the centre), dragged
        g.click(*ADD)
        g.run(40)
        g.shot("palette")
        g.click(*CELL[0])
        g.run(30)
        g.drag([(284, 160), (230, 150)], speed=4)
        g.run(10)
        # a small cut dragged out of the palette onto the patient
        g.click(*ADD)
        g.run(40)
        g.drag([CELL[1], (300, 120), (340, 200)], speed=6)
        g.run(10)
        # a creature, turned by its handle
        g.click(*ADD)
        g.run(30)
        g.click(*TABS[2])
        g.run(40)
        g.shot("palette_creatures")
        g.click(*CELL[0])
        g.run(30)
        g.drag([(284, 160), (330, 110)], speed=4)
        g.run(10)
        n = len(g.log)
        g.click(330, 110)  # (picked again where it is now: its handle traced)
        g.run(10)
        sel = g.find(r"^editor: selected (\S+) at (\d+),(\d+) handle (\d+),(\d+)", n)
        if not sel:
            raise pt.Timeout("no creature selected")
        cx, cy, hx, hy = (int(sel.group(k)) for k in range(2, 6))
        n = len(g.log)
        g.drag([(hx, hy), (cx + 60, cy - 30)], speed=4)
        g.run(10)
        turned = g.find(r"^editor: placed \S+ at \d+,\d+ rot (-?\d+)", n)
        if not turned or turned.group(1) == "0":
            raise pt.Timeout("the handle didn't turn it")
        # the clock: one step more
        g.click(*STAGE)
        g.run(30)
        g.click(ROW_MORE_X, STAGE_CLOCK_Y)
        g.run(20)
        g.click(*DONE)
        g.run(20)
        g.shot("stage1")
        # stage 2 (past the last): an extraction and a poison, then back to stage 1
        g.click(*NEXT)
        g.run(20)
        g.click(*YES)
        g.run(40)
        g.click(*ADD)
        g.run(30)
        g.click(*TABS[0])
        g.run(40)
        g.click(*CELL[3])
        g.run(30)
        g.drag([(284, 160), (250, 170)], speed=4)
        g.run(10)
        g.click(*ADD)
        g.run(30)
        g.click(*CELL[5])
        g.run(30)
        g.drag([(284, 160), (340, 140)], speed=4)
        g.run(10)
        g.click(*STAGE)
        g.run(30)
        g.shot("stage_props")
        g.click(ROW_LESS_X, STAGE_THEN_Y)
        g.run(20)
        g.click(*DONE)
        g.run(20)
        g.shot("stage2")
        # stage 1: an incision opening stage 2 when its first step ends
        g.click(*PREV)
        g.run(40)
        g.click(*ADD)
        g.run(30)
        g.click(*CELL[2])
        g.run(30)
        g.drag([(284, 160), (300, 205)], speed=4)
        g.run(10)
        g.click(*EDIT)
        g.run(30)
        g.click(ROW_MORE_X, INJ_OPENS_Y)
        g.click(ROW_MORE_X, INJ_OPENS_Y)
        g.run(10)
        g.click(ROW_MORE_X, INJ_STEP_Y)
        g.run(10)
        g.shot("injury_props")
        g.click(*DONE)
        g.run(20)
        g.shot("editor")
        # saved, tested to the end
        g.click(*MENU)
        g.run(30)
        g.shot("menu")
        g.click(*MENU_SAVE)
        g.run(30)
        g.click(*OK)
        g.run(20)
        before = slot_state(r.save)
        g.click(*MENU)
        g.run(30)
        fresh(g)
        g.click(*MENU_TEST)
        g.run(10)
        result = play_to_end(g)
        g.shot("test_result")
        stages = [l for l in g.log if l.startswith("stage ")]
        if result != "won":
            raise pt.Timeout("the test was " + result)
        if slot_state(r.save) != before:
            raise pt.Timeout("the test changed the slot")
        print("editor: tested, won (%s); stages %s; the slot unchanged" % (g.find(r"^surgery won.*")[0],
                                                                          " ".join(s[6:] for s in stages)))
        g.click(*RESULT_BACK)
        g.run(150)
        g.shot("back_in_editor")
        # a new version, then the share code
        g.click(*MENU)
        g.run(30)
        g.click(*MENU_SAVE_NEW)
        g.run(30)
        g.click(*OK)
        g.run(20)
        g.click(*MENU)
        g.run(30)
        g.click(*MENU_CODE)
        g.run(30)
        g.shot("code")
        g.click(*OK)
        g.run(20)
        files = sorted(os.listdir(os.path.join(r.save, "editedLevels")))
        print("editor: files", files)
        if len([f for f in files if f.endswith(".json")]) != 2:
            raise pt.Timeout("expected two versions")
    finally:
        g.close()
    with open(clip, encoding="utf-8") as f:
        code = f.read()
    print("editor: code (%d letters) %s..." % (len(code), code[:48]))
    return r.save, code


def step_shared(exe, clip):
    r = Run(exe, "shared", False, clip)
    g = r.g
    try:
        r.to_map()
        g.shot("map_shared")
        g.click(*MAP_ENTRY)
        g.run(120)
        g.click(*HUB_PASTE_LOCKED)
        g.run(30)
        g.shot("paste")
        g.click(*YES)
        g.run(30)
        g.shot("hub_shared")
        before = slot_state(r.save)
        fresh(g)
        g.click(*HUB_ROW0_PLAY_LOCKED)
        g.run(10)
        result = play_to_end(g)
        g.shot("shared_result")
        if result != "won":
            raise pt.Timeout("the shared level was " + result)
        if slot_state(r.save) != before:
            raise pt.Timeout("playing the shared level changed the slot")
        print("shared: pasted in a slot without the editor, played, won (%s); the slot unchanged" %
              g.find(r"^surgery won.*")[0])
        g.click(*RESULT_BACK)
        g.run(150)
        g.shot("back_in_list")
    finally:
        g.close()
    return r.save


def encode(obj):
    text = obj if isinstance(obj, (bytes, str)) else json.dumps(obj, separators=(",", ":"))
    if isinstance(text, str):
        text = text.encode()
    return "AS3L1:" + base64.urlsafe_b64encode(zlib.compress(text, 9)).decode().rstrip("=")


def decode(code):
    b = code.strip().split(":", 1)[1]
    return json.loads(zlib.decompress(base64.urlsafe_b64decode(b + "=" * (-len(b) % 4))))


def bad_codes(good):
    level = decode(good)

    def mod(f):
        lv = copy.deepcopy(level)
        f(lv)
        return encode(lv)
    first = lambda lv: lv["stages"][0]["injuries"][0]  # noqa: E731
    return [
        ("empty", ""), ("words", "hello there"), ("newer", "AS3L2:" + good[6:]), ("bad letters", "AS3L1:!!!!****"),
        ("truncated", good[:-25]), ("zip bomb", encode(b" " * 3000000 + b"{}")),
        ("deep", encode("[" * 100000 + "]" * 100000)), ("deep small", encode("[" * 40 + "]" * 40)),
        ("not JSON", encode("{not json")), ("too long", "AS3L1:" + "A" * 40000),
        ("unknown class", mod(lambda lv: first(lv).__setitem__("class_name", "DeathRay"))),
        ("out of range", mod(lambda lv: first(lv).__setitem__("x", 99999))),
        ("huge number", mod(lambda lv: first(lv).__setitem__("rotation", 1e300))),
        ("unknown key", mod(lambda lv: lv.__setitem__("cheat_mode", True))),
        ("wrong type", mod(lambda lv: lv.__setitem__("stages", "lots"))),
        ("no injuries", mod(lambda lv: [s.__setitem__("injuries", []) for s in lv["stages"]])),
        ("bad title", mod(lambda lv: lv["editor"].__setitem__("title", "Bad\u0001title"))),
        ("too many", mod(lambda lv: lv["stages"][0].__setitem__(
            "injuries", [dict(first(lv), name="c%d" % i) for i in range(60)]))),
        ("bad background", mod(lambda lv: lv["stages"][0].__setitem__("bg", "../../evil.png"))),
        ("bad dependency", mod(lambda lv: first(lv).__setitem__("visual_dependencies", ["ghost_99"]))),
        ("bad tool", mod(lambda lv: lv.__setitem__("tools", ["Lighter", "Laser"]))),
        ("no clock", mod(lambda lv: lv["stages"][0].__setitem__("time", -1))),
        ("same names", mod(lambda lv: lv["stages"][0]["injuries"][1].__setitem__("name", first(lv)["name"]))),
        ("bad stage jump", mod(lambda lv: lv["stages"][0].__setitem__("to_surgery_stage", 9))),
        ("tutorial", mod(lambda lv: lv.__setitem__("tutorial_filename", "tutorial_bunker_mrgiblets"))),
    ]


def step_refused(exe, clip, good):
    r = Run(exe, "refused", False, clip)
    g = r.g
    ok = True
    try:
        r.to_map()
        g.click(*MAP_ENTRY)
        g.run(120)
        for name, code in bad_codes(good):
            with open(clip, "w", encoding="utf-8") as f:
                f.write(code)
            n = len(g.log)
            g.click(*HUB_PASTE_LOCKED)
            g.run(30)
            out = r.after("editor: paste refused:", n)
            print("refused: %-15s %s" % (name, out[0][len("editor: paste refused: "):] if out else "ACCEPTED"))
            if name == "zip bomb":
                g.shot("refused")
            if out:
                g.click(*OK)
            else:
                ok = False
                g.click(*NO)
            g.run(20)
        files = [f for f in os.listdir(os.path.join(r.save, "editedLevels")) if f.endswith(".json")]
        if files:
            ok = False
    finally:
        g.close()
        with open(clip, "w", encoding="utf-8") as f:
            f.write(good)
    if not ok:
        raise pt.Timeout("a bad code was taken")
    return r.save


def step_import(exe, clip, good, level_file):
    r = Run(exe, "import", True, clip)
    g = r.g
    folder = os.path.join(r.save, "editedLevels", "import")
    os.makedirs(folder)
    shutil.copy(level_file, os.path.join(folder, "a_level.json"))
    with open(os.path.join(folder, "a_code.txt"), "w", encoding="utf-8") as f:
        f.write(good)
    with open(os.path.join(folder, "broken.json"), "w", encoding="utf-8") as f:
        f.write('{"stages": [')
    # a level file edited by hand into something the game can't play: listed as such
    with open(os.path.join(r.save, "editedLevels", "hacked_v1_2020_1_1_0_0_0.json"), "w", encoding="utf-8") as f:
        f.write('{"stages": "everywhere", "tools": ["Laser"]}')
    try:
        r.to_map()
        g.click(*MAP_ENTRY)
        g.run(120)
        n = len(g.log)
        g.click(*HUB_IMPORT_UNLOCKED)
        g.run(40)
        g.shot("import")
        msg = r.after("editor: import:", n)
        print("import:", msg[0] if msg else "(nothing)")
        g.click(*OK)
        g.run(20)
        g.shot("hub_imported")
        left = sorted(os.listdir(folder))
        levels = [f for f in os.listdir(os.path.join(r.save, "editedLevels"))
                  if f.endswith(".json") and not f.startswith("hacked")]
        print("import: left in import/", left, "levels", len(levels))
        if left != ["broken.json"] or len(levels) != 2:
            raise pt.Timeout("the import didn't do what it should")
    finally:
        g.close()
    return r.save


def main():
    args = sys.argv[1:]
    build = "build-dev"
    if "--build" in args:
        i = args.index("--build")
        build = args[i + 1]
        del args[i:i + 2]
    steps = args or ["editor", "shared", "refused", "import"]
    exe = os.path.join(PORT, build, "AmateurSurgeon3.exe")
    pt.EXE_DIR = os.path.dirname(exe)
    clip = os.path.join(tempfile.mkdtemp(prefix="as3clip_"), "clipboard.txt")
    saves = []
    editor_save, code = step_editor(exe, clip)
    saves.append(editor_save)
    level_file = os.path.join(editor_save, "editedLevels",
                              sorted(f for f in os.listdir(os.path.join(editor_save, "editedLevels"))
                                     if f.endswith(".json"))[0])
    if "shared" in steps:
        saves.append(step_shared(exe, clip))
    if "refused" in steps:
        saves.append(step_refused(exe, clip, code))
    if "import" in steps:
        saves.append(step_import(exe, clip, code, level_file))
    print("all passed; screenshots and saves in:")
    for s in saves:
        print("  ", s)


if __name__ == "__main__":
    try:
        main()
    except pt.Timeout as e:
        print("FAILED:", e)
        sys.exit(1)
