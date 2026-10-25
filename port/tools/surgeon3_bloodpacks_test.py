#!/usr/bin/env python3
"""Amateur Surgeon 3's blood packs, tested by real play (docs/as3.md, "Blood
packs"; port/src/as3/BloodPacks.h).

Driven live like surgeon3_playtest.py (whose Live driver and Surgery player
it uses), each step on its own scratch save (AS_SAVE_DIR), through the real
menus: title, slot, world map, the Bunker's map, Mr Giblets' case file, his
intro story, the pre-op, the partner select's start. The save is a slot past
the first two surgeries (Mr Giblets and Proxy 4 won), so a start costs a pack.

 1. cost: full (3/3): a start takes one (2 left, the count to the next begun),
    the "Introducing Bloodpacks!" popup over the surgery (once a slot); the
    pause's quit warns with the pack pictured, the quit leaves it used (the
    map's top bar 2/3 and the time to the next); started again (1 left) and
    won: the win gives it back (2).
 2. empty: none, the next due in 25 s: the start refused with the
    out-of-packs popup counting down, no surgery; it closes by itself when the
    pack is back (real time), and the start then takes it (0 left).
 3. recharge: old and odd slots loaded (title, slot, world map): none with a
    timestamp two hours old → 3 (capped); 1 with 45 minutes → 2, the
    timestamp moved on by one recharge; -1 (a new slot's) and 255 (1.45's
    byte-read -1) → 3; a timestamp in the future (the clock put back) → kept
    0, counted from now.
 4. debug: the F1 menu's "Save: no packs", "Save: pack in 10 s" (one back
    after 10 s real time), "Save: packs full".
Screenshots of each step are kept in the saves (printed at the end).

Usage: surgeon3_bloodpacks_test.py [--build build-dev] [step...]   (steps: cost empty recharge debug)
"""
import json
import os
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import surgeon3_playtest as pt  # noqa: E402

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# the menus (screen points, y down)
TITLE_PLAY, SLOT, WORLD_BUNKER = (300, 270), (284, 160), (284, 160)
MAP_GIBLETS, INFO_START, DIALOGUE_SKIP, PREOP_START, PARTNER_START = (
    (180, 150), (520, 295), (40, 295), (520, 295), (520, 295))
POPUP_OK = (335, 215)  # the blood pack popups' OK
PAUSE_QUIT, WARNING_QUIT = (425, 220), (388, 231)
# the F1 menu: its second page, the blood pack items
F1_SAVE_PAGE, F1_NO_PACKS, F1_PACK_IN_10, F1_PACKS_FULL = (190, 39), (173, 155), (285, 155), (63, 155)


class Failed(Exception):
    pass


