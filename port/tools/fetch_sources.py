"""Fetches and verifies the Flash SWFs and IPAs the build needs.

usage: python port/tools/fetch_sources.py [--game as1|xmas|as2|as3|all]

Reads sources_manifest.json (one entry per required source file: its path
relative to the workspace root, its sha256, and an optional archive.org
fetch spec). For each entry in scope:
  - present and hash matches: left alone, reported "ok".
  - missing, with a fetch spec: downloaded from archive.org, then
    re-hashed; a mismatch after download is an error, never a silent
    half-written file (downloaded to a .part file, renamed only once the
    hash is confirmed).
  - missing, no fetch spec: reported "MISSING" with the manifest's note.
  - present but hash does NOT match: reported "MISMATCH" and left alone.
    This never auto-replaces a file that exists but differs, since that
    could be the user's own edit, not corruption.

Exit code is non-zero if anything in scope is MISSING or MISMATCH after
fetching. Two of the IPAs (AS1's iPad build, AS2 HD's iPad build) and
Christmas's/AS2's Flash SWFs have no automated source at all (see the
manifest's "note" field and docs/port.md) -- this script will say so
plainly rather than claim to have fetched them.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request

# Where the engine/game C++ source, CMakeLists.txt and this tools/ folder
# itself live when this isn't already sitting inside a checkout of them
# (see ensure_project_root below) -- the full build tree (game-specific
# code, build system, tool scripts), never the copyrighted assets
# (re/, IPAs/, port/games/, music restored/ are still fetched/supplied
# separately, same as always).
SOURCE_REPO = "https://github.com/Hortus1556/Re-Generared-Project.git"


def _is_project_root(path):
    return os.path.isfile(os.path.join(path, "port", "CMakeLists.txt"))


# Frozen into an exe (PyInstaller), __file__ points inside the temp
# unpack dir, not where the exe actually sits -- use sys.executable's own
# location instead, which PyInstaller keeps correct.
if getattr(sys, "frozen", False):
    TOOLS_DIR = os.path.dirname(os.path.abspath(sys.executable))
else:
    TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))

# Prefer an existing checkout this tool already sits inside (port/tools/,
# two levels under the root) -- the common case, no network touched. Only
# when that's not where this is running from (the exe copied somewhere
# else, or run from a bare download) does this fall back to a sibling
# clone next to wherever it's running, bootstrapped by
# ensure_project_root() below.
_inplace_root = os.path.abspath(os.path.join(TOOLS_DIR, "..", ".."))
if _is_project_root(_inplace_root):
    ROOT = _inplace_root
else:
    ROOT = os.path.join(TOOLS_DIR, "Re-Generated-Project")
MANIFEST = os.path.join(ROOT, "port", "tools", "sources_manifest.json")


def ensure_project_root(log=print):
    """Makes sure ROOT holds a real checkout before anything else touches
    it: uses one already in place with no network access, clones one next
    to this tool if there's nothing there yet, and keeps an existing clone
    from going stale with `git pull` (best-effort -- offline or a pull
    conflict doesn't block building with whatever's already there)."""
    if _is_project_root(ROOT):
        if os.path.isdir(os.path.join(ROOT, ".git")):
            if not shutil.which("git"):
                return
            proc = subprocess.run(["git", "-C", ROOT, "pull", "--ff-only"],
                                   capture_output=True, text=True)
            if proc.returncode == 0:
                log(f"updated {ROOT} ({proc.stdout.strip() or 'already up to date'})")
            else:
                log(f"warning: couldn't update {ROOT} (offline? local changes?) -- "
                    f"building with what's there.\n{proc.stderr.strip()}")
        return

    if os.path.isdir(ROOT) and os.listdir(ROOT):
        raise RuntimeError(
            f"{ROOT} exists, isn't empty, and isn't this project (no port/CMakeLists.txt) -- "
            "not touching it. Move it aside, or point this tool at a real checkout."
        )
    if not shutil.which("git"):
        raise RuntimeError(
            f"{TOOLS_DIR} isn't inside a checkout of this project, and git isn't on PATH to "
            f"clone one. Install git, or run this from inside the project's own port/tools/."
        )
    log(f"cloning {SOURCE_REPO} -> {ROOT}")
    proc = subprocess.run(["git", "clone", SOURCE_REPO, ROOT], capture_output=True, text=True)
    if proc.returncode != 0 or not _is_project_root(ROOT):
        raise RuntimeError(f"cloning {SOURCE_REPO} failed:\n{proc.stderr.strip()}")


def python_exe():
    """A real Python interpreter, for running a port/tools/*.py script as
    a subprocess (extract_ipa.py, as2_data.py's own emulate() call into
    re/scripts/emucalls.py, ...). sys.executable is only safe to reuse
    when this process itself is a plain Python script -- once build_gui.py
    is frozen into an exe (PyInstaller), sys.executable is that exe, not
    a Python interpreter, so relaunching it with a script path as an
    argument would just start the GUI again instead of running the
    script. The one place this is computed, so every caller agrees."""
    if not getattr(sys, "frozen", False):
        return sys.executable
    for name in ("python", "py"):
        found = shutil.which(name)
        if found:
            return found
    raise RuntimeError("No Python interpreter found on PATH (needed to run a port/tools/*.py script)")


def load_manifest():
    with open(MANIFEST, "r", encoding="utf-8") as f:
        return json.load(f)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url, dest_part):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=60) as resp, open(dest_part, "wb") as out:
        while True:
            chunk = resp.read(1024 * 1024)
            if not chunk:
                break
            out.write(chunk)


def ensure(games, log=print):
    ensure_project_root(log)
    manifest = load_manifest()
    results = []
    for entry in manifest:
        if entry["game"] not in games:
            continue
        dest = os.path.join(ROOT, entry["path"])
        expected = entry["sha256"]
        status = None

        if os.path.isfile(dest):
            actual = sha256_of(dest)
            if actual == expected:
                status = "ok"
            elif entry.get("fallback") and actual == entry["fallback"]["sha256"]:
                status = f"ok (fallback already in place: {entry['fallback']['label']})"
            else:
                status = "MISMATCH"
        elif entry.get("fetch"):
            status = _fetch_one(entry["fetch"], entry["path"], dest, expected)
        elif entry.get("fallback"):
            # Not the exact file (different hash, verified against its
            # OWN expected hash below -- never skip verification just
            # because it's a fallback), but close enough to use when the
            # real one can't be found at all: see the fallback's own
            # "label" for exactly how it differs.
            fb = entry["fallback"]
            status = _fetch_one(fb, entry["path"], dest, fb["sha256"])
            if status == "fetched":
                status = f"fetched (fallback: {fb['label']})"
        else:
            status = "MISSING"

        results.append((entry["path"], status, entry.get("note", "")))
    return results


def _fetch_one(fetch, entry_path, dest, expected_sha256):
    """Downloads one file per a manifest entry's "fetch" (or "fallback")
    spec into dest, verified against expected_sha256. Returns "fetched",
    or a status string describing why not."""
    if fetch["source"] == "flashpoint":
        # entry_path is "re/flash/<game>/<host>/<rest>" -- the part after
        # "re/flash/<game>/" is exactly the host+path Flashpoint's own
        # Legacy htdocs mirror serves (verified by hash):
        # https://infinity.flashpointarchive.org/Flashpoint/Legacy/htdocs/<host>/<rest>
        game = entry_path.split("/")[2]
        _, _, after = entry_path.partition(f"re/flash/{game}/")
        url = "https://infinity.flashpointarchive.org/Flashpoint/Legacy/htdocs/" + urllib.parse.quote(after)
    else:
        url = f"https://archive.org/download/{fetch['item']}/" + urllib.parse.quote(fetch["file"])
    part = dest + ".part"
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    try:
        download(url, part)
        actual = sha256_of(part)
        if actual == expected_sha256:
            os.replace(part, dest)
            return "fetched"
        os.remove(part)
        return f"MISMATCH after download (got {actual[:12]}, wanted {expected_sha256[:12]})"
    except Exception as e:
        if os.path.exists(part):
            os.remove(part)
        return f"FETCH FAILED: {e}"


def is_success(status):
    """True for "ok" and "fetched" -- including a fallback fetch, whose
    status is "fetched (fallback: ...)" so the per-file report can say so,
    but which still counts as success: a real, hash-verified file is now
    in place, even if it isn't byte-for-byte the primary one."""
    return status.startswith("ok") or status.startswith("fetched")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", default="all", choices=["as1", "xmas", "as2", "as3", "all"])
    args = ap.parse_args()
    games = {"as1", "xmas", "as2", "as3"} if args.game == "all" else {args.game}

    results = ensure(games)
    bad = False
    for path, status, note in results:
        print(f"{status:9s} {path}")
        if not is_success(status):
            bad = True
            if note:
                print(f"          -> {note}")
    if bad:
        print("\nSome required sources are missing or don't match. See above.")
        sys.exit(1)
    print("\nAll sources in scope present and verified.")


if __name__ == "__main__":
    main()
