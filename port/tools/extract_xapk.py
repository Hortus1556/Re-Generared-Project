"""Add Amateur Surgeon 3's Hospital area ("Bleed Everywhere") from the Android
build 1.43 to the iOS 1.02 package.

A REFERENCE ONLY, no longer part of making the package: the package is the
final iOS build, 1.45, which has the Hospital with iOS's own 4.0x art
(python port/tools/extract_ipa.py "IPAs/Amateur Surgeon 3 v1.45 (iOS).ipa"
port/games/as3; docs/as3.md). Kept for how the Android build's content maps
onto iOS's (3.5x art used at its own scale, supplement sheets, OGG to IMA4).

usage: python extract_xapk.py <AS3 .xapk> <assets-dir>
       (on a package extracted from the 1.02 IPA, "IPAs/Amateur Surgeon 3.ipa")

Run it after extract_ipa.py (which writes the iOS 1.02 package and would undo
the merges below). The XAPK is a zip holding the APK (assets/: Data/,
common/, Sounds/ as OGG, art by content scale in 1.5x/, 2.0x/, 3.5x/: plists
as XML, some PNGs) and the OBB (a zip of the remaining PNGs of 1.5x/, 2.0x/,
3.5x/). The existing content stays iOS 1.02; only what the Hospital needs is
added (docs/as3.md "The Hospital"):

- New files, copied: the Hospital's surgeries, injuries and dialogues
  (Data/), its animations, map layout and transplant layouts (common/).
- Merged into the iOS files (by key, so a re-run changes nothing):
  Data/areas.json gains area_hospital; Data/Surgeries/surgeries_info.json
  the four Hospital patients; Data/default_data.json the Hospital's unlock
  cost (area_unlock_star) and its surgeries' content entries. The iOS values
  of everything else are kept (Android 1.43 rebalanced many surgeries).
- Art (3.5x/): the Android art is drawn for a content scale of 3.5 where the
  iOS art is 4.0; it is kept at 3.5, unscaled, in its own folder, and the
  engine gives each texture its folder's scale (CCTexture2D: "N.Nx/" in its
  path). New sheets and particles are copied (plists to JSON as
  extract_ipa.py does). A sheet that iOS has too but Android gained frames in
  (the Hospital's world map button, save slot button, patient photos...)
  gets a supplement: 3.5x/<sheet> holding only the frames iOS lacks, cut out
  of the Android sheet; the game loads it after the iOS sheet of the same
  name (mt::loadSheets).
- Sounds: the new OGG Vorbis sounds become IMA4 CAF (ffmpeg's adpcm_ima_qt,
  tools/ffmpeg-*), the iOS sounds' own format, which the engine decodes
  (platform/AudioFile) and the game asks for ("Sounds/<name>.caf").

Online and purchase parts (Facebook, downloads, purchases, revive packs, coin
doubler, the Android title screen) are left out, as the rest of the port does.
"""
import glob, io, json, os, plistlib, re, subprocess, sys, tempfile, zipfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
MARKER = '.extracted_from_xapk'
SCALE = '3.5x'

# --- what is taken -------------------------------------------------------

HOSPITAL = 'area_hospital'
# new files (paths under the APK's assets/), by pattern
NEW_FILES = [
    r'^Data/Surgeries/surgery_hospital_\w+\.json$',
    r'^Data/Dialogues/dialogue_(hospital_\w+|area_hospital_\w+)\.json$',
    r'^common/anim_creature_(ghost|spider)_anim\.json$',
    r'^common/anim_injury_(coffeeartery|darkcloud)_anim\.json$',
    r'^common/hospital_map\.json$',
    r'^common/transplant_\w+\.json$',
    r'^common/in_game_tutorial_gotohospital\.json$',
]
# every injury file iOS lacks is the Hospital's
NEW_INJURIES = r'^Data/Injuries/\w+\.json$'
# new sheets (plist + png), particles (plist + png when there is one)
NEW_SHEETS = [
    'anim_ghost_sheet', 'anim_spider_sheet', 'areacommon_hospital', 'map_hospital',
    'dialogue_char_chastity', 'dialogue_char_emo', 'dialogue_char_gossamer', 'dialogue_char_karl',
    'preop_hospital1', 'preop_hospital2',
    'surgery_chastitylock_sheet', 'surgery_chastitylock_sheet2',
    'surgery_emoshun_sheet1', 'surgery_emoshun_sheet2', 'surgery_emoshun_sheet3',
    'surgery_gossamerrag_sheet1', 'surgery_gossamerrag_sheet2', 'surgery_gossamerrag_sheet3',
    'surgery_gossamerrag_sheet4',
    'surgery_karlpuccino_sheet1', 'surgery_karlpuccino_sheet2', 'surgery_karlpuccino_sheet3',
]
NEW_PARTICLES = [
    'particle_coffeeFX', 'particle_darkcloud_poisonFX', 'particle_darkcloud_poisonareaFX',
    'particle_dark_darkcloud_poisonFX', 'particle_dark_darkcloud_poisonareaFX',
]
# sheets iOS has, whose frames Android added are needed: the supplements
SUPPLEMENTS = [
    'world_map_sheet1',            # worldmap_mainbutton_hospital-*, worldmap_mainbutton_comingsoon-locked
    'savescreen_ui_sheet',         # savebutton-area_hospital
    'surgeryinfo_part2',           # the four patients' photos
    'dialogue_char_alan',          # dialogueChar_Hospital_Alan_*
    'surgery_common1_sheet',       # particleSprite_black, particleSprite_coffee
    'end_of_surgery_popups_sheet', # the "go to the Hospital" popup's pictures
]
ONLINE_FRAME = re.compile(r'facebook|revive|getbp|coindoubler|currency_doubler|employee|freetag', re.I)
# new sounds (OGG names; iOS has the rest under other spellings)
NEW_SOUNDS = ['remember_rio', 'valium_housewife']

