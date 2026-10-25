"""Copy the game data out of an Amateur Surgeon IPA into an assets folder.

usage: python extract_ipa.py <game.ipa> <assets-dir>
       python extract_ipa.py --convert <assets-dir>
       python extract_ipa.py --correct <assets-dir>
       python extract_ipa.py --restore-music <assets-dir>


Only game data is copied: the iOS executable, OpenFeint's nibs and images,
the Facebook bundle, AS2's analytics model (Bango) and unused GL shader
template, and the developers' notes (Readme, changeLog, setup) are skipped
because the port does not use them.
Nibs (the menus' layouts in AS2) and property lists become JSON
(nib2json.py, plistlib); see CONVERTED. CAF sounds (AS2's PCM, AS3's
IMA4), AS3's .pvr.ccz sheets and .fnt fonts are kept as they are: the engine
reads them (platform/AudioFile, platform/ImageFile).
PNGs are converted from Apple's CgBI form (BGRA, premultiplied alpha, raw
deflate) to standard straight-alpha RGBA PNGs that any tool can open; the
pixels are exactly what the engine's CgBI decoder (stb_image) produced.
--convert does the same in place for an assets folder extracted before.
The shipped data's own mistakes listed in CORRECTIONS and TEXT_CORRECTIONS
(AS3's patients' screams, the mini worm's art) are mended as the files are
written; --correct does it in place
for an assets folder extracted before.
AS3's package also gets the port's tool art (toolart/, as3_toolart.py).
The user's restored music ("music restored/<package folder>/", see
REPLACES) goes into Sounds/ in place of the shipped tracks;
--restore-music does it in place.
AS3: the package is the final iOS build, 1.45 (App Store 1.1), which has
every area (the Hospital and Le Flairground Fantastique included):
    python port/tools/extract_ipa.py "IPAs/Amateur Surgeon 3 v1.45 (iOS).ipa" port/games/as3
(extract_xapk.py, which added the Android 1.43 Hospital to the 1.02 package,
is kept only as a reference.)

Root-level bundle files are sorted into one folder per type (see CATEGORY);
the engine maps an original bundle file name to its folder by extension, so
this table and the engine's lookup must stay in sync. Files that were already
in a subfolder of the bundle (lang/) keep their path.
"""
import io, json, os, re, shutil, struct, sys, zipfile, zlib

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nib2json
import fetch_sources

CATEGORY = {
    '.png': 'images', '.jpg': 'images',
    '.mp3': 'audio', '.wav': 'audio',
    '.mp4': 'video', '.m4v': 'video',
    '.txt': 'anim',
    '.xml': 'dialogue',
    '.pod': 'models',
    '.anu': 'anim',     # MotionWelder animations (Christmas)
    '.swf': 'flash',    # the Flash games' movies (not in IPAs; re/flash/)
    '.m4a': 'audio',    # AAC music (AS2), decoded by the engine
    '.caf': 'audio',    # Core Audio files (AS2's PCM, AS3's IMA4), decoded by the engine
    '.json': 'data',    # AS2's cut scenes
    '.plist': 'data',   # AS2's tutorials (property lists, written as .json)
    '.nib': 'ui',       # compiled Interface Builder nibs, written as .json (nib2json.py)
}

# Formats the engine reads in another form: the file is converted and given
# this extension (the engine maps the bundle name to it, platform/Assets.cpp).
CONVERTED = {'.nib': '.json', '.plist': '.json'}

SKIP = re.compile(r'(^OF\w*\.nib$|^FBConnect\.bundle/|^Bango\.momd/|^openfeint_offline_config\.xml$|'
                  r'^Shader\.[vf]sh$|^LICENSE$|^most_uniQue$|^ResourcesRules\.plist$|^_CodeSignature/|^SC_Info/|^Info\.plist$|^PkgInfo$|'
                  r'^iTunesArtwork$|^embedded\.mobileprovision$|^ResourceRules\.plist$|^CodeResources$|'
                  r'^OF[A-Za-z0-9_]*\.(inl|png)$|^(?i:readme|changeLog|setup)\.txt$|'
                  # AS3: Adult Swim's news feed (online), its libraries' build leftovers (kazmath,
                  # marmalade, JsonCpp, Push IO), launch images by size
                  # (AS2's plain Default2.png is its splash) and icons
                  r'^ASNewsFeed\w*\.nib$|^asnf-|^CMakeLists\.txt$|^ChangeLog$|^cocos2dx-ext|^json_\w+\.inl$|'
                  r'^pushio_config\.json$|\.entitlements$|\.sh$|^Default[-@~][-@~\w]*\.png$|^(?i:icon)[-@~\w]*\.png$|'
                  # AS3 1.45: the Facebook SDK's bundle and the game's Facebook layouts (online),
                  # the developers' PDFs (release notes, comScore's analytics spec)
                  r'^FacebookSDKResources\.bundle/|^common/(in_game_facebookconfirm|ui_start_dialogue_facebook)\.json$|'
                  r'\.pdf$|'
                  # AS3's art by content scale: only 4.0x, the largest, is kept (the engine draws at
                  # the window's resolution; 2.0x's two extra sheets are unused copies)
                  r'^[12]\.0x/)')

