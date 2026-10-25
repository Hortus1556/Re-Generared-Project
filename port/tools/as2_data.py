"""Amateur Surgeon 2's game data from its iOS binary, as the package's
games/as2/data/library.json.

usage: python port/tools/as2_data.py

The game builds its catalogue in code: Library setupInjuries makes every
injury type (id, linkage, class, name, description) and the ten tools
(the same, and their enterFrame flag). The binary is run in an ARM emulator
(re/scripts/emucalls.py) and the arguments of each initWithId:... message
are read back, so the data is exactly the binary's.

Each injury class also has a static table of its stages (stageArray_t, 48
bytes a stage: what tool treats it, what a wrong one does, damage and its
period, the special timer, the effect, the guide line's colour...), read
from the binary's data into "stages": {class: [stage...]}.

"flashStages": {class: [stage...]} are the Flash AS2's tables, the same
fields, read from its decompiled injury classes (re/flash/as2/scripts:
each constructor's `stageArray = [...]`; a stage's tools as "a,b", its
size `bigStage`/`smallStage` as 240/120): the monsters whose logic is the
Flash AS2's take theirs (`--flash` refreshes only these, "flashLevels" and the port's own).
"flashLevels": [mission][stage][injury] = [id, x, y], where the Flash AS2's
levels.txt (re/flash/as2/.../media/levels.txt) placed each injury of its 20
missions on its stage: the iPad's levels.txt moved many to suit its own
pictures, and the monsters drawn as Flash's stand where Flash put them.

The port's own injuries (PORT_INJURIES, made on the user's request: not the
iPad's or Flash's) are added to "injuries", their tables to "stages" and
their superclass to "classes", ids from 200 up.
"classes": {class: superclass} is the game's class tree under
InjuryGraphic (from re/as2/classes.txt, the binary's ObjC metadata), for
isKindOfClass: tests.

Regenerate it; never edit library.json by hand.
"""
import json, os, re, struct, subprocess, sys

import fetch_sources

# fetch_sources.ROOT (not __file__, which points inside a frozen exe's
# temp unpack dir when this is bundled rather than run as a script).
ROOT = fetch_sources.ROOT
MACHO = os.path.join(ROOT, 're', 'as2', 'AS2')
EMU = os.path.join(ROOT, 're', 'scripts', 'emucalls.py')
OUT = os.path.join(ROOT, 'port', 'games', 'as2', 'data', 'library.json')
CLASSES = os.path.join(ROOT, 're', 'as2', 'classes.txt')
FLASH_INJURIES = os.path.join(ROOT, 're', 'flash', 'as2', 'scripts', 'game', 'scripts', 'game', 'model', 'assets',
                              'injuries')

FLASH_LEVELS = os.path.join(ROOT, 're', 'flash', 'as2', 'i.adultswim.com', 'adultswim', 'games2', 'game-files',
                            'amateursurgeon2', 'media', 'levels.txt')
# The port's own injuries: catalogue entry, stage table, superclass.
# A monster's remains (Remains.h): a dead dove, wasp, squid or small
# centipede (its head) lies where it
# died, to be lifted away with the tongs (a thing lifted away, as the iPad's
# ChickenLeg: the same table).
LIFTED = [{'tool': 'tongs', 'wrongTool': 'nothing', 'i8': 1000, 'damagePeriod': 250, 'damage': 0.6, 'specialPeriod': 200,
           'effect': 'nothing', 's1c': 'nothing', 'c20': 1, 'lineColour': 'invisible', 'c28': 0, 'i2c': 120}]
PORT_INJURIES = [
    ({'id': 200, 'linkage': 'deadDove', 'class': 'DeadDove', 'name': '', 'description': ''}, LIFTED, 'Remains'),
    ({'id': 201, 'linkage': 'deadWasp', 'class': 'DeadWasp', 'name': '', 'description': ''}, LIFTED, 'Remains'),
    ({'id': 202, 'linkage': 'deadSquid', 'class': 'DeadSquid', 'name': '', 'description': ''}, LIFTED, 'Remains'),
    ({'id': 204, 'linkage': 'deadCentipede', 'class': 'DeadCentipede', 'name': '', 'description': ''}, LIFTED,
     'Remains'),
]
# Their class tree above them: Remains is an Extraction.
PORT_CLASSES = {'Remains': 'Extraction'}
# -[Library setupInjuries] (re/as2/symbols.txt).
SETUP_INJURIES = '1b584'

STR = r'"((?:[^"\\]|\\.)*)"'
INJURY = re.compile(r'initWithId:Linkage:ClassType:Name:Description:\] (-?\d+), ' + ', '.join([STR] * 4) + r'$')
TOOL = re.compile(r'initWithId:Linkage:ClassType:Name:Description:EnterFrame:\] (-?\d+), ' +
                  ', '.join([STR] * 4) + r', (-?\d+)$')


def emulate(addr):
    # creationflags: no console window flash for each call (there are
    # many) when this runs inside a windowed/frozen GUI.
    no_window = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
    out = subprocess.run([fetch_sources.python_exe(), EMU, MACHO, addr], capture_output=True, text=True, check=True,
                         creationflags=no_window).stdout
    return out.splitlines()


