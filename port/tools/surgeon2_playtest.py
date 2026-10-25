#!/usr/bin/env python3
"""Scripted play-throughs of Amateur Surgeon 2 HD (the iPad port) with the
real menus and tools.

The game runs in debug mode with a scratch save (AS_SAVE_DIR), hidden, and
driven live (AS_CONTROL): this script reads its trace (screens shown,
stages, tutorials, every injury's stage, the tool it wants and where its
boxes still to treat are on the screen) and answers with taps and drags
where a player would make them: through the menus from the title screen,
the tutorials' buttons, the tools' number keys, and along each injury's boxes.

Usage: surgeon2_playtest.py [--build build-dev] [--keep] [--save <dir>] [scenario...]
       scenarios: missionN (missions 1 to N in turn, N up to 6), fromN (with --save,
       a kept run's save: missions N to 10), onlyN (with --save: mission N alone),
       start (default: mission1)
       --keep: the save and its trace (trace.txt) kept, the run passed or not
"""
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The tools in their ids' order: picked with the number keys 1-9 and 0, as
# the Flash HUD numbers its cards.
TOOLS = ["cutter", "tongs", "lighter", "cream", "stitch", "vacuum", "chainsaw", "injection", "battery", "scanner"]
TOOL_KEY = {name: str((k + 1) % 10) for k, name in enumerate(TOOLS)}


# The HUD (the Flash AS2's): a press there picks a tool or pauses: the tool
# cards across the top, the time card bottom left, the score card bottom
# right. Strokes are kept out of it.
def on_hud(q):
    return q[1] < 168 or (q[1] > 650 and (q[0] < 200 or q[0] > 860))


def in_play(q):
    return 10 < q[0] < 1014 and q[1] < 758 and not on_hud(q)


def keep_in_play(q):
    """The nearest point to q off the HUD."""
    x, y = min(max(q[0], 12), 1012), min(max(q[1], 170), 756)
    if y > 650 and (x < 200 or x > 860):
        y = 650
    return (int(x), int(y))
