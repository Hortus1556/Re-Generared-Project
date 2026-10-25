"""Fetches sources, restores music, and builds the games.

usage: python port/tools/build_all.py [--game as1|xmas|as2|as3|all]
                                       [--platform windows|linux|android]
                                       [--no-fetch]

1. Ensures the Flash SWFs and IPAs are present (fetch_sources.ensure),
   unless --no-fetch. Stops if anything required for the requested games
   is missing and has no automated source (see sources_manifest.json).
2. Ensures "music restored/" exists at the workspace root: if it's
   missing, unzips music-restored.zip (next to this script) in place.
   Never touches it if it already exists.
3. Runs the IPA extraction steps that aren't wired into CMake
   (games/as2 and games/as3 -- unlike xmas_package/as1_package/as2_flash/
   as3_pointer, which rebuild themselves from the SWF sources as part of
   the normal build graph). This is the same platform-independent data
   whichever target platform is built next:
     as2: extract_ipa.py, then brain_dial.py, then as2_data.py
     as3: extract_ipa.py
4. Builds for the chosen platform:
   - windows (default): configures port/build-release (a dedicated
     directory, so this never collides with port/build-dev's test
     builds) and builds each requested game's *_dist target. Output in
     the usual dist/surgeon1, dist/surgeonxmas, dist/surgeon2, dist/surgeon3.
   - linux: runs port/tools/linux/build.sh inside WSL (installed
     separately; see docs/port.md), which builds SDL3 for Linux the
     first time (no sudo needed) and then the games, each *_dist target
     landing in dist/<name>-linux/ (CMakeLists.txt adds the suffix, so a
     Linux build never overwrites a Windows one). AS2's and AS3's AAC
     music and movies don't play yet on Linux (no decoder there) --
     AS1 and Christmas need neither and are unaffected.
   - android: runs port/tools/android/build_apk.py per game (needs the
     Android SDK/NDK and a JDK; see docs/port.md). Output in
     dist/<name>-android/.

run_build(games, platform, skip_fetch, log) is the one entry point both
this CLI and build_gui.py use, so the two never drift apart.
"""
import argparse
import contextlib
import os
import shutil
import subprocess
import sys
import zipfile

import fetch_sources

ROOT = fetch_sources.ROOT
ROOT_TOOLS = os.path.join(ROOT, "port", "tools")  # port/tools/ in the checkout at ROOT -- not
                                                   # necessarily where this exe/script itself sits
                                                   # (see fetch_sources.ensure_project_root)
MUSIC_RESTORED = os.path.join(ROOT, "music restored")
MUSIC_ZIP = os.path.join(ROOT_TOOLS, "music-restored.zip")

PLATFORMS = ["windows", "linux", "android"]

DIST_TARGET = {
    "as1": "AmateurSurgeon_dist",
    "xmas": "AmateurSurgeonXmas_dist",
    "as2": "AmateurSurgeon2_dist",
    "as3": "AmateurSurgeon3_dist",
}
IPA = {
    "as2": "IPAs/Amateur Surgeon 2 HD v1.2 iPad os32most_uniQue.ipa",
    "as3": "IPAs/Amateur Surgeon 3 v1.45 (iOS).ipa",
}
# build_apk.py's --game spelling differs from this script's ("xmas" here,
# "christmas" there) -- translate rather than rename one side to match.
ANDROID_GAME = {"as1": "as1", "xmas": "christmas", "as2": "as2", "as3": "as3"}

# Hide the console window a subprocess would otherwise flash open when
# this runs from a windowed (--noconsole) frozen exe. No effect on Linux/mac.
_NO_WINDOW = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0


class BuildError(Exception):
    pass


python_exe = fetch_sources.python_exe


def run(cmd, cwd=ROOT, log=print):
    log("+ " + " ".join(cmd))
    proc = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, bufsize=1, creationflags=_NO_WINDOW)
    for line in proc.stdout:
        log(line.rstrip("\n"))
    proc.wait()
    if proc.returncode != 0:
        raise BuildError(f"command failed (exit {proc.returncode}): {' '.join(cmd)}")


def ensure_music_restored(log=print):
    if os.path.isdir(MUSIC_RESTORED):
        log(f"ok        {MUSIC_RESTORED} already present, left alone")
        return
    if not os.path.isfile(MUSIC_ZIP):
        raise BuildError(f"{MUSIC_ZIP} not found -- cannot restore 'music restored/'")
    log(f"unzipping {MUSIC_ZIP} -> {MUSIC_RESTORED}")
    with zipfile.ZipFile(MUSIC_ZIP) as z:
        z.extractall(ROOT)


class _LogWriter:
    """A file-like object that forwards each printed line to `log`, so a
    bundled tool's own print() calls show up in the GUI's log pane too."""
    def __init__(self, log):
        self.log, self.buf = log, ""

    def write(self, s):
        self.buf += s
        while "\n" in self.buf:
            line, self.buf = self.buf.split("\n", 1)
            self.log(line)

    def flush(self):
        pass


