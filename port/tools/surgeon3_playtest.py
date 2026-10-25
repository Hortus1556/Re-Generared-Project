#!/usr/bin/env python3
"""Scripted play-throughs of Amateur Surgeon 3 with the real menus and tools.

The game runs in debug mode with a scratch save (AS_SAVE_DIR), hidden, fast
forwarded (AS_FAST) and driven live (AS_CONTROL): this script reads its trace
and answers with taps and drags where a player would make them. What it reads
is what a player sees: every stage shown, every injury's stage with the tools
it takes and its hit points still to treat, the creatures' places
(AS_TRACE_CREATURES), the heart and the clock, the tool buttons, the tray, the
tutorial's boxes, the tag partner's button and charge, the end.

It plays as a skilled player: what drains the heart most for the time it
takes first (each injury's drain over the time its tool needs), the creatures
that bite, spread or breed before that, the dead taken out last; each injury
with the tool that suits it: the lighter, cream and cutter stroked along its
hit points, the finger lifted as soon as it is done; the stapler pressed on
each point; the tongs pulling an embedded object out along its axis (turned
aside and round an arc near the screen's edges) to the tray; the tazer tapped
where a creature is going (in a sudden death challenge only once a hopping
one sits still); the lighter kept on a stunned frog and on a spider as it
scurries (the others run from the flame); a leech burnt then pulled
off by its head; a snake tazed while out, then sawn; a scorpion tazed,
clamped, sawn; worms shocked and taken, the giant one sawn while it looks
round; a pygmy (or its halves) taken where it walks to the tray; poisons and
boils drawn out with the injection, not cut; broken veins clamped (two
clamps at most: one vein at a time), their halves and the bones' turned into
line; transplants cut round, pieces to the tray, the new organ
on. The injection heals whenever the heart is low or falling fast (off every
injury's area, past its cooldown), the tag partner is called when charged,
frostbites where any other tool than the lighter would touch are melted
first (any other tool on one freezes the tools 5 s). It never touches what
it cannot treat (that hurts, breaks the combo, and in a sudden death
challenge kills).

A direct scenario seeds the save as a player reaching that surgery has it (the
surgeries before won, Mr Giblets selected; a challenge's main surgery won).
Every run uses AS_SEED (printed when it fails; --seed plays it again exactly).

Usage: surgeon3_playtest.py [--build build-dev] [--keep] [--seed n] [--shots n] [--keys] [--layout top|sides]
                           [--aspect w:h] [scenario...]
       (--aspect w:h: the screen's shape, AS_ASPECT, e.g. 4:3, 16:10, 20:9; the default window is 16:9)
       (--shots n: a screenshot every n frames, play_<frame>.png in the kept save)
       (--keys: the tools picked with their number keys, as the trace's "tool <name> button ... key <k>"
       gives them, instead of a click on their buttons; --layout: the HUD's layout forced, AS_HUD_LAYOUT:
       a PC has the tool bar across the top, a phone the two columns)
       scenarios: surgery1 (the first surgery from a new game, to the win);
       direct:<surgery id>[:<mode>[:<stage>]] (that surgery opened at once, AS_SURGERY, to the win);
       edited:<file> (a surgery editor's level, a .json file, opened at once, AS_EDITED, to the win)
"""
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# the screen in points: the trace is in cocos points (y up), input y down. It
# follows the window's shape (docs/as3.md): 568x320 at 16:9, the default; the
# trace's "screen <w>x<h> canvas points" says when it changes (at a screen change)
WIDTH, HEIGHT = 568, 320

# the chainsaw gets away moved more than about 7 points a frame without its
# upgrade (0x6b080: 4200 canvas points squared a second)
CHAINSAW_SPEED = 3.5
# the tray rises a quarter second once something is taken (0x39d3c): what is
# let go of over it before it is up goes back
TRAY_RISE = 16

# the menus' buttons (screen points, y down, as at 568x320) and what their
# layouts anchor them to: the canvas's middle, a corner of the screen
TITLE_PLAY = ("canvas", 300, 270)
DIALOGUE_SKIP = ("bottomleft", 40, 295)
PREOP_START = ("bottomright", 520, 295)


def menu_point(button):
    """A menu button's place on the screen as it is now (its layout's anchor
    kept: the screen is 512 to 640 points wide, 320 to 384 high)."""
    anchor, x, y = button
    dx, dy = WIDTH - 568, HEIGHT - 320
    if anchor == "canvas":
        return (int(round(x + dx / 2)), int(round(y + dy / 2)))
    if anchor == "bottomleft":
        return (x, y + dy)
    return (x + dx, y + dy)


class Timeout(Exception):
    pass


class Live:
    """The game, driven as it runs."""

    def __init__(self, exe, save, extra_env=None, shot_every=0):
        env = dict(os.environ, AS_HIDDEN="1", AS_FAST="1", AS_CONTROL="1", AS_SAVE_DIR=save,
                   AS_AUDIO_AVOID="A50", AS_TRACE_CREATURES="1")
        env.update(extra_env or {})
        self.p = subprocess.Popen([exe, "-d"], env=env, cwd=os.path.dirname(exe), stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
                                  errors="replace", bufsize=1)
        self.save = save
        self.log = []
        self.frames = []  # the frame each line of the log was read at
        self.starts = []  # and the frame the run it came in began at
        self.guard = None
        self.frame = 0
        self.shot_every = shot_every
        self.next_shot = 0
        self._read()

    def send(self, line):
        # (what was done, kept in the trace for reading a failed run)
        if not line.startswith("run "):
            self.log.append(">> " + line)
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()

    def _read(self):
        for line in self.p.stdout:
            line = line.rstrip("\n")
            m = re.match(r"screen (\d+)x(\d+) canvas points", line)
            if m:
                global WIDTH, HEIGHT
                WIDTH, HEIGHT = int(m[1]), int(m[2])
            if line.startswith("@frame "):
                # (each line known to have come between the run's first frame
                # and its last)
                self.starts.extend([self.frame] * (len(self.log) - len(self.starts)))
                self.frame = int(line[7:])
                self.frames.extend([self.frame] * (len(self.log) - len(self.frames)))
                return
            self.log.append(line)
        raise Timeout("the game ended (crashed?) at frame %d" % self.frame)

    def run(self, frames):
        # (--shots n: a screenshot every n frames into the save, to look at the
        # play afterwards)
        if self.shot_every and self.frame >= self.next_shot:
            self.send("shot " + os.path.join(self.save, "play_%06d.png" % self.frame))
            self.next_shot = self.frame + self.shot_every
        self.send("run %d" % frames)
        self._read()

    def find(self, pattern, since=0):
        rx = re.compile(pattern)
        for line in self.log[since:]:
            m = rx.search(line)
            if m:
                return m
        return None

    def wait_for(self, pattern, limit=1800, step=10, since=None):
        start = len(self.log) if since is None else since
        end = self.frame + limit
        while True:
            m = self.find(pattern, start)
            if m:
                return m
            if self.frame >= end:
                raise Timeout("no '%s' within %d frames (frame %d)" % (pattern, limit, self.frame))
            self.run(step)

    def blocked(self, points):
        """Whether the player would hold back this touch (guard: a hook the
        surgery sets, given the touch's path)."""
        if not self.guard:
            return False
        path = [points[0]]
        for a, b in zip(points, points[1:]):
            n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 10))
            path += [(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(1, n + 1)]
        if self.guard(path):
            return False
        self.run(2)
        return True

    def click(self, x, y, settle=8):
        if self.blocked([(x, y)]):
            return
        self.send("click %d:%d:%d" % (self.frame, x, y))
        self.run(settle)

    def key(self, k, settle=4):
        self.send("key %d:%s" % (self.frame, k))
        self.run(settle)

    def drag(self, points, speed=3.0, hold=0, settle=4):
        """A press held along the points (speed: points a frame, or one for
        each leg), let go."""
        if self.blocked(points):
            return
        spec = "%d:%d:%d" % (self.frame, points[0][0], points[0][1])
        total = 0
        speeds = speed if isinstance(speed, list) else [speed] * (len(points) - 1)
        for a, b, v in zip(points, points[1:], speeds):
            n = max(2, int(math.hypot(b[0] - a[0], b[1] - a[1]) / v))
            spec += ":%d:%d:%d" % (b[0], b[1], n)
            total += n
        if hold:
            spec += ":%d:%d:%d" % (points[-1][0], points[-1][1], hold)
            total += hold
        self.send("drag " + spec)
        self.run(total + settle)

    def stroke(self, points, speed=3.0, hold=0, stop=None):
        """A press held along the points live, a frame at a time, let go at
        the end or as soon as stop() says (the finger lifted once what it
        treats is done, so it does not go on into what comes next)."""
        if self.blocked(points):
            return
        path = [points[0]]
        for a, b in zip(points, points[1:]):
            n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / speed))
            path += [(round(a[0] + (b[0] - a[0]) * k / n), round(a[1] + (b[1] - a[1]) * k / n)) for k in range(1, n + 1)]
        path += [path[-1]] * hold
        self.send("down %d %d" % path[0])
        at = path[0]
        for p in path[1:]:
            self.run(1)
            if stop and stop():
                break
            self.send("move %d %d" % p)
            at = p
        self.send("up %d %d" % at)
        self.run(2)

    def press(self, spots):
        """A press held at each (x, y, frames) in turn, moved between them quickly, let go."""
        if self.blocked([(x, y) for x, y, _ in spots]):
            return
        spec = "%d:%d:%d" % (self.frame, spots[0][0], spots[0][1])
        total = 0
        for i, (x, y, n) in enumerate(spots):
            if i:
                q = spots[i - 1]
                m = max(2, int(math.hypot(x - q[0], y - q[1]) / 4))
                spec += ":%d:%d:%d" % (x, y, m)
                total += m
            spec += ":%d:%d:%d" % (x + 1, y, n)
            total += n
        self.send("drag " + spec)
        self.run(total + 4)

    def shot(self, name):
        self.send("shot " + os.path.join(self.save, name + ".png"))
        self.run(1)

    def close(self):
        try:
            self.send("quit")
            self.p.wait(timeout=20)
        except Exception:
            self.p.kill()


