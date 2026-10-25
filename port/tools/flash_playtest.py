#!/usr/bin/env python3
"""Scripted play-throughs of the Flash rebuilds (Christmas, AS1) with the real tools.

Each scenario starts the game in debug mode with a scratch save, plays with
injected clicks, drags and keys (AS_CLICKS, AS_DRAG, AS_KEYS), and checks
the injury trace (AS_TRACE) for the progress the player should make: an
injury coming out, a stage moving on, an organ reaching the tray. Run it
after every change to the operation.

Usage: flash_playtest.py [--game christmas|as1] [--build build-dev] [scenario...]
       (default: Christmas, all its scenarios)
"""
import math
import json
import os
import re
import subprocess
import sys
import tempfile

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The game played: its exe, package, save file and missions; from --game.
GAMES = {"christmas": ("AmateurSurgeonXmas.exe", "christmas", "christmas_save.txt", 5),
         "as1": ("AmateurSurgeon.exe", "as1", "as1_save.txt", 31)}
GAME = sys.argv[sys.argv.index("--game") + 1] if "--game" in sys.argv else "christmas"
EXE, PACKAGE, SAVE_FILE, MISSIONS = GAMES[GAME]
# The trace line of a patient's result saved (Christmas's operation writes
# its own; AS1's scripts save their SharedObject).
SAVED = {"christmas": r"saved \S*christmas_save\.txt: .*grade%d=[1-6](\s|$)",
         "as1": r"saved \S*as1_save\.txt: .*level%d=[1-6](\s|$)"}[GAME]


def menu_button(row, col):
    """The debug menu (F1): a button's centre (rows of 112x26 from (12, 40), 6 apart)."""
    return (12 + col * 118 + 56, 40 + row * 32 + 13)


