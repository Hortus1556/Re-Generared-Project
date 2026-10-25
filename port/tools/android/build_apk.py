"""An Amateur Surgeon game as an Android APK (Bleed Engine on SDL3): AS1 or
its Christmas Edition (the Flash rebuilds), AS2 HD or AS3 (--game, see GAMES).

usage: python port/tools/android/build_apk.py [--game as1|christmas|as2|as3] [--abi arm64-v8a,x86_64] [--out <dir>]

What it does, with the Android SDK's own tools (no Gradle):
1. builds the game for each ABI with the NDK's CMake toolchain
   (port/build-android[-<game>]-<abi>: libmain.so, the game, chosen by
   CMake's ANDROID_GAME, and libSDL3.so, SDL3 from tools/sdl3-src), stripped;
2. compiles SDL's Java glue (its android-project's org.libsdl.app) against
   the platform's android.jar, and d8 makes it classes.dex;
3. links the manifest and resources (port/android/: the theme; the game's
   name, its package id and its launcher icons, made from its Windows icon,
   from GAMES) with aapt2;
4. adds the dex, the libraries and the game's package (port/games/<game>,
   as the APK's assets/game/, which the engine reads by SDL's relative
   paths);
5. aligns it (16 KB pages) and signs it with the debug key Android Studio
   keeps (~/.android/debug.keystore).

The SDK is found by ANDROID_HOME (else %LOCALAPPDATA%/Android/Sdk), Java
by JAVA_HOME; the newest NDK, build tools and platform there are used.
"""
import argparse
import glob
import io
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
PORT = os.path.join(ROOT, "port")
ANDROID = os.path.join(PORT, "android")
# Each game: its name, its app id, its Windows icon, its APK's name, its
# build folders' infix (AS2's, the first, have none) and where it goes.
GAMES = {
    "as1": {"name": "Amateur Surgeon", "id": "org.bleedengine.amateursurgeon", "icon": "AmateurSurgeon.ico",
            "apk": "AmateurSurgeon.apk", "build": "as1-", "out": "surgeon1-android"},
    "christmas": {"name": "Amateur Surgeon Christmas", "id": "org.bleedengine.amateursurgeonxmas",
                  "icon": "AmateurSurgeonXmas.ico", "apk": "AmateurSurgeonXmas.apk", "build": "xmas-",
                  "out": "surgeonxmas-android"},
    "as2": {"name": "Amateur Surgeon 2", "id": "org.bleedengine.amateursurgeon2", "icon": "AmateurSurgeon2.ico",
            "apk": "AmateurSurgeon2.apk", "build": "", "out": "surgeon2-android"},
    "as3": {"name": "Amateur Surgeon 3", "id": "org.bleedengine.amateursurgeon3", "icon": "AmateurSurgeon3.ico",
            "apk": "AmateurSurgeon3.apk", "build": "as3-", "out": "surgeon3-android"},
}
SDL_JAVA = glob.glob(os.path.join(ROOT, "tools", "sdl3-src", "SDL3-*", "android-project", "app", "src", "main", "java"))
MIN_SDK = 28  # AMediaDataSource (the music's decoding from memory)
TARGET_SDK = 35
# Already compressed: stored, so they are read straight from the APK.
STORED = {".png", ".jpg", ".mp3", ".m4a", ".m4v", ".mp4", ".wav", ".ogg", ".ccz"}


def newest(pattern):
    found = sorted(glob.glob(pattern), key=lambda p: [int(x) if x.isdigit() else x for x in
                                                      os.path.basename(p.rstrip("/\\")).replace("-", ".").split(".")])
    if not found:
        sys.exit("not found: " + pattern)
    return found[-1]


def run(cmd, **kw):
    print(">", " ".join(os.path.basename(c) if i == 0 else c for i, c in enumerate(cmd))[:200])
    subprocess.run(cmd, check=True, **kw)


def build_native(game, abi, ndk):
    out = os.path.join(PORT, "build-android-" + GAMES[game]["build"] + abi)
    if not os.path.exists(os.path.join(out, "CMakeCache.txt")):
        run(["cmake", "-S", PORT, "-B", out, "-G", "Ninja",
             "-DCMAKE_TOOLCHAIN_FILE=" + os.path.join(ndk, "build", "cmake", "android.toolchain.cmake"),
             "-DANDROID_ABI=" + abi, "-DANDROID_PLATFORM=android-%d" % MIN_SDK, "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
             "-DANDROID_GAME=" + game])
    run(["cmake", "--build", out, "--target", "main", "SDL3-shared"])
    return [os.path.join(out, "libmain.so"), os.path.join(out, "sdl3", "libSDL3.so")]