# --- reading -------------------------------------------------------------

class Xapk:
    """The APK's assets/ and the OBB's files by their path under assets/."""
    def __init__(self, path):
        outer = zipfile.ZipFile(path)
        apk = next(n for n in outer.namelist() if n.endswith('.apk'))
        obb = next(n for n in outer.namelist() if n.endswith('.obb'))
        self.apk = zipfile.ZipFile(io.BytesIO(outer.read(apk)))
        self.obb = zipfile.ZipFile(io.BytesIO(outer.read(obb)))
        self.files = {}
        for z in (self.apk, self.obb):
            for n in z.namelist():
                if n.endswith('/'):
                    continue
                rel = n[len('assets/'):] if n.startswith('assets/') else n
                if z is self.apk and not n.startswith('assets/'):
                    continue
                self.files.setdefault(rel, z)

    def has(self, rel):
        return rel in self.files

    def read(self, rel):
        z = self.files[rel]
        return z.read(('assets/' + rel) if z is self.apk else rel)


def load_json(data):
    # Android's surgeries_info.json has raw control characters in strings
    return json.loads(data.decode('utf-8-sig'), strict=False)


def dump_json(value):
    return (json.dumps(value, indent=4, ensure_ascii=False) + '\n').encode('utf-8')


def plist_to_json(data):
    return (json.dumps(plistlib.loads(data), indent=1, ensure_ascii=False) + '\n').encode('utf-8')


def rect(s):
    n = [int(float(v)) for v in re.findall(r'-?\d+(?:\.\d+)?', s)]
    return n


# --- writing -------------------------------------------------------------

class Out:
    def __init__(self, dest):
        self.dest = dest
        self.written, self.merged = [], []

    def write(self, rel, data, merged=False):
        p = os.path.join(self.dest, *rel.split('/'))
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'wb') as f:
            f.write(data)
        (self.merged if merged else self.written).append(rel)

    def path(self, rel):
        return os.path.join(self.dest, *rel.split('/'))


def supplement(x, out, name):
    """The frames of Android's <name> sheet that the iOS sheet lacks, cut out
    into a sheet of their own (3.5x/<name>.png/.json); None if none."""
    ios = json.load(open(out.path(f'4.0x/{name}.json'), encoding='utf-8'))['frames']
    plist = plistlib.loads(x.read(f'{SCALE}/{name}.plist'))
    sheet = Image.open(io.BytesIO(x.read(f'{SCALE}/{name}.png'))).convert('RGBA')
    want = [(k, v) for k, v in plist['frames'].items() if k not in ios and not ONLINE_FRAME.search(k)]
    if not want:
        return []
    # cut each frame as stored (a turned one stays turned), then shelf-pack them
    pieces = []
    for k, v in want:
        x0, y0, w, h = rect(v['frame'])
        if v.get('rotated'):
            w, h = h, w
        pieces.append((k, v, sheet.crop((x0, y0, x0 + w, y0 + h))))
    pieces.sort(key=lambda p: -p[2].height)
    width = max(512, max(p[2].width for p in pieces) + 4)
    while True:
        places, x_at, y_at, row = [], 2, 2, 0
        for k, v, img in pieces:
            if x_at + img.width + 2 > width:
                x_at, y_at, row = 2, y_at + row + 2, 0
            places.append((x_at, y_at))
            x_at += img.width + 2
            row = max(row, img.height)
        height = y_at + row + 2
        if height <= width * 2:
            break
        width *= 2
    packed = Image.new('RGBA', (width, height))
    frames = {}
    for (k, v, img), (px, py) in zip(pieces, places):
        packed.paste(img, (px, py))
        f = dict(v)
        _, _, w, h = rect(v['frame'])
        f['frame'] = '{{%d,%d},{%d,%d}}' % (px, py, w, h)
        frames[k] = f
    buf = io.BytesIO()
    packed.save(buf, 'PNG')
    out.write(f'{SCALE}/{name}.png', buf.getvalue())
    meta = dict(plist.get('metadata', {}))
    meta.update({'textureFileName': f'{name}.png', 'realTextureFileName': f'{name}.png',
                 'size': '{%d,%d}' % (width, height), 'supplement': 'frames the iOS sheet lacks (extract_xapk.py)'})
    meta.pop('smartupdate', None)
    out.write(f'{SCALE}/{name}.json', (json.dumps({'frames': frames, 'metadata': meta}, indent=1) + '\n').encode('utf-8'))
    return sorted(frames)