# Row 0: Title, Intro, Levels; the missions four a row (as many fit); then the operation's
# items: Skip stage, Win now, Heal, +60 s, Kill patient.
MENU_MISSION = {m: menu_button(1 + (m - 1) // 4, (m - 1) % 4) for m in range(1, MISSIONS + 1)}
MENU_OP_ROW = 1 + (MISSIONS + 3) // 4
MENU_SKIP = menu_button(MENU_OP_ROW, 0)
MENU_HEAL = menu_button(MENU_OP_ROW, 2)
MENU_KILL = menu_button(MENU_OP_ROW + 1, 0)
MENU_GOD = menu_button(MENU_OP_ROW + 2, 0)  # (its own row: God mode, Smooth, Unlock all)
CARD = {"cutter": (125, 40), "tongs": (180, 40), "lighter": (230, 40), "cream": (280, 40), "stapler": (330, 40),
        "vacuum": (380, 40), "chainsaw": (425, 40), "corkscrew": (475, 40), "battery": (520, 40), "sketch": (570, 40)}


def circle(frame, cx, cy, r, steps, per):
    pts = [(cx + r * math.cos(2 * math.pi * k / steps), cy + r * math.sin(2 * math.pi * k / steps))
           for k in range(steps + 1)]
    s = "%d:%.0f:%.0f" % (frame, pts[0][0], pts[0][1])
    for p in pts[1:]:
        s += ":%.0f:%.0f:%d" % (p[0], p[1], per)
    return s


class Run:
    """One game run: input lists built up, then played."""

    def __init__(self, mission, skips=0):
        self.clicks, self.drags, self.keys, self.hovers = [], [], [], []
        mx, my = MENU_MISSION[mission]
        self.key(58, "f1")
        self.click(60, mx, my)
        self.frame = 100
        for _ in range(skips):  # stages before the one tested
            self.key(self.frame, "f1")
            self.click(self.frame + 2, *MENU_SKIP)
            self.frame += 60

    def click(self, f, x, y):
        self.hover(f, x, y)
        self.clicks.append("%d:%d:%d" % (f, x, y))

    def hover(self, f, x, y):
        # (the pointer onto the point a frame before a press there, as a
        # player's: the stage's buttons and the scripts' cursor are where it
        # was last moved)
        self.hovers.append("%d:%d:%d:%d:%d:1" % (f - 1, x, y, x, y))

    def key(self, f, k):
        self.keys.append("%d:%s" % (f, k))

    def tool(self, name, wait=10):
        self.click(self.frame, *CARD[name])
        self.frame += wait

    def drag(self, *points, per=20, wait=10):
        """A press at the first point, through the others (each `per` frames)."""
        self.hover(self.frame, *points[0])
        s = "%d:%d:%d" % (self.frame, points[0][0], points[0][1])
        for p in points[1:]:
            s += ":%d:%d:%d" % (p[0], p[1], per)
        self.drags.append(s)
        self.frame += per * (len(points) - 1) + wait

    def path(self, spec, frames):
        f, x, y = (int(v) for v in spec.split(":")[:3])
        self.hover(f, x, y)
        self.drags.append(spec)
        self.frame += frames

    def play(self, exe, save):
        env = dict(os.environ, AS_HIDDEN="1", AS_TRACE="1", AS_AUDIO_AVOID="A50", AS_SAVE_DIR=save, AS_CLICKS=",".join(self.clicks),
                   AS_DRAG=",".join(self.drags), AS_KEYS=",".join(self.keys), AS_HOVER=",".join(self.hovers),
                   AS_SHOT=os.path.join(save, "end") + ":%d" % (self.frame + 60))
        p = subprocess.run([exe, "-d"], env=env, capture_output=True, text=True, timeout=300, cwd=os.path.dirname(exe))
        return p.stdout


def tray(run):
    run.drag((60, 385), (62, 386), per=10)


class Timeout(Exception):
    pass


class Live:
    """A game driven as it runs (AS_CONTROL): input aimed at what the trace
    reports (an injury's hit pieces, its glass), stage by stage."""

    TRAY = (60, 385)

    def __init__(self, exe, save, mission, seed_save=None):
        env = dict(os.environ, AS_HIDDEN="1", AS_TRACE="1", AS_CONTROL="1", AS_SAVE_DIR=save,
                   AS_AUDIO_AVOID="A50")  # (its sound off the player's headset)
        if seed_save:  # progress to start from (the save file's lines)
            open(os.path.join(save, SAVE_FILE), "w", newline="\n").write(seed_save)
        self.p = subprocess.Popen([exe, "-d"], env=env, cwd=os.path.dirname(exe), stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1,
                                  encoding="utf-8", errors="replace")
        self.save = save
        self.selected = None
        self.actions = []  # "<frame> <command>": the input given, for a Ruffle replay
        self.log = []
        self.frames = []  # the frame each log line came by
        self.frame = 0
        self._read()
        self.run(58)
        if mission:  # straight into the operation, through the debug menu
            self.send("key %d:f1" % self.frame)
            self.run(2)
            self.send("click %d:%d:%d" % ((self.frame,) + MENU_MISSION[mission]))
            self.actions = []  # (the debug menu has no Ruffle counterpart)
        self.opStart = self.frame
        self.run(40)

    def send(self, line):
        if line.split(" ", 1)[0] in ("click", "drag", "shot", "down", "move", "up"):
            self.actions.append("%d %s" % (self.frame, line))
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()

    def _read(self):
        new = []
        for line in self.p.stdout:
            line = line.rstrip("\n")
            if line.startswith("@frame "):
                self.frame = int(line[7:])
                self.log += new
                self.frames += [self.frame] * len(new)
                return new
            new.append(line)
        self.log += new
        self.frames += [self.frame] * len(new)
        raise Timeout("the game ended at frame %d" % self.frame)

    def run(self, frames):
        self.send("run %d" % frames)
        return self._read()

    def mark(self):
        return len(self.log)

    def wait_for(self, pattern, limit=900, step=5, since=None):
        """Runs until a trace line from `since` (a mark; default now)
        matches; the match."""
        start = len(self.log) if since is None else since
        rx = re.compile(pattern)
        end = self.frame + limit
        while True:
            for line in self.log[start:]:
                m = rx.search(line)
                if m:
                    return m
            if self.frame >= end:
                break
            self.run(step)
        raise Timeout("no '%s' within %d frames (frame %d)" % (pattern, limit, self.frame))

    def shot(self, name):
        self.send("shot " + os.path.join(self.save, name + ".png"))
        self.run(1)

    def close(self):
        try:
            self.send("quit")
            self.p.wait(timeout=20)
        except Exception:
            self.p.kill()

    # ---- input, from the current frame
    def tool(self, name):
        # (a click on the tool in hand puts it down)
        if self.selected == name:
            return
        self.send("move %d %d" % CARD[name])  # (over it first, as a player's pointer: its rollOver)
        self.run(1)
        self.send("click %d:%d:%d" % ((self.frame,) + CARD[name]))
        self.run(10)
        self.selected = name

    def drag(self, points, speed=4.0, hold=0):
        """A press held along the points (speed: pixels a frame), let go."""
        self.send("move %d %d" % points[0])  # (over it first, as a player's pointer)
        self.run(1)
        spec = "%d:%d:%d" % (self.frame, points[0][0], points[0][1])
        total = 0
        for a, b in zip(points, points[1:]):
            n = max(2, int(math.hypot(b[0] - a[0], b[1] - a[1]) / speed))
            spec += ":%d:%d:%d" % (b[0], b[1], n)
            total += n
        if hold:
            spec += ":%d:%d:%d" % (points[-1][0], points[-1][1], hold)
            total += hold
        self.send("drag " + spec)
        self.run(total + 4)

    # ---- what the trace says
    def injury(self, make):
        """The latest report of an injury: (stage, glass (x, y), {piece: (x, y)})."""
        head = re.compile(r"^injury (?:created|stage) %s '[^']*' stage (\d+) at \S+ rot \S+ glass (-?\d+),(-?\d+)" % make)
        for k in range(len(self.log) - 1, -1, -1):
            m = head.match(self.log[k])
            if not m:
                continue
            pieces = {}
            for line in self.log[k + 1:]:
                if not line.startswith("  "):  # (its block: holder frame, hitbox, hit pieces)
                    break
                h = re.match(r"^  hit (\S+) (-?\d+),(-?\d+)", line)
                if h:
                    pieces[h.group(1)] = (int(h.group(2)), int(h.group(3)))
            return int(m.group(1)), (int(m.group(2)), int(m.group(3))), pieces
        raise KeyError(make)


def line_order(points):
    """Points along a line, from one end to the other."""
    pts = list(points)
    if len(pts) < 3:
        return pts
    far = lambda p, qs: max(qs, key=lambda q: math.hypot(q[0] - p[0], q[1] - p[1]))
    start = far(far(pts[0], pts), pts)
    path = [start]
    left = [p for p in pts if p != start]
    while left:
        nxt = min(left, key=lambda q: math.hypot(q[0] - path[-1][0], q[1] - path[-1][1]))
        path.append(nxt)
        left.remove(nxt)
    return path


def saw(live, make):
    """The chainsaw along the injury's dotted line (its hit pieces)."""
    stage, _, pieces = live.injury(make)
    live.tool("chainsaw")
    live.drag(line_order(pieces.values()), speed=6)
    return stage


def lift(live, make):
    """The tongs: the injury's loose piece (its glass) onto the tray."""
    stage, glass, _ = live.injury(make)
    live.tool("tongs")
    live.drag([glass, (glass[0] + 2, glass[1] + 2), Live.TRAY], speed=6, hold=10)
    return stage


def cut(live, make):
    """The cutter along the injury's line (its hit pieces)."""
    stage, _, pieces = live.injury(make)
    live.tool("cutter")
    pts = line_order(pieces.values())
    # from just before one end to just past the other
    (x0, y0), (x1, y1) = pts[0], pts[-1]
    d = math.hypot(x1 - x0, y1 - y0) or 1
    ux, uy = (x1 - x0) / d, (y1 - y0) / d
    live.drag([(x0 - 12 * ux, y0 - 12 * uy)] + pts + [(x1 + 12 * ux, y1 + 12 * uy)], speed=3)
    return stage


def rub(live, tool, make, passes=3, speed=2):
    """The lighter or cream worked back and forth over the injury's pieces."""
    stage, _, pieces = live.injury(make)
    live.tool(tool)
    pts = line_order(pieces.values())
    path = []
    for k in range(passes):
        path += pts if k % 2 == 0 else pts[::-1]
    live.drag(path, speed=speed)
    return stage


# ---------------------------------------------------------------- playing a stage

def stage_tools():
    """Each injury type's tool per stage, from the game's data
    (games/<game>/data/gamedata.json)."""
    data = json.load(open(os.path.join(PORT, "games", PACKAGE, "data", "gamedata.json"), encoding="utf-8"))
    return {t["linkage"]: [s["tool"] for s in t["stages"]] for t in data["injuryTypes"]}


TOOLS = stage_tools()
HEAD = re.compile(r"^injury (created|stage|removed) (\w+) '([^']*)' stage (\d+) at (-?\d+),(-?\d+) rot (-?\d+) "
                  r"glass (-?\d+),(-?\d+)(?: level (\d+))?(?: glassbox (-?\d+),(-?\d+),(-?\d+),(-?\d+))?(?: id (\d+))?")


class Seen:
    """An injury as the trace last reported it."""

    def __init__(self, m, pieces, index):
        self.make, self.frame = m.group(2), m.group(3)
        self.stage = int(m.group(4))
        self.x, self.y, self.rotation = int(m.group(5)), int(m.group(6)), int(m.group(7))
        self.glass = (int(m.group(8)), int(m.group(9)))
        self.level = int(m.group(10) or 0)  # its body level (layer)
        self.glassbox = tuple(int(v) for v in m.group(11, 12, 13, 14)) if m.group(11) else None
        self.id = int(m.group(15)) if m.group(15) else None
        self.pieces = pieces
        self.index = index  # its trace line

    @property
    def key(self):
        return self.id if self.id else (self.make, self.frame, self.x, self.y)

    def progress(self):
        """A trace line that this injury moved on (a stage, or out; or an
        organ put in, as the five pies one by one)."""
        if self.id:
            return (r"^injury (stage|removed) %s .* id %d$|^dropped %s at .*: in$|^cut through %s to .* id %d$"
                    % (self.make, self.id, self.make, self.make, self.id))
        return r"^injury (stage|removed) %s '%s' stage \d+ at %d,%d" % (self.make, re.escape(self.frame), self.x, self.y)

    def now(self, live, ahead=0):
        """Where it is now (a critter moves), or `ahead` frames on at the
        pace of its last move."""
        if self.id:
            seen = []
            for line in reversed(live.log):
                m = re.match(r"^critter %d at (-?\d+),(-?\d+)" % self.id, line)
                if m:
                    seen.append((int(m.group(1)), int(m.group(2))))
                    if not ahead or len(seen) == 2:
                        break
            if seen:
                (x, y), (px, py) = seen[0], seen[-1]
                return x + (x - px) * ahead, y + (y - py) * ahead
        return self.x, self.y

    @property
    def tool(self):
        tools = TOOLS.get(self.make, [])
        return tools[self.stage - 1] if 0 < self.stage <= len(tools) else ""

    def __repr__(self):
        return "%s '%s' stage %d (%s) at %d,%d" % (self.make, self.frame, self.stage, self.tool, self.x, self.y)


def alive(live, everywhere=False):
    """The injuries on the patient now, by the trace: those on the body
    level the mission stage is on (the ones that count), or all."""
    found = {}
    level = 0
    for k, line in enumerate(live.log):
        b = re.match(r"^mission stage \d+ body level (\d+)", line)
        if b:
            level = int(b.group(1))
        m = HEAD.match(line)
        if not m:
            continue
        pieces, slots, hitbox, loose = {}, {}, None, {}
        for l2 in live.log[k + 1:]:
            if l2.startswith("  holder frame "):
                continue
            lp = re.match(r"^  loose (\d+) (-?\d+),(-?\d+)", l2)
            if lp:
                loose[int(lp.group(1))] = (int(lp.group(2)), int(lp.group(3)))
                continue
            b2 = re.match(r"^  hitbox (-?\d+),(-?\d+),(-?\d+),(-?\d+)", l2)
            if b2:
                hitbox = tuple(map(int, b2.groups()))
                continue
            h = re.match(r"^  hit (\S+) (-?\d+),(-?\d+)", l2)
            if h:
                pieces[h.group(1)] = (int(h.group(2)), int(h.group(3)))
                continue
            q = re.match(r"^  piece (\d+) at (-?\d+),(-?\d+) slot (-?\d+),(-?\d+) box (-?\d+),(-?\d+),(-?\d+),(-?\d+)", l2)
            if q:
                v = list(map(int, q.groups()))
                slots[v[0]] = {"at": (v[1], v[2]), "slot": (v[3], v[4]), "box": tuple(v[5:9])}
                continue
            break
        s = Seen(m, pieces, k)
        s.slots = slots
        s.hitbox = hitbox  # (AS1's trace: where its hit area is)
        s.loose = loose  # (AS1's loose pieces still out, for the tray)
        if m.group(1) == "removed":
            found.pop(s.key, None)
        else:
            if s.key in found and not s.level:
                s.level = found[s.key].level
            found[s.key] = s
    return [s for s in found.values() if everywhere or not level or s.level == level]


def organ_on_tray(live, inj):
    """Where the trace last put this injury's organ on the tray, if it is
    there now (the injury has not moved on since)."""
    # (where it is, give or take a few pixels: AS1's injuries shift with the
    # screen's shake)
    near = lambda x, y: abs(x - inj.x) <= 8 and abs(y - inj.y) <= 8
    for k in range(len(live.log) - 1, -1, -1):
        m = re.match(r"^organ \w+ on the tray at (-?\d+),(-?\d+) for (\w+) at (-?\d+),(-?\d+)", live.log[k])
        if m and m.group(3) == inj.make and near(int(m.group(4)), int(m.group(5))):
            # (put in since: it moved on in a later frame; the same frame's
            # lines are what put the organ out)
            for j, l in enumerate(live.log[k + 1:]):
                h = HEAD.match(l)
                if (h and h.group(1) != "created" and h.group(2) == inj.make and near(int(h.group(5)), int(h.group(6)))
                        and live.frames[k + 1 + j] > live.frames[k]):
                    return None
            return int(m.group(1)), int(m.group(2))
    return None


FIELD = (65, 105, 535, 395)  # where a press reaches the patient (not the HUD or the dialogue strip)


def in_field(p):
    return min(max(p[0], FIELD[0]), FIELD[2]), min(max(p[1], FIELD[1]), FIELD[3])


def pull_out(live, inj, progress):
    """The tongs on its loose piece (glass), pulled slowly out along its way
    (rotation - 90 degrees) until it comes, then carried onto the tray
    without letting go."""
    hb = getattr(inj, "hitbox", None)
    if inj.glass != (0, 0):
        g = inj.glass
    elif hb:  # (no glass: a transplant's piece, taken from its hit area, in the field)
        f = in_field(((hb[0] + hb[2]) // 2, (hb[1] + hb[3]) // 2))
        g = (min(max(f[0], hb[0] + 4), hb[2] - 4), min(max(f[1], hb[1] + 4), hb[3] - 4))
    else:
        g = (inj.x, inj.y)
    if inj.glassbox and inj.glassbox[2] > inj.glassbox[0] and g != in_field(g):
        # where the piece is in the field (a player takes it there)
        b = inj.glassbox
        g = in_field(g)
        g = (min(max(g[0], b[0] + 4), b[2] - 4), min(max(g[1], b[1] + 4), b[3] - 4))
    if hb and inj.glassbox and not (hb[0] <= g[0] <= hb[2] and hb[1] <= g[1] <= hb[3]):
        # where the glass and the hit area meet (the tongs take it only there)
        b = inj.glassbox
        x0, y0, x1, y1 = max(b[0], hb[0]), max(b[1], hb[1]), min(b[2], hb[2]), min(b[3], hb[3])
        if x0 <= x1 and y0 <= y1:
            g = ((x0 + x1) // 2, (y0 + y1) // 2)
    a = math.radians(inj.rotation - 90)
    dx, dy = math.cos(a), math.sin(a)
    mark = live.mark()
    x, y = g
    live.send("move %d %d" % (x, y))
    live.run(1)
    live.send("down %d %d" % (x, y))
    live.run(1)
    out = False
    # (slowly: the glass slides a tenth of the tongs' way from the wound a
    # frame, and the tongs must stay on it)
    for k in range(1, 120):
        if any(re.search(progress, l) for l in live.log[mark:]):
            out = True
            break
        x, y = g[0] + dx * 1.2 * k, g[1] + dy * 1.2 * k
        live.send("move %d %d" % (x, y))
        live.run(1)
    if out:
        tx, ty = Live.TRAY
        n = max(1, int(math.hypot(tx - x, ty - y) / 8))
        for k in range(1, n + 1):
            live.send("move %d %d" % (x + (tx - x) * k / n, y + (ty - y) * k / n))
            live.run(1)
        x, y = tx, ty
        live.run(5)
    live.send("up %d %d" % (x, y))
    live.run(3)


def solve_puzzle(live, inj):
    """A puzzle's pieces taken to their slots with the tongs, one by one. The
    tongs take the last piece under them, so each is taken at a point no
    later piece covers (and off the HUD)."""
    left = dict(inj.slots)
    inside = lambda p, b, m=6: b[0] - m <= p[0] <= b[2] + m and b[1] - m <= p[1] <= b[3] + m
    while left:
        for k in sorted(left):
            b = left[k]["box"]
            others = [left[j]["box"] for j in left if j > k]
            spot = next(((x, y) for y in range(b[1] + 3, b[3] - 2, 3) for x in range(b[0] + 3, b[2] - 2, 3)
                         if 100 < y < 395 and 20 < x < 580 and not any(inside((x, y), o) for o in others)), None)
            if spot:
                slot = left.pop(k)["slot"]
                stroke(live, [spot, slot], 5, hold=4)
                break
        else:
            return  # no piece free to take


DIALOGUE_STRIP = (300, 435)


def press(live, x, y):
    """The button pressed at (x, y) for the tool in hand. If the press did
    not reach it (the dialogue strip over the tray, say), the strip is
    clicked away first, as a player would, and it is pressed again."""
    live.send("move %d %d" % (x, y))  # (the pointer there first, as a player's: the cursor clip follows it each frame)
    live.run(1)
    mark = live.mark()
    live.send("down %d %d" % (x, y))
    live.run(1)
    if live.selected and not any(l.startswith("tool ") and " down at " in l for l in live.log[mark:]):
        live.send("up %d %d" % (x, y))
        live.run(2)
        live.send("move %d %d" % DIALOGUE_STRIP)
        live.run(1)
        live.send("click %d:%d:%d" % ((live.frame,) + DIALOGUE_STRIP))
        live.run(6)
        live.send("move %d %d" % (x, y))
        live.run(1)
        live.send("down %d %d" % (x, y))
        live.run(1)


def stroke(live, pts, speed, stop=None, hold=0):
    """A press at the first point moved along the points (`speed` pixels a
    frame), one frame at a time, let go at once when a trace line matches
    `stop` (the progress looked for; a player stops there too) or at the
    end, after `hold` frames still. True if `stop` was met."""
    mark = live.mark()
    rx = re.compile(stop) if stop else None
    done = lambda: rx is not None and any(rx.search(l) for l in live.log[mark:])
    steps = []
    for a, b in zip(pts, pts[1:]):
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / speed))
        steps += [(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(1, n + 1)]
    steps += [pts[-1]] * hold
    x, y = pts[0]
    press(live, x, y)
    for x, y in steps:
        if done():
            break
        live.send("move %d %d" % (x, y))
        live.run(1)
    live.send("up %d %d" % (x, y))
    live.run(2)
    return done()


def treat(live, inj):
    """One go at the injury's stage with the tool it needs."""
    tool = inj.tool
    pts = line_order(inj.pieces.values())
    if inj.make == "surfaceExtraction" and inj.frame == "ice":
        tool = "lighter"  # (missions 3 and 4: the lighter melts ice)
        pts = [inj.glass]
    progress = inj.progress()
    hb = getattr(inj, "hitbox", None)
    if tool in ("chainsaw", "cutter") and len(pts) < 2 and hb:
        # one drawn hit area: across it, through its middle
        y = pts[0][1] if pts else (hb[1] + hb[3]) // 2
        pts = [in_field((hb[0] + 4, y)), in_field((hb[2] - 4, y))]
    if tool in CARD:
        live.tool(tool)
    if inj.make in CRITTERS and tool in ("battery", "chainsaw"):
        strike(live, inj, progress)
    elif tool == "chainsaw" and inj.make.endswith("Nest"):
        # held on it: AS1's nests take twenty frames of the saw a hit, three hits
        c = (sum(p[0] for p in pts) // len(pts), sum(p[1] for p in pts) // len(pts)) if pts else (inj.x, inj.y)
        c = in_field(c)
        stroke(live, [c, (c[0] + 1, c[1])], 1, progress, hold=150)
    elif tool == "chainsaw":
        stroke(live, pts if len(pts) >= 2 else [(inj.x, inj.y), (inj.x + 2, inj.y + 2)], 6, progress)
    elif tool == "cutter":
        if len(pts) >= 2:
            if pts[0][1] > pts[-1][1]:
                pts = pts[::-1]  # from the upper end (the lower may be under the dialogue strip)
            (x0, y0), (x1, y1) = pts[0], pts[-1]
            d = math.hypot(x1 - x0, y1 - y0) or 1
            ux, uy = (x1 - x0) / d, (y1 - y0) / d
            pts = [in_field((x0 - 12 * ux, y0 - 12 * uy))] + pts + [in_field((x1 + 12 * ux, y1 + 12 * uy))]
            # (a poison takes three cuts through, 35 of its 100 each; each
            # leaves a cut over it to close before the next)
        stroke(live, pts if len(pts) >= 2 else [(inj.x, inj.y), (inj.x + 2, inj.y + 2)], 6, progress)
    elif tool == "stapler":
        mark = live.mark()
        for x, y in pts:
            live.send("move %d %d" % (x, y))
            live.run(1)
            live.send("click %d:%d:%d" % (live.frame, x, y))
            live.run(10)
            if any(re.search(progress, l) for l in live.log[mark:]):
                break
    elif tool in ("lighter", "cream"):
        path = pts + pts[::-1] + pts if len(pts) >= 2 else [pts[0] if pts else (inj.x, inj.y)] * 2
        if len(pts) < 2:
            path[1] = (path[1][0] + 2, path[1][1] + 2)
        # (AS1's explosive glass slowly: it burns ten frames a piece)
        stroke(live, path, 2 if inj.make == "explosiveExtraction" else 4, progress, hold=40)
    elif tool == "tongs" and getattr(inj, "loose", None):
        # each loose piece taken to the tray and let go there
        for k in sorted(inj.loose):
            if any(re.search(progress, l) for l in live.log):
                break
            x, y = in_field(inj.loose[k])
            stroke(live, [(x, y), (x + 2, y + 2), Live.TRAY], 6, hold=4)
    elif tool == "tongs" and getattr(inj, "slots", None):
        solve_puzzle(live, inj)
    elif tool == "tongs":
        tray = organ_on_tray(live, inj)
        if tray:  # an organ to put in: from the tray to its place (its hit)
            if pts:
                to = (sum(p[0] for p in pts) // len(pts), sum(p[1] for p in pts) // len(pts))
            else:
                to = (inj.x, inj.y)
            stroke(live, [tray, (tray[0] + 2, tray[1] - 2), to], 5, hold=10)
        else:  # out: its loose piece onto the tray
            pull_out(live, inj, progress)
    elif tool == "battery" and inj.make.endswith("Nest"):
        # shocked again and again, each once it is still again (AS1's
        # nests take three, its shock playing in between)
        c = in_field((sum(p[0] for p in pts) // len(pts), sum(p[1] for p in pts) // len(pts)) if pts else (inj.x, inj.y))
        mark = live.mark()
        for _ in range(6):
            if any(re.search(progress, l) for l in live.log[mark:]):
                break
            stroke(live, [c, (c[0] + 1, c[1])], 1, progress, hold=3)
            live.run(30)
    elif tool == "battery":
        # a shock on it (where it is now, if it moves)
        if inj.make in CRITTERS:
            to = inj.now(live)
        else:
            to = (sum(p[0] for p in pts) // len(pts), sum(p[1] for p in pts) // len(pts)) if pts else (inj.x, inj.y)
        stroke(live, [in_field(to), in_field((to[0] + 1, to[1]))], 1, progress, hold=10)
    elif tool == "vacuum" and inj.make in ("fire", "batt"):  # (AS1's robot's batteries fill so)
        put_out(live, inj, progress)
    elif tool == "vacuum" or (tool in ("", "nothing") and inj.make in DRAINS):
        # Sucking up its drops (and blood) from beside it.
        live.tool("vacuum")
        # (long: AS1's poison spills some 25 drops, all to be taken up, and
        # the vacuum takes one at a time, at random among the blood near)
        follow(live, in_field((inj.x, inj.y)), 600 if inj.make in DRAINS else 250, progress)
    else:
        live.run(10)


# What spills drops the vacuum takes up (its stage "nothing": waiting on that).
DRAINS = ("water", "soot", "poison", "coffee", "sugar", "metalPoison", "advancedPoison")
CRITTERS = ("ant", "queenAnt", "spider", "queenSpider", "scorpion", "queenScorpion", "movingLeech", "centipede",
            "tapeworm", "evilTapeworm")


def traced(live, name):
    """The last value the trace gave at a tool press ("health", "vacuum blood")."""
    for line in reversed(live.log):
        m = re.match(r"^    %s (-?[\d.]+)" % name, line)
        if m:
            return float(m.group(1))
    return None


def strike(live, inj, progress, times=3):
    """A moving critter hit where it is now (the tool already in hand), the
    pointer kept on it while pressed; again, up to `times` (a scorpion
    takes three saw hits)."""
    mark = live.mark()
    inside = lambda p: FIELD[0] + 15 <= p[0] <= FIELD[2] - 15 and FIELD[1] + 15 <= p[1] <= FIELD[3] - 15
    for _ in range(times):
        if any(re.search(progress, l) for l in live.log[mark:]):
            return
        # (led by two frames, as a player's hand: the press lands a frame
        # after the pointer moves; and never at one leaving the field, as
        # AS1's battery missing stops the heart)
        if not inside(inj.now(live, 2)):
            live.run(5)
            continue
        x, y = inj.now(live, 2)
        press(live, x, y)
        for _ in range(6):
            live.run(1)
            x, y = in_field(inj.now(live, 1))
            live.send("move %d %d" % (x, y))
        live.send("up %d %d" % (x, y))
        live.run(2)


def put_out(live, inj, progress):
    """A fire: the vacuum first sucks blood up away from it (it blows out
    what it holds), then blows on the flames until they are out."""
    live.tool("vacuum")
    # (a press there tells the trace how much it holds)
    if (traced(live, "vacuum blood") or 0) < 60:
        spot = in_field((inj.x + (120 if inj.x < 300 else -120), inj.y))
        # (where the blood is, if the trace told: AS1's vacuum takes up only
        # what lies within 150 pixels of where it starts)
        blood = None
        for line in reversed(live.log):
            m = re.match(r"^    blood at (-?\d+),(-?\d+) count", line)
            if m:
                blood = (int(m.group(1)), int(m.group(2)))
                break
        hb = getattr(inj, "hitbox", None)
        if blood and hb:
            # (outside its hit area, where the vacuum would blow instead:
            # the point just beside it nearest the blood)
            # A fire grows from the box first reported: well beside it, yet
            # within the vacuum's 150 pixels of the blood.
            cx, cy = (hb[0] + hb[2]) // 2, (hb[1] + hb[3]) // 2
            sides = [p for m in (70, 50, 30)
                     for p in ((hb[0] - m, cy), (hb[2] + m, cy), (cx, hb[1] - m), (cx, hb[3] + m), (hb[0] - m, hb[1] - m),
                               (hb[2] + m, hb[1] - m), (hb[0] - m, hb[3] + m), (hb[2] + m, hb[3] + m))]
            sides = [p for p in sides if p == in_field(p) and math.hypot(p[0] - blood[0], p[1] - blood[1]) < 140]
            if sides:
                spot = sides[0]  # (the farthest out that reaches the blood)
        elif blood and math.hypot(blood[0] - inj.x, blood[1] - inj.y) > 70:
            spot = in_field(blood)
        follow(live, spot, 200)
    # (blown on its hit area: AS1's robot's batteries take it on their tops)
    hits = list(inj.pieces.values())
    at = (sum(p[0] for p in hits) // len(hits), sum(p[1] for p in hits) // len(hits)) if hits else (inj.x, inj.y)
    stroke(live, [in_field(at), in_field((at[0] + 1, at[1]))], 1, progress, hold=90)


def health(live):
    """The patient's health at the last tool press (the trace), or None."""
    for line in reversed(live.log):
        m = re.match(r"^    health (-?[\d.]+)", line)
        if m:
            return float(m.group(1))
    return None


def follow(live, at, frames, stop=None):
    """A press at `at` kept on the corkscrew's or vacuum's moving hit (the
    trace says where it is each frame) until it goes, `stop` matches, or
    `frames` pass."""
    x, y = at
    live.send("move %d %d" % (x, y))
    live.run(1)
    mark0 = live.mark()
    live.send("down %d %d" % (x, y))
    seen = False
    last = None
    missing = 0
    for _ in range(frames):
        mark = live.mark()
        live.run(1)
        if stop and any(re.search(stop, l) for l in live.log[mark0:]):
            break
        hits = [re.match(r"^follow hit (-?\d+),(-?\d+) left", l) for l in live.log[mark:]]
        hits = [h for h in hits if h]
        if not hits:
            missing += 1
            if seen and missing >= 4:
                break  # it is gone (not a frame or two of its animation without it)
            continue
        seen = True
        missing = 0
        hx, hy = int(hits[-1].group(1)), int(hits[-1].group(2))
        # (aimed where it will be next frame, as a player's hand leads it:
        # AS1's cursor clip moves to the pointer while the hit moves on)
        x, y = (hx + (hx - last[0]), hy + (hy - last[1])) if last else (hx, hy)
        last = (hx, hy)
        live.send("move %d %d" % (x, y))
    live.send("up %d %d" % (x, y))
    live.run(2)


def heal(live, near):
    """Pixie dust put in with the corkscrew, held still on the patient until
    it is in, at the injury at `near` (a corkscrew finished far from every
    injury leaves a cut)."""
    # (on the injury: there it is on the patient; off the body the
    # health does not change)
    x, y = near
    live.tool("corkscrew")
    live.send("move %d %d" % CARD["cream"])  # (over it first: its rollOver)
    live.run(1)
    live.send("click %d:%d:%d" % ((live.frame,) + CARD["cream"]))  # (the cream with it)
    live.run(10)
    # The screw's hit turns round where it went in: the pointer follows it
    # until it is all the way in (and the hit gone).
    follow(live, (x, y), 300)
    # Put down (the cream card would only add more Pixie dust to it).
    live.send("move %d %d" % CARD["corkscrew"])
    live.run(1)
    live.send("click %d:%d:%d" % ((live.frame,) + CARD["corkscrew"]))
    live.run(10)
    live.selected = None


def play_mission(live, limit=30000, shots=False):
    """Plays the operation to the end, stage after stage, with clear();
    raises Timeout if stuck or the patient is lost."""
    end = live.frame + limit
    while live.frame < end:
        if any(l.startswith("mission won") for l in live.log):
            return
        lost = [l for l in live.log if l.startswith("patient lost")]
        if lost:
            raise Timeout(lost[0])
        if not clear(live, limit=end - live.frame, shots=shots):
            if any(l.startswith("mission won") for l in live.log):
                return
            raise Timeout("stuck: %r" % alive(live))
        live.run(30)  # the next stage coming in
    raise Timeout("the mission took too long")


def urgency(inj):
    """Which injuries come first, as a player knows: what keeps hurting
    the patient (AS1's coffee and sugar race the heart towards the crash),
    and a bomb's order (AS1's patient 25: its wires cut, then its rods out,
    its fuses burnt last; anything out of turn costs time), and a nest
    before the critters it keeps sending (AS1's spiders that reach their
    mark turn to poison; a nest killed leaves a queen, who lays another)."""
    if inj.make == "bonnetTransplant":
        # (AS1's car, patient 30: its bonnet off first, covering the rest,
        # and back on last, once the car is started)
        return 0 if inj.stage <= 2 else 5
    if inj.make in ("coffee", "sugar", "wires") or inj.make.endswith("Nest") or inj.make.startswith("queen"):
        return 0
    if inj.make == "blenderTransplant":
        return 3.5  # (after its pipes: once the blender is in, the tongs take only from the tray)
    if inj.make == "startup":
        return 4  # (the car started once mended)
    if inj.make.startswith("loosePieces"):  # (the scrap over the bomb's rods)
        return 1
    if inj.make == "surfaceExtraction" and inj.frame == "rod":
        return 2
    return 4 if inj.make == "fuse" else 3


def clear(live, limit=6000, tries=4, shots=False, low=25):
    """Treats the injuries on the patient until none are left (the stage is
    done), each step aimed at what the trace reports. False if stuck."""
    end = live.frame + limit
    failures = {}
    start = live.mark()
    while live.frame < end:
        lost = [l for l in live.log[start:] if l.startswith("patient lost")]
        if lost:
            raise Timeout(lost[0])
        todo = [i for i in alive(live) if (i.tool not in ("", "nothing") or i.make in DRAINS)
                and failures.get((i.key, i.stage), 0) < tries
                # (a critter walking off the patient is not chased)
                and not (i.make in CRITTERS and i.now(live) != in_field(i.now(live)))
                # an implant waits for its organ on the tray
                and not (i.make.startswith("implant") and i.tool == "tongs" and not organ_on_tray(live, i))]
        if not todo:
            if not alive(live):
                return True
            live.run(20)  # waiting on something timed
            if all(failures.get((i.key, i.stage), 0) >= tries for i in alive(live) if i.tool not in ("", "nothing")):
                if any(i.tool not in ("", "nothing") or i.make in DRAINS for i in alive(live)):
                    return False
            continue
        # A stopped heart first: the battery on the patient starts it again.
        beats = [l for l in live.log if l in ("heart stopped", "heart started")]
        if beats and beats[-1] == "heart stopped":
            print("    heart stopped: the battery", flush=True)
            live.tool("battery")
            at = in_field((todo[0].x, todo[0].y))
            stroke(live, [at, (at[0] + 1, at[1])], 1, r"^heart started$", hold=5)
            continue
        # What comes first (urgency), then the stuck ones last, then (as a
        # player would) the small ones first.
        # (last, the newest: on top, and what AS1's GetInjury finds first
        # where hit areas overlap, as its bomb's rods)
        inj = min(todo, key=lambda i: (urgency(i), failures.get((i.key, i.stage), 0), len(TOOLS.get(i.make, [])), -(i.id or 0)))
        h = health(live)
        if h is not None and h < low:
            print("    health %.1f: pixie dust" % h, flush=True)
            heal(live, (inj.x, inj.y))
        if shots:
            live.shot("f%05d_%s%d" % (live.frame, inj.make, inj.stage))
        mark = live.mark()
        treat(live, inj)
        try:
            live.wait_for(inj.progress(), limit=40, since=mark)
        except Timeout:
            failures[(inj.key, inj.stage)] = failures.get((inj.key, inj.stage), 0) + 1
            print("    no progress: %r" % inj, flush=True)
    return False


# ---------------------------------------------------------------- live scenarios

def live_m3_ice(live):
    """Mission 3: the ice transplant sawn along each line and lifted out, stage by stage."""
    for stage in range(1, 14):
        live.shot("stage%02d" % stage)
        mark = live.mark()
        if stage % 2 == 1 and stage < 13:
            saw(live, "iceTransplant")
        else:
            lift(live, "iceTransplant")
        live.wait_for(r"injury (stage|removed) iceTransplant", limit=120, since=mark)
    return [r"removed iceTransplant"]


def live_release_over_card(live):
    """Mission 1: the lighter, the cream and the cutter each pressed on the
    patient, dragged up onto a tool card and let go there: each one's loop
    must stop (Flash ended a tool only if let go off the buttons; its loop
    went on for good)."""
    for tool in ("lighter", "cream", "cutter"):
        live.tool(tool)
        live.drag([(300, 250), (300, 200), CARD["tongs"]], speed=6)
        live.run(30)
    return [r"stop\('%s_s'\): playing -> stopped" % t for t in ("lighter", "cream", "cutter")]


def live_m3(live):
    """Mission 3 played through: the ice transplant, the incision, the kidney."""
    live_m3_ice(live)
    live.wait_for(r"injury created incision", limit=300)
    live.shot("incision")
    mark = live.mark()
    cut(live, "incision")
    live.wait_for(r"injury (stage|removed) incision", limit=120, since=mark)
    live.wait_for(r"injury created kidneyTransplant", limit=300, since=mark)
    live.run(30)
    live.shot("kidney")
    clear(live, shots=True)
    return [r"removed iceTransplant", r"created kidneyTransplant", r"removed kidneyTransplant"]


# ---------------------------------------------------------------- scenarios

def m1_candy(r):
    """Mission 1: the candy cane pulled out along its way and dropped on the tray."""
    r.tool("tongs")
    r.drag((265, 390), (325, 395), (400, 300), (60, 380), (60, 385), per=30)
    return [r"removed surfaceExtraction 'candy'", r"created largeCut"]


def m1_cut_heals(r):
    """Mission 1: a branch out, its cut stapled shut, burnt, creamed."""
    r.tool("tongs")
    r.drag((150, 165), (215, 168), (60, 380), (60, 385), per=30)
    r.tool("stapler")
    for y in (123, 152, 171, 197):
        r.click(r.frame, 125, y)
        r.frame += 10
    r.tool("lighter")
    r.drag((125, 115), (125, 205), per=30)
    r.tool("cream")
    r.drag((125, 115), (125, 205), per=150)
    r.frame += 60
    return [r"stage largeCut '1' stage 2", r"stage largeCut '1' stage 3", r"stage largeCut '1' stage 4",
            r"removed largeCut"]


def m1_incision(r):
    """Mission 1 stage 2: the incision cut open: the heart's layer comes up."""
    r.tool("cutter")
    r.drag((333, 266), (442, 198), per=40)
    return [r"stage incision '1' stage 2", r"created treelights"]


def m1_lights(r):
    """Mission 1 stage 3: the tree lights cut and pulled out, three times."""
    r.tool("cutter")
    r.drag((186, 122), (215, 362), per=60)
    r.tool("tongs")
    r.drag((150, 150), (110, 300), (60, 385), (62, 386), per=20)
    r.tool("cutter")
    r.drag((376, 150), (383, 370), per=60)
    r.tool("tongs")
    r.drag((290, 232), (150, 320), (60, 385), (62, 386), per=20)
    r.drag((433, 241), (300, 330), (60, 385), (62, 386), per=20)
    return [r"stage treelights '1' stage 3", r"stage treelights '1' stage 5", r"removed treelights"]


def m2_eye(r):
    """Mission 2: the cut closed, the eye carried from the tray into place, burnt, creamed."""
    r.tool("stapler")
    for x, y in ((146, 232), (167, 213), (183, 201), (203, 184)):
        r.click(r.frame, x, y)
        r.frame += 10
    r.tool("lighter")
    r.drag((140, 240), (210, 178), per=30)
    r.tool("cream")
    r.drag((140, 240), (210, 178), per=150)
    r.frame = max(r.frame, 400)
    r.tool("tongs")
    r.drag((54, 432), (140, 170), per=40)
    r.tool("lighter")
    r.path(circle(r.frame, 138, 167, 11, 12, 2), 40)
    r.tool("cream")
    r.path(circle(r.frame, 138, 167, 11, 24, 6), 160)
    # The burn's last tick must not hide a piece of the cream stage's new
    # hit clip (Flash: new instances, met by for..in newest first).
    return [r"organ \d+ on the tray at \S+ for implantEye", r"stage implantEye '1' stage 3",
            r"stage implantEye '1' stage 4", r"removed implantEye", r"organ \d+ on the tray at \S+ for implantAntler"]


def m2_ribcage(r):
    """Mission 2 stage 3: the eleven bones put back: the next stage comes."""
    r.tool("tongs")
    plan = [(296, 103, 94, 142), (296, 176, 68, 200), (351, 101, 84, 321), (157, 274, 202, 190),
            (409, 114, 193, 274), (158, 101, 341, 170), (198, 103, 335, 267), (366, 176, 470, 158),
            (313, 116, 477, 202), (343, 286, 436, 250), (79, 103, 464, 289)]
    for x, y, sx, sy in plan:
        r.drag((x, y), (sx, sy), (sx, sy), per=30, wait=5)
    return [r"removed ribCagePuzzle", r"created implantLeg1"]


def m1_corkscrew_cut(r):
    """Mission 1: the corkscrew pulled out too soon cuts the skin; lighter and cream heal it."""
    r.tool("corkscrew")
    r.drag((300, 150), (301, 150), per=10)
    r.tool("lighter")
    r.drag((301, 128), (301, 172), (301, 128), per=30)
    r.tool("cream")
    r.drag((301, 128), (301, 172), (301, 128), per=60)
    return [r"created smallCut", r"stage smallCut '1' stage 2", r"removed smallCut"]


SCENARIOS = {
    "m1_candy": (1, 0, m1_candy),
    "m1_corkscrew_cut": (1, 0, m1_corkscrew_cut),
    "m1_cut_heals": (1, 0, m1_cut_heals),
    "m1_incision": (1, 1, m1_incision),
    "m1_lights": (1, 2, m1_lights),
    "m2_eye": (2, 0, m2_eye),
    "m2_ribcage": (2, 2, m2_ribcage),
}
def live_god(n):
    """Mission n played the same way with god mode on (the patient's health
    kept at 50 or more): every stage and the end checked where the driver
    is too slow to keep the patient alive (AS1's swarms, its car)."""
    def play(live):
        live.send("key %d:f1" % live.frame)
        live.run(3)
        live.send("click %d:%d:%d" % ((live.frame,) + MENU_GOD))
        live.run(3)
        live.send("key %d:f1" % live.frame)  # (the item keeps the menu open)
        live.run(3)
        play_mission(live, shots=True)
        return [r"mission won"]
    return play


def live_mission(n):
    def play(live):
        play_mission(live, shots=True)
        return [r"mission won"]
    play.__doc__ = "Mission %d played to the end, every stage with the real tools." % n
    return play


def live_story(live):
    """The game through its own menus, as a player goes (the scripts on the
    stage): a new game, the intro skipped, mission 1's story scene skipped,
    the patient started, the operation played, its won story scene and
    banners, back on the level select with the grade saved."""
    live.wait_for(r"^stage start$", limit=200, since=0)
    live.run(60)
    live.send("click %d:190:420" % live.frame)       # NEW GAME
    live.wait_for(r"^stage intro$", limit=200)
    live.run(100)
    live.send("click %d:555:432" % live.frame)       # SKIP the intro
    live.wait_for(r"^stage dialog$", limit=300)
    live.run(60)
    live.send("click %d:545:432" % live.frame)       # SKIP the story scene
    live.wait_for(r"^stage opready$", limit=300)
    live.run(40)
    live.send("click %d:520:425" % live.frame)       # START
    live.wait_for(r"^stage game$", limit=300)
    live.run(40)
    m = live.mark()
    play_mission(live)
    live.wait_for(r"^stage dialog$", limit=600, since=m)
    won = live.mark()
    for _ in range(6):  # the won story scenes skipped, then the banner's continue
        live.run(60)
        live.send("click %d:545:432" % live.frame)
        live.run(60)
        if any(re.match(r"^stage levels$", l) for l in live.log[won:]):
            break
        live.send("click %d:300:300" % live.frame)
    live.wait_for(r"^stage levels$", limit=900, since=won)
    live.run(40)
    return [r"(?m)^stage start$", r"(?m)^stage intro$", r"(?m)^stage opready$", r"(?m)^stage game$",
            r"mission won", SAVED % 1, r"(?m)^stage levels$"]


def live_story2(live):
    """Mission 2 (the Mincer) the way a player reaches it with mission 1
    done: Continue, its card on the level select, its story scene skipped,
    the patient started, the operation played, its won story scenes and
    banner, back on the level select, whose grade marks must stay put (they
    once kept rolling there, the game stuck)."""
    live.wait_for(r"^stage start$", limit=200, since=0)
    live.run(60)
    live.send("move %d %d" % STORY2["continue"])
    live.run(1)
    live.send("click %d:%d:%d" % ((live.frame,) + STORY2["continue"]))  # CONTINUE GAME
    live.wait_for(r"^stage levels$", limit=300)
    live.run(40)
    live.send("move %d %d" % STORY2["card"])
    live.run(1)
    live.send("click %d:%d:%d" % ((live.frame,) + STORY2["card"]))  # patient 2's card
    live.wait_for(r"^stage dialog$", limit=300)
    live.run(60)
    live.send("click %d:545:432" % live.frame)       # SKIP the story scene
    live.wait_for(r"^stage opready$", limit=300)
    live.run(40)
    live.send("click %d:520:425" % live.frame)       # START
    live.wait_for(r"^stage game$", limit=300)
    live.run(40)
    m = live.mark()
    play_mission(live)
    live.wait_for(r"^stage dialog$", limit=600, since=m)
    won = live.mark()
    for _ in range(12):  # the won story scenes' NEXT, then the banner's continue
        live.run(60)
        live.send("click %d:545:432" % live.frame)
        live.run(60)
        if any(re.match(r"^stage levels$", l) for l in live.log[won:]):
            break
        live.send("click %d:300:300" % live.frame)
    live.wait_for(r"^stage levels$", limit=900, since=won)
    live.run(60)
    live.shot("levels_a")
    live.run(10)
    live.shot("levels_b")
    from PIL import Image, ImageChops
    a = Image.open(os.path.join(live.save, "levels_a.png")).convert("RGB").crop((60, 40, 280, 140))
    b = Image.open(os.path.join(live.save, "levels_b.png")).convert("RGB").crop((60, 40, 280, 140))
    still = ImageChops.difference(a, b).getbbox() is None
    live.log.append("level marks %s" % ("still" if still else "moving"))
    return [r"mission won", SAVED % 2, r"(?m)^stage levels$", r"level marks still"]


# The second patient's way in: the title's CONTINUE, patient 2's card on the
# level select, and a save with patient 1 done (the save file's lines).
STORY2 = {"christmas": {"continue": (390, 420), "card": (330, 170), "save": "score1=70773\ntimer1=3699\ngrade1=4\n"},
          "as1": {"continue": (410, 435), "card": (160, 190),
                  "save": "balls=0\ntimer=0\nlevel1=4\nscore1=146\ntimer1=621\n"}}[GAME]


def live_death_final(live):
    """Mission 1: the patient killed (debug menu), then healed a few frames
    into the flatline (debug menu: as the corkscrew's Pixie dust would): the
    death must still run its course to the lost board (it once began again
    after each heal: the runner died over and over)."""
    live.run(30)
    live.send("key %d:f1" % live.frame); live.run(3)
    live.send("click %d:%d:%d" % ((live.frame,) + MENU_KILL)); live.run(10)      # Kill patient
    live.send("key %d:f1" % live.frame); live.run(3)
    live.send("click %d:%d:%d" % ((live.frame,) + MENU_HEAL)); live.run(3)       # Heal
    live.wait_for(r"^patient lost", limit=120, since=0)
    return [r"patient lost"]


LIVE = {
    "story": (None, live_story),
    "death_final": (1, live_death_final),
    "story2": (None, live_story2, STORY2["save"]),
    "m3_ice": (3, live_m3_ice),
    "m3": (3, live_m3),
    "release_over_card": (1, live_release_over_card),
}
LIVE.update({"mission%d" % n: (n, live_mission(n)) for n in range(1, MISSIONS + 1)})
LIVE.update({"god%d" % n: (n, live_god(n)) for n in range(1, MISSIONS + 1)})


def main():
    args = sys.argv[1:]
    build = "build-dev"
    if "--build" in args:
        i = args.index("--build")
        build = args[i + 1]
        del args[i:i + 2]
    if "--game" in args:
        i = args.index("--game")
        del args[i:i + 2]
    exe = os.path.join(PORT, build, EXE)
    names = args or list(SCENARIOS) + list(LIVE)
    failed = 0
    for name in names:
        save = tempfile.mkdtemp(prefix="xmas_play_")
        if name in LIVE:
            mission, fn, *seed = LIVE[name]
            live = Live(exe, save, mission, seed[0] if seed else None)
            try:
                expect = fn(live)
                error = None
            except (Timeout, OSError) as e:  # (OSError: the game is gone)
                expect, error = [], str(e) or repr(e)
            try:
                live.shot("end")
            except OSError:
                pass
            live.close()
            out = "\n".join(live.log)
            open(os.path.join(save, "trace.txt"), "w").write(
                "".join("%6d %s\n" % (f, l) for f, l in zip(live.frames, live.log)))
            with open(os.path.join(save, "actions.txt"), "w") as f:
                f.write("mission %s\nopstart %d\n" % (mission, live.opStart))
                f.write("".join(a + "\n" for a in live.actions))
            end = os.path.join(save, "end.png")
        else:
            mission, skips, fn = SCENARIOS[name]
            r = Run(mission, skips)
            expect = fn(r)
            out = r.play(exe, save)
            error = None
            end = os.path.join(save, "end_%d.png" % (r.frame + 60))
        missing = [e for e in expect if not re.search(e, out)]
        status = "ok" if not missing and not error else "FAIL (%s)" % (error or "missing: " + ", ".join(missing))
        failed += bool(missing or error)
        print("%-16s %s   [%s]" % (name, status, end), flush=True)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