def extract_ipa_packages(games, log=print):
    # Imported and called directly (not as a subprocess of an external
    # python): extract_ipa.py, brain_dial.py and as2_data.py are plain,
    # side-effect-free-at-import modules, so this -- unlike the Android
    # and Linux builds below, which still need Python installed to run
    # build_apk.py / the Linux toolchain -- needs no external Python
    # interpreter at all, even when this whole tool is frozen into an exe.
    # (as2_data.py's own emulate() still shells out to
    # re/scripts/emucalls.py -- a real ARM emulator, kept a separate
    # process on purpose -- using fetch_sources.python_exe(), the same
    # frozen-aware lookup.)
    if "as2" not in games and "as3" not in games:
        return
    import extract_ipa
    # Only capture-and-forward when `log` is a real callback (build_gui.py):
    # if it's the default `print`, redirecting stdout to a writer that
    # itself calls `log` (= print) would recurse into its own redirected
    # stdout. With the default, the modules' own print() calls already
    # reach the real console directly -- nothing to forward.
    ctx = contextlib.redirect_stdout(_LogWriter(log)) if log is not print else contextlib.nullcontext()
    with ctx:
        if "as2" in games:
            import brain_dial
            import as2_data
            log("+ extract_ipa.main(...) for AS2")
            extract_ipa.main(os.path.join(ROOT, IPA["as2"]), os.path.join(ROOT, "port", "games", "as2"))
            log("+ brain_dial.main()")
            brain_dial.main()
            log("+ as2_data.main()")
            as2_data.main()
        if "as3" in games:
            log("+ extract_ipa.main(...) for AS3")
            extract_ipa.main(os.path.join(ROOT, IPA["as3"]), os.path.join(ROOT, "port", "games", "as3"))


def build_windows(games, log=print):
    run(["cmake", "-S", "port", "-B", "port/build-release", "-G", "Ninja",
         "-DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake",
         "-DCMAKE_BUILD_TYPE=RelWithDebInfo"], log=log)
    cmd = ["cmake", "--build", "port/build-release"]
    for g in games:
        cmd += ["--target", DIST_TARGET[g]]
    run(cmd, log=log)


def _wsl_path(windows_path):
    """C:\\a\\b -> /mnt/c/a/b, for handing a Windows path to a command run
    inside WSL."""
    drive, rest = os.path.splitdrive(os.path.abspath(windows_path))
    return "/mnt/" + drive[0].lower() + rest.replace("\\", "/")


def build_linux(games, log=print):
    if not shutil.which("wsl"):
        raise BuildError("WSL isn't available on this machine (wsl.exe not found on PATH); "
                          "a Linux build needs a WSL distro (see docs/port.md).")
    script = _wsl_path(os.path.join(ROOT_TOOLS, "linux", "build.sh"))
    cmd = ["wsl", "-d", "Ubuntu", "--", "bash", script] + [DIST_TARGET[g] for g in games]
    run(cmd, log=log)


def build_android(games, log=print):
    py = python_exe()
    for g in games:
        run([py, "port/tools/android/build_apk.py", "--game", ANDROID_GAME[g]], log=log)


def run_build(games, platform="windows", skip_fetch=False, log=print):
    """Raises BuildError (or lets fetch/subprocess errors surface) on
    failure; returns normally on success. The one function both the CLI
    below and build_gui.py call."""
    if platform not in PLATFORMS:
        raise BuildError(f"unknown platform {platform!r} (want one of {PLATFORMS})")
    fetch_sources.ensure_project_root(log)  # clones/updates ROOT if needed; covers --no-fetch too

    if not skip_fetch:
        results = fetch_sources.ensure(set(games), log=log)
        bad = [r for r in results if r[1] not in ("ok", "fetched")]
        for path, status, note in results:
            log(f"{status:9s} {path}")
            if status not in ("ok", "fetched") and note:
                log(f"          -> {note}")
        if bad:
            raise BuildError("Required sources are missing; not building. See above.")
        log("Sources verified.\n")

    ensure_music_restored(log=log)
    extract_ipa_packages(games, log=log)
    {"windows": build_windows, "linux": build_linux, "android": build_android}[platform](games, log=log)

    log(f"\nBuild finished for {platform}: " + ", ".join(games))
    for g in games:
        log(f"  {g}: see dist/ for the packaged game")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", default="all", choices=["as1", "xmas", "as2", "as3", "all"])
    ap.add_argument("--platform", default="windows", choices=PLATFORMS)
    ap.add_argument("--no-fetch", action="store_true")
    args = ap.parse_args()
    games = ["as1", "xmas", "as2", "as3"] if args.game == "all" else [args.game]

    try:
        run_build(games, platform=args.platform, skip_fetch=args.no_fetch, log=print)
    except (BuildError, Exception) as e:
        print(f"\nFAILED: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