def screen(x, y):
    """A cocos point (y up) as an input point (y down)."""
    return (int(round(x)), int(round(HEIGHT - y)))


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


class Injury:
    RX = re.compile(r"^injury #(\d+) (\S+) state (\S*) tools (\S*) at (-?\d+),(-?\d+) rot (-?\d+) "
                    r"size (\d+),(\d+) points(.*?)(?: drain ([\d.]+))?( waits)?$")

    def __init__(self, m):
        self.id = int(m[1])
        self.cls = m[2]
        self.state = m[3]
        self.tools = [t for t in m[4].split(",") if t]
        self.x, self.y, self.rot = int(m[5]), int(m[6]), int(m[7])
        self.w, self.h = int(m[8]), int(m[9])
        self.points = [tuple(int(v) for v in p.split(",")) for p in m[10].split()]
        self.drain = float(m[11]) if m[11] else 0.1  # damage a second
        self.waits = bool(m[12])  # for others to be treated first
        self.track = []  # a creature's places: (frame, x, y)

    def axis(self):
        """Its own up (cocos), turned with it (clockwise by its rotation)."""
        r = math.radians(self.rot)
        return (math.sin(r), math.cos(r))

    def where(self, frame):
        """Where it is at that frame (a creature: from its last places)."""
        if len(self.track) < 2:
            return (self.x, self.y)
        f1, x1, y1 = self.track[-1]
        f0, x0, y0 = self.track[-2]
        if f1 == f0 or frame - f1 > 30:
            return (x1, y1)
        k = (frame - f1) / float(f1 - f0)
        return (x1 + (x1 - x0) * k, y1 + (y1 - y0) * k)


CREATURES = ("Flea", "IceAnt", "Leech", "MiniFrog", "PoisonFrog", "BlackFrog", "FireMaggot", "MiniWorm", "MiniBat",
             "RobotScorpion", "Snake", "GiantWorm", "Bat", "Spider", "Ghost", "Locust", "Jellyfish",
             "Pygmy", "PygmyHalf")

# the creatures that hop and sit a moment between hops (the locust is 1.45's
# flea: Locust.h)
HOPPERS = ("Flea", "Locust")


# a ghost's states no tool works on (hidden, dying: Ghost.h)
GHOST_AWAY = (86, 88)


def creature(inj):
    return inj.cls in CREATURES


# What a player sees hurting most, treated first (lower first): the creatures
# that bite, spread or breed, then the fire and the drops, bleeding, the rest;
# the dead taken out and the burns creamed last.
def urgency(inj):
    c, s = inj.cls, inj.state
    if s.endswith("Dead") or s.endswith("Died"):
        return 8
    if (c in CREATURES and c != "Bat") or s in ("BatFlying",):
        return 0
    if c in ("SoakableParticle", "PassiveParticle") or c.endswith("Particle"):
        return 1  # (quick to suck up; a soakable one soaks in as more)
    veins = ("BrokenVein", "BrokenLegVein", "RobotBrokenLegVein")
    if (c == "Artery" and s in ("Shut", "Burnt")) or (c in veins and s in ("Open", "Burnt")):
        return 1  # (its clamps, two at most, stay on till it is healed)
    if (s == "Open" and c == "Artery") or (s == "Broken" and c in veins):
        return 2
    if s == "Broken":
        return 6  # (a bone: once the veins over it are set)
    if c == "SuperPoisonArea":
        return -1  # (spilling drops that soak in as fire and poison)
    if c in ("SuperPoisonScab", "Boil"):
        return 3
    return 5


# About how long a treatment takes (seconds), by the tool: what it costs
# against what it stops hurting.
def treat_time(inj, tool):
    n = len(inj.points)
    return {"Stapler": 0.15 * n + 0.1, "Lighter": 0.3 + 0.06 * n, "Cream": 0.3 + 0.06 * n,
            "Cutter": 0.4 + 0.06 * n, "Chainsaw": 0.8 + 0.1 * n, "Vacuum": 0.2 * max(1, n), "Tongs": 2.0,
            "Injection": 3.0, "Clamp": 2.5, "Tazer": 0.5}.get(tool, 1.0)