MARKER = '.extracted_from'

# Data corrections: mistakes of the shipped data mended in the package as it
# is written (and by --correct in a package extracted before), so the engine
# reads the data as meant and keeps no game-specific exception. Each sets a
# top-level string key of a JSON file of the package, only where the file
# still holds the value it shipped with (None: the key absent), so another
# IPA's files and a second run are left alone. Done on the text: the file is
# otherwise byte for byte the original. Where a key is given twice, the last
# one is the one the game reads (JsonCpp's rule, core/Json.cpp) and changed.
#   (package path, key, shipped value, corrected value, why)
_SURGERIES = 'Data/Surgeries/surgery_'
_MODES = ('', '_challenge1', '_challenge2')
CORRECTIONS = (
    # AS3's patients' screams (SurgeryScreen::scream: "<scream>_scream_1..4",
    # the sets in Sounds/): each surgery's own patient, as its case file
    # (surgeries_info.json) and its "sounds" list have it.
    # Chastity Beltt is a woman: her files list the female screams.
    [(f'{_SURGERIES}hospital_chastitybeltt{m}.json', 'scream', 'male', 'female',
      'a woman (her sounds list the female screams)') for m in _MODES] +
    # the whale: its challenges scream "whale", its sounds list whale_scream_1..5
    [(f'{_SURGERIES}pocketgods_whale.json', 'scream', 'dog', 'whale',
      'a whale, as its challenges and sounds list')] +
    # Chartreuse Flair, a man ("his shirt"), as the other two triplets
    [(f'{_SURGERIES}pocketgods_locusts_triplet3{m}.json', 'scream', 'dog', 'male',
      'a man, as the other triplets') for m in _MODES] +
    # left as the template's "" (or without the key) where the other files
    # were filled in: Alan Probe's second challenge, Useful Brother, Real
    # Bumps Grinder (all with male screams in their sounds lists), the
    # Reverend's Organ (its own organ_scream_1..4 listed, never played)
    [(f'{_SURGERIES}exileclinic_alanprobe_challenge2.json', 'scream', None, 'male',
      'a man, as his surgery and first challenge')] +
    [(f'{_SURGERIES}exileclinic_usefulbrother1{m}.json', 'scream', '', 'male',
      'a man (male screams in his sounds list)') for m in _MODES] +
    [(f'{_SURGERIES}exileclinic_realbumpsgrinder{m}.json', 'scream', s, 'male',
      'a man (male screams in his sounds list)') for m, s in zip(_MODES, ('', None, ''))] +
    [(f'{_SURGERIES}robotopia_reverendsorgan{m}.json', 'scream', s, 'organ',
      'the organ (its organ_scream set in its sounds list)') for m, s in zip(_MODES, (None, None, ''))]
)

# Text the shipped data has wrong, replaced wherever it stands (only while
# the corrected text is not there yet).
#   (package path, shipped text, corrected text, why)
TEXT_CORRECTIONS = (
    # AS3's mini worm (Hubris 2 and its challenges): its animation names the
    # copy of its art in surgery_hubris1_sheet, which shipped with blank
    # frames (1 by 1: its crawl's 5 and 6, its bite's 5 and 7-9, the
    # taser's 7 and 9, the stretch's 3), so it blinked as it crawled; the
    # same art whole is in surgery_hubris2_sheet (anim_mini_worm_*), which
    # those surgeries load too.
    ('common/anim_creature_mini_worm_anim.json', '"anim_miniworm_', '"anim_mini_worm_',
     'its art whole, in surgery_hubris2_sheet'),
    # the whale's rib to replace (whaleribs1_piece2) and its new one
    # (whaleribs1_replaced) shipped as one picture (the sheet gives both the
    # same rect), so a healthy rib was swapped for a healthy rib: the old one
    # tinted sick (InjuryPieceDef's tint, the port's)
    ('common/transplant_whaleribs1.json', '"sprite": "whaleribs1_piece2.png",',
     '"sprite": "whaleribs1_piece2.png",	"tint": [205, 212, 160],', 'the rib to replace looks sick'),
)