def seed(save, packs, stamp, stats=None):
    """A slot past the first two surgeries, with these blood packs."""
    won = ("surgery_bunker_mrgiblets", "surgery_bunker_prxy4")
    slot = {"uniqueId": 1, "lastSaveTimestamp": "0", "starsMain": {s: 1 for s in won}, "upgradesMain": {},
            "partnersMain": {}, "starsChallenge1": {}, "starsChallenge2": {}, "completedTutorials": [],
            "statProgress": stats or {}, "highScores": {}, "transactions": [], "selectedTagPartner": "giblets",
            "lastSelectedArea": "area_bunker", "sfxOn": True, "musicOn": True, "numBloodPacks": packs,
            "lastBloodPackTimestamp": str(stamp), "tag_cooldowns": {}, "lastFailedSurgery": "",
            "lastFailedSurgeryMode": 0, "lastFailedSurgeryStage": 0}
    state = {"formatVersion": 1, "lastLoaded": 1, "last1DayLocalNotificationTimestamp": "0",
             "last3DayLocalNotificationTimestamp": "0", "nextAppReviewPopupTimestamp": "0", "monetized": False,
             "savedGames": [slot]}
    with open(os.path.join(save, "state.json"), "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)


def slot_of(save):
    with open(os.path.join(save, "state.json"), encoding="utf-8") as f:
        return json.load(f)["savedGames"][0]


def check(what, ok):
    print("  %s %s" % ("ok:  " if ok else "FAIL:", what))
    if not ok:
        raise Failed(what)


def traced(g, text, since):
    return any(text in line for line in g.log[since:])


def wait_real(g, text, since, seconds):
    """Frames run until the trace says `text` or `seconds` of real time pass
    (the packs come back by the wall clock)."""
    end = time.time() + seconds
    while time.time() < end:
        if traced(g, text, since):
            return True
        g.run(30)
        time.sleep(0.2)
    return traced(g, text, since)


def to_map(g):
    g.run(320)
    g.click(*TITLE_PLAY)
    g.run(300)
    g.click(*SLOT)
    g.run(330)
    g.click(*WORLD_BUNKER)
    g.run(330)


def map_to_partners(g):
    """From the Bunker's map to Mr Giblets' partner select (main mode: his
    case file, his story skipped, the pre-op)."""
    g.click(*MAP_GIBLETS)
    g.run(300)
    g.click(*INFO_START)
    g.run(260)
    g.click(*DIALOGUE_SKIP)
    g.run(220)
    g.click(*PREOP_START)
    g.run(200)


class Run:
    def __init__(self, exe, name, packs, stamp, stats=None):
        self.save = tempfile.mkdtemp(prefix="as3bp_%s_" % name)
        seed(self.save, packs, stamp, stats)
        print("  save:", self.save)
        self.g = pt.Live(exe, self.save, {"AS_SEED": "7"})

    def slot(self):
        return slot_of(self.save)


def step_cost(exe):
    r = Run(exe, "cost", 3, 0)
    g = r.g
    try:
        to_map(g)
        g.shot("1_map_full")
        check("full at the start (3)", r.slot()["numBloodPacks"] == 3)
        map_to_partners(g)
        g.shot("2_partners")
        since = len(g.log)
        g.click(*PARTNER_START)
        g.run(400)
        check("a start takes one: " + "; ".join(l for l in g.log[since:] if "blood packs" in l),
              traced(g, "one taken for surgery_bunker_mrgiblets, 2 left", since))
        s = r.slot()
        check("the save has 2", s["numBloodPacks"] == 2)
        check("the count to the next begun now", abs(int(s["lastBloodPackTimestamp"]) - time.time()) < 120)
        check("the blood packs introduced", traced(g, "blood packs: popup bloodpacks_first", since))
        g.shot("3_introducing")
        g.click(*POPUP_OK)
        g.run(200)
        g.shot("4_surgery_after_ok")
        g.key("esc")
        g.run(120)
        g.click(*PAUSE_QUIT)
        g.run(120)
        g.shot("5_quit_warning")
        since = len(g.log)
        g.click(*WARNING_QUIT)
        g.run(400)
        g.shot("6_map_after_quit")
        s = r.slot()
        check("the quit leaves it used (2)", s["numBloodPacks"] == 2)
        check("introduced once a slot (stat UsedBloodpack)", s["statProgress"].get("UsedBloodpack") == 1)
        # again, played to the win: the pack back
        map_to_partners(g)
        g.log, g.frames, g.starts = [], [], []  # (the player reads the trace from the start of its surgery)
        g.click(*PARTNER_START)
        result = pt.Surgery(g).play(limit=60000)
        check("again: one taken (1 left)", traced(g, "one taken for surgery_bunker_mrgiblets, 1 left", 0))
        check("not introduced again", not traced(g, "popup bloodpacks_first", 0))
        g.run(240)
        g.shot("7_won")
        check("the surgery won (%s)" % result, result == "won")
        check("the win gives it back", traced(g, "one back for the win, 2", 0))
        check("the save has 2 again", r.slot()["numBloodPacks"] == 2)
    finally:
        g.close()
    return r.save


def step_empty(exe):
    r = Run(exe, "empty", 0, int(time.time()) - 1800 + 25, {"UsedBloodpack": 1})
    g = r.g
    try:
        to_map(g)
        g.shot("1_map_empty")
        map_to_partners(g)
        since = len(g.log)
        g.click(*PARTNER_START)
        g.run(200)
        check("none left: refused", traced(g, "none left, surgery_bunker_mrgiblets refused", since))
        check("the out-of-packs popup", traced(g, "blood packs: popup bloodpacks_out", since))
        check("no surgery started", not traced(g, "one taken", since) and not traced(g, "stage 1", since))
        g.shot("2_out_of_packs")
        time.sleep(3)
        g.run(60)
        g.shot("3_out_of_packs_later")
        check("the save still has 0", r.slot()["numBloodPacks"] == 0)
        check("the popup closes itself when one is back (real time)",
              wait_real(g, "one back, out-of-packs popup closed", since, 60))
        g.run(60)
        g.shot("4_partners_one_back")
        check("the save has 1", r.slot()["numBloodPacks"] == 1)
        since = len(g.log)
        g.click(*PARTNER_START)
        g.run(300)
        check("the start takes it (0 left)", traced(g, "one taken for surgery_bunker_mrgiblets, 0 left", since))
        g.shot("5_surgery")
    finally:
        g.close()
    return r.save


def step_recharge(exe):
    now = int(time.time())
    cases = [  # packs, timestamp, expected packs, expected timestamp (None: any)
        (0, now - 7200, 3, None),
        (1, now - 2700, 2, now - 2700 + 1800),
        (-1, now, 3, None),
        (255, now, 3, None),
        (0, now + 3600, 0, "now"),
    ]
    saves = []
    for packs, stamp, want, want_stamp in cases:
        r = Run(exe, "recharge", packs, stamp, {"UsedBloodpack": 1})
        try:
            to_map(r.g)
            r.g.shot("map")
        finally:
            r.g.close()
        s = r.slot()
        got, got_stamp = s["numBloodPacks"], int(s["lastBloodPackTimestamp"])
        what = "%d packs, timestamp %+d s -> %d (got %d, timestamp %+d s)" % (packs, stamp - now, want, got,
                                                                             got_stamp - now)
        ok = got == want
        if want_stamp == "now":
            ok = ok and abs(got_stamp - time.time()) < 120
        elif want_stamp is not None:
            ok = ok and got_stamp == want_stamp
        check(what, ok)
        saves.append(r.save)
    return " ".join(saves)


def f1_save_item(g, item):
    """The F1 menu opened on its "Surgery and save" page, an item clicked
    (the menu closes after it)."""
    g.key("f1")
    g.run(10)
    g.click(*F1_SAVE_PAGE)
    g.run(10)
    g.click(*item)
    g.run(30)


def step_debug(exe):
    r = Run(exe, "debug", 3, 0, {"UsedBloodpack": 1})
    g = r.g
    try:
        to_map(g)
        g.key("f1")
        g.run(10)
        g.click(*F1_SAVE_PAGE)
        g.run(10)
        g.shot("1_f1")
        g.key("esc")
        g.run(10)
        f1_save_item(g, F1_NO_PACKS)
        g.run(60)
        g.shot("2_none")
        check("no packs", r.slot()["numBloodPacks"] == 0)
        since = len(g.log)
        f1_save_item(g, F1_PACK_IN_10)
        check("the next due in 10 s", traced(g, "debug: blood packs 0, the next in 10 s", since))
        g.run(60)
        g.shot("3_none_next_soon")
        check("one back 10 s later", wait_real(g, "blood packs: 1 back, 1 now", since, 40))
        g.run(80)
        g.shot("4_one_back")
        check("the save has 1", r.slot()["numBloodPacks"] == 1)
        f1_save_item(g, F1_PACKS_FULL)
        g.run(80)
        g.shot("5_full")
        check("packs full", r.slot()["numBloodPacks"] == 3)
    finally:
        g.close()
    return r.save


STEPS = {"cost": step_cost, "empty": step_empty, "recharge": step_recharge, "debug": step_debug}


def main():
    args = sys.argv[1:]
    build = "build-dev"
    if "--build" in args:
        i = args.index("--build")
        build = args[i + 1]
        del args[i:i + 2]
    exe = os.path.join(PORT, build, "AmateurSurgeon3.exe")
    pt.EXE_DIR = os.path.dirname(exe)
    ok = True
    for name in args or list(STEPS):
        print(name + ":")
        try:
            STEPS[name](exe)
        except (Failed, pt.Timeout) as e:
            ok = False
            print("  %s FAILED: %s" % (name, e))
    print("all passed" if ok else "FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
