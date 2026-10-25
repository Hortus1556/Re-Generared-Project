#!/usr/bin/env python3
"""Reference screenshots of a Flash original, played by Ruffle.

The native rebuilds of the Flash games (docs/christmas.md) are checked
against the original movie running with its own scripts. This plays the SWF
in Ruffle (tools/ruffle/ruffle.exe, a Flash player), drives it with a small
script of waits, clicks and screenshots, and saves the stage's pixels.

Script lines (times in seconds, coordinates in stage pixels):
    wait <seconds>
    mark                        times for at count from here (else the start)
    at <seconds>                until that long after the mark
    click <x> <y>
    move <x> <y>
    drag <x1> <y1> <x2> <y2> <seconds>
    down <x> <y> / up <x> <y>   the button pressed or let go there
    glide <x1> <y1> <x2> <y2> <seconds>   moves (the button as it is)
    shot <name>                 saves <outdir>/<name>.png (the stage, 600x460)
    shots <name> <count> <gap>  <count> shots <gap> seconds apart

Usage: ruffle_ref.py <swf> <outdir> <script file> [--files dir] [--so name k=v,...]
    --files: a folder whose files the movie loads by relative URL (dialogue.xml);
             its subfolders too (the AS2's core/skin/shell.swf, media/levels.txt)
    --so: the movie starts with this saved shared object (path "/"), numbers
          and true/false, e.g. Christmas with mission 1 done:
          --so user_9848945498456554546545454 balls=0,timer=0,level1=1

The Ruffle window must stay visible while it runs (screen capture): it is
kept topmost, and runs wait for each other (a named mutex), so several
callers at once take turns.
"""
import ctypes
import ctypes.wintypes as wt
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import time

from PIL import ImageGrab

WORKSPACE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RUFFLE = os.path.join(WORKSPACE, "tools", "ruffle", "ruffle.exe")
STAGE_W, STAGE_H = 600, 460

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
# Per-monitor DPI aware: window coordinates and screen grabs both in
# physical pixels.
user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))


def find_window(pid, timeout=20):
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def enum(hwnd, _):
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(hwnd):
            t = ctypes.create_unicode_buffer(200)
            user32.GetWindowTextW(hwnd, t, 200)
            if t.value.startswith("Ruffle"):  # not its helper windows
                found.append(hwnd)
        return True

    end = time.time() + timeout
    while time.time() < end:
        found.clear()
        user32.EnumWindows(enum, 0)
        if found:
            return found[0]
        time.sleep(0.2)
    sys.exit("Ruffle's window did not appear")


def client_rect(hwnd):
    r = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    pt = wt.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return pt.x, pt.y, r.right, r.bottom


class Stage:
    """Stage pixels <-> screen pixels (the movie letterboxed in the client area)."""

    def __init__(self, hwnd):
        self.hwnd = hwnd

    def rect(self):
        x, y, w, h = client_rect(self.hwnd)
        s = min(w / STAGE_W, h / STAGE_H)
        return x + (w - STAGE_W * s) / 2, y + (h - STAGE_H * s) / 2, s

    def to_screen(self, sx, sy):
        x, y, s = self.rect()
        return int(x + sx * s), int(y + sy * s)

    def grab(self):
        # The screen's pixels (the window must be visible); grabs of all
        # screens count from the virtual desktop's corner.
        x, y, s = self.rect()
        vx, vy, vw = user32.GetSystemMetrics(76), user32.GetSystemMetrics(77), user32.GetSystemMetrics(78)
        im = ImageGrab.grab(all_screens=True)
        k = im.size[0] / vw  # the grab may be in scaled (not physical) pixels
        box = (int((x - vx) * k), int((y - vy) * k), int((x - vx + STAGE_W * s) * k), int((y - vy + STAGE_H * s) * k))
        return im.crop(box).resize((STAGE_W, STAGE_H))


MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP = 0x0002, 0x0004


def mouse(stage, x, y, down=None):
    user32.SetCursorPos(*stage.to_screen(x, y))
    if down is True:
        user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    elif down is False:
        user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)


def write_sol(path, name, fields):
    """A Flash shared object (AMF0 .sol) holding numbers and booleans."""
    def utf(s):
        b = s.encode("utf-8")
        return struct.pack(">H", len(b)) + b

    body = b"TCSO\x00\x04\x00\x00\x00\x00" + utf(name) + b"\x00\x00\x00\x00"
    for key, value in fields:
        body += utf(key)
        if value in ("true", "false"):
            body += b"\x01" + (b"\x01" if value == "true" else b"\x00")
        else:
            body += b"\x00" + struct.pack(">d", float(value))
        body += b"\x00"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "wb").write(b"\x00\xbf" + struct.pack(">I", len(body)) + body)


