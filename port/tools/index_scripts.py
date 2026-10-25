#!/usr/bin/env python3
"""What each of a Flash game's scripts does: translated, native or unported.

Usage:
  index_scripts.py <game> [--swf <movie.swf>] [--report] [--diff <other game>]

The game's package is port/games/<game>/ (docs/engine.md). Its scripts/
folder holds the decompiled ActionScript (JPEXS FFDec's export; made from
--swf when missing). Every frame script, button handler and clip event is
parsed (port/tools/as2.py) and each of its statements sorted:

  translated  mechanical: timeline control (stop, play, goto... on a path),
              variable sets (literal data included), `if` on expressions,
              calls to named functions (the engine's or a module's). Kept as
              actions in scripts/index.json for the runtime to run.
  native      real logic: function definitions, loops, handlers, objects.
              Each is listed with what it is, and the module that
              implements it in C++ when data/modules.json names one.
  unported    native with no module: what porting the game still needs.

Writes scripts/index.json. --report prints the checklist (counts, the
_root variables, screens, transitions, sounds, the unported list). --diff
compares the functions both games define (the Amateur Surgeon games share
Mediatonic's code).
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

import as2

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORKSPACE = os.path.dirname(PORT)
FFDEC = os.path.join(WORKSPACE, "tools", "ffdec_26.3.0", "ffdec-cli.exe")

TIMELINE = {"stop", "play", "gotoAndStop", "gotoAndPlay", "nextFrame", "prevFrame"}
# Built-in functions an expression may call and stay translated.
PURE = {"random", "int", "Number", "String", "parseInt", "parseFloat", "isNaN", "Boolean", "getTimer",
        "Math.floor", "Math.ceil", "Math.round", "Math.abs", "Math.min", "Math.max", "Math.random", "Math.sqrt",
        "Math.sin", "Math.cos", "Math.atan2", "Math.pow"}


def package(game):
    return os.path.join(PORT, "games", game)


# ------------------------------------------------------------ exporting

def export(game, swf):
    """FFDec's script export of the SWF into the package's scripts/."""
    out = os.path.join(package(game), "scripts")
    tmp = tempfile.mkdtemp(prefix="ffdec_")
    subprocess.check_call([FFDEC, "-export", "script", tmp, swf], stdout=subprocess.DEVNULL)
    src = os.path.join(tmp, "scripts")
    for name in os.listdir(src):
        dst = os.path.join(out, name)
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        shutil.move(os.path.join(src, name), dst)
    shutil.rmtree(tmp, ignore_errors=True)


# --------------------------------------------------------- script units

def number_key(name):
    m = re.search(r"_(\d+)\.as$", name)
    return int(m.group(1)) if m else 1


def owner_of(folder):
    """DefineSprite_<id>[_<linkage>] -> id; the main timeline is 0."""
    m = re.match(r"Define(?:Sprite|Button2?)_(\d+)", folder)
    return int(m.group(1)) if m else 0


def units(scripts):
    """Every script: (kind, owner, frame, events, files, statements)."""
    for folder in sorted(os.listdir(scripts)):
        path = os.path.join(scripts, folder)
        if not os.path.isdir(path) or folder in ("__Packages",):
            continue
        if folder.startswith("frame_"):
            yield from frame_units(scripts, 0, path)
        elif folder.startswith("DefineSprite_"):
            for fd in sorted(os.listdir(path), key=lambda f: int(f[6:]) if f[6:].isdigit() else 0):
                if fd.startswith("frame_"):
                    yield from frame_units(scripts, owner_of(folder), os.path.join(path, fd))
        elif folder.startswith("DefineButton"):
            for f in sorted(os.listdir(path)):
                if f.endswith(".as"):
                    yield button_unit(scripts, owner_of(folder), os.path.join(path, f))
    pk = os.path.join(scripts, "__Packages")
    if os.path.isdir(pk):
        for d, _, fs in os.walk(pk):
            for f in sorted(fs):
                if f.endswith(".as"):
                    p = os.path.join(d, f)
                    yield ("class", 0, 0, [], [rel(scripts, p)], parse_file(p))


def rel(scripts, p):
    return os.path.relpath(p, scripts).replace(os.sep, "/")


def parse_file(p):
    try:
        return as2.parse(open(p, encoding="utf-8").read())
    except as2.ParseError as e:
        raise SystemExit("%s: %s" % (p, e))


def frame_units(scripts, owner, folder):
    frame = int(os.path.basename(folder)[6:])
    files = sorted((f for f in os.listdir(folder) if f.endswith(".as")), key=number_key)
    if files:
        body = []
        for f in files:
            body += parse_file(os.path.join(folder, f))
        yield ("frame", owner, frame, [], [rel(scripts, os.path.join(folder, f)) for f in files], body)
    # Clip events of the clips placed on this frame (PlaceObject folders).
    for sub in sorted(os.listdir(folder)):
        p = os.path.join(folder, sub)
        if os.path.isdir(p):
            m = re.match(r"PlaceObject\d?_(\d+)(?:_(.*?))?_(\d+)$", sub)
            for f in sorted(os.listdir(p)):
                if f.endswith(".as"):
                    for st in parse_file(os.path.join(p, f)):
                        if st[0] == "on":
                            yield ("clip", owner, frame, st[2], [rel(scripts, os.path.join(p, f))], st[3],
                                   {"char": int(m.group(1)) if m else None, "name": m and m.group(2),
                                    "depth": int(m.group(3)) if m else None})


def button_unit(scripts, owner, path):
    body = parse_file(path)
    events, stmts = [], []
    for st in body:
        if st[0] == "on":
            events += st[2]
            stmts += st[3]
        else:
            stmts.append(st)
    return ("button", owner, 0, events, [rel(scripts, path)], stmts)


# ------------------------------------------------------------ translating

class Native(Exception):
    """A statement that is logic, not data: why."""


def dotted(e):
    """a.b.c for a plain path, else None."""
    if e[0] == "var":
        return e[1]
    if e[0] == "get":
        base = dotted(e[1])
        return base and base + "." + e[2]
    return None


def expr(e, calls):
    """An expression as index JSON (["lit", v], ["get", obj, name], ...);
    Native if it is not a plain one. `calls` collects functions it calls."""
    k = e[0]
    if k == "lit":
        return ["lit", e[1]]
    if k == "undef":
        return ["undef"]
    if k == "var":
        return ["this"] if e[1] == "this" else ["var", e[1]]
    if k == "get":
        return ["get", expr(e[1], calls), e[2]]
    if k == "index":
        return ["index", expr(e[1], calls), expr(e[2], calls)]
    if k == "op":
        if e[1] == ",":
            raise Native("comma expression")
        return ["op", e[1], expr(e[2], calls), expr(e[3], calls)]
    if k == "unary":
        if e[1] == "delete":
            raise Native("delete")
        return ["unary", e[1], expr(e[2], calls)]
    if k == "cond":
        return ["cond", expr(e[1], calls), expr(e[2], calls), expr(e[3], calls)]
    if k == "array":
        return ["array", [expr(a, calls) for a in e[1]]]
    if k == "object":
        return ["object", [[key, expr(v, calls)] for key, v in e[1]]]
    if k == "new":
        name = dotted(e[1])
        if name in ("Array", "Object") and not e[2]:
            return ["array", []] if name == "Array" else ["object", []]
        if name is None:
            raise Native("new of a computed class")
        # Flash's built-in classes (Sound, XML, SharedObject...): the
        # engine's (Script.h); any other: a plain object.
        return ["new", name, [expr(a, calls) for a in e[2]]]
    if k == "call":
        if e[1][0] not in ("var", "get", "index"):
            raise Native("call of " + e[1][0])
        name = dotted(e[1]) or as2.source(e[1])
        if name not in PURE:
            calls.append(name)
        return ["call", expr(e[1], calls), [expr(a, calls) for a in e[2]]]
    if k == "func":
        if not e[3]:
            return ["object", []]  # an empty function as an object to hang fields on
        raise Native("function value")
    raise Native(k)


def target_of(e, ctx):
    """The object an assignment or a timeline call acts on, and the name."""
    if e[0] == "var":
        # A bare name: a local, else the timeline the code runs on (in a
        # function, the one it was defined on, not `this`).
        return ["scope"], e[1]
    if e[0] == "get":
        return expr(e[1], ctx.calls), e[2]
    if e[0] == "index":
        return expr(e[1], ctx.calls), expr(e[2], ctx.calls)
    raise Native("assignment to " + e[0])


class Context:
    """One script being translated: the functions it calls, its native
    statements (an action {"do": "native", "ref": n} stands where each ran),
    the functions it defines."""

    def __init__(self, forced=()):
        self.forced = forced  # (regex, module): statements a module takes whole
        self.calls = []
        self.native = []
        self.functions = []  # every function the script defines (translated or not)

    def actions(self, body):
        out = []
        for st in body:
            try:
                if st[0] != "function":
                    text = as2.source(st)
                    for rx, module in self.forced:
                        if rx.search(text):
                            raise Native("matches " + module)
                a = action(st, self)
                out += a if isinstance(a, list) else [a]
            except Native as e:
                self.native.append({"what": what(st), "why": str(e), "module": None})
                if st[0] != "function":
                    out.append({"do": "native", "ref": len(self.native) - 1})
            if st[0] == "function":
                self.functions.append(st)
        return out


def action(st, ctx):
    """A statement as an index action (or a list of them); Native if the
    statement itself is logic (its nested statements are sorted one by one)."""
    k = st[0]
    calls = ctx.calls
    if k == "block":
        return ctx.actions(st[1])
    if k == "function":
        # A function whose body is all translated: kept as actions, defined
        # on the timeline before the script runs (AS hoists them).
        sub = Context(ctx.forced)
        body = sub.actions(st[3])
        if sub.native:  # (why: its first native statement's reason)
            raise Native("function: %s: %s" % (sub.native[0]["what"], sub.native[0]["why"]))
        ctx.calls += sub.calls
        ctx.functions += sub.functions
        return {"do": "function", "name": st[1], "params": st[2], "body": body}
    if k == "return":
        return {"do": "return", "value": expr(st[1], calls) if st[1] else ["undef"]}
    # Loops whose parts are all translated (the runner caps them).
    if k == "while":
        return {"do": "while", "cond": expr(st[1], calls), "body": ctx.actions(st[2])}
    if k == "dowhile":
        return {"do": "while", "cond": expr(st[2], calls), "body": ctx.actions(st[1]), "first": True}
    if k == "for":
        init = [] if st[1] is None else action(st[1], ctx)
        step = [] if st[3] is None else action(("expr", st[3]), ctx)
        return {"do": "for", "init": init if isinstance(init, list) else [init],
                "cond": expr(st[2], calls) if st[2] else ["lit", True], "step": step if isinstance(step, list) else [step],
                "body": ctx.actions(st[4])}
    if k in ("break", "continue"):
        return {"do": k}
    if k == "vardecl":
        return [{"do": "set", "on": ["scope"], "name": name, "local": True,
                 "value": expr(init, calls) if init else ["undef"]} for name, init in st[1]]
    if k == "if":
        return {"do": "if", "cond": expr(st[1], calls), "then": ctx.actions(st[2]), "else": ctx.actions(st[3])}
    if k == "delete" or (k == "expr" and st[1][0] == "unary" and st[1][1] == "delete"):
        on, name = target_of(st[1] if k == "delete" else st[1][2], ctx)
        return {"do": "delete", "on": on, "name": name}
    if k == "forin":
        # for (var i in clip) {...}: the members in Flash's for..in order.
        return {"do": "forin", "name": st[1], "in": expr(st[3], calls), "body": ctx.actions(st[4])}
    if k == "expr":
        e = st[1]
        if e[0] == "assign":
            if e[3][0] == "func" and e[3][3]:
                # An event handler (this.onEnterFrame = function () {...}):
                # kept as actions when its body is all translated.
                sub = Context(ctx.forced)
                body = sub.actions(e[3][3])
                if sub.native:
                    raise Native("handler: %s: %s" % (sub.native[0]["what"], sub.native[0]["why"]))
                ctx.calls += sub.calls
                on, name = target_of(e[2], ctx)
                return {"do": "handler", "on": on, "name": name, "params": e[3][2], "body": body}
            on, name = target_of(e[2], ctx)
            return {"do": "set", "on": on, "name": name, "op": e[1], "value": expr(e[3], calls)}
        if e[0] == "incdec":
            on, name = target_of(e[3], ctx)
            return {"do": "set", "on": on, "name": name, "op": "+=" if e[1] == "++" else "-=", "value": ["lit", 1]}
        if e[0] == "call":
            callee = e[1]
            method = callee[2] if callee[0] == "get" else callee[1] if callee[0] == "var" else None
            if method in TIMELINE:
                on = expr(callee[1], calls) if callee[0] == "get" else ["scope"]
                args = [expr(a, calls) for a in e[2]]
                if method in ("stop", "play", "nextFrame", "prevFrame"):
                    return {"do": method, "on": on}
                return {"do": "goto", "on": on, "frame": args[0] if args else ["lit", 1],
                        "play": method == "gotoAndPlay"}
            fn = expr(callee, calls)
            calls.append(dotted(callee) or as2.source(callee))
            return {"do": "call", "fn": fn, "args": [expr(a, calls) for a in e[2]]}
        if e[0] in ("undef", "lit", "var") or not has_effects(e):
            # A value alone does nothing: the decompiler's leftovers
            # (`_root.storedTimeLeft;`, `(a._rotation + random(21)) - 10;`)
            # whose result the bytecode dropped.
            return []
        raise Native("expression " + e[0])
    raise Native(k)


# Calls with no effect but their result (random() only draws a number).
PURE_CALLS = {"random", "Math.random", "Math.floor", "Math.ceil", "Math.round", "Math.abs", "Math.sqrt",
              "Math.sin", "Math.cos", "Math.atan2", "Math.min", "Math.max", "Math.pow"}


def has_effects(e):
    """Whether evaluating expression `e` can change anything: a call (but a
    pure one), an assignment, ++/--, a new object, a delete."""
    if not isinstance(e, (list, tuple)) or not e:
        return False
    k = e[0]
    if k in ("assign", "incdec", "new"):
        return True
    if k == "unary" and len(e) > 1 and e[1] == "delete":
        return True
    if k == "call":
        name = dotted(e[1]) or ""
        if name not in PURE_CALLS:
            return True
    return any(has_effects(x) for x in e[1:] if isinstance(x, (list, tuple)))


def what(st):
    """A native statement in a few words."""
    k = st[0]
    if k == "function":
        return "function " + st[1]
    if k == "expr" and st[1][0] == "assign" and st[1][3][0] == "func" and st[1][3][3]:
        return "handler " + (dotted(st[1][2]) or as2.source(st[1][2]))
    if k == "class":
        return "class " + st[1]
    head = as2.source(st)
    return k + ": " + (head[:70] + "..." if len(head) > 70 else head)


# --------------------------------------------------------- patterns

def pattern_of(body):
    """Recurring Mediatonic script shapes, recognised whole: (name, its
    parameters) or (None, None).

      soundClip  a clip that holds one sound: playme(loops), stopme(),
                 changeVolume(v); `mysound.attachSound(name)`, its volume,
                 `music = true` for music (the sound buttons mute by it)
    """
    names = {st[1] for st in body if st[0] == "function"}
    text = as2.body_source(body)
    if {"playme", "stopme"} <= names and "attachSound" in text:  # (noted, and run as it is)
        m = re.search(r'attachSound\("([^"]+)"\)', text)
        v = re.search(r"setVolume\((\d+)\);(?!\})", text)
        return "soundClip", {"sound": m and m.group(1), "volume": int(v.group(1)) if v else 100,
                             "music": "music=true;" in text}
    return None, None


# --------------------------------------------------------------- index

def load_modules(game):
    p = os.path.join(package(game), "data", "modules.json")
    return json.load(open(p, encoding="utf-8"))["modules"] if os.path.exists(p) else {}


def module_for(modules, unit, native, why):
    """The module a native statement is assigned to, or None: by pattern or
    reason, then by function name, then by where it is (button, frame,
    sprite)."""
    fn = native.split(" ", 1)[1] if native.startswith(("function ", "handler ", "class ")) else None
    where = "%d:%d" % (unit["owner"], unit["frame"])
    rules = [
        lambda m: unit.get("pattern") in m.get("patterns", []) or why == "matches " + name_of(m),
        lambda m: fn is not None and fn in m.get("functions", []),
        lambda m: unit["kind"] == "button" and unit["owner"] in m.get("buttons", []),
        lambda m: unit["kind"] != "button" and where in m.get("frames", []),
        lambda m: unit["kind"] in ("frame", "clip") and unit["owner"] in m.get("sprites", []),
    ]
    def name_of(m):
        return next(n for n, x in modules.items() if x is m)

    for rule in rules:
        for name, m in modules.items():
            if rule(m):
                return name
    return None


def build(game):
    scripts = os.path.join(package(game), "scripts")
    modules = load_modules(game)
    forced = [(re.compile(rx), name) for name, m in modules.items() for rx in m.get("matches", [])]
    index = {"game": game, "scripts": [], "functions": []}
    for u in units(scripts):
        kind, owner, frame, events, files, body = u[:6]
        unit = {"kind": kind, "owner": owner, "frame": frame, "files": files}
        if events:
            unit["events"] = events
        if len(u) > 6:
            unit["clip"] = u[6]
        pattern, arg = pattern_of(body)
        if pattern:
            unit["pattern"] = pattern
            unit["patternArgs"] = arg
        ctx = Context(forced)
        acts = ctx.actions(body)
        acts = [a for a in acts if a.get("do") == "function"] + [a for a in acts if a.get("do") != "function"]
        native, calls = ctx.native, ctx.calls
        for st in ctx.functions:
            index["functions"].append({"name": st[1], "owner": owner, "frame": frame, "kind": kind,
                                       "params": st[2], "source": as2.body_source(st[3]),
                                       "translated": not any(n["what"] == "function " + st[1] for n in native)})
        for n in native:
            n["module"] = module_for(modules, unit, n["what"], n["why"])
        unit["actions"] = acts
        if native:
            unit["native"] = native
        if calls:
            unit["calls"] = sorted(set(calls))
        index["scripts"].append(unit)
    out = os.path.join(scripts, "index.json")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(index, f, indent=1, ensure_ascii=False)
        f.write("\n")
    return index


# -------------------------------------------------------------- report

def walk_expr(e, fn):
    if isinstance(e, list) and e:
        if isinstance(e[0], str):
            fn(e)
        for x in e[1:] if isinstance(e[0], str) else e:
            walk_expr(x, fn)


def walk_actions(acts, fn):
    for a in acts:
        fn(a)
        for key in ("then", "else"):
            if key in a:
                walk_actions(a[key], fn)


def report(game, index):
    scripts = index["scripts"]
    translated = sum(1 for s in scripts if "native" not in s)
    natives = [(s, n) for s in scripts for n in s.get("native", [])]
    unported = [(s, n) for s, n in natives if not n["module"]]
    print("%s: %d scripts: %d fully translated, %d with native parts (%d native statements, %d unported)" % (
        game, len(scripts), translated, len(scripts) - translated, len(natives), len(unported)))

    roots = {}

    def root_use(e):
        if e[0] == "get" and e[1] == ["var", "_root"]:
            roots[e[2]] = roots.get(e[2], 0) + 1

    def scan(a):
        for key in ("on", "value", "cond", "frame", "fn"):
            if key in a:
                walk_expr(a[key], root_use)
        for x in a.get("args", []):
            walk_expr(x, root_use)
        if a.get("do") == "set" and a["on"] == ["var", "_root"] and isinstance(a["name"], str):
            roots[a["name"]] = roots.get(a["name"], 0) + 1

    for s in scripts:
        walk_actions(s["actions"], scan)
    print("\n_root variables (translated uses): " + ", ".join("%s %d" % kv for kv in sorted(roots.items())))

    labels = os.path.join(package(game), "vector", "symbols", "0.json")
    if os.path.exists(labels):
        main = json.load(open(labels, encoding="utf-8"))
        print("\nscreens (main timeline labels): " + ", ".join(
            "%s@%d" % (k, v) for k, v in sorted(main["labels"].items(), key=lambda kv: kv[1])))

    calls = {}
    sounds = set()
    for s in scripts:
        for c in s.get("calls", []):
            calls[c] = calls.get(c, 0) + 1
        if s.get("pattern") == "soundClip" and s["patternArgs"]["sound"]:
            sounds.add(s["patternArgs"]["sound"])

        def sound_arg(a):
            if a.get("do") == "call":
                name = as2_path(a["fn"])
                if name and "sound" in name.lower():
                    for x in a["args"]:
                        if x[0] == "lit" and isinstance(x[1], str):
                            sounds.add(x[1])

        walk_actions(s["actions"], sound_arg)
    print("\nfunctions called (scripts calling): " + ", ".join(
        "%s %d" % kv for kv in sorted(calls.items(), key=lambda kv: -kv[1])))
    print("\nsounds named: " + ", ".join(sorted(sounds)))

    by_module = {}
    for s, n in natives:
        by_module.setdefault(n["module"] or "UNPORTED", []).append((s, n))
    for m in sorted(by_module, key=lambda m: (m == "UNPORTED", m)):
        print("\nmodule %s: %d native statements" % (m, len(by_module[m])))
        if m == "UNPORTED":
            for s, n in by_module[m]:
                print("  %s %d frame %d: %s" % (s["kind"], s["owner"], s["frame"], n["what"]))


def as2_path(e):
    if e[0] == "var":
        return e[1]
    if e[0] == "get":
        base = as2_path(e[1])
        return base and base + "." + e[2]
    return None


def diff(index, other_game):
    other = json.load(open(os.path.join(package(other_game), "scripts", "index.json"), encoding="utf-8"))
    mine = {f["name"]: f for f in index["functions"]}
    theirs = {f["name"]: f for f in other["functions"]}
    same = sorted(n for n in mine if n in theirs and mine[n]["source"] == theirs[n]["source"])
    changed = sorted(n for n in mine if n in theirs and mine[n]["source"] != theirs[n]["source"])
    print("\nfunctions shared with %s: %d the same, %d changed" % (other_game, len(same), len(changed)))
    print("  same: " + ", ".join(same))
    print("  changed: " + ", ".join(changed))
    print("  only here: " + ", ".join(sorted(n for n in mine if n not in theirs)))


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    game = args.pop(0)

    def option(name):
        if name in args:
            i = args.index(name)
            v = args[i + 1]
            del args[i:i + 2]
            return v
        return None

    swf, other = option("--swf"), option("--diff")
    if not os.path.isdir(os.path.join(package(game), "scripts")) or swf:
        if not swf:
            sys.exit("no scripts/ in the package: give --swf")
        export(game, swf)
    index = build(game)
    if "--report" in args:
        report(game, index)
    if other:
        diff(index, other)


if __name__ == "__main__":
    main()
