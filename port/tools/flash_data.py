#!/usr/bin/env python3
"""A Flash game's operation data, from its script index, as data/gamedata.json.

The Flash games (Christmas, AS1) keep their data as ActionScript literals,
which the script index (index_scripts.py; scripts/index.json) has as
translated `set` actions:
- the injury types (`game.<type>.<field> = ...`, the operation clip's frame 3);
- the missions (`missionArray1..N`, main timeline frame 1);
- the lines said during an operation (`dialogueOperation1..N`,
  `dialogueInjury1..N`, main timeline frame 1);
- the tools each mission offers (`usableTools1..N`, the operation clip's
  frame 2).
The operation clip is the package's `operation` module's sprite
(data/modules.json); N is the number of missionArrays the scripts set.

Also the clips whose own frame is picked by something the translator cannot
run as a script, for the native operation's clips (the stage runs the
others' scripts):
- picked by the level (`if (_root.level == 15 or ...) gotoAndStop(n); else
  gotoAndStop(...);`): `levelFrames` when none of the game's levels is
  named (the same on every level), else `levelFramesByLevel` (the levels
  named, the `then` frame, the `else` choices);
- a piece's own random variant cached on its parent (`if (_parent.rando ==
  0) { _parent.rando = random(n) + 1; } gotoAndStop(_parent.rando);`): in
  version 8 SWFs `rando` is never 0 (undefined is NaN as a number: rules.md
  section 7), so this never runs and the piece sits on frame 1, in the
  original too. Held here as one of the n choices instead, so the variety
  the script never reached is not lost.

Reads port/games/<game>/scripts/index.json (run index_scripts.py first) and
writes port/games/<game>/data/gamedata.json, which the game reads at start.
Do not edit the output; re-run this script instead.

Usage: flash_data.py <game>      (christmas, as1)
"""
import json
import os
import sys

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def value(e):
    """A literal expression's value (lists for arrays); KeyError if not one."""
    k = e[0]
    if k == "lit":
        return e[1]
    if k == "undef":
        return None
    if k == "array":
        return [value(x) for x in e[1]]
    if k == "object":
        return {key: value(v) for key, v in e[1]}
    if k == "unary" and e[1] == "-":
        return -value(e[2])
    raise KeyError(k)


def sets(script):
    """The script's top-level `name = literal` sets: [(on, name, value)]."""
    out = []
    for a in script["actions"]:
        if a["do"] == "set" and a.get("op", "=") == "=" and isinstance(a["name"], str):
            try:
                out.append((a["on"], a["name"], value(a["value"])))
            except KeyError:
                pass
    return out


def num(v, otherwise=0):
    if isinstance(v, bool):
        return 1 if v else 0
    return v if isinstance(v, (int, float)) else otherwise


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    game = sys.argv[1]
    package = os.path.join(PORT, "games", game)
    index = json.load(open(os.path.join(package, "scripts", "index.json"), encoding="utf-8"))
    modules = json.load(open(os.path.join(package, "data", "modules.json"), encoding="utf-8"))
    # The operation clip: its native module's sprite, or (no native module:
    # AS1, whose operation runs by its scripts) the one modules.json names.
    operation = modules["modules"]["operation"]
    op = operation["sprites"][0] if operation["sprites"] else operation["sprite"]
    frames = {}
    for s in index["scripts"]:
        if s["kind"] == "frame":
            frames.setdefault((s["owner"], s["frame"]), []).append(s)

    def root_value(owner, frame, name):
        for s in frames.get((owner, frame), []):
            for on, n, v in sets(s):
                if n == name and on in (["this"], ["scope"]):
                    return v
        return None

    # Injury types: game.<type>.<field> = ...
    types, order = {}, []
    for s in frames[(op, 3)]:
        for on, field, v in sets(s):
            if on[:2] == ["get", ["var", "game"]] and len(on) == 3:
                name = on[2]
                if name not in types:
                    types[name] = {}
                    order.append(name)
                types[name][field] = v
    injury_types = []
    for name in order:
        t = types[name]
        stages = []
        k = 1
        while "stage%dArray" % k in t:
            a = list(t["stage%dArray" % k]) + [None] * 7
            stages.append({"tool": a[0] or "", "onTimeout": a[1] or "", "sTimer": num(a[2]), "dTimer": num(a[3]),
                           "damage": num(a[4]), "specialTimer": num(a[5]), "behaviour": a[6] or ""})
            k += 1
        injury_types.append({
            "linkage": name, "make": t.get("make", name), "currentStage": num(t.get("currentStage", 1)),
            "numberStages": num(t.get("numberStages", 1)), "startInvis": bool(t.get("startInvis")),
            "transplant": bool(t.get("transplant")), "pieces": num(t.get("pieces")), "oPieces": num(t.get("oPieces")),
            "covers": bool(t.get("covers")), "bone": bool(t.get("bone")), "metal": bool(t.get("metal")),
            "speed": num(t.get("speed")), "slotted": num(t.get("slotted")), "isImplant": bool(t.get("isImplant")),
            "isProb": bool(t.get("isProb")), "heavy": bool(t.get("heavy")), "stages": stages})

    levels = 0
    while root_value(0, 1, "missionArray%d" % (levels + 1)) is not None:
        levels += 1

    missions = []
    for level in range(1, levels + 1):
        mission = []
        for stage in root_value(0, 1, "missionArray%d" % level):
            spots = []
            for inj in stage[0]:
                inj = list(inj) + [None] * 5
                spots.append({"type": inj[0], "x": num(inj[1]), "y": num(inj[2]), "rotation": num(inj[3]),
                              "frame": "" if inj[4] is None else str(inj[4])})
            time = stage[3] if len(stage) > 3 else "nothing"
            health = stage[4] if len(stage) > 4 else "nothing"
            mission.append({"injuries": spots, "bodyLevel": num(stage[1]), "background": str(stage[2]),
                            "time": -1 if time == "nothing" else num(time),
                            "health": -1 if health == "nothing" else num(health),
                            "speeches": [int(x) for x in stage[5:]]})
        missions.append(mission)

    operation_lines, injury_lines = [], []
    for level in range(1, levels + 1):
        operation_lines.append([{"stage": num(a[0]), "text": a[1], "character": a[2],
                                 "tool": a[3] if len(a) > 3 else ""}
                                for a in root_value(0, 1, "dialogueOperation%d" % level) or [] if a])
        injury_lines.append([{"make": a[0], "stage": num(a[1]), "bodyLevel": num(a[2]), "text": a[3], "doing": a[4],
                              "character": a[5], "tool": a[6] if len(a) > 6 else ""}
                             for a in root_value(0, 1, "dialogueInjury%d" % level) or [] if a])
    tools = [root_value(op, 2, "usableTools%d" % level) or [] for level in range(1, levels + 1)]

    level_frames, by_level = [], []
    for (owner, frame), scripts in sorted(frames.items()):
        if owner == 0:
            continue
        acts = [a for s in scripts for a in s["actions"]]
        # if (_root.level == 15 or ...) gotoAndStop(n); else gotoAndStop(...);
        for a in acts:
            picked = level_frame(a)
            if not picked:
                continue
            named, then, choices = picked
            if any(1 <= v <= levels for v in named):
                if then is not None:
                    by_level.append({"sprite": owner, "frame": frame, "levels": sorted(named), "then": then,
                                     "choices": choices})
            else:
                level_frames.append({"sprite": owner, "frame": frame, "choices": choices})
        # if (_parent.rando == 0) { _parent.rando = random(n) + 1; } gotoAndStop(_parent.rando);
        for i in range(len(acts) - 1):
            choices = random_variant_frame(acts[i], acts[i + 1])
            if choices:
                level_frames.append({"sprite": owner, "frame": frame, "choices": choices})

    out = {"about": "Generated by port/tools/flash_data.py %s from scripts/index.json. Do not edit." % game,
           "injuryTypes": injury_types, "missions": missions, "operationLines": operation_lines,
           "injuryLines": injury_lines, "usableTools": tools, "levelFrames": level_frames,
           "levelFramesByLevel": by_level}
    path = os.path.join(package, "data", "gamedata.json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("wrote", path, len(injury_types), "injury types,", levels, "missions,", len(level_frames), "level frames,",
          len(by_level), "by level")