def correct(path, data):
    """The data corrections for a package path applied to its bytes."""
    fixes = [c for c in CORRECTIONS if c[0] == path]
    texts = [c for c in TEXT_CORRECTIONS if c[0] == path]
    if not fixes and not texts:
        return data
    text = data.decode('utf-8')
    for _, shipped, value, _why in texts:
        if value not in text:
            text = text.replace(shipped, value)
    for _, key, shipped, value, _why in fixes:
        found = list(re.finditer(r'"%s"\s*:\s*"((?:[^"\\]|\\.)*)"' % re.escape(key), text))
        if found and found[-1].group(1) == shipped:
            last = found[-1]
            text = text[:last.start(1)] + value + text[last.end(1):]
        elif not found and shipped is None and not re.search(r'"%s"\s*:' % re.escape(key), text):
            brace = text.index('{') + 1
            text = text[:brace] + f'\n\t"{key}" : "{value}",' + text[brace:]
    return text.encode('utf-8')

def correct_package(dest):
    count = 0
    for path in sorted({c[0] for c in list(CORRECTIONS) + list(TEXT_CORRECTIONS)}):
        p = os.path.join(dest, *path.split('/'))
        if not os.path.isfile(p):
            continue
        with open(p, 'rb') as fh:
            data = fh.read()
        fixed = correct(path, data)
        if fixed != data:
            with open(p, 'wb') as fh:
                fh.write(fixed)
            count += 1
    print(f'{count} files corrected in {dest}')

# Restored music: the user's own takes of a game's music, in
# "music restored/<package folder name>/" at the workspace root (as3 for
# port/games/as3), put in the package's Sounds/ in place of the shipped
# tracks. A file replaces the shipped track of its own name (any extension:
# the game asks for a sound by name and SoundMgr finds its file); REPLACES
# lists the shipped tracks a file stands for under other names (the game's
# code calls it by its own name instead).
#   package folder name: {restored file: [shipped files it replaces]}
REPLACES = {
    'as3': {'super-hero-loop.m4a': ['music_start.caf', 'win_loop.caf']},
}
# fetch_sources.ROOT (not __file__, which points inside a frozen exe's
# temp unpack dir when this is bundled rather than run as a script).
RESTORED_ROOT = os.path.join(fetch_sources.ROOT, 'music restored')

def restored_music(dest):
    """{package path of a restored track: its source file} and the package
    paths of the shipped tracks they replace, for the package at dest."""
    name = os.path.basename(os.path.normpath(dest))
    src = os.path.join(RESTORED_ROOT, name)
    files = {f'Sounds/{f}': os.path.join(src, f) for f in sorted(os.listdir(src))} if os.path.isdir(src) else {}
    gone = {f'Sounds/{os.path.splitext(f)[0]}.caf' for f in os.listdir(src)} if files else set()
    for f in REPLACES.get(name, {}).values():
        gone.update(f'Sounds/{n}' for n in f)
    return files, gone

def restore_music(dest, written):
    """Puts the package's restored music in it, removing the shipped tracks
    they replace; returns the marker's list of written files updated."""
    files, gone = restored_music(dest)
    for rel in gone:
        p = os.path.join(dest, *rel.split('/'))
        if os.path.isfile(p):
            os.remove(p)
    for rel, src in files.items():
        out = os.path.join(dest, *rel.split('/'))
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copyfile(src, out)
    if files:
        print(f'{len(files)} restored tracks, {len(gone)} shipped tracks replaced')
    return sorted({w for w in written if w not in gone} | set(files))

def restore_package(dest):
    """--restore-music for a package extracted before."""
    marker = os.path.join(dest, MARKER)
    with open(marker, encoding='utf-8') as m:
        lines = m.read().splitlines()
    written = restore_music(dest, lines[1:])
    with open(marker, 'w', encoding='utf-8') as m:
        m.write(lines[0] + '\n' + ''.join(w + '\n' for w in written))

def derived_art(dest, written):
    """Art the port makes from the package's own (AS3's tools without their
    cards: as3_toolart.py), added to the marker's list of written files."""
    if os.path.basename(os.path.normpath(dest)) != 'as3':
        return written
    import as3_toolart
    return sorted(set(written) | set(as3_toolart.main(dest)))

def target(rel):
    ext = os.path.splitext(rel)[1].lower()
    if '/' in rel:  # kept where it was, converted as anywhere (AS3's 4.0x/*.plist)
        return os.path.splitext(rel)[0] + CONVERTED.get(ext, ext)
    if ext not in CATEGORY:
        raise SystemExit(f'no asset category for {rel!r}; add its extension to CATEGORY')
    return f'{CATEGORY[ext]}/{os.path.splitext(rel)[0] + CONVERTED.get(ext, ext)}'

def chunks(data):
    p = 8
    while p < len(data):
        n, = struct.unpack('>I', data[p:p + 4])
        yield data[p + 4:p + 8], data[p + 8:p + 8 + n]
        p += 12 + n