def ogg_to_caf(data):
    ffmpeg = glob.glob(os.path.join(ROOT, 'tools', 'ffmpeg-*', 'bin', 'ffmpeg.exe')) or ['ffmpeg']
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, 'in.ogg'), os.path.join(tmp, 'out.caf')
        with open(src, 'wb') as f:
            f.write(data)
        subprocess.run([ffmpeg[0], '-hide_banner', '-loglevel', 'error', '-y', '-i', src,
                        '-c:a', 'adpcm_ima_qt', '-f', 'caf', dst], check=True)
        return open(dst, 'rb').read()


# --- the merges ----------------------------------------------------------

def merge_areas(x, out):
    ios = json.load(open(out.path('Data/areas.json'), encoding='utf-8'))
    android = load_json(x.read('Data/areas.json'))
    hospital = next(a for a in android['areas'] if a['area_id'] == HOSPITAL)
    ios['areas'] = [a for a in ios['areas'] if a['area_id'] != HOSPITAL] + [hospital]
    out.write('Data/areas.json', dump_json(ios), merged=True)


def merge_surgeries_info(x, out):
    ios = json.load(open(out.path('Data/Surgeries/surgeries_info.json'), encoding='utf-8'), strict=False)
    android = load_json(x.read('Data/Surgeries/surgeries_info.json'))['surgeries_info']
    for k, v in android.items():
        if v.get('area_id') == HOSPITAL:
            ios['surgeries_info'][k] = v
    out.write('Data/Surgeries/surgeries_info.json', dump_json(ios), merged=True)


def merge_default_data(x, out):
    ios = json.load(open(out.path('Data/default_data.json'), encoding='utf-8'))
    android = load_json(x.read('Data/default_data.json'))
    unlock = next(u for u in android['area_unlock_star'] if u['id'] == HOSPITAL)
    ios['area_unlock_star'] = [u for u in ios['area_unlock_star'] if u['id'] != HOSPITAL] + [unlock]
    hospital = [s for s in android['surgeries'] if s['id'].startswith('surgery_hospital_')]
    ios['surgeries'] = [s for s in ios['surgeries'] if not s['id'].startswith('surgery_hospital_')] + hospital
    out.write('Data/default_data.json', dump_json(ios), merged=True)


# --- main ----------------------------------------------------------------

def main(xapk, dest):
    if not os.path.isfile(os.path.join(dest, '.extracted_from')):
        raise SystemExit(f'{dest} has no iOS package (run extract_ipa.py first)')
    marker = os.path.join(dest, MARKER)
    if os.path.isfile(marker):  # files of the last run go; merged files are merged again
        for rel in open(marker, encoding='utf-8').read().splitlines()[1:]:
            if rel.startswith('merged '):
                continue
            p = os.path.join(dest, *rel.split('/'))
            if os.path.isfile(p):
                os.remove(p)
    x = Xapk(xapk)
    out = Out(dest)
    ios_has = lambda rel: os.path.exists(out.path(rel))

    for rel in sorted(x.files):
        if any(re.match(p, rel) for p in NEW_FILES) or (re.match(NEW_INJURIES, rel) and not ios_has(rel)):
            data = x.read(rel)
            try:
                json.loads(data.decode('utf-8-sig'))
            except ValueError:
                data = dump_json(load_json(data))  # raw control characters: written as escapes
            out.write(rel, data)

    for name in NEW_SHEETS + NEW_PARTICLES:
        out.write(f'{SCALE}/{name}.json', plist_to_json(x.read(f'{SCALE}/{name}.plist')))
        if x.has(f'{SCALE}/{name}.png'):
            out.write(f'{SCALE}/{name}.png', x.read(f'{SCALE}/{name}.png'))
    for name in SUPPLEMENTS:
        frames = supplement(x, out, name)
        print(f'supplement {name}: {", ".join(frames)}')

    for name in NEW_SOUNDS:
        out.write(f'Sounds/{name}.caf', ogg_to_caf(x.read(f'Sounds/{name}.ogg')))

    merge_areas(x, out)
    merge_surgeries_info(x, out)
    merge_default_data(x, out)

    with open(marker, 'w', encoding='utf-8') as m:
        m.write(os.path.basename(xapk) + '\n' + ''.join(w + '\n' for w in sorted(out.written)) +
                ''.join(f'merged {w}\n' for w in sorted(out.merged)))
    print(f'{len(out.written)} files added, {len(out.merged)} merged -> {dest}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