class Surgery:
    """Plays the surgery on the screen from its trace."""

    def __init__(self, live):
        self.g = live
        self.buttons = {}
        self.button_sizes = {}  # (the HUD's buttons' sizes, by the trace: the bar's are smaller)
        self.keys = {}  # (their number keys)
        self.out_key = None  # (the injection's out button's key: I)
        self.pause_at = (202, 21)
        self.out_button = (483, 186)  # (the injection's out button, shut; at the side by default)
        self.top_bar = False  # (the HUD's tools in the bar across the top: "hud layout top")
        self.injuries = {}
        self.tool = None
        self.tries = {}
        self.read_from = 0
        self.tray = None
        # the tutorial: the tools it turned off, the injuries it points at,
        # its box's OK button while the box is up
        self.disabled = set()
        self.highlighted = set()
        self.ok = None
        self.popups = 0
        self.tutorial_text = ""
        # a transplant's pieces cut free (where to take them), its new organ on the tray
        self.free = {}
        self.organ = None
        # the heart and the clock; the injection's cooldown; the tools frozen
        self.heart = None
        self.heart_max = None
        self.falls = []
        self.time = None
        self.injection_ready = 0
        self.injection = None  # the plunger, full and empty points of the injection stuck in
        self.injection_menu = None  # the other mode's button beside, when its menu opened
        self.injection_out = None  # its fill when it came out
        self.injection_mode = 1  # 1 in (heals), 0 out (extracts)
        self.frozen = False
        # the tag partner: its button, the actions it needs, its charge
        self.tag = None
        self.tag_need = None
        self.tag_charge = 0
        self.tag_ok = None
        self.stage = None
        # the last clamp placed (its point, its wheel), the clamps tightened
        self.clamp = None
        self.tightened = 0
        # the injuries the clamps are on (two clamps at most, 0x694cc)
        self.clamped_on = []
        self.clamping = None
        self.clamp_spots = []  # (where they were put; they go with the injury)
        self.clamp_way = 1
        # the injuries' areas on the body (no healing there), their places
        self.areas = {}
        self.places = {}
        # a broken bone's or vein's halves: the end at the break of each, and
        # where it goes in line; the clamps shut on each injury (where)
        self.halves = {}
        self.cpoints = {}
        # the injuries tried in vain, left a moment
        self.resting = {}
        self.sudden = False
        self.bad_popups = 0
        live.guard = self.touch_ok
        # the creatures just tapped (dying: not tapped again a moment)
        self.zapped = {}
        # the worms' own states
        self.cstate = {}
        # the leeches' sizes (they burst at 1.25), burnt or not
        self.leeches = {}
        # the creatures stunned (a poison frog flashing green)
        self.stunned = set()
        # an embedded object: how far it sticks out, how far to pull it
        self.sticks = {}

    def update(self):
        for i in range(self.read_from, len(self.g.log)):
            line = self.g.log[i]
            frame = self.g.frames[i] if i < len(self.g.frames) else self.g.frame
            start = self.g.starts[i] if i < len(self.g.starts) else frame
            m = re.match(r"^creature #(\d+) \w+ state (\d+)", line)
            if m:
                # (a worm's own state: 70 a mini worm shocked, 73 the giant
                # worm looking around)
                self.cstate[int(m[1])] = int(m[2])
                continue
            m = re.match(r"^creature #(\d+) \S+ at (-?\d+),(-?\d+)", line)
            if m:
                inj = self.injuries.get(int(m[1]))
                if inj:
                    inj.x, inj.y = int(m[2]), int(m[3])
                    inj.track.append((frame, inj.x, inj.y))
                    del inj.track[:-4]
                continue
            m = re.match(r"^area #(-?\d+) (-?\d+),(-?\d+) size (\d+),(\d+) rot (-?\d+)", line)
            if m:
                self.areas.setdefault(int(m[1]), []).append(tuple(int(m[k]) for k in range(2, 7)))
                continue
            m = re.match(r"^injury #(\d+) half (\d) at (-?\d+),(-?\d+) turn (-?[\d.]+) to (-?[\d.]+) "
                         r"size (\d+),(\d+)", line)
            if m:
                self.halves.setdefault(int(m[1]), {})[int(m[2])] = (
                    screen(int(m[3]), int(m[4])), float(m[5]), float(m[6]), int(m[7]), int(m[8]))
                continue
            m = re.match(r"^injury #(\d+) clamp point (\d) at (-?\d+),(-?\d+)(?: (clamped|loose))?", line)
            if m:
                self.cpoints.setdefault(int(m[1]), {})[int(m[2])] = (screen(int(m[3]), int(m[4])), m[5] or "")
                continue
            m = re.match(r"^creature #(\d+) Leech scale ([\d.]+)( burnt)?", line)
            if m:
                self.leeches[int(m[1])] = (float(m[2]), bool(m[3]))
                continue
            m = re.match(r"^creature #(\d+) (stunned|unstunned)", line)
            if m:
                (self.stunned.add if m[2] == "stunned" else self.stunned.discard)(int(m[1]))
                continue
            m = re.match(r"^heart (-?[\d.]+) time (-?\d+)", line)
            if m:
                h = float(m[1])
                # (how fast it falls: the drops of the last seconds)
                if self.heart is not None:
                    self.falls = (self.falls + [max(0.0, self.heart - h)])[-3:]
                self.heart = h
                self.time = int(m[2])
                if self.heart_max is None or self.heart > self.heart_max:
                    self.heart_max = self.heart
                continue
            m = re.match(r"^tool (\w+) selected", line)
            if m:
                self.tool = m[1]
                continue
            m = re.match(r"^tool (\w+) button (-?\d+),(-?\d+)(?: size (\d+),(\d+) key (\S+))?", line)
            if m:
                self.buttons[m[1]] = screen(int(m[2]), int(m[3]))
                if m[4]:
                    self.button_sizes[m[1]] = (int(m[4]), int(m[5]))
                    self.keys[m[1]] = m[6]
                continue
            m = re.match(r"^injection out button (-?\d+),(-?\d+)(?: size \d+,\d+ key (\S+))?", line)
            if m:
                self.out_button = screen(int(m[1]), int(m[2]))
                self.out_key = m[3]
                continue
            m = re.match(r"^hud layout (top|sides) ", line)
            if m:
                self.top_bar = m[1] == "top"
                continue
            m = re.match(r"^pause button (-?\d+),(-?\d+)", line)
            if m:
                self.pause_at = screen(int(m[1]), int(m[2]))
                continue
            m = re.match(r"^tag partner (\w+) button (-?\d+),(-?\d+) needs (\d+)", line)
            if m:
                self.tag = screen(int(m[2]), int(m[3]))
                self.tag_need = int(m[4])
                continue
            m = re.match(r"^tag charge (\d+)/(\d+)", line)
            if m:
                self.tag_charge, self.tag_need = int(m[1]), int(m[2])
                continue
            m = re.match(r"^lowhealth ok (-?\d+),(-?\d+)", line)
            if m:
                # (the low heart rate warning: the surgery waits for its OK)
                self.ok = screen(int(m[1]), int(m[2]))
                continue
            m = re.match(r"^tag info ok (-?\d+),(-?\d+)", line)
            if m:
                self.tag_ok = screen(int(m[1]), int(m[2]))
                continue
            if line.startswith("clamp released") and self.clamped_on:
                self.clamped_on.pop()
                self.clamp_spots = self.clamp_spots[-len(self.clamped_on):] if self.clamped_on else []
                continue
            m = re.match(r"^clamp placed at (-?\d+),(-?\d+) wheel (-?\d+),(-?\d+)", line)
            if m:
                self.clamped_on.append(self.clamping)
                self.clamp_spots.append(screen(int(m[3]), int(m[4])))
                self.clamp = ((int(m[1]), int(m[2])), screen(int(m[3]), int(m[4])))
                continue
            m = re.match(r"^robotscorpion clamped: wheel at (-?\d+),(-?\d+)", line)
            if m:
                # (the clamp put on its tail: its wheel moved there)
                if self.clamp:
                    self.clamp = (self.clamp[0], screen(int(m[1]), int(m[2])))
                continue
            if line.startswith("clamp tightened"):
                self.tightened += 1
                continue
            if line.startswith("tools frozen"):
                self.frozen = True
                continue
            if line.startswith("tools unfrozen"):
                self.frozen = False
                continue
            m = re.match(r"^injection in at \S+ mode (\d) plunger (-?\d+),(-?\d+) full (-?\d+),(-?\d+)"
                         r"(?: empty (-?\d+),(-?\d+))?", line)
            if m:
                self.injection = (screen(int(m[2]), int(m[3])), screen(int(m[4]), int(m[5])),
                                  screen(int(m[6]), int(m[7])) if m[6] else None)
                continue
            m = re.match(r"^injection menu second (\w+) (-?\d+),(-?\d+)", line)
            if m:
                self.injection_menu = (m[1], screen(int(m[2]), int(m[3])))
                continue
            if line.startswith("injection menu hidden"):
                self.injection_menu = None
                continue
            m = re.match(r"^injection out fill ([\d.]+)(?: cooldown (-?[\d.]+))?", line)
            if m:
                self.injection_out = float(m[1])
                if m[2] and float(m[2]) > 0:
                    # (its in button off till the bars are full again)
                    self.injection_ready = max(self.injection_ready, frame + int(float(m[2]) * 60) + 15)
                continue
            m = re.match(r"^tutorial (?:tool|item) (\w+) (enabled|disabled)", line)
            if m:
                (self.disabled.discard if m[2] == "enabled" else self.disabled.add)(m[1])
                continue
            m = re.match(r"^tutorial highlight injury #(\d+)", line)
            if m:
                self.highlighted.add(int(m[1]))
                continue
            if line.startswith("tutorial effects removed"):
                self.highlighted.clear()
                continue
            m = re.match(r"^tutorial popup ok (-?\d+),(-?\d+): (.*)", line)
            if m:
                self.ok = screen(int(m[1]), int(m[2]))
                self.tutorial_text = m[3]
                self.popups += 1
                continue
            if line.startswith("tutorial popup: "):
                self.ok = None
                self.tutorial_text = line[16:]
                self.popups += 1
                continue
            if line.startswith("tutorial popup closed"):
                self.ok = None
                continue
            if line.startswith("bad popup"):
                self.bad_popups += 1
                continue
            m = re.match(r"^surgery (\S+)( sudden death)?$", line)
            if m:
                # (a sudden death challenge: the first miss kills)
                self.sudden = bool(m[2])
                continue
            m = re.match(r"^stage (\d+)", line)
            if m:
                self.injuries = {}
                self.stage = int(m[1])
                continue
            m = Injury.RX.match(line)
            if m:
                inj = Injury(m)
                inj.since = frame
                inj.since_early = start  # (it may have come as early as this)
                old = self.injuries.get(inj.id)
                if old and old.track:
                    inj.track = old.track
                self.injuries[inj.id] = inj
                self.places[inj.id] = (inj.x, inj.y)
                continue
            m = re.match(r"^injury #(\d+) (?:finished|gone)", line)
            if m:
                kept = [k for k, c in enumerate(self.clamped_on) if c != int(m[1])]
                self.clamp_spots = [self.clamp_spots[k] for k in kept if k < len(self.clamp_spots)]
                self.clamped_on = [self.clamped_on[k] for k in kept]
                self.injuries.pop(int(m[1]), None)
                self.areas.pop(int(m[1]), None)
                continue
            m = re.match(r"^injury #(\d+) piece (\d+) free at (-?\d+),(-?\d+)", line)
            if m:
                self.free.setdefault(int(m[1]), {})[int(m[2])] = (int(m[3]), int(m[4]))
                continue
            m = re.match(r"^injury #(\d+) sticks out (-?\d+) pull (-?\d+)", line)
            if m:
                self.sticks[int(m[1])] = (int(m[2]), int(m[3]))
                continue
            m = re.match(r"^tray organ at (-?\d+),(-?\d+)", line)
            if m:
                self.organ = (int(m[1]), int(m[2]))
                continue
            m = re.match(r"^tray at (-?\d+),(-?\d+)", line)
            if m:
                self.tray = (int(m[1]), int(m[2]))
        self.read_from = len(self.g.log)

    def ended(self):
        return self.g.find(r"^surgery (won|lost)")

    def usable(self, tool):
        return tool in self.buttons and tool not in self.disabled

    def select(self, tool):
        if tool == "Injection":
            return self.select_injection(1)
        if self.tool == tool:
            return
        if tool not in self.buttons:
            raise Timeout("the tool %s has no button" % tool)
        # (its button pressed till the game says it is the one: "tool <name>
        # selected"; a press on a turned-off button does nothing)
        for _ in range(3):
            if KEYS and tool in self.keys:
                self.g.key(self.keys[tool])  # (--keys: its number key, as a PC player)
            else:
                self.g.click(*self.buttons[tool])
            self.update()
            if self.tool == tool:
                return
            self.g.run(10)
            self.update()
        # (no word: it was the one already, which the game does not trace)
        self.tool = tool

    def to_tray(self):
        return (int(self.tray[0] + 60), int(HEIGHT - self.tray[1] + 50)) if self.tray else (80, 200)

    # ---- the heart ----

    def touch_ok(self, path):
        """Whether a touch along the path (screen points) is safe: not over a
        poison frog that is not stunned, unless with the tazer (any other
        tool on it puffs poison and puts the tool down, 0x193304: what it
        held slips back and hurts)."""
        # (never a press on the pause button: it would stop the surgery under
        # its menu; a button takes only a touch that begins on it)
        # (where the game traced it: the bar's layout moves it beside the heart)
        if abs(path[0][0] - self.pause_at[0]) < 17 and abs(path[0][1] - self.pause_at[1]) < 18:
            return False
        if self.tool in ("Tazer", "Injection"):
            return True
        frogs = [screen(*i.where(self.g.frame)) for i in self.injuries.values()
                 if i.cls == "PoisonFrog" and urgency(i) != 8 and i.id not in self.stunned]
        for p in path:
            if self.on_hud(p) or (self.ok and dist(p, self.ok) < 30):
                continue
            if any(abs(p[0] - f[0]) < 58 and abs(p[1] - f[1]) < 58 for f in frogs):
                return False
        return True

    def in_injury(self, inj, p, margin=0):
        """Whether a point (cocos) is on an injury, its size turned with it."""
        a = math.radians(inj.rot)
        dx, dy = p[0] - inj.x, p[1] - inj.y
        lx = dx * math.cos(a) - dy * math.sin(a)
        ly = dx * math.sin(a) + dy * math.cos(a)
        return abs(lx) < inj.w * 0.5 + margin and abs(ly) < inj.h * 0.5 + margin

    def in_area(self, p, margin=10):
        """Whether a screen point is in an injury's area (turned with it)."""
        x, y = p[0], HEIGHT - p[1]
        for iid, rects in self.areas.items():
            cx, cy = self.places.get(iid, (0, 0))
            for rx, ry, rw, rh, rot in rects:
                a = math.radians(rot)
                dx, dy = x - cx, y - cy
                lx = cx + dx * math.cos(a) - dy * math.sin(a)
                ly = cy + dx * math.sin(a) + dy * math.cos(a)
                if rx - margin < lx < rx + rw + margin and ry - margin < ly < ry + rh + margin:
                    return True
        return False

    def clear_spot(self):
        """A place on the body off every injury's area and the HUD, as far from
        the injuries as can be (the injection heals only there), or None."""
        best, far = None, -1
        for sx in range(70, 500, 12):
            for sy in range(30, 300, 12):
                if self.on_hud((sx, sy)) or self.in_area((sx, sy)):
                    continue
                d = min([dist((sx, sy), screen(i.x, i.y)) for i in self.injuries.values()] or [999])
                d = min(d, 120) - 0.2 * dist((sx, sy), (284, 160))
                if d > far:
                    best, far = (sx, sy), d
        return best

    def select_injection(self, mode):
        """The injection in the mode asked (1 in, 0 out): its button opens the
        menu with the other mode beside; that one pressed swaps them."""
        if self.tool == "Injection" and self.injection_mode == mode:
            return
        if mode == 1 and self.g.frame < self.injection_ready:
            # (its in button is off while it cools down: a press there goes
            # through to the patient)
            self.g.run(self.injection_ready - self.g.frame)
        if KEYS and "Injection" in self.keys and self.out_key:
            # (--keys: each mode its key, the injection's to inject, the out
            # button's (I) to draw out; one press picks it, whichever button
            # is in the injection's place)
            self.g.key(self.keys["Injection"] if mode == 1 else self.out_key.lower())
            self.injection_mode = mode
            self.update()
            return
        # (the button in the injection's place: in or out, whichever was
        # picked last; in the bar across the top the other under it, shown
        # with the menu)
        place = self.buttons["Injection"]
        if self.top_bar and self.out_button and self.out_button[1] < place[1]:
            place = self.out_button
        self.injection_menu = None
        self.g.click(*place)
        self.update()
        if not self.injection_menu:
            # (it was open: the press shut it; again opens it)
            self.g.click(*place)
            self.update()
        if self.injection_menu:
            name, at = self.injection_menu
            beside = 0 if name.endswith("_out") else 1
            if beside == mode:
                self.g.click(*at)
                self.injection_mode = mode
            else:
                self.injection_mode = 1 - beside
        self.update()

    def inject(self):
        """The injection pushed in slowly, healing (its cooldown after)."""
        spot = self.clear_spot()
        if not spot:
            self.injection_ready = self.g.frame + 120
            return
        self.select_injection(1)
        self.injection = None
        self.g.click(*spot, settle=4)
        self.update()
        if not self.injection:
            self.injection_ready = self.g.frame + 120
            return
        plunger, full, empty = self.injection
        # (the plunger moves the fill only from its empty mark to its full
        # one, at most half of that a quarter second: 0x6c430, 0x6ccd4; down
        # to the mark at once, then a full push in two thirds of a second,
        # on a little past the end so it is done)
        end = (int(full[0] + (full[0] - empty[0]) * 0.15), int(full[1] + (full[1] - empty[1]) * 0.15))
        self.g.drag([plunger, empty, end], speed=[3.0, max(0.3, dist(empty, end) / 48.0)], settle=6)
        self.injection_ready = self.g.frame + 4 * 60 + 30
        self.injection = None
        self.update()

    def extract(self, inj):
        """The injection stuck in a poison and its plunger pulled back
        slowly, drawing it out, again until it is gone: stuck in where no
        other poison is (the first one there would be drawn)."""
        self.select_injection(0)
        spots = [(inj.x + dx, inj.y + dy) for dx in (0, -25, 25) for dy in (0, -20, 20)]
        others = [i for i in self.injuries.values() if i.id != inj.id and "Injection" in i.tools]
        spot = max(spots, key=lambda p: (min([dist(p, (o.x, o.y)) for o in others] or [999]) > 90,
                                         -dist(p, (inj.x, inj.y))))
        for _ in range(5):
            self.injection = None
            self.g.click(*screen(*spot), settle=4)
            self.update()
            if not self.injection or not self.injection[2]:
                return
            plunger, full, empty = self.injection
            # (at most its whole travel a quarter second: 0x6c430; on a little
            # past the end, so it comes out)
            end = (int(empty[0] + (empty[0] - plunger[0]) * 0.2), int(empty[1] + (empty[1] - plunger[1]) * 0.2))
            self.g.drag([plunger, end], speed=max(0.5, dist(plunger, end) / 45.0), settle=6)
            self.injection = None
            self.update()
            if inj.id not in self.injuries:
                return

    def call_partner(self):
        """The tag partner called (its explanation the first time, then the high five)."""
        self.tag_ok = None
        self.g.click(*self.tag, settle=30)
        self.tag_charge = 0
        self.update()
        if self.tag_ok:
            self.g.run(60)
            self.g.click(*self.tag_ok, settle=30)
            self.tag_ok = None
        self.g.run(120)
        self.update()

    # ---- the injuries ----

    def tool_for(self, inj):
        tools = [t for t in inj.tools if self.usable(t)]
        if not tools:
            return None
        # (a pygmy: only the tongs take it out, to the tray; the others make
        # it react, and hurt the patient, Pygmy.h)
        if inj.cls in ("Pygmy", "PygmyHalf"):
            return "Tongs" if "Tongs" in tools else None
        # a poison drawn out by the injection (no drops spilt: cut, they
        # soak in as more poison, and a super poison's as fire too); the
        # tazer for a creature, the cutter for a boil
        if "Injection" in tools and (inj.cls.endswith("PoisonArea") or inj.cls == "Boil"):
            return "Injection"  # (a boil cut bursts into poison too)
        # (a frog's egg hatched with the lighter: the cutter on it would
        # swipe the poison under it first, as that comes first, 0x398e4)
        if inj.state == "MiniFrogEggClosed" and "Lighter" in tools:
            return "Lighter"
        for t in ("Tazer", "Cutter", "Lighter"):
            if t in tools and len(tools) > 1 and inj.cls not in ("Bat",):
                return t
        return tools[0]

    def creature_tap(self, inj, tool):
        """A tap where the creature is going (its place the frame the tap
        lands, from its last places); then it is left a moment to die."""
        self.select(tool)
        if not inj.track or self.g.frame - inj.track[-1][0] > 6:
            # (no fresh place: one first)
            last = inj.track[-1][0] if inj.track else -1
            for _ in range(8):
                self.g.run(1)
                self.update()
                if inj.id not in self.injuries:
                    return
                if inj.track and inj.track[-1][0] != last:
                    break
        if self.sudden and inj.cls in HOPPERS + ("MiniFrog", "Bat"):
            # (a sudden death challenge: a miss kills; one that hops tapped
            # only once it sits still, its last two places the same; an ant,
            # a poison frog or a worm walks on smoothly, where it is going is
            # safe)
            for _ in range(40):
                cur = self.injuries.get(inj.id)
                if cur is None:
                    return
                t = cur.track
                # (a flea just landed: it sits half a second, 0x18b07c)
                landed = cur.cls not in HOPPERS or (len(t) >= 3 and t[-3][1:] != t[-2][1:])
                if len(t) >= 2 and t[-1][1:] == t[-2][1:] and landed and self.g.frame - t[-1][0] <= 3:
                    inj = cur
                    break
                self.g.run(1)
                self.update()
            else:
                self.resting[inj.id] = self.g.frame + 20
                return
        cur = self.injuries.get(inj.id)
        if cur is None or cur.state != inj.state or tool not in cur.tools:
            return  # (it died meanwhile: the tazer on it now would miss)
        x, y = inj.where(self.g.frame + 1)
        # (over a frostbite tested before it, 0x398e4: any other tool on that
        # freezes the tools 5 s; melted first)
        for f in self.injuries.values():
            if f.cls == "FrostBite" and f.id < inj.id and self.in_injury(f, (x, y), 6):
                self.treat(f)
                return
        if self.on_hud(screen(x, y)):
            # (under a button: tapped there it would press the button; left
            # till it moves)
            self.resting[inj.id] = self.g.frame + 30
            return
        self.g.click(*screen(x, y), settle=2)
        # (its dying lasts a moment, its stage changing at the end: an ant's
        # "chainsaw" twice, 0x145364)
        self.zapped[inj.id] = self.g.frame + 90

    def follow(self, inj, tool, frames, done):
        """The tool held on a creature as it moves (where it is going each
        frame) until done() or the frames are out."""
        self.select(tool)
        at = screen(*inj.where(self.g.frame + 1))
        self.g.send("down %d %d" % at)
        for _ in range(frames):
            self.g.run(1)
            self.update()
            if done() or inj.id not in self.injuries or self.ended():
                break
            at = screen(*self.injuries[inj.id].where(self.g.frame + 1))
            self.g.send("move %d %d" % at)
        self.g.send("up %d %d" % at)
        self.g.run(3)
        self.update()

    def ghost(self, inj):
        """A ghost (Android's Ghost): the vacuum held on it as it drifts and
        through its struggles, till its strength is out and it splits (both
        halves hidden 3.5 s) or dies. The frostbites the dead leave are
        thawed first: any tool but the lighter on one freezes the tools 5 s,
        and the vacuum goes where the ghost drifts (lifted before one)."""
        bites = [f for f in self.injuries.values() if f.cls == "FrostBite" and not f.waits and self.tool_for(f)]
        if bites:
            self.treat(min(bites, key=lambda f: f.id))
            return

        def bitten(p):
            return any(f.cls == "FrostBite" and self.in_injury(f, p, 8) for f in self.injuries.values())

        if bitten(inj.where(self.g.frame + 1)):
            self.resting[inj.id] = self.g.frame + 20
            return
        self.follow(inj, "Vacuum", 300, lambda: self.cstate.get(inj.id) in GHOST_AWAY or (
            inj.id in self.injuries and bitten(self.injuries[inj.id].where(self.g.frame + 1))))

    def leech(self, inj):
        """A leech: burnt with the lighter (so it lets go of its teeth), then
        taken by its head with the tongs and pulled off (200 canvas points
        times its size from its mouth, 0x17ccc0) on to the tray. Anything
        cutting bursts it into poison."""
        scale, burnt = self.leeches.get(inj.id, (0.6, False))
        h = inj.h * scale
        # (its rect rises from its mouth along its own turn, the head its top
        # quarter: 0x17ccc0 tests the touch turned back by it)
        ax = inj.axis()
        if not burnt and self.usable("Lighter"):
            self.select("Lighter")
            body = screen(inj.x + ax[0] * h * 0.45, inj.y + ax[1] * h * 0.45)
            self.g.drag([body, (body[0], body[1] - 2)], speed=1.0)
            self.update()
        self.select("Tongs")
        mouth = screen(inj.x, inj.y)
        head = screen(inj.x + ax[0] * h * 0.82, inj.y + ax[1] * h * 0.82)
        pull = 178 * scale + 20
        tray = self.to_tray()
        path = [head]
        if dist(mouth, tray) < pull:
            # (the tray too near: pulled off away from it first)
            away = (mouth[0] + (mouth[0] - tray[0]), mouth[1] + (mouth[1] - tray[1]))
            k = pull / max(1.0, dist(mouth, away))
            path.append((int(mouth[0] + (away[0] - mouth[0]) * k), int(mouth[1] + (away[1] - mouth[1]) * k)))
        path.append(tray)
        self.g.drag(path, speed=6.0, hold=TRAY_RISE)
        self.update()

    def scorpion(self, inj):
        """A robot scorpion stunned (by the tazer, as it dashes): a clamp on
        its tail, wound tight (it holds it 6 s), then the chainsaw held on
        it 150 frames (0x16210c) blows it apart."""
        # (a clamp on it already: not tapped twice again, that takes it off)
        if inj.id in self.clamped_on or (self.usable("Clamp") and inj.points
                                         and self.clamp_on(inj, screen(*inj.points[0]))):
            # (PLAYTEST_SCORPION_HOLD=1: the first clamp left on until it is
            # shrugged off (6 s), to see that; then as usual)
            held = self.__dict__.setdefault("held_once", set())
            if os.environ.get("PLAYTEST_SCORPION_HOLD") and inj.id not in held:
                held.add(inj.id)
                for _ in range(7 * 60):
                    self.g.run(1)
                    self.update()
                if inj.id in self.clamped_on:  # (shrugged off: "robotscorpion clamp released")
                    self.clamped_on.remove(inj.id)
                    self.clamp_spots = self.clamp_spots[-len(self.clamped_on):] if self.clamped_on else []
                return
            self.select("Chainsaw")
            at = screen(*inj.where(self.g.frame))
            # (small rounds over it, slow enough to keep hold of the saw)
            path = [(int(at[0] + 6 * math.cos(math.radians(k))), int(at[1] + 6 * math.sin(math.radians(k))))
                    for k in range(0, 360 * 8, 40)]
            self.g.stroke(path, speed=1.5, stop=lambda: (self.update() or self.injuries.get(inj.id) is None
                                                         or self.injuries[inj.id].state != inj.state))
            return
        # (no clamp yet: tried again; the tazer again would hold it stunned
        # for good, 0x16210c)

    def mini_worm(self, inj):
        """A mini worm: shocked with the tazer (still 4 s), then taken by its
        body with the tongs to the tray."""
        if self.cstate.get(inj.id) != 70:
            self.creature_tap(inj, "Tazer")
            for _ in range(20):
                self.g.run(1)
                self.update()
                if self.cstate.get(inj.id) == 70 or inj.id not in self.injuries:
                    break
            if self.cstate.get(inj.id) != 70:
                return
        self.select("Tongs")
        at = screen(*self.injuries.get(inj.id, inj).where(self.g.frame))
        self.g.drag([at, (at[0] + 2, at[1]), self.to_tray()], speed=10.0, hold=TRAY_RISE)
        self.update()

    def pygmy(self, inj):
        """A pygmy (whole or a half): taken where it is walking with the
        tongs (it walks slowly) and carried to the tray, which finishes it."""
        self.select("Tongs")
        at = self.reachable(screen(*self.injuries.get(inj.id, inj).where(self.g.frame + 1)), 30)
        self.g.drag([at, (at[0] + 2, at[1]), self.to_tray()], speed=10.0, hold=TRAY_RISE)
        self.update()

    def giant_worm(self, inj):
        """The giant worm: the chainsaw on its head while it looks around
        (each a cut; the third kills it)."""
        self.select("Chainsaw")
        if self.cstate.get(inj.id) != 73:
            return
        at = screen(*self.injuries.get(inj.id, inj).where(self.g.frame))
        path = [(int(at[0] + 5 * math.cos(math.radians(k))), int(at[1] + 5 * math.sin(math.radians(k))))
                for k in range(0, 360, 45)]
        self.g.stroke(path, speed=1.5, stop=lambda: self.update() or self.cstate.get(inj.id) != 73)

    def snake(self, inj):
        """A snake out of its hole: tazed while it is out (a second), then
        stunned, the chainsaw held on it two seconds cuts it back in."""
        at = screen(inj.x, inj.y + inj.h * 0.1)
        if inj.id not in self.stunned:
            self.select("Tazer")
            self.update()
            cur = self.injuries.get(inj.id)
            # (out a second, 0x14af38: tapped only early in it, counted from
            # the earliest it may have come out)
            if not cur or cur.waits or cur.state != "SnakeOut" or                     self.g.frame - cur.since_early > (22 if self.sudden else 40):
                if self.sudden and cur and cur.state == "SnakeOut":
                    self.resting[inj.id] = self.g.frame + 30  # (till it is in and out again)
                return
            self.g.click(*at, settle=3)
            self.update()
        if inj.id in self.stunned and self.usable("Chainsaw"):
            self.select("Chainsaw")
            self.g.press([(at[0], at[1], 150)])
            self.update()

    def half_grip(self, iid, k):
        """Where to take a half (on its middle line, on the screen, off the
        HUD and off the other half, which the tongs would take instead) and
        where that point is with the half in line (the half turns to point
        from its middle at the touch, 0x1a59c0)."""
        halves = self.halves.get(iid, {})
        if k not in halves:
            return None
        c, turn, target, w, h = halves[k]

        def along(t, d):
            a = math.radians(t)
            return (int(round(c[0] + d * math.sin(a))), int(round(c[1] - d * math.cos(a))))

        def on_other(p):
            o = halves.get(1 - k)
            if not o:
                return False
            oc, ot, _, ow, oh = o
            a = math.radians(ot)
            dx, dy = p[0] - oc[0], -(p[1] - oc[1])
            # (turned back into the other half's frame: along it, across it)
            u = dx * math.sin(a) + dy * math.cos(a)
            v = dx * math.cos(a) - dy * math.sin(a)
            return abs(u) < oh * 0.5 + 6 and abs(v) < ow * 0.5 + 6

        # (taken a little toward the line: a half made at the very end of its
        # range turns out of it at the first touch on its own middle line)
        toward = turn + max(-2.0, min(2.0, target - turn))
        # (farther along when those are off the screen or under the bar
        # across the top: a half coming in from above it)
        for f in (0.3, 0.25, 0.35, 0.2, 0.4, 0.15, 0.45, 0.1, 0.5, 0.55, 0.6, 0.65, 0.7):
            grip, to = along(toward, h * f), along(target, h * f)
            if all(8 <= p[0] <= WIDTH - 8 and 8 <= p[1] <= HEIGHT - 8 and not self.on_hud(p) for p in (grip, to))                     and not on_other(grip):
                return grip, to
        return None

    def set_halves(self, inj):
        """A broken vein's ends clamped shut first; then (a vein or a bone)
        each half turned back into line with the tongs: its end at the break
        taken and drawn to where it is in line."""
        if "Clamp" in inj.tools:
            for k, (p, clamp) in sorted(self.cpoints.get(inj.id, {}).items()):
                if clamp == "loose":
                    self.select("Clamp")
                    self.tighten(p)
                    return
                if not clamp:
                    # (a clamp point takes a touch 37 points about it: 85
                    # canvas points wide, 0x852e4)
                    self.clamp_on(inj, self.reachable(p, 30))
                    return
        self.select("Tongs")
        for k in (0, 1):
            grip_to = self.half_grip(inj.id, k)
            if grip_to:
                grip, to = grip_to
                self.g.drag([grip, to, (to[0] + 1, to[1])], speed=4.0, hold=4)
                self.update()
                if inj.id not in self.injuries or self.injuries[inj.id].state != "Broken":
                    return

    def clamp_on(self, inj, at):
        """A clamp put on the point (a double tap) and its wheel turned round
        till it is tight."""
        if len(self.clamped_on) >= 2:
            return False
        self.select("Clamp")
        self.clamp = None
        self.clamping = inj.id
        self.g.click(*at, settle=6)
        self.g.click(*at, settle=10)
        self.update()
        if not self.clamp:
            return False
        if inj.cls == "RobotScorpion":
            self.g.run(3)
            self.update()
        return self.tighten(self.clamp[1])

    def tighten(self, wheel):
        """A clamp's wheel turned round with the finger till it is tight."""
        tightened = self.tightened
        # (a round small enough to keep off the other clamps: a touch within
        # 1.15 of a clamp's picture, about 97 by 62 points, is that clamp's,
        # the first in the list, 0x69048)
        others = [c for c in self.clamp_spots if dist(c, wheel) > 3]
        radius = 4
        for r in (12, 9, 6):
            ring = [(wheel[0] + r * math.cos(math.radians(k)), wheel[1] + r * math.sin(math.radians(k)))
                    for k in range(0, 360, 30)]
            if not any(abs(p[0] - c[0]) < 99 and abs(p[1] - c[1]) < 64 for p in ring for c in others):
                radius = r
                break
        # (begun off the HUD's buttons: a clamp just under the bar across the
        # top has its round begun below it; the round may pass under them)
        start = next((s for s in (0, 90, 180, 270) if not self.on_hud(
            (wheel[0] + radius * math.cos(math.radians(s)), wheel[1] + radius * math.sin(math.radians(s))))), None)
        lead = []
        if start is None:
            # (the whole round under the bar: begun straight below it, still
            # on the clamp's picture, and drawn up into the round)
            start = 90
            below = next((wheel[1] + d for d in range(radius, 31) if not self.on_hud((wheel[0], wheel[1] + d))), None)
            if below is not None:
                lead = [(wheel[0], below)]
        for way in (self.clamp_way, -self.clamp_way):
            path = list(lead)
            # (a tenth of tightness a degree, 0x68ec0: ten turns at most)
            for k in range(0, 5 * 360 + 1, 30):
                a = math.radians(start + k * way)
                path.append((int(round(wheel[0] + radius * math.cos(a))), int(round(wheel[1] + radius * math.sin(a)))))
            self.g.drag(path, speed=6.0)
            self.update()
            if self.tightened > tightened:
                self.clamp_way = way
                return True
            # (a turn short: on the same way)
            self.g.drag(path[:len(path) // 2], speed=6.0)
            self.update()
            if self.tightened > tightened:
                self.clamp_way = way
                return True
        return False

    def on_hud(self, p):
        """Whether a screen point is on a HUD button (the heart meter and the
        combo are pictures only: a touch there goes to the patient)."""
        if p[0] < 6 or p[0] > WIDTH - 6 or p[1] < 6 or p[1] > HEIGHT - 6:
            return True
        # (the pause button 27 by 30 points; the injection menu's second
        # button beside its own)
        if abs(p[0] - self.pause_at[0]) < 17 and abs(p[1] - self.pause_at[1]) < 18:
            return True
        for name, b in self.buttons.items():
            # (a button of the bar across the top: its size, a few points
            # spare; at the side the columns' wide margins)
            w, h = self.button_sizes.get(name, (0, 0))
            if w and abs(p[0] - b[0]) < w / 2 + 1 and abs(p[1] - b[1]) < h / 2 + 1:
                return True
            if not w and abs(p[0] - b[0]) < 40 and abs(p[1] - b[1]) < 29:
                return True
        sized = bool(self.button_sizes)
        # (the injection's out button: under the bar only while its menu is
        # open; at the side kept clear always, as before)
        out = self.out_button if (not sized or self.injection_menu) else (-99, -99)
        for b in [self.tag or (-99, -99), out]:
            if abs(p[0] - b[0]) < (40 if not sized or b == self.tag else 22) and abs(p[1] - b[1]) < (29 if not sized else 20):
                return True
        return False

    def reachable(self, p, by):
        """The point, or one up to `by` from it toward the middle off the HUD."""
        if not self.on_hud(p):
            return p
        for k in range(2, by + 1, 2):
            for a in range(0, 360, 30):
                q = (int(p[0] + k * math.cos(math.radians(a))), int(p[1] + k * math.sin(math.radians(a))))
                if not self.on_hud(q):
                    return q
        return p

    def pull_path(self, inj):
        """An embedded object's pull: out along its own direction (the angle
        from where the pull began, within 30 degrees on average, 0x99f6c),
        the object coming out by the length moved; near the screen's edge
        turned aside, then on round an arc within that angle."""
        ax = inj.axis()
        out, need = self.sticks.get(inj.id, (inj.h * 0.3, inj.h * 0.75))
        need += 12
        # (taken just above the skin: its piece is what sticks out, 0x77c4c,
        # and the most room is left for the pull)
        sx, sy = inj.x + ax[0] * 3, inj.y + ax[1] * 3

        def at(turn, r):
            a = math.radians(turn)
            d = (ax[0] * math.cos(a) - ax[1] * math.sin(a), ax[0] * math.sin(a) + ax[1] * math.cos(a))
            return screen(sx + d[0] * r, sy + d[1] * r)

        def room(turn):
            r = 0
            while r < need:
                p = at(turn, r + 4)
                if not (6 <= p[0] <= WIDTH - 6 and 6 <= p[1] <= HEIGHT - 6):
                    break
                r += 4
            return r
        best = max((0, 8, -8, 16, -16, 22, -22), key=lambda t: (room(t), -abs(t)))
        r = room(best)
        path = [screen(sx, sy), at(best, r)]
        left = need - r
        if left > 0 and r > 20:
            # round an arc between 20 degrees either side of its axis
            turns = [t for t in (20, -20) if room(t) >= r] or [best]
            k = 0
            while left > 0 and len(turns) > 1:
                t = turns[k % 2]
                path.append(at(t, r))
                left -= r * math.radians(40)
                k += 1
        return path

    def treat_stroke(self, inj, path, speed):
        """A stroke over an injury, the finger lifted as soon as it is done
        (or the stage changes: the tool would go on working on the next)."""
        stage = self.stage

        def stop():
            self.update()
            cur = self.injuries.get(inj.id)
            return self.stage != stage or cur is None or cur.state != inj.state
        self.g.stroke(path, speed=speed, stop=stop)

    def thaw_first(self, inj, tool):
        """A frostbite over where the tool will touch thawed first with the
        lighter: any other tool on one (its whole rect) freezes the tools 5 s
        (FrostBite, Android 0x56bbac), and the injury under it would be tried
        again and again, a freeze each time."""
        if tool == "Lighter" or inj.cls == "FrostBite" or not self.usable("Lighter"):
            return False
        spots = [inj.where(self.g.frame + 1)] if creature(inj) else (list(inj.points) or [(inj.x, inj.y)])
        bites = [f for f in self.injuries.values() if f.cls == "FrostBite" and f.id != inj.id and not f.waits
                 and any(self.in_injury(f, p, 8) for p in spots)]
        if not bites:
            return False
        self.treat(min(bites, key=lambda f: f.id))
        return True

    def treat(self, inj):
        tool = self.tool_for(inj)
        # a transplant: its pieces cut free taken to the tray, its new organ
        # from the tray onto the patient
        if inj.state == "Free" and self.free.get(inj.id):
            self.select("Tongs")
            n, at = sorted(self.free[inj.id].items())[0]
            del self.free[inj.id][n]
            self.g.drag([screen(*at), self.to_tray()], speed=8.0, hold=TRAY_RISE)
            return
        if inj.state == "Replacing" and self.organ:
            self.select("Tongs")
            # (it rises onto the tray a quarter second first)
            self.g.run(TRAY_RISE)
            organ = screen(*self.organ)
            self.g.drag([organ, (organ[0] + 2, organ[1]), screen(inj.x, inj.y)], speed=8.0)
            return
        if tool is None:
            return
        if self.thaw_first(inj, tool):
            return
        pts = [screen(*p) for p in inj.points]
        if inj.cls == "PoisonFrog" and inj.id in self.stunned and urgency(inj) != 8 and self.usable("Lighter"):
            # stunned: burnt with the lighter till a life goes (it runs)
            self.follow(inj, "Lighter", 150, lambda: inj.id not in self.stunned or self.injuries.get(inj.id) is None
                        or self.injuries[inj.id].state != inj.state)
            return
        if inj.state == "Broken":
            self.set_halves(inj)
            return
        if inj.cls == "Leech":
            self.leech(inj)
            return
        if inj.cls == "Ghost":
            self.ghost(inj)
            return
        if inj.cls in ("Pygmy", "PygmyHalf"):
            self.pygmy(inj)
            return
        if creature(inj) and tool in ("Lighter", "Vacuum") and urgency(inj) != 8:
            # (a creature burnt or sucked as it hops about: the tool kept on
            # it till its state changes; a spider dies after its lives'
            # worth of frames under the flame, Spider 0x7119e4)
            self.follow(inj, tool, 240, lambda: self.injuries.get(inj.id) is None
                        or self.injuries[inj.id].state != inj.state)
            return
        if inj.cls == "Snake":
            self.snake(inj)
            return
        if inj.cls == "MiniWorm":
            self.mini_worm(inj)
            return
        if inj.cls == "GiantWorm":
            self.giant_worm(inj)
            return
        if inj.cls == "RobotScorpion" and inj.state == "RobotScorpionTazered":
            self.scorpion(inj)
            return
        if tool == "Tazer":
            self.creature_tap(inj, tool)
            return
        if tool == "Injection":
            self.extract(inj)
            return
        self.select(tool)
        if tool == "Clamp":
            # (a point under a button: tapped beside it, within a clamp's reach)
            self.clamp_on(inj, self.reachable(pts[0], 30) if pts else screen(inj.x, inj.y))
            return
        if tool == "Stapler":
            # (a point under a button: pressed beside it, within its reach,
            # or left)
            for p in pts or [screen(inj.x, inj.y)]:
                q = self.reachable(p, 10)
                if not self.on_hud(q):
                    self.g.click(*q, settle=5)
            return
        if tool == "Vacuum":
            # held over each point half a second (a drop: over it, drawn in;
            # one under a button drawn out from beside it)
            # (a fire's points: worn a frame each at the vacuum's power; a
            # drop drawn in from the suction's reach takes longer)
            spots = [self.reachable(p, 30) for p in (pts or [screen(inj.x, inj.y)])]
            hold = 8 if inj.cls == "Fire" else 30
            if inj.cls.endswith("Particle"):
                # (a drop under the nozzle goes at once: 0x19df78's reach; iOS
                # 1.45's drops hurt after a second, not ten, so none is left
                # a moment longer than it takes)
                hold = 12
            self.g.press([(p[0], p[1], hold if p == q else 60) for p, q in zip(spots, pts or [screen(inj.x, inj.y)])])
            return
        if tool == "Tongs" and not pts and urgency(inj) == 8:
            # a dead creature to the tray (one that died under a tool
            # button taken by its side off the button)
            at = self.reachable(screen(*inj.where(self.g.frame)), 18)
            self.g.drag([at, (at[0] + 2, at[1]), self.to_tray()], speed=14.0, hold=TRAY_RISE)
            return
        if len(pts) > 6:
            # along each of its cuts in order (a transplant's: round it, or
            # its lines)
            runs = [[pts[0]]]
            for p in pts[1:]:
                q = runs[-1][-1]
                if dist(p, q) > 40:
                    runs.append([p])
                else:
                    runs[-1].append(p)
            # (once along: the chainsaw hurts while it cuts nothing, 0x34470;
            # the others there and back)
            for run in runs:
                if len(run) == 1:
                    run = [run[0], (run[0][0] + 1, run[0][1])]
                elif tool != "Chainsaw":
                    run = run + run[-2::-1]
                # (one begun under a button: the press begun beside it, off
                # the button, the stroke passing under it)
                if self.on_hud(run[0]):
                    run = [self.reachable(run[0], 80)] + run
                self.treat_stroke(inj, run, CHAINSAW_SPEED if tool == "Chainsaw" else 5.0)
                if self.injuries.get(inj.id) is None or self.injuries[inj.id].state != inj.state:
                    return
            return
        if tool == "Tongs":
            # pulled out along its own direction, then to the tray
            self.g.drag(self.pull_path(inj) + [self.to_tray()], speed=5.0, hold=TRAY_RISE)
            return
        if not pts:
            ax = inj.axis()
            half = inj.h * 0.4
            pts = [screen(inj.x - ax[0] * half, inj.y - ax[1] * half), screen(inj.x + ax[0] * half, inj.y + ax[1] * half)]
        if len(pts) > 1:
            # along the line the points make, end to end and back
            ax = inj.axis()
            pts.sort(key=lambda p: p[0] * ax[0] - p[1] * ax[1])
        # (a point under the pause button: taken at the edge of its reach,
        # 11 points about it)
        pts = [self.reachable(p, 10) if abs(p[0] - self.pause_at[0]) < 17 and abs(p[1] - self.pause_at[1]) < 18 else p
               for p in pts]
        path = pts + pts[-2::-1] if len(pts) > 1 else [pts[0], (pts[0][0] + 1, pts[0][1] + 1)]
        # (one under a button: the press begun beside it, off the button)
        if self.on_hud(path[0]):
            path = [self.reachable(path[0], 80)] + path
        self.treat_stroke(inj, path, CHAINSAW_SPEED if tool == "Chainsaw" else 5.0)

    def choose(self):
        todo = [i for i in self.injuries.values() if self.tool_for(i) and not i.waits
                and not (self.zapped.get(i.id, 0) > self.g.frame and urgency(i) != 8
                         and i.state != "RobotScorpionTazered")
                and not self.resting.get(i.id, 0) > self.g.frame
                and not (i.cls == "GiantWorm" and self.cstate.get(i.id) not in (None, 73))
                and not (i.cls == "Ghost" and self.cstate.get(i.id) in GHOST_AWAY)]
        # (one broken vein clamped at a time: two clamps at most, held till
        # it is healed)
        begun = [i for i in todo if i.state == "Broken" and any(c for _, c in self.cpoints.get(i.id, {}).values())]
        if begun:
            todo = [i for i in todo if i.state != "Broken" or i in begun]
        # (no clamp left: what needs one waits; a bone, once no vein over
        # it is broken: the tongs would take the vein's half first)
        if len(self.clamped_on) >= 2:
            todo = [i for i in todo if i in begun or i.id in self.clamped_on
                    or (self.tool_for(i) != "Clamp" and "Clamp" not in i.tools)]
        if any(i.state == "Broken" and "Clamp" in i.tools for i in self.injuries.values()):
            todo = [i for i in todo if not (i.state == "Broken" and "Clamp" not in i.tools)]
        # (an object to pull out near one made before it waits: the earlier
        # comes first in the hit test and would take the pull, 0x398e4)
        pulls = [i for i in self.injuries.values() if i.state == "Extracting" and "Tongs" in i.tools]
        todo = [i for i in todo if not (i in pulls and any(o.id < i.id and dist((o.x, o.y), (i.x, i.y)) < 90
                                                            for o in pulls))]
        if not todo:
            return None
        # the ones the tutorial points at first, then the most urgent, the
        # earliest made (one waiting for others to be treated first is tried
        # again last; the big ones, an organ to cut round, after the small)
        # (a creature, which moves and may be missed, is tried again and
        # again; a stunned one first, while it is)
        def key(i):
            # (a robot scorpion tazed: clamped and sawn before it comes round
            # and burrows, leaving cuts; two at once in iOS 1.45's revsorgan c2)
            stunned = i.id in self.stunned or i.state == "RobotScorpionTazered"
            u = -2 if stunned and urgency(i) != 8 else urgency(i)
            if i.cls == "Leech":
                return (i.id not in self.highlighted, 0, u, -self.leeches.get(i.id, (0.6, 0))[0], i.id)
            tries = 0 if creature(i) else self.tries.get((i.id, i.state), 0) // 3
            tool = self.tool_for(i)
            # (the most hurt stopped for the time it takes; a change of tool
            # costs a moment)
            t = treat_time(i, tool) + (0 if tool == self.tool else 0.2)
            # (an embedded object stops its wound bleeding too: the wound
            # under it waits for it to come out, 0x4c0c8's +0x70)
            drain = i.drain
            if i.state == "Extracting":
                drain += sum(o.drain for o in self.injuries.values()
                             if o.waits and o is not i and dist((o.x, o.y), (i.x, i.y)) < 12)
            return (i.id not in self.highlighted, tries, u, -round(drain / t, 3), i.id)
        return sorted(todo, key=key)[0]

    def play(self, limit=20000):
        end = self.g.frame + limit
        self.g.wait_for(r"^stage \d+", since=0)
        while not self.ended():
            if self.g.frame > end:
                raise Timeout("the surgery did not end by frame %d" % self.g.frame)
            self.update()
            if self.ok:
                # the tutorial box read and closed with its OK, once it has
                # sprung up (a second)
                self.g.run(60)
                self.g.click(*self.ok)
                self.ok = None
                self.update()
                continue
            if self.frozen:
                self.g.run(10)
                continue
            # the tag partner called once charged (or when the tutorial asks)
            if self.tag and self.tag_need is not None and self.tag_charge >= self.tag_need and "ingame_tagpartner" not in self.disabled:
                self.call_partner()
                continue
            # the injection when the heart runs low (or the tutorial asks)
            # (low, or falling fast: then whenever it is ready, up to a
            # good margin; healing has no ceiling, 0x3a188)
            fall = sum(self.falls) / len(self.falls) if self.falls else 0
            low = self.heart is not None and self.heart_max and (
                self.heart < min(0.6 * self.heart_max, 60) or (fall > 1.5 and self.heart < 20 + 15 * fall))
            asked = "INJECTOR" in self.tutorial_text or "plunger" in self.tutorial_text
            if (low or asked) and self.usable("Injection") and self.g.frame >= self.injection_ready:
                self.inject()
                if asked:
                    self.tutorial_text = ""
                continue
            inj = self.choose()
            if not inj:
                self.g.run(10)
                continue
            key = (inj.id, inj.state)
            self.tries[key] = self.tries.get(key, 0) + 1
            # (one that does not take it, out of reach a while: left a moment
            # every third try)
            if self.tries[key] % 3 == 0:
                self.resting[inj.id] = self.g.frame + (40 if creature(inj) else 90)
            if self.tries[key] > (60 if creature(inj) else 30):
                raise Timeout("stuck on injury #%d %s state %s (tools %s) at frame %d" %
                              (inj.id, inj.cls, inj.state, ",".join(inj.tools), self.g.frame))
            bad = self.bad_popups
            self.treat(inj)
            self.g.run(2)
            self.update()
            if self.bad_popups > bad:
                # (a miss: not the same again at once; what was in the way
                # moves on)
                self.resting[inj.id] = self.g.frame + 60
        return self.ended()[1]


def new_game_to_first_surgery(g):
    g.run(320)
    g.click(*menu_point(TITLE_PLAY))
    g.run(220)
    g.key("esc")  # the intro movie
    g.run(230)
    g.click(*menu_point(DIALOGUE_SKIP))
    g.run(190)
    g.click(*menu_point(PREOP_START))


def surgery1(g):
    new_game_to_first_surgery(g)
    result = Surgery(g).play()
    if result != "won":
        raise Timeout("the first surgery was " + result)
    g.run(200)
    g.shot("surgery1_end")
    print("surgery1: won at frame %d (%s)" % (g.frame, g.find(r"^surgery won.*")[0]))


def direct(surgery_id):
    """A surgery opened directly (AS_SURGERY=<id>[:mode[:stage]]), played to its end."""
    def run(g):
        g.run(200)
        result = Surgery(g).play(limit=60000)
        g.run(60)
        g.shot(surgery_id.replace(":", "_") + "_end")
        if result != "won":
            m = g.find(r"^surgery lost.*")
            raise Timeout("%s was %s (%s)" % (surgery_id, result, m[0] if m else ""))
        print("%s: won at frame %d (%s)" % (surgery_id, g.frame, g.find(r"^surgery won.*")[0]))
    run.env = {"AS_SURGERY": surgery_id}
    parts = surgery_id.split(":")
    run.seed = lambda save: seed_save(save, parts[0], int(parts[1]) if len(parts) > 1 else 0)
    return run


def edited(path):
    """A surgery editor's level (a file of editedLevels/ in the save, or a
    path) opened at once (AS_EDITED), played to its end (docs/as3.md,
    "Surgery editor"). Its results are the editor's own: nothing is kept."""
    def run(g):
        g.run(200)
        result = Surgery(g).play(limit=60000)
        g.run(60)
        g.shot("edited_end")
        if result != "won":
            m = g.find(r"^surgery lost.*")
            raise Timeout("%s was %s (%s)" % (path, result, m[0] if m else ""))
        print("edited %s: won at frame %d (%s)" % (os.path.basename(path), g.frame, g.find(r"^surgery won.*")[0]))
    run.env = {"AS_EDITED": os.path.abspath(path) if os.path.exists(path) else path}
    run.seed = lambda save: seed_save(save, "")
    return run


def seed_save(save, surgery, mode=0):
    """A save as a player reaching that surgery has it: the surgeries before
    it won (a star each; for a challenge, this one too, and its first
    challenge for the second), the tag partner the game gives for free (Mr
    Giblets, from the second surgery) selected. Its tutorials are still to
    do. Its blood packs are full (none and an old timestamp: recharged at
    once); a surgery opened directly takes none."""
    import json
    with open(os.path.join(package(), "Data", "areas.json"), encoding="utf-8") as f:
        order = [s for a in json.load(f)["areas"] for s in a["surgeries"]]
    before = order[:order.index(surgery)] if surgery in order else []
    if mode:
        before = before + [surgery]
    # the partners the story gave by then (their first wins' gifts: item type
    # 2, the partner's index), and the one selected (Mr Giblets, or one
    # given by then named in PLAYTEST_PARTNER)
    gifts = {"surgery_mumbojungle_deceasedkeith": ("saturday", 1), "surgery_robotopia_reverendsorgan": ("reverend", 2)}
    given = {gifts[s][0]: gifts[s][1] for s in before if s in gifts}
    transactions = [{"formatVersion": 1, "deviceId": "", "timestamp": "0", "currency": 0, "currencyInc": 0,
                     "itemType": 2, "itemSubtype": i, "itemInc": 1} for i in given.values()]
    partner = "giblets" if before else ""
    if before and os.environ.get("PLAYTEST_PARTNER") in given:
        partner = os.environ["PLAYTEST_PARTNER"]
    slot = {"uniqueId": 1, "lastSaveTimestamp": "0", "starsMain": {s: 1 for s in before}, "upgradesMain": {},
            "partnersMain": {}, "starsChallenge1": {surgery: 1} if mode == 2 else {}, "starsChallenge2": {},
            "completedTutorials": [],
            # (past the first two surgeries such a player has met the blood
            # packs: their popup, shown once a slot, already seen)
            "statProgress": ({"UsedBloodpack": 1} if {"surgery_bunker_mrgiblets", "surgery_bunker_prxy4"} <= set(before)
                             else {}),
            "highScores": {}, "transactions": transactions,
            "selectedTagPartner": partner, "lastSelectedArea": "", "sfxOn": True,
            "musicOn": True, "numBloodPacks": 0, "lastBloodPackTimestamp": "0", "tag_cooldowns": {},
            "lastFailedSurgery": "", "lastFailedSurgeryMode": 0, "lastFailedSurgeryStage": 0}
    # (PLAYTEST_DIRECTORS_CUT=1: the slot's Director's cut on, RestoredContent.h)
    if os.environ.get("PLAYTEST_DIRECTORS_CUT"):
        slot["directorsCut"] = True
    state = {"formatVersion": 1, "lastLoaded": 1, "last1DayLocalNotificationTimestamp": "0",
             "last3DayLocalNotificationTimestamp": "0", "nextAppReviewPopupTimestamp": "0", "monetized": False,
             "savedGames": [slot]}
    with open(os.path.join(save, "state.json"), "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)


SCENARIOS = {"surgery1": surgery1}
EXE_DIR = os.path.join(PORT, "build-dev")
KEYS = False  # (--keys: the tools picked with their number keys)
LAYOUT = None  # (--layout top|sides: AS_HUD_LAYOUT)


def package():
    """The package the exe reads: an assets folder beside it (as installed),
    else port/games/as3 (platform/Assets.cpp's order)."""
    beside = os.path.join(EXE_DIR, "assets")
    return beside if os.path.isdir(beside) else os.path.join(PORT, "games", "as3")


def main():
    args = sys.argv[1:]
    build = "build-dev"
    keep = False
    if "--build" in args:
        i = args.index("--build")
        build = args[i + 1]
        del args[i:i + 2]
    if "--keep" in args:
        keep = True
        args.remove("--keep")
    # (--shots n: a screenshot every n frames, kept with the save)
    shot_every = 0
    if "--shots" in args:
        i = args.index("--shots")
        shot_every = int(args[i + 1])
        del args[i:i + 2]
        keep = True
    # (AS_SEED: the game's random numbers, so a failed run can be played
    # again exactly with --seed)
    seed = None
    if "--seed" in args:
        i = args.index("--seed")
        seed = int(args[i + 1])
        del args[i:i + 2]
    global KEYS, LAYOUT
    if "--keys" in args:
        KEYS = True
        args.remove("--keys")
    if "--layout" in args:
        i = args.index("--layout")
        LAYOUT = args[i + 1]
        del args[i:i + 2]
    # (--aspect w:h: the screen's shape, AS_ASPECT: the window made that shape)
    aspect = None
    if "--aspect" in args:
        i = args.index("--aspect")
        aspect = args[i + 1]
        del args[i:i + 2]
    scenarios = args or ["surgery1"]
    exe = os.path.join(PORT, build, "AmateurSurgeon3.exe")
    global EXE_DIR
    EXE_DIR = os.path.dirname(exe)
    ok = True
    for name in scenarios:
        save = tempfile.mkdtemp(prefix="as3play_")
        # "direct:<surgery id>[:mode[:stage]]": that surgery opened at once
        # "edited:<file>": a surgery editor's level opened at once (AS_EDITED)
        scenario = (direct(name[7:]) if name.startswith("direct:") else
                    edited(name[7:]) if name.startswith("edited:") else SCENARIOS[name])
        if getattr(scenario, "seed", None):
            scenario.seed(save)
        run_seed = seed if seed is not None else int.from_bytes(os.urandom(3), "little")
        env = dict(getattr(scenario, "env", None) or {}, AS_SEED=str(run_seed))
        if LAYOUT:
            env["AS_HUD_LAYOUT"] = LAYOUT
        if aspect:
            env["AS_ASPECT"] = aspect
        global WIDTH, HEIGHT
        WIDTH, HEIGHT = 568, 320  # (until the trace says otherwise)
        g = Live(exe, save, env, shot_every)
        try:
            scenario(g)
        except Timeout as e:
            ok = False
            print("%s: FAILED: %s (seed %d)" % (name, e, run_seed))
            with open(os.path.join(save, "trace.txt"), "w", encoding="utf-8") as f:
                f.write("\n".join(g.log))
            print("  trace kept in", save)
            keep = True
        finally:
            g.close()
        if not keep:
            shutil.rmtree(save, ignore_errors=True)
        else:
            print("  save kept in", save)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