def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)

def decrush_png(data):
    """CgBI PNG bytes -> standard PNG bytes (other PNGs are returned as is)."""
    if data[12:16] != b'CgBI':
        return data
    ihdr, idat = None, b''
    for kind, body in chunks(data):
        if kind == b'IHDR':
            ihdr = body
        elif kind == b'IDAT':
            idat += body
    if ihdr[8:10] != bytes([8, 6]):
        raise SystemExit('unexpected CgBI PNG format (only 8-bit RGBA is handled)')
    # Re-wrap the raw deflate stream so Pillow can decode (and de-interlace) it.
    plain = (data[:8] + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(zlib.decompress(idat, -15))) +
             chunk(b'IEND', b''))
    px = np.array(Image.open(io.BytesIO(plain)).convert('RGBA'), dtype=np.uint32)
    b, g, r, a = px[..., 0], px[..., 1], px[..., 2], px[..., 3]
    # stb_image's unpremultiply, to the bit: (c * 255 + a / 2) / a as uint8.
    safe = np.maximum(a, 1)
    half = a // 2
    def unpremul(c):
        return np.where(a > 0, ((c * 255 + half) // safe) & 0xff, c)
    out = np.stack([unpremul(r), unpremul(g), unpremul(b), a], axis=-1).astype(np.uint8)
    buf = io.BytesIO()
    Image.fromarray(out, 'RGBA').save(buf, 'PNG')
    return buf.getvalue()

def nib_to_json(data):
    """A compiled nib -> its views, outlets and actions as JSON (nib2json.py)."""
    return (json.dumps(nib2json.convert(nib2json.parse(data)), indent=1) + '\n').encode('utf-8')

def plist_to_json(data):
    """A property list (binary or XML) -> the same values as JSON."""
    import plistlib
    return (json.dumps(plistlib.loads(data), indent=1, ensure_ascii=False) + '\n').encode('utf-8')

def convert(dest):
    count = 0
    for root, _, files in os.walk(dest):
        for f in files:
            if not f.lower().endswith('.png'):
                continue
            path = os.path.join(root, f)
            with open(path, 'rb') as fh:
                data = fh.read()
            if data[12:16] == b'CgBI':
                with open(path, 'wb') as fh:
                    fh.write(decrush_png(data))
                count += 1
    print(f'{count} CgBI PNGs converted in {dest}')

def main(ipa, dest):
    # Start clean so stale files cannot linger, but only ever delete what
    # this script wrote (listed in MARKER after the IPA's name): a package
    # also holds files of its own (AS2's data/library.json, by as2_data.py).
    marker = os.path.join(dest, MARKER)
    if os.path.isdir(dest) and os.listdir(dest):
        if not os.path.isfile(marker):
            raise SystemExit(f'{dest} is not empty and was not created by this script; pick another folder')
        with open(marker, encoding='utf-8') as m:
            written = m.read().splitlines()[1:]
        if not written:
            # A marker of an older run, before it listed its files: it made
            # the whole folder.
            shutil.rmtree(dest)
        for rel in written:
            p = os.path.join(dest, *rel.split('/'))
            if os.path.isfile(p):
                os.remove(p)
    os.makedirs(dest, exist_ok=True)
    written = []
    with zipfile.ZipFile(ipa) as z:
        app = next(n.split('/')[1] for n in z.namelist() if n.startswith('Payload/') and n.count('/') >= 2)
        exe = app[:-4]
        prefix = f'Payload/{app}/'
        for info in z.infolist():
            if not info.filename.startswith(prefix) or info.is_dir():
                continue
            rel = info.filename[len(prefix):]
            if rel == exe or SKIP.search(rel):
                continue
            out = os.path.join(dest, *target(rel).split('/'))
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with z.open(info) as src:
                data = src.read()
            low = rel.lower()
            if low.endswith('.png'):
                data = decrush_png(data)
            elif low.endswith('.nib'):
                data = nib_to_json(data)
            elif low.endswith('.plist'):
                data = plist_to_json(data)
            data = correct(target(rel), data)
            with open(out, 'wb') as dst:
                dst.write(data)
            written.append(target(rel))
    written = restore_music(dest, written)
    written = derived_art(dest, written)
    with open(marker, 'w', encoding='utf-8') as m:
        m.write(os.path.basename(ipa) + '\n' + ''.join(w + '\n' for w in sorted(written)))
    print(f'{len(written)} files -> {dest}')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    if sys.argv[1] == '--convert':
        convert(sys.argv[2])
    elif sys.argv[1] == '--correct':
        correct_package(sys.argv[2])
    elif sys.argv[1] == '--restore-music':
        restore_package(sys.argv[2])
    else:
        main(sys.argv[1], sys.argv[2])