def icons(res, icon):
    """The launcher icon at each density, from the Windows icon's largest."""
    from PIL import Image
    ico = Image.open(os.path.join(PORT, "res", icon))
    sizes = sorted(ico.info.get("sizes", {ico.size}), key=lambda s: s[0])
    ico.size = sizes[-1]
    big = ico.convert("RGBA")
    for name, px in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = os.path.join(res, "mipmap-" + name)
        os.makedirs(d, exist_ok=True)
        big.resize((px, px), Image.LANCZOS).save(os.path.join(d, "ic_launcher.png"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", default="as2", choices=sorted(GAMES))
    ap.add_argument("--abi", default="arm64-v8a,x86_64")
    ap.add_argument("--out")
    # (a test copy beside the real one: its own package id and name, so its
    # own save; debuggable, so `adb shell run-as <id>` reads and writes it)
    ap.add_argument("--test", action="store_true")
    args = ap.parse_args()
    game = dict(GAMES[args.game])
    if args.test:
        game.update(id=game["id"] + ".test", name=game["name"] + " (test)",
                    apk=game["apk"].replace(".apk", "-test.apk"))
    package = os.path.join(PORT, "games", args.game)
    out_dir = args.out or os.path.join(ROOT, "dist", game["out"])

    sdk = os.environ.get("ANDROID_HOME") or os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk")
    ndk = newest(os.path.join(sdk, "ndk", "*"))
    tools = newest(os.path.join(sdk, "build-tools", "*"))
    android_jar = os.path.join(newest(os.path.join(sdk, "platforms", "android-*")), "android.jar")
    java_home = os.environ.get("JAVA_HOME", "")
    javac = os.path.join(java_home, "bin", "javac") if java_home else "javac"
    strip = os.path.join(ndk, "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin", "llvm-strip.exe")
    exe = ".bat" if os.name == "nt" else ""
    if not SDL_JAVA:
        sys.exit("SDL's android-project not found under tools/sdl3-src")

    work = tempfile.mkdtemp(prefix=args.game + "_apk_")
    try:
        # 1. The game and SDL, per ABI.
        libs = {}
        for abi in args.abi.split(","):
            for so in build_native(args.game, abi, ndk):
                dst = os.path.join(work, "lib", abi, os.path.basename(so))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                run([strip, "--strip-unneeded", "-o", dst, so])
                libs["lib/%s/%s" % (abi, os.path.basename(so))] = dst

        # 2. SDL's Java glue as classes.dex.
        classes = os.path.join(work, "classes")
        sources = glob.glob(os.path.join(SDL_JAVA[0], "org", "libsdl", "app", "*.java"))
        run([javac, "-nowarn", "-encoding", "UTF-8", "-source", "11", "-target", "11", "-classpath", android_jar,
             "-d", classes] + sources)
        dex = os.path.join(work, "dex")
        os.makedirs(dex)
        run([os.path.join(tools, "d8" + exe), "--release", "--min-api", str(MIN_SDK), "--lib", android_jar,
             "--output", dex] + glob.glob(os.path.join(classes, "**", "*.class"), recursive=True))

        # 3. Manifest and resources.
        res = os.path.join(work, "res")
        shutil.copytree(os.path.join(ANDROID, "res"), res)
        with open(os.path.join(res, "values", "strings.xml"), "w", encoding="utf-8") as f:
            f.write('<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <string name="app_name">%s</string>\n'
                    '</resources>\n' % game["name"])
        icons(res, game["icon"])
        flat = os.path.join(work, "res.zip")
        run([os.path.join(tools, "aapt2.exe" if os.name == "nt" else "aapt2"), "compile", "--dir", res, "-o", flat])
        base = os.path.join(work, "base.apk")
        run([os.path.join(tools, "aapt2.exe" if os.name == "nt" else "aapt2"), "link", "-o", base, "-I", android_jar,
             "--manifest", os.path.join(ANDROID, "AndroidManifest.xml"), "--min-sdk-version", str(MIN_SDK),
             "--target-sdk-version", str(TARGET_SDK), "--version-code", "1", "--version-name", "1.0",
             "--rename-manifest-package", game["id"]] + (["--debug-mode"] if args.test else []) + [flat])

        # 4. The dex, the libraries and the game.
        unsigned = os.path.join(work, "unsigned.apk")
        shutil.copy(base, unsigned)
        with zipfile.ZipFile(unsigned, "a", zipfile.ZIP_DEFLATED) as z:
            z.write(os.path.join(dex, "classes.dex"), "classes.dex")
            for name, path in sorted(libs.items()):
                z.write(path, name)
            count = 0
            for dirpath, _, files in os.walk(package):
                for f in sorted(files):
                    if f.startswith("."):
                        continue
                    src = os.path.join(dirpath, f)
                    rel = os.path.relpath(src, package).replace(os.sep, "/")
                    kind = zipfile.ZIP_STORED if os.path.splitext(f)[1].lower() in STORED else zipfile.ZIP_DEFLATED
                    z.write(src, "assets/game/" + rel, compress_type=kind)
                    count += 1
        print("  %d game files" % count)

        # 5. Aligned and signed.
        aligned = os.path.join(work, "aligned.apk")
        run([os.path.join(tools, "zipalign" + (".exe" if os.name == "nt" else "")), "-P", "16", "-f", "4",
             unsigned, aligned])
        os.makedirs(out_dir, exist_ok=True)
        apk = os.path.join(out_dir, game["apk"])
        keystore = os.path.join(os.path.expanduser("~"), ".android", "debug.keystore")
        if not os.path.exists(keystore):
            run([os.path.join(java_home, "bin", "keytool") if java_home else "keytool", "-genkeypair", "-keystore",
                 keystore, "-storepass", "android", "-alias", "androiddebugkey", "-keypass", "android", "-keyalg",
                 "RSA", "-keysize", "2048", "-validity", "10000", "-dname", "CN=Android Debug,O=Android,C=US"])
        run([os.path.join(tools, "apksigner" + exe), "sign", "--ks", keystore, "--ks-pass", "pass:android",
             "--key-pass", "pass:android", "--ks-key-alias", "androiddebugkey", "--out", apk, aligned])
        print("%s (%.1f MB)" % (apk, os.path.getsize(apk) / 1e6))
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