# The menus' buttons (their nibs).
NEW_GAME = (299, 718)
LEVELS = (754, 719)  # MainMenu's second button: the file select
FILE_FIRST = (353, 463)  # FileSelect: the first pack's folder (missions 1-20)
FILE_CFO = (760, 570)  # FileSelect: the CFO pack's folder (missions 21-30)
CUTSCENE_NEXT = (886, 725)
START_SCENE_GO = (887, 701)
TUTORIAL_OK = (512, 514)
# LevelSelect.nib: the first page's mission buttons (tags 1..6).
LEVEL_BUTTON = {n: (219 + 304 * ((n - 1) % 3), 179 + 310 * ((n - 1) // 3)) for n in range(1, 7)}
# The second page (missions 7-10: tags 7-10, the first row and the bonus)
# behind "next".
LEVEL_BUTTON.update({7: (219, 179), 8: (523, 179), 9: (827, 179), 10: (232, 516)})
LEVEL_NEXT = (886, 725)
LEVEL_BACK = (148, 726)
# Where the tongs let go to put a thing on the tray (Tongs doTurnOff: the
# finger over 64..192, below 220 in the world's points).
TRAY = (256, 560)


class Timeout(Exception):
    pass


class Live:
    """The game, driven as it runs."""

    def __init__(self, exe, save):
        # Its sound on another output than the player's headset (A50).
        env = dict(os.environ, AS_HIDDEN="1", AS_CONTROL="1", AS_SAVE_DIR=save, AS_AUDIO_AVOID="A50")
        self.vacuum_rest = 0  # frame before which the splatter is left alone
        self.forced_lying = 1 << 30  # the splatter left when last vacuumed with nothing else to do
        self.alert_seen = -1  # the last alert line answered
        self.shots_useless = False  # the heart's shots do nothing (the graveyard)
        # Gauges to keep needles on (transistors'): serial -> (gauge,
        # needle, its way, axis) from its last "injury balance" line.
        self.balance = {}
        self.p = subprocess.Popen([exe, "-d"], env=env, cwd=os.path.dirname(exe), stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8", errors="replace", bufsize=1)
        self.save = save
        self.log = []
        self.frames = []  # the frame each log line came by
        self.frame = 0
        self.tool = None
        self.popups_done = 0
        self.vacuumed = False
        self.injected = False
        self._read()

    def send(self, line):
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()

    def _read(self):
        for line in self.p.stdout:
            line = line.rstrip("\n")
            if line.startswith("@frame "):
                self.frame = int(line[7:])
                return
            self.log.append(line)
            self.frames.append(self.frame)
            m = re.match(r"^injury balance #(\d+) gauge (-?\d+) needle (-?\d+) dir (-?[\d.]+) axis (\w+)", line)
            if m:
                self.balance[m[1]] = (int(m[2]), int(m[3]), float(m[4]), m[5])
            elif line.startswith(("injury steady", "injury removed")):
                self.balance.pop(line.rsplit("#", 1)[-1], None)
            elif line.startswith(("stage ", "mission ")):
                self.balance = {}
        try:
            code = self.p.wait(timeout=10)
        except Exception:
            code = None
        raise Timeout("the game ended at frame %d (exit code %s)" % (self.frame,
                      hex(code & 0xffffffff) if isinstance(code, int) else code))

    def run(self, frames):
        # While a needle is to be kept on its gauge, a tap of the arrow
        # toward where the gauge is going every other frame (whatever else
        # is being done meanwhile: the keys are the other hand's).
        while frames > 0:
            step = min(frames, 2) if self.balance else frames
            if self.balance:
                self._steer()
            self.send("run %d" % step)
            self._read()
            frames -= step

    def _steer(self):
        # (Each gauge on its own axis: the device tilted both ways at once.)
        for gauge, needle, way, axis in self.balance.values():
            target = gauge + way * 3
            key = None
            if needle < target - 1:
                key = "right" if axis == "across" else "down"
            elif needle > target + 1:
                key = "left" if axis == "across" else "up"
            if key:
                self.send("key %d:%s" % (self.frame, key))

    def mark(self):
        return len(self.log)

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

    def click(self, x, y, settle=8):
        self.send("click %d:%d:%d" % (self.frame, x, y))
        self.run(settle)

    def key(self, k, settle=4):
        self.send("key %d:%s" % (self.frame, k))
        self.run(settle)

    def drag(self, points, speed=6.0, hold=0):
        """A press held along the points (speed: screen pixels a frame), let go."""
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

    def shot(self, name):
        self.send("shot " + os.path.join(self.save, name + ".png"))
        self.run(1)

    def close(self):
        try:
            self.send("quit")
            self.p.wait(timeout=20)
        except Exception:
            self.p.kill()

    # ---- what the trace says
    def injuries(self):
        """The latest report of each injury:
        {key: (class, stage, max, tool, alive, boxes, rotation, hidden, at)}."""
        out = {}
        head = re.compile(r"^injury (\w+) (\w+) id (\d+) stage (\d+)/(\d+) tool (\S*) alive (\d) "
                          r"at (-?\d+),(-?\d+) rot (-?\d+)(?: hidden (\d))?(?: #(\d+))?")
        stage_start = 0
        removed = set()
        for k, line in enumerate(self.log):
            if line.startswith("stage "):
                stage_start = k
        for k in range(stage_start, len(self.log)):
            m = head.match(self.log[k])
            if not m:
                continue
            # Each injury by its serial (its screen place moves when the
            # screen shakes).
            key = m[12] or (m[2], m[3], m[8], m[9])
            # (Gone for good: a tool's "treated" line after an injury took
            # itself away does not bring it back.)
            if m[1] == "removed" or key in removed:
                removed.add(key)
                out.pop(key, None)
                continue
            boxes, healthy, pieces = [], [], []
            for line in self.log[k + 1:]:
                b = re.match(r"^  box (-?\d+),(-?\d+) (-?[\d.]+)( piece)?", line)
                if not b:
                    break
                if b[4]:
                    pieces.append((int(b[1]), int(b[2])))
                    continue
                boxes.append((int(b[1]), int(b[2])))
                if float(b[3]) > 0:
                    healthy.append(boxes[-1])
            tool = m[6]
            if tool == "chainsaw,tongs":
                # A transplant's first stage: each part's line sawn (its
                # boxes left to saw), then the loose pieces taken.
                if healthy:
                    tool, boxes = "chainsaw", healthy
                else:
                    tool, boxes = "tongs", pieces[:1] or boxes[:1]
            elif tool not in ("tongs", "injection", "anything", "battery", "tap"):
                # The tongs take by any box, the injection is given at any
                # (a poison blob's has nothing to heal), the battery shocks
                # at any (a squid's mouth); other tools treat the boxes still
                # to heal.
                boxes = healthy
            out[key] = (m[2], int(m[4]), int(m[5]), tool, m[7] == "1", boxes, int(m[10]), m[11] == "1",
                        (int(m[8]), int(m[9])), m[12])
        return out

    def tray(self):
        """What waits on the tongs' tray: (where it lies, where it goes) in
        screen points, or None (the last "tray" line)."""
        for line in reversed(self.log):
            if line == "tray empty" or line.startswith("menu "):
                return None
            m = re.match(r"^tray \S+ at (-?\d+),(-?\d+) to (-?\d+),(-?\d+)$", line)
            if m:
                return (int(m[1]), int(m[2])), (int(m[3]), int(m[4]))
        return None

    def moving(self, serial):
        """Where an injury that walks about is now (its last "injury moved"
        line), or None."""
        t = self.track(serial)
        return t[0] if t else None

    def track(self, serial):
        """An injury that walks about: (where it is, where it was before
        that, whether it can be treated now), or None."""
        tag = "injury moved #%s at " % serial
        seen = []
        for line in reversed(self.log):
            if line.startswith(tag):
                rest = line[len(tag):].split()
                x, y = rest[0].split(",")
                seen.append(((int(x), int(y)), len(rest) < 3 or rest[2] == "1"))
                if len(seen) == 2:
                    break
        if not seen:
            return None
        before = seen[1][0] if len(seen) > 1 else seen[0][0]
        return seen[0][0], before, seen[0][1]

    def shocked(self):
        """The patient in shock (its last "heart" line says so): revive."""
        for line in reversed(self.log):
            if line.startswith("heart "):
                return line.endswith(" shock")
        return False

    def heart(self):
        """The heart rate the monitor shows now (its last "heart" line)."""
        for line in reversed(self.log):
            if line.startswith("heart "):
                return int(line.split()[1])
        return 100

    def drops(self, kind=None):
        """The blood and other drops lying on the body now (screen points):
        each "drop landed" not matched by a "drop gone" near it (blood lasts
        until a new background clears it, other splatters the stage)."""
        lying = []
        for line in self.log:
            if line == "blood cleared":
                lying = []
                continue
            if line.startswith("stage "):
                lying = [d for d in lying if d[0]]
                continue
            m = re.match(r"^drop (landed|gone) (blood|splatter) (-?\d+),(-?\d+)(?: #(\d+))?$", line)
            if not m:
                continue
            p = (int(m[3]), int(m[4]))
            if m[1] == "landed":
                lying.append((m[2] == "blood", p, m[5]))
            elif m[5]:
                # Taken: that drop (wherever the nozzle had drawn it to).
                lying = [d for d in lying if d[2] != m[5]]
            elif lying:
                # Taken where the nozzle had drawn it to: the nearest one.
                dist = lambda k: (lying[k][1][0] - p[0]) ** 2 + (lying[k][1][1] - p[1]) ** 2
                k = min(range(len(lying)), key=dist)
                if dist(k) < 200 ** 2:
                    lying.pop(k)
        return [d[1] for d in lying if kind is None or (kind == "blood") == d[0]]

    # ---- playing
    def dismiss_tutorials(self, since=None):
        """Taps the button of each tutorial popup not answered yet."""
        n = 0
        while True:
            shown = sum(1 for line in self.log if line.startswith("tutorial popup"))
            if shown <= self.popups_done:
                return n
            self.click(*TUTORIAL_OK, settle=10)
            self.popups_done += 1
            n += 1

    def offered(self):
        """The tools the HUD offers now (its last "tools ..." line)."""
        for line in reversed(self.log):
            if line == "tools" or line.startswith("tools "):
                return set(line.split()[1:])
        return set()

    def in_hand(self):
        """The tool the game says is in hand (its last "tool" line)."""
        for line in reversed(self.log):
            m = re.match(r"^tool (\S*)$", line)
            if m:
                return m[1]
        return None

    def pick(self, tool):
        """Presses the tool's number key until the game has it in hand (a key
        in a stage's fade or under a popup is not taken)."""
        for _ in range(6):
            if self.in_hand() == tool:
                break
            self.key(TOOL_KEY[tool], settle=4)
            if self.in_hand() != tool:
                self.run(20)
        self.tool = tool


def start_first_mission(g):
    """From the title screen through the intro to mission 1's first stage."""
    g.wait_for(r"^menu MainMenu", limit=900)
    g.run(420)  # the legal text fades, the buttons rise
    m = g.mark()
    g.click(*NEW_GAME, settle=20)
    g.wait_for(r"^movie ", limit=300, since=m)
    m = g.mark()
    g.key("esc", settle=20)
    g.wait_for(r"^menu CutScene", limit=600, since=m)
    m = g.mark()
    for _ in range(60):
        if g.find(r"^menu StartScene", m):
            break
        g.click(*CUTSCENE_NEXT, settle=40)
    g.wait_for(r"^menu StartScene", limit=600, since=m)
    g.run(30)
    g.shot("m1_start")
    m = g.mark()
    g.click(*START_SCENE_GO, settle=20)
    g.wait_for(r"^stage 1$", limit=1200, since=m)
    g.run(20)
    return m


def close_alert(g, since):
    """A text message in the story (the CFO's), not yet answered: closed by
    its button. True if one was."""
    k = max((i for i in range(since, len(g.log)) if g.log[i].startswith("alert ")), default=None)
    if k is None or k <= g.alert_seen:
        return False
    g.alert_seen = k
    x, y = g.log[k].split(" button ")[1].split(",")
    g.click(int(x), int(y), settle=40)
    return True


def start_mission(g, n):
    """From the level select (its first page) through the mission's story to
    its first stage."""
    m = g.mark()
    # To its page (the level select opens on the last mission's: of each
    # ten, 1-6 on one page and 7-10 on the next), by the trace's "level
    # page <n>"; the button is the one of its place in the ten.
    tens, place = (n - 1) // 10, (n - 1) % 10 + 1
    want = tens * 2 + (0 if place <= 6 else 1)
    for _ in range(8):
        shown = [int(line.split()[2]) for line in g.log if line.startswith("level page ")]
        if not shown or shown[-1] == want:
            break
        g.click(*(LEVEL_NEXT if shown[-1] < want else LEVEL_BACK), settle=20)
    g.click(*LEVEL_BUTTON[place], settle=20)
    g.wait_for(r"^menu (CutScene|OperationDetails)", limit=600, since=m)
    for _ in range(30):
        if g.find(r"^menu StartScene", m):
            break
        if close_alert(g, m):
            continue
        g.click(*CUTSCENE_NEXT, settle=40)
    g.wait_for(r"^menu StartScene", limit=600, since=m)
    g.run(30)
    m = g.mark()
    g.click(*START_SCENE_GO, settle=20)
    g.wait_for(r"^stage 1$", limit=1200, since=m)
    g.run(20)
    return m


def flier(g, v):
    """An injury that flies about and is shocked with the battery."""
    return bool(v[9] and g.moving(v[9]) and v[3] == "battery")


def walking(g, serial):
    """Something that walks about and has moved within the last second (a
    flier landed for good, the big tick sucking again, is treated where it
    is)."""
    return bool(serial and g.moving(serial) and still_for(g, serial) < 30)


def reachable(g, v):
    """Within reach now: open, and in the play area (off the HUD)."""
    t = g.track(v[9])
    return bool(t and t[2] and in_play(t[0]))


def still_for(g, serial):
    """Frames since an injury's last "injury moved" line (they come only
    as it moves)."""
    tag = "injury moved #%s at " % serial
    k = max(i for i in range(len(g.log)) if g.log[i].startswith(tag))
    return g.frame - g.frames[k]


def rests_seen(g, serial):
    """How long each of a flier's earlier rests off the play area lasted
    (frames between "injury moved" lines, more than 10 apart, the first
    of them outside)."""
    tag = "injury moved #%s at " % serial
    moves = [(g.frames[i], g.log[i][len(tag):].split()[0]) for i in range(len(g.log)) if g.log[i].startswith(tag)]
    out = []
    for (f0, p0), (f1, _) in zip(moves, moves[1:]):
        x, y = (int(c) for c in p0.split(","))
        if f1 - f0 > 10 and not in_play((x, y)):
            out.append(f1 - f0)
    return out


def resting_off(g, v):
    """How long a flier has been still off the play area, or None."""
    if not g.track(v[9]) or reachable(g, v):
        return None
    return still_for(g, v[9])


def shake(g, strokes=8):
    """The pointer shaken (hovering, not pressed) below the body: a stroke
    each 3 frames (the game's passes come every other one)."""
    for k in range(strokes):
        g.send("hover %d %d" % (512 + (80 if k % 2 else -80), 720))
        g.run(3)


def await_flier(g, serial, limit=320):
    """With the battery in hand, waits for a flier resting off the body to
    move: shocked if it comes within reach, else back to work once it has
    gone still again elsewhere."""
    g.pick("battery")
    start = g.track(serial)
    for _ in range(limit):
        g.run(1)
        t = g.track(serial)
        if not t:
            return
        (x, y), _, open_ = t
        if open_ and in_play((x, y)):
            zap(g, serial, limit=30)
            return
        if t[0] != start[0] and still_for(g, serial) > 10:
            return


def treat(g, want_tools, limit=600):
    """Treats injuries needing the given tools until none is left, the stage
    ends (or no progress)."""
    entry = g.mark()
    tries, last_attempt = 0, None
    for _ in range(limit):
        if g.find(r"^(stage \d+|mission (won|failed))", entry):
            return True
        m = g.mark()
        g.run(4)
        g.dismiss_tutorials(m)
        # The stage may have ended meanwhile: the next one's injuries are
        # not this loop's (they wait for its fade).
        if g.find(r"^(stage \d+|mission (won|failed))", entry):
            return True
        # The patient shocked (first of all): revived with the battery.
        if g.shocked() and "battery" in g.offered():
            g.pick("battery")
            if os.environ.get("PLAYTEST_VERBOSE"):
                print("  frame %d: reviving" % g.frame)
            # (Through a new brain whose last stage is the restart.)
            via = [v for v in g.injuries().values() if v[4] and v[0] == "BrainTransplant" and v[3] == "battery"]
            # (Its box nearest the middle: one by the screen's edge may be
            # off the play area.)
            spot = min(via[0][5], key=lambda b: (b[0] - 512) ** 2 + (b[1] - 384) ** 2) if via else clear_spot(g)
            g.click(*spot, settle=8)
            continue
        # The heart sinking (a long clean-up, a boss's craze): a shot before
        # anything else, as a careful player would (not where shots do
        # nothing, the graveyard).
        if (0 < g.heart() < 30 and "injection" in g.offered() and g.injected and not g.shots_useless
                and g.frame >= getattr(g, "heal_after", 0)):
            if os.environ.get("PLAYTEST_VERBOSE"):
                print("  frame %d: heart %d, a shot" % (g.frame, g.heart()))
            before = g.heart()
            inject(g)
            if g.heart() <= before:
                # (A shot that did not help, as the heart running down with a
                # brain out: not again for a while.)
                g.heal_after = g.frame + 300
            continue
        # Something that flies come within reach (a wasp landed, its rest
        # brief): shocked before anything else.
        near = [v for v in g.injuries().values() if v[4] and flier(g, v) and reachable(g, v)]
        if near and "battery" in g.offered():
            zap(g, near[0][9], limit=30)
            continue
        # One resting off the body that may come back any moment (a dove's
        # low pass, a wasp landing: both brief): waited for, from 60 frames
        # before its shortest rest so far would end, rather
        # than starting something long.
        def due_soon(v):
            r = resting_off(g, v)
            seen = rests_seen(g, v[9])
            return r is not None and r > max(60, min(seen) - 60 if seen else 60)

        due = [v for v in g.injuries().values() if v[4] and flier(g, v) and due_soon(v)]
        if due and "battery" in g.offered():
            await_flier(g, due[0][9])
            continue
        # Something to rub fast to stun (a big centipede attacking: no
        # tool works meanwhile): the
        # finger dragged back and forth across it, 40 points a frame.
        rub = [v for v in g.injuries().values() if v[4] and v[3] == "rub" and v[5]]
        if rub:
            c = rub[0][5][0]
            if os.environ.get("PLAYTEST_VERBOSE"):
                print("  frame %d: rubbing %s at %s" % (g.frame, rub[0][0], c))
            g.drag([(c[0] + (90 if k % 2 else -90), c[1] + 60) for k in range(16)], speed=40.0, hold=1)
            continue
        # Something knocked off by shaking the device (a tick sucking): on
        # a PC the pointer shaken, back and forth 160 points a frame, not
        # pressed.
        if any(v[4] and v[3] == "shake" for v in g.injuries().values()):
            if os.environ.get("PLAYTEST_VERBOSE"):
                print("  frame %d: shaking" % g.frame)
            shake(g)
            continue
        # A shot of the injection once it is to hand (its tutorial asks),
        # and while the heart is low (one a round, more when it is very low,
        # so a landed flier is not missed meanwhile).
        if "injection" in g.offered() and not g.shots_useless:
            shots = 0
            while (not g.injected or 0 < g.heart() < 45) and shots < (4 if g.heart() < 15 else 1):
                before = g.heart()
                inject(g)
                # (A shot that did nothing for the heart, the graveyard's:
                # no more.)
                if g.injected and g.heart() <= before:
                    g.shots_useless = True
                    break
                g.injected = True
                shots += 1
        # Blood sucked up once, when the vacuum comes to hand (its tutorial
        # asks for it; blood does the patient no harm).
        if "vacuum" in g.offered() and not g.vacuumed and g.drops():
            vacuum_blood(g, limit=2)
            g.vacuumed = True
        # A replacement (a healthy bone, a pin) waiting on the tray: taken
        # from it and let go where it goes.
        waiting = g.tray()
        if waiting and "tongs" in g.offered():
            g.pick("tongs")
            if os.environ.get("PLAYTEST_VERBOSE"):
                print("  frame %d: from the tray %s to %s" % (g.frame, waiting[0], waiting[1]))
            a, b = waiting
            g.drag([a, (a[0], a[1] - 20), b], speed=4.0, hold=6)
            g.run(4)
            continue
        live = [v for v in g.injuries().values() if v[4] and v[3] in want_tools and v[5]]
        # Something crawling about that bites (a centipede) is seen to
        # before the splatter.
        crawling = [v for v in live if walking(g, v[9]) and not flier(g, v) and v[3] in g.offered()]
        # (Splatter is sucked up in one go: once nothing else is to be done,
        # or a lot of it lies about.)
        busy = [v for v in live if v[3] in g.offered() and v[3] != "vacuum"]
        lying = g.drops("splatter")
        if ("vacuum" in g.offered() and lying and not crawling and g.frame >= g.vacuum_rest
                and (not busy or len(lying) >= 30)):
            # Splatter that only the vacuum clears (a dove's droppings, a
            # burst poison blob's, which saps the heart while it lies):
            # sucked up as soon as it lands. (Drops it does not lessen are
            # left a while for the rest to be seen to.)
            before = len(lying)
            vacuum_blood(g, limit=2, kind="splatter")
            if len(g.drops("splatter")) >= before:
                g.vacuum_rest = g.frame + 300
            continue
        if not live:
            # Something that can't be treated just now (a squid resting,
            # angry or spitting, or dying, its ink still to come): waited
            # for.
            if any(v[3] == "wait" for v in g.injuries().values()):
                g.run(10)
                continue
            # Splatter only the vacuum clears, left alone a while after a
            # pass that did not lessen it, while there was more to do: with
            # nothing else left, vacuumed now (while each pass lessens it).
            lying = g.drops("splatter")
            if "vacuum" in g.offered() and not lying and any(
                    v[4] and v[0].endswith("Splatter") for v in g.injuries().values()):
                # A blob just burst: its drops land a moment later.
                g.run(10)
                continue
            if "vacuum" in g.offered() and lying and len(lying) < g.forced_lying:
                g.forced_lying = len(lying)
                g.vacuum_rest = 0
                continue
            return True
        # Only what the HUD offers yet (the tutorials unlock tools as they go).
        todo = [v for v in live if v[3] in g.offered() or v[3] in ("anything", "tap")]
        if not todo:
            g.run(30)
            continue
        # What walks about first (a crab makes new cuts), then the tool in
        # hand (each change of tool is a tap). Something that flies, shocked
        # with the battery, comes first only while it is within reach: while
        # it is away (a wasp resting off the body) the rest is seen to.
        if any(not flier(g, v) for v in todo) and not any(flier(g, v) and reachable(g, v) for v in todo):
            todo = [v for v in todo if not flier(g, v)]
        todo.sort(key=lambda v: (not walking(g, v[9]), v[3] != g.in_hand()))
        cls, stage, mx, tool, alive, boxes, rot, hidden, at, serial = todo[0]
        # The very same thing tried 20 times over to no avail: stuck (the
        # trace kept to see why).
        attempt = (serial, cls, stage, tool, tuple(map(tuple, boxes)))
        tries = tries + 1 if attempt == last_attempt else 1
        last_attempt = attempt
        if tries > 20:
            print("  stuck on %s stage %d with %s" % (cls, stage, tool))
            return False
        if os.environ.get("PLAYTEST_VERBOSE"):
            print("  frame %d: %s stage %d with %s, %d boxes%s" % (g.frame, cls, stage, tool, len(boxes),
                                                                  " (hidden)" if hidden else ""))
        if hidden and "scanner" in g.offered():
            # Under the skin: found with the scanner first (its pulse turns
            # green near it), from a little way off.
            g.pick("scanner")
            c = boxes[0]
            g.drag([(c[0] + 200, c[1] - 120), (c[0] + 60, c[1] - 30), c], speed=8.0, hold=15)
            g.run(4)
            g.dismiss_tutorials()
        if tool == "tap":
            # A button on it (a risen patient's thumb): tapped.
            g.click(*boxes[0], settle=6)
            continue
        if tool == "anything":
            # Any tool will do (a type-n-tell's keys): one that does no
            # harm where it misses (not the battery).
            tool = next((t for t in ("cream", "lighter", "cutter") if t in g.offered()), sorted(g.offered())[0])
            g.pick(tool)
            type_word(g, boxes)
            continue
        g.pick(tool)
        if serial and g.moving(serial) and tool == "battery":
            # It flies about: shocked with a tap as it passes (low, and
            # not still stunned).
            zap(g, serial)
        elif walking(g, serial):
            # It walks about: followed with the tool held on it.
            chase(g, serial, stage)
        elif tool == "injection":
            # (The big centipede's eye: the craze it sets off bites a cut
            # every 10 frames for 3 seconds, so the heart is seen to first,
            # as a careful player would, while it stays stunned.)
            if cls == "BigCentipede":
                for _ in range(2):
                    if g.heart() >= 55:
                        break
                    inject(g)
                    g.pick("injection")
                cur = g.injuries().get(serial)
                if not cur or cur[3] != "injection" or not cur[5]:
                    continue
                boxes = cur[5]
            # Its close-up opened over it, the shot given.
            inject(g, at=boxes[0])
        else:
            use(g, tool, boxes, rot, at, serial, cls)
    return False


KEY_MAP = "abcdefghijklmnopqrstuvwx+-yz#="


def type_word(g, keys):
    """Taps the keys (a type-n-tell's, in its key order) of the word it
    shows (the trace's "word <w>"), each letter until the trace says it was
    typed (a first tap may only wake the tool)."""
    k = max((i for i in range(len(g.log)) if g.log[i].startswith("word ")), default=None)
    if k is None or len(keys) < len(KEY_MAP):
        g.run(10)
        return
    word = g.log[k][5:]
    typed = sum(1 for line in g.log[k:] if line.endswith(" ok") and line.startswith("type "))
    if os.environ.get("PLAYTEST_VERBOSE"):
        print("  frame %d: typing %s from %d" % (g.frame, word, typed))
    for c in word[typed:]:
        for _ in range(3):
            m = g.mark()
            g.click(*keys[KEY_MAP.index(c)], settle=4)
            if g.find(r"^type ", m):
                break
        if g.find(r"^type . wrong", k) or g.find(r"^(stage \d+|mission (won|failed))", k):
            return


def vacuum_blood(g, limit=6, kind=None):
    """Sucks up the drops lying on the body: the nozzle held a while over
    each group of them in turn (its suction draws in those about it), the
    nearest group next, until none is left."""
    for _ in range(limit):
        lying = g.drops(kind)
        if not lying:
            return True
        g.pick("vacuum")
        # Groups: a drop and those within 240 points of it, the next group
        # the nearest to the last. (The suction reaches far: held still
        # long enough the drops all round come in; moved on early they are
        # only dragged about.)
        groups, left, at = [], list(lying), lying[0]
        while left and len(groups) < 8:
            first = min(left, key=lambda q: (q[0] - at[0]) ** 2 + (q[1] - at[1]) ** 2)
            near = [q for q in left if (q[0] - first[0]) ** 2 + (q[1] - first[1]) ** 2 < 240 ** 2]
            for q in near:
                left.remove(q)
            at = (sum(q[0] for q in near) / len(near), sum(q[1] for q in near) / len(near))
            groups.append(at)
        if os.environ.get("PLAYTEST_VERBOSE"):
            print("  frame %d: vacuum over %d drops in %d groups" % (g.frame, len(lying), len(groups)))
        for c in groups:
            # (Kept off the HUD: the suction draws in a drop lying by it.)
            c = keep_in_play(c)
            g.drag([c, (c[0] + 1, c[1])], speed=0.04, hold=4)
            if g.find(r"^(stage \d+|mission (won|failed))", len(g.log) - 1):
                break
        g.run(4)
        g.dismiss_tutorials()
    return not g.drops(kind)


def clear_spot(g):
    """Somewhere to tap for the heart's shot: away from every injury's boxes
    (a tap on one gives the shot to it)."""
    boxes = [b for v in g.injuries().values() for b in v[5]]
    for c in ((560, 330), (300, 600), (760, 600), (300, 180), (760, 180), (520, 680)):
        if all((b[0] - c[0]) ** 2 + (b[1] - c[1]) ** 2 > 110 ** 2 for b in boxes):
            return c
    return (560, 330)


def inject(g, at=None):
    """One shot of the injection, as the Flash AS2's: pressed on `at` (or,
    for the heart, somewhere clear of the injuries) its syringe appears with
    the needle there; held down, the finger goes up the maze to the ring
    along the route the trace gives ("injection path x,y ...": points on the
    path, one a frame), then is let go."""
    if at is None:
        at = clear_spot(g)
    g.pick("injection")
    m = g.mark()
    g.send("down %d %d" % at)
    g.run(2)
    for _ in range(10):
        if g.find(r"^injection (path|closed)", m):
            break
        g.run(1)
    route = g.find(r"^injection path(.*)$", m)
    points = [] if not route or route[1].strip() == "none" else [
        tuple(int(c) for c in q.split(",")) for q in route[1].split()]
    if os.environ.get("PLAYTEST_VERBOSE"):
        print("  frame %d: injection up a maze of %d points" % (g.frame, len(points)))
    last = at
    for q in points:
        if g.find(r"^injection closed", m):
            break
        g.send("move %d %d" % q)
        g.run(1)
        last = q
    g.send("up %d %d" % last)
    g.run(10)
    g.dismiss_tutorials()


def zap(g, serial, limit=600):
    """Waits for a flying injury to be within reach (in the play area, not
    out of reach) and taps the battery a little ahead of it."""
    g.pick("battery")
    m = g.mark()
    for _ in range(limit):
        t = g.track(serial)
        if not t:
            return
        (x, y), (px, py), open_ = t
        # About two frames ahead (it moves 12 points or so between lines).
        ax, ay = x + (x - px), y + (y - py)
        if open_ and in_play((ax, ay)) and in_play((x, y)):
            if os.environ.get("PLAYTEST_VERBOSE"):
                print("  frame %d: shocking #%s at %d,%d" % (g.frame, serial, ax, ay))
            g.click(int(ax), int(ay), settle=12)
            return
        g.run(1)
        if g.find(r"^(stage \d+|mission (won|failed))", m):
            return


def chase(g, serial, stage, limit=300):
    """Holds the tool on a walking injury, following it each frame while it
    is in the play area (off the HUD), until it moves on a
    stage, dies, or `limit` frames pass (then the heart is seen to)."""
    if os.environ.get("PLAYTEST_VERBOSE"):
        print("  frame %d: chasing #%s from %s" % (g.frame, serial, g.moving(serial)))
    m = g.mark()
    done = re.compile(r"^injury (treated|removed) \S+ id \d+ stage (\d+)/.* #%s$" % serial)
    inside = lambda q: q and in_play(q)
    pressed, p = False, None
    aim = None
    for _ in range(limit):
        t = g.track(serial)
        q = t[0] if t else None
        if q and t[1] != q:
            # Waited for a little ahead on its way and held still there,
            # until it has gone past (a walker that shies from the pointer,
            # as the Flash AS2's crab, cannot turn before it has slowed: it
            # walks into the tool).
            dx, dy = q[0] - t[1][0], q[1] - t[1][1]
            n = math.hypot(dx, dy) or 1
            ahead = (aim[0] - q[0]) * dx + (aim[1] - q[1]) * dy if aim else -1
            if ahead < 0:
                aim = keep_in_play((q[0] + dx / n * 40, q[1] + dy / n * 40))
            q = aim
        if inside(q):
            if not pressed:
                g.send("down %d %d" % q)
                pressed = True
            elif q != p:
                g.send("move %d %d" % q)
            p = q
        elif pressed:
            g.send("up %d %d" % p)
            pressed = False
        g.run(1)
        if any(done.match(line) for line in g.log[m:]) or g.find(r"^(stage \d+|mission (won|failed))", m):
            break
    if pressed:
        g.send("up %d %d" % p)
    g.run(4)


def use(g, tool, boxes, rot, at=None, serial=None, cls=None):
    """One use of the tool on an injury's boxes (screen points), as a
    player would."""
    r = math.radians(rot)
    along = (-math.sin(r), math.cos(r))  # down the cut (its boxes' column)
    across = (math.cos(r), math.sin(r))
    if cls == "Eyelid":
        # An eyelid is drawn open away from the eye (up, unturned).
        across = (math.sin(r), -math.cos(r))
    if cls == "Snake":
        # A snake's hole is sewn across: from its middle to the right,
        # crossing the level line.
        along, across = (math.cos(r), math.sin(r)), (-math.sin(r), math.cos(r))
    if tool == "tongs":
        # Press on the object, pull it out along its direction, carry it to
        # the tray (bottom left) and let go there.
        # (As far as the play area allows: some things take a long pull.)
        c = boxes[0]
        reach = 180
        while reach < 600:
            nx, ny = c[0] + across[0] * (reach + 20), c[1] + across[1] * (reach + 20)
            # (Only the press must be off the HUD: the pull may cross it.)
            if not (10 < nx < 1014 and 5 < ny < 760):
                break
            reach += 20
        out = [(c[0] + across[0] * d, c[1] + across[1] * d) for d in range(0, reach + 1, 20)]
        # Hand over hand: a stroke out, and while it has not come out (a
        # clown's scarf is longer than the play area, and slips back when let
        # go), let go and take it again where it was first pressed.
        done = re.compile(r"^injury (taken|treated|removed) .* #%s$" % serial) if serial else None
        for _ in range(16):
            m = g.mark()
            g.send("down %d %d" % out[0])
            g.run(3)
            for q in out[1:]:
                # (Taken on the press, a thing lifted away: no pull.)
                if done is not None and any(done.match(line) for line in g.log[m:]):
                    break
                g.send("move %d %d" % (int(q[0]), int(q[1])))
                g.run(2)
            if done is None or any(done.match(line) for line in g.log[m:]):
                break
            g.send("up %d %d" % (int(out[-1][0]), int(out[-1][1])))
            g.run(1)
            if g.find(r"^(stage \d+|mission (won|failed))", m):
                return
        else:
            return
        # Out (held under the finger): carried to the tray, let go there.
        g.send("move %d %d" % TRAY)
        # (A slippery thing, an egg, slides after the finger a while.)
        g.run(30 if cls == "FireFlyEgg" else 10)
        g.send("up %d %d" % TRAY)
        g.run(6)
    elif tool == "stitch":
        # Zigzag across the cut's middle line, down its length and back.
        pts = sorted(boxes, key=lambda p: p[0] * along[0] + p[1] * along[1])
        if len(pts) < 3:
            # One box (a risen patient's body): a few stitches down from it.
            c = pts[0]
            pts = [(c[0] + along[0] * d, c[1] + along[1] * d) for d in (10, 30, 50, 70)]
        zig = []
        for k, p in enumerate(pts + pts[::-1]):
            side = 40 if k % 2 == 0 else -40
            zig.append((p[0] + across[0] * side, p[1] + across[1] * side))
        # (Kept off the HUD.)
        zig = [keep_in_play(q) for q in zig]
        g.drag(zig, speed=12.0, hold=2)
    elif tool == "chainsaw" or len(boxes) > 8:
        # A dashed line (a transplant's, round its piece): along it box by
        # box, each the nearest not yet passed (the table's order can jump
        # across the piece, and a saw off the line cuts the patient),
        # slowly enough to saw (or treat) each.
        # (Only as far as the line goes: a jump to boxes elsewhere, a
        # squid's other tentacles, waits for its own turn.)
        if len(boxes) == 1:
            # One box sawn a long while (a snake): the saw worked about on it.
            c = boxes[0]
            g.drag([c] + [(c[0] + dx, c[1]) for dx in (20, -20) * 2] + [c], speed=3.0, hold=2)
            return
        # (Pressed first on a box off the HUD: a line may pass under the
        # tool cards, and a press there picks a tool; dragging over is fine.)
        start = next((b for b in boxes if in_play(b)), None)
        if start is None:
            # (All of them under the cards: pressed just below, dragged up.)
            start = boxes[0]
            left, path = [b for b in boxes if b is not start], [keep_in_play(start), start]
        else:
            left, path = [b for b in boxes if b is not start], [start]
        while left:
            last = path[-1]
            k = min(range(len(left)), key=lambda k: (left[k][0] - last[0]) ** 2 + (left[k][1] - last[1]) ** 2)
            if (left[k][0] - last[0]) ** 2 + (left[k][1] - last[1]) ** 2 > 80 ** 2:
                break
            path.append(left.pop(k))
        # (The saw slowly; the lighter and cream treat a box in a frame or
        # two, at a careful person's pace, 360 points a second: a brain out
        # gives 18 seconds for the new one's two lines and the battery.)
        g.drag(path, speed=4.0 if tool == "chainsaw" else 12.0, hold=2)
    else:
        # Along the boxes, end to end, slowly enough for each to be treated
        # (2 health, the tool's heal amount a frame).
        pts = sorted(boxes, key=lambda p: p[0] * along[0] + p[1] * along[1])
        # (A little past each end, where that is not on the HUD: a press there
        # picks a tool.)
        first = (pts[0][0] - along[0] * 10, pts[0][1] - along[1] * 10)
        last = (pts[-1][0] + along[0] * 10, pts[-1][1] + along[1] * 10)
        ends = ([first] if in_play(first) else []) + pts + ([last] if in_play(last) else [])
        if on_hud(ends[0]):
            ends.reverse()
        g.drag(ends, speed=9.0, hold=2)


def play_mission(g, name, since=0, limit=24):
    """Treats every stage in turn (the trace's "stage <n>" after `since`), a
    screenshot at each start, until the mission ends; true if it is won."""
    for n in range(1, limit + 1):
        g.wait_for(r"^(stage %d$|mission (won|failed))" % n, limit=900, since=since)
        # A stage with an injury the port lacks would end without it.
        missing = g.find(r"^injury: class (\S+) is not ported", since)
        if missing:
            print("  stage %d: %s is not ported" % (n, missing[1]))
            return False
        if g.find(r"^mission (won|failed)", since):
            break
        # A new body part fades in: the tools do nothing until it has.
        begun = next(k for k in range(since, len(g.log)) if g.log[k] == "stage %d" % n)
        g.wait_for(r"^fade done$", limit=300, since=begun)
        g.run(10)
        g.dismiss_tutorials()
        g.shot("%s_stage%d" % (name, n))
        if not treat(g, set(TOOLS) | {"anything", "tap"}):
            print("  stage %d: stuck" % n)
            return False
    g.wait_for(r"^mission (won|failed)", limit=300, since=since)
    return bool(g.find(r"^mission won", since))


def after_win(g, name):
    """From the end cut scene through the upgrade (the first tool) and the
    mission's summary to the level select."""
    m = g.mark()
    for _ in range(60):  # (mission 9's has 28 pages)
        if g.find(r"^menu (UpgradeTool|(CFO)?MissionWin)", m):
            break
        if close_alert(g, m):
            continue
        g.click(*CUTSCENE_NEXT, settle=40)
    if g.find(r"^menu UpgradeTool", m):
        g.run(20)
        g.shot(name + "_upgrade")
        upgrade(g)
    shown = g.wait_for(r"^menu (CFO)?MissionWin", limit=300, since=m)
    if shown[1]:
        # The CFO pack's own summary (its nib): continue at the bottom right,
        # once the grade has drawn in.
        g.run(120)
        at = (None, 895, 722)
    else:
        # The discharge form (the Flash AS2's): its continue where the trace
        # says, once it has come in and its grade drawn in.
        at = g.wait_for(r"^flash piece summary\.holder\.start at (\d+),(\d+)", limit=300, since=m)
        g.run(30)
    g.shot(name + "_win")
    m = g.mark()
    # (Again if it stays: the summary can come up twice, the story's last
    # page shown both by its timer and by a tap.)
    for _ in range(3):
        g.click(int(at[1]), int(at[2]), settle=30)
        if g.find(r"^(menu LevelSelect|movie )", m):
            break
        g.run(60)
    if g.find(r"^movie ", m):
        # A pack's ending movie (after mission 19): ended with a double tap.
        g.run(60)
        g.click(512, 384, settle=1)
        g.click(512, 384, settle=10)
    g.wait_for(r"^menu LevelSelect", limit=300, since=m)
    g.run(30)
    g.shot(name + "_levels")
    return True


def upgrade(g):
    """Buys an upgrade on the scrapbook (the Flash AS2's): the first card it
    offers (a tool below its best), then "upgrade tool", where the trace
    says they are once the book has come in."""
    shown = max(i for i, line in enumerate(g.log) if line.startswith("menu UpgradeTool"))
    card = g.wait_for(r"^flash piece upgrade\.holder\.\w+ at (\d+),(\d+)", limit=400, since=shown)
    m = g.mark()
    g.click(int(card[1]), int(card[2]), settle=10)
    at = g.wait_for(r"^flash piece upgrade\.holder\.restore at (\d+),(\d+)", limit=120, since=m)
    m = g.mark()
    g.click(int(at[1]), int(at[2]), settle=10)
    g.wait_for(r"^menu (MissionWin|LevelSelect)", limit=300, since=m)


def to_level_select(g, cfo=False):
    """From the title screen with a save to the level select (the file
    select's first folder, or the CFO pack's; an owed upgrade first)."""
    g.wait_for(r"^menu MainMenu", limit=900)
    g.run(420)
    m = g.mark()
    g.click(*LEVELS, settle=20)
    g.wait_for(r"^menu FileSelect", limit=300, since=m)
    g.run(30)
    m = g.mark()
    g.click(*(FILE_CFO if cfo else FILE_FIRST), settle=20)
    g.wait_for(r"^menu (LevelSelect|UpgradeTool)", limit=300, since=m)
    if g.find(r"^menu UpgradeTool", m):
        g.run(20)
        upgrade(g)
        g.wait_for(r"^menu LevelSelect", limit=300, since=m)
    g.run(30)


def play_from(g, first, last):
    """With a save that has missions before `first` won: missions `first`
    to `last` in turn."""
    to_level_select(g, cfo=first > 20)
    for n in range(first, last + 1):
        m = start_mission(g, n)
        name = "m%d" % n
        if not (play_mission(g, name, since=m) and after_win(g, name)):
            return False
    return True


def play_up_to(g, last):
    """Missions 1 to `last` in turn, each won and on to the level select."""
    start_first_mission(g)
    if not (play_mission(g, "m1") and after_win(g, "m1")):
        return False
    for n in range(2, last + 1):
        m = start_mission(g, n)
        name = "m%d" % n
        if not (play_mission(g, name, since=m) and after_win(g, name)):
            return False
    return True


def scenario_start(g):
    """Only into mission 1 and a little of it."""
    start_first_mission(g)
    g.run(200)
    return True


# missionN: missions 1 to N, in order (the level select's first page: up
# to 6).
SCENARIOS = {"mission%d" % n: (lambda g, n=n: play_up_to(g, n)) for n in range(1, 7)}
SCENARIOS["start"] = scenario_start
# fromN (with --save): missions N to 10 from a save that has the ones
# before won (a real run's, kept with --keep).
SCENARIOS.update({"from%d" % n: (lambda g, n=n: play_from(g, n, 10)) for n in range(2, 11)})
# onlyN (with --save): mission N alone, from a save that opens it.
SCENARIOS.update({"only%d" % n: (lambda g, n=n: play_from(g, n, n)) for n in range(1, 31)})


def main():
    args = sys.argv[1:]
    build = "build-dev"
    keep = False
    if "--build" in args:
        k = args.index("--build")
        build = args[k + 1]
        del args[k:k + 2]
    seed = None
    if "--save" in args:
        k = args.index("--save")
        seed = args[k + 1]
        del args[k:k + 2]
    if "--keep" in args:
        keep = True
        args.remove("--keep")
    # A copy runs (port/build-test, beside port/games like the build; or
    # PLAYTEST_DIR there, for tests side by side), so the build can be
    # relinked while a test plays.
    test_dir = os.path.join(PORT, os.environ.get("PLAYTEST_DIR", "build-test"))
    os.makedirs(test_dir, exist_ok=True)
    for f in os.listdir(os.path.join(PORT, build)):
        if f.endswith((".exe", ".dll")):
            shutil.copy2(os.path.join(PORT, build, f), test_dir)
    exe = os.path.join(test_dir, "AmateurSurgeon2.exe")
    names = args or ["mission1"]
    failed = 0
    for name in names:
        save = tempfile.mkdtemp(prefix="as2_" + name + "_")
        if seed:
            shutil.copy(os.path.join(seed, "UserDefaults.txt"), save)
        g = Live(exe, save)
        try:
            ok = SCENARIOS[name](g)
        except Timeout as e:
            ok = False
            print("  timeout:", e)
        finally:
            g.close()
        print("%s: %s (frame %d, save %s)" % (name, "ok" if ok else "FAILED", g.frame, save))
        # Pictures the game asked for and could not find.
        missing = sorted({line for line in g.log if line.startswith("image not found")})
        for line in missing:
            print("  " + line)
        if not ok or keep:
            # The whole trace kept beside the save, to look into.
            with open(os.path.join(save, "trace.txt"), "w", encoding="utf-8") as f:
                for fr, line in zip(g.frames, g.log):
                    f.write("%6d %s\n" % (fr, line))
        if not ok:
            failed += 1
            for f, line in list(zip(g.frames, g.log))[-40:]:
                if not line.startswith("  box") and "is not ported" not in line:
                    print("    %6d %s" % (f, line))
        if ok and not keep:
            shutil.rmtree(save, ignore_errors=True)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