def main():
    args = sys.argv[1:]
    files = None
    so = None
    if "--so" in args:
        i = args.index("--so")
        so = (args[i + 1], [kv.split("=", 1) for kv in args[i + 2].split(",")])
        del args[i:i + 3]
    if "--files" in args:
        i = args.index("--files")
        files = args[i + 1]
        del args[i:i + 2]
    if len(args) != 3:
        sys.exit(__doc__)
    swf, outdir, script = args
    os.makedirs(outdir, exist_ok=True)
    base = tempfile.mkdtemp(prefix="ruffle_ref_")
    shutil.copy(swf, os.path.join(base, "movie.swf"))
    if files:
        shutil.copytree(files, base, dirs_exist_ok=True)
    storage = ["--storage", "memory"]
    if so:
        saves = os.path.join(base, "saves")
        write_sol(os.path.join(saves, "localhost", so[0] + ".sol"), so[0], so[1])
        storage = ["--storage", "disk", "--save-directory", saves]
    # One capture at a time on this machine (several agents or tests may
    # call this at once, and the capture reads the screen): a named mutex,
    # held until this process ends.
    kernel32 = ctypes.windll.kernel32
    kernel32.CreateMutexW.restype = wt.HANDLE
    mutex = kernel32.CreateMutexW(None, False, "ruffle_ref_screen_capture")
    kernel32.WaitForSingleObject(mutex, 0xFFFFFFFF)
    log = open(os.path.join(outdir, "ruffle.log"), "w")
    proc = subprocess.Popen(stdout=log, stderr=subprocess.STDOUT, args=[RUFFLE, "--no-gui", "--volume", "0", "-g", os.environ.get("RUFFLE_GRAPHICS", "gl"), "--width", str(STAGE_W * 2), "--height", str(STAGE_H * 2),
                             *storage, "--base", pathlib.Path(base).resolve().as_uri() + "/", "--quality", "high",
                             os.path.join(base, "movie.swf")])
    try:
        hwnd = find_window(proc.pid)
        user32.SetForegroundWindow(hwnd)
        # Kept above other windows (a terminal over it spoiled the capture).
        user32.SetWindowPos(hwnd, wt.HWND(-1), 0, 0, 0, 0, 0x0001 | 0x0002)  # HWND_TOPMOST; NOSIZE | NOMOVE
        stage = Stage(hwnd)
        start = origin = time.time()
        for line in open(script, encoding="utf-8"):
            w = line.split()
            if not w or w[0].startswith("#"):
                continue
            cmd = w[0]
            if cmd == "wait":
                time.sleep(float(w[1]))
            elif cmd == "mark":
                origin = time.time()
            elif cmd == "at":  # until this long after the mark (no drift)
                time.sleep(max(0.0, origin + float(w[1]) - time.time()))
            elif cmd == "move":
                mouse(stage, float(w[1]), float(w[2]))
            elif cmd == "click":
                mouse(stage, float(w[1]), float(w[2]))
                time.sleep(0.05)
                mouse(stage, float(w[1]), float(w[2]), True)
                time.sleep(0.08)
                mouse(stage, float(w[1]), float(w[2]), False)
            elif cmd in ("down", "up"):
                # the move first: Ruffle takes a press where it last saw the pointer
                mouse(stage, float(w[1]), float(w[2]))
                time.sleep(0.05)
                mouse(stage, float(w[1]), float(w[2]), cmd == "down")
            elif cmd == "glide":
                x1, y1, x2, y2, secs = map(float, w[1:6])
                steps = max(2, int(secs * 50))
                for k in range(1, steps + 1):
                    t = k / steps
                    mouse(stage, x1 + (x2 - x1) * t, y1 + (y2 - y1) * t)
                    time.sleep(secs / steps)
            elif cmd == "drag":
                x1, y1, x2, y2, secs = map(float, w[1:6])
                mouse(stage, x1, y1)
                time.sleep(0.05)
                mouse(stage, x1, y1, True)
                steps = max(2, int(secs * 50))
                for k in range(1, steps + 1):
                    t = k / steps
                    mouse(stage, x1 + (x2 - x1) * t, y1 + (y2 - y1) * t)
                    time.sleep(secs / steps)
                mouse(stage, x2, y2, False)
            elif cmd == "shot":
                stage.grab().save(os.path.join(outdir, w[1] + ".png"))
                print("%7.2f s  %s" % (time.time() - start, w[1]))
            elif cmd == "shots":
                for k in range(int(w[2])):
                    stage.grab().save(os.path.join(outdir, "%s_%03d.png" % (w[1], k)))
                    time.sleep(float(w[3]))
            else:
                sys.exit("unknown command: " + line)
    finally:
        proc.kill()
        shutil.rmtree(base, ignore_errors=True)


if __name__ == "__main__":
    main()