def stage_tables():
    """{class: [stage dict...]} from the initWithInjuryData: statics."""
    sys.argv = ['annotate', MACHO, '.', '.']
    src = open(os.path.join(ROOT, 're', 'scripts', 'annotate.py'), encoding='utf-8').read()
    env = {}
    exec(src.split('src, dst = sys.argv[2]')[0], env)
    data, syms, fo, u32, cstr, in_sect = (env[k] for k in ('data', 'syms', 'fo', 'u32', 'cstr', 'in_sect'))
    addrs = sorted(syms)

    def string(p):
        if p == 0:
            return None
        if fo(p) is None or not in_sect(p, '__cfstring'):
            raise ValueError('not a CFString: %x' % p)
        return cstr(u32(fo(p) + 8))

    tables = {}
    for a in addrs:
        # (Splatter copies its table in performInitStuff.)
        m = re.search(r'-\[(\w+) (?:initWithInjuryData:Instance:PlayerCreated:Force:|performInitStuff)\]E\d+C\.\d+$',
                      syms[a])
        if not m:
            continue
        nxt = next((b for b in addrs if b > a), a + 0x30)
        stages = []
        for k in range((nxt - a) // 0x30):
            w = struct.unpack_from('<12I', data, fo(a + k * 0x30))
            try:
                stages.append({
                    'tool': string(w[0]), 'wrongTool': string(w[1]), 'i8': w[2], 'damagePeriod': w[3],
                    'damage': round(struct.unpack('<f', struct.pack('<I', w[4]))[0], 6), 'specialPeriod': w[5],
                    'effect': string(w[6]), 's1c': string(w[7]), 'c20': w[8] & 0xff, 'lineColour': string(w[9]),
                    'c28': w[10] & 0xff, 'i2c': w[11]})
            except ValueError:
                break  # past the table: other data follows
        tables[m.group(1)] = stages
    return tables


def flash_stage_tables():
    """{class: [stage dict...]} from the Flash AS2's injury classes."""
    sizes = {'bigStage': 240, 'smallStage': 120}
    tables = {}
    for name in sorted(os.listdir(FLASH_INJURIES)):
        if not name.endswith('.as'):
            continue
        src = open(os.path.join(FLASH_INJURIES, name), encoding='utf-8').read()
        m = re.search(r'stageArray = (\[\[.*?\]\]);', src, re.S)
        if not m:
            continue
        text = m[1]
        for k, v in sizes.items():
            text = text.replace(k, str(v))
        text = text.replace('true', 'True').replace('false', 'False')
        try:
            rows = eval(text, {'__builtins__': {}})
        except Exception:
            continue  # (built from variables: not a plain table)
        stages = []
        for r in rows:
            if len(r) != 12:
                break
            tool = ','.join(r[0]) if isinstance(r[0], list) else r[0]
            stages.append({'tool': tool, 'wrongTool': r[1], 'i8': r[2], 'damagePeriod': r[3], 'damage': r[4],
                           'specialPeriod': r[5], 'effect': r[6], 's1c': r[7], 'c20': int(bool(r[8])),
                           'lineColour': r[9], 'c28': int(bool(r[10])), 'i2c': r[11]})
        else:
            tables[name[:-3]] = stages
    return tables


def flash_levels():
    """[mission][stage][injury] = [id, x, y] from the Flash AS2's levels.txt."""
    missions = json.load(open(FLASH_LEVELS, encoding='utf-8'))
    return [[[[i['id'], i['x'], i['y']] for i in stage['injuries']] for stage in m['mission_data']] for m in missions]


def injury_classes():
    """{class: superclass} for every class descending from InjuryGraphic."""
    parent = {}
    for line in open(CLASSES, encoding='utf-8'):
        m = re.match(r'@interface (\w+) : (\w+)', line)
        if m:
            parent[m[1]] = m[2]
    def under(c):
        while c in parent:
            c = parent[c]
            if c == 'InjuryGraphic':
                return True
        return False
    return {c: p for c, p in sorted(parent.items()) if under(c)}


def add_port_injuries(doc):
    """The port's own injuries into the catalogue, tables and class tree
    (any it no longer has taken out)."""
    kept = {entry['id'] for entry, _, _ in PORT_INJURIES}
    for gone in [i for i in doc['injuries'] if i['id'] >= 200 and i['id'] not in kept]:
        doc['injuries'].remove(gone)
        doc['stages'].pop(gone['class'], None)
        doc['classes'].pop(gone['class'], None)
    for entry, stages, parent in PORT_INJURIES:
        doc['injuries'] = [i for i in doc['injuries'] if i['id'] != entry['id']] + [entry]
        doc['stages'][entry['class']] = stages
        doc['classes'][entry['class']] = parent
    doc['classes'].update(PORT_CLASSES)


def main():
    if '--flash' in sys.argv:
        doc = json.load(open(OUT, encoding='utf-8'))
        doc['flashStages'] = flash_stage_tables()
        doc['flashLevels'] = flash_levels()
        add_port_injuries(doc)
        with open(OUT, 'w', encoding='utf-8') as f:
            json.dump(doc, f, indent=1)
            f.write('\n')
        print(f"{len(doc['flashStages'])} Flash stage tables -> {os.path.relpath(OUT, ROOT)}")
        return
    injuries, tools = [], []
    for line in emulate(SETUP_INJURIES):
        m = INJURY.search(line)
        if m:
            injuries.append({'id': int(m[1]), 'linkage': m[2], 'class': m[3], 'name': m[4], 'description': m[5]})
        m = TOOL.search(line)
        if m:
            tools.append({'id': int(m[1]), 'linkage': m[2], 'class': m[3], 'name': m[4], 'description': m[5],
                          'enterFrame': int(m[6]) != 0})
    if not injuries or len(tools) != 10:
        raise SystemExit(f'emulation gave {len(injuries)} injuries and {len(tools)} tools')
    doc = {'injuries': injuries, 'tools': tools, 'stages': stage_tables(), 'flashStages': flash_stage_tables(),
           'flashLevels': flash_levels(), 'classes': injury_classes()}
    add_port_injuries(doc)
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(doc, f, indent=1)
        f.write('\n')
    print(f'{len(injuries)} injuries, {len(tools)} tools -> {os.path.relpath(OUT, ROOT)}')


if __name__ == '__main__':
    main()