def path_of(e):
    if e in (["this"], ["scope"]):
        return "this"
    if e[0] == "var":
        return e[1]
    if e[0] == "get":
        base = path_of(e[1])
        return base and base + "." + e[2]
    return None


def level_frame(a):
    """A level-picked goto: (the levels named, the `then` frame (None if not
    a number), the `else` frame's choices); None if `a` is not one."""
    if a["do"] != "if" or len(a["then"]) != 1 or len(a["else"]) != 1:
        return None
    levels = []

    def collect(c):
        if c[0] == "op" and c[1] == "||":
            return collect(c[2]) and collect(c[3])
        if c[0] == "op" and c[1] == "==" and path_of(c[2]) == "_root.level" and c[3][0] == "lit":
            levels.append(c[3][1])
            return True
        return False

    if not collect(a["cond"]):
        return None
    then, other = a["then"][0], a["else"][0]
    if then["do"] != "goto" or other["do"] != "goto" or then["play"] or other["play"]:
        return None
    if then["on"] not in (["this"], ["scope"]) or other["on"] not in (["this"], ["scope"]):
        return None
    t = then["frame"]
    then_frame = t[1] if t[0] == "lit" and isinstance(t[1], int) else None
    f = other["frame"]
    if f[0] == "lit" and isinstance(f[1], int):
        return levels, then_frame, [f[1]]
    # random(n) + k
    if f[0] == "op" and f[1] == "+" and f[2][0] == "call" and f[2][1] == ["var", "random"] and f[3][0] == "lit":
        n = f[2][2][0][1]
        return levels, then_frame, [f[3][1] + k for k in range(n)]
    return None


def random_variant_frame(a, b):
    """The choices of a piece's own cached-random-variant pick (see the
    module docstring) if `a`, `b` are its two consecutive actions; else
    None."""
    if a["do"] != "if" or len(a["then"]) != 1 or a["else"]:
        return None
    cond, set_, goto = a["cond"], a["then"][0], b

    def var_path(e):
        return e[0] == "get" and e[1] == ["var", "_parent"] and e[2]

    name = var_path(cond[2]) if cond[0] == "op" and cond[1] == "==" and cond[3] == ["lit", 0] else None
    if not name:
        return None
    if set_["do"] != "set" or set_["on"] != ["var", "_parent"] or set_["name"] != name:
        return None
    v = set_["value"]
    if v[0] != "op" or v[1] != "+" or v[2][0] != "call" or v[2][1] != ["var", "random"] or v[3][0] != "lit":
        return None
    n = v[2][2][0][1]
    if goto["do"] != "goto" or goto["play"] or goto["on"] not in (["this"], ["scope"]):
        return None
    if var_path(goto["frame"]) != name:
        return None
    return [v[3][1] + k for k in range(n)]


if __name__ == "__main__":
    main()
