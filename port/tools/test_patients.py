"""Automated smoke test of every patient (operation) in the port.

usage: python tools/test_patients.py [--exe build-dev/AmateurSurgeon.exe]
                                     [--out <folder>] [patient numbers...]

For each operation (default 1-30) it runs the game three times, always
with AS_SAVE_DIR pointing at a scratch folder (never the player's save):

  load    start straight in the operation (AS_EVENT), screenshot it.
  monkey  select every toolbar tool in turn and tap / drag it all over the
          body; passes if the game neither crashes nor hangs.
  (--quick stops here)
  win     let the operation count as won after a few seconds (AS_AUTOWIN)
          and check that the game moves on to the story board (event 50).

Screenshots go to <out>/opNN_<phase>_<frame>.png.
Exit status is non-zero when any ported patient fails.
"""
import argparse
import os
import subprocess
import sys
import tempfile

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Toolbar button centres (Toolbar kPosX + half the 80 px button), y in the bar.
TOOL_X = [177, 271, 360, 444, 524, 610, 696, 780, 862, 944]
TOOL_Y = 60


def run(exe, out, env_extra, timeout):
    env = dict(os.environ)
    env.update(env_extra)
    env['AS_SAVE_DIR'] = os.path.join(out, 'save')
    env['AS_TRACE'] = '1'
    try:
        p = subprocess.run([exe, '-d'], cwd=os.path.dirname(exe), env=env, capture_output=True, text=True,
                           timeout=timeout)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 'timeout', (e.stdout or b'').decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or '')


def monkey_input(start):
    """Clicks and drags: each tool, then strokes and taps over the body."""
    clicks, drags = [], []
    f = start
    for i, x in enumerate(TOOL_X):
        clicks.append(f'{f}:{x + 40}:{TOOL_Y}')
        f += 10
        for (x1, y1, x2, y2) in ((150, 250, 900, 650), (900, 250, 150, 650), (500, 200, 520, 700)):
            drags.append(f'{f}:{x1}:{y1}:{x2}:{y2}:25')
            f += 32
        for (tx, ty) in ((300, 400), (512, 450), (700, 350)):
            clicks.append(f'{f}:{tx}:{ty}')
            f += 6
        # Drop anything held on the tray area.
        drags.append(f'{f}:512:450:120:700:15')
        f += 20
    return ','.join(clicks), ','.join(drags), f


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=os.path.join(PORT, 'build-dev', 'AmateurSurgeon.exe'))
    ap.add_argument('--out', default=os.path.join(tempfile.gettempdir(), 'as_patient_tests'))
    ap.add_argument('--quick', action='store_true', help='load phase only')
    ap.add_argument('patients', nargs='*', type=int)
    a = ap.parse_args()
    exe = os.path.abspath(a.exe)
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    patients = a.patients or list(range(1, 31))

    failures = 0
    for n in patients:
        event = 9 + n
        tag = f'op{n:02d}'
        results = {}

        code, log = run(exe, out, {'AS_EVENT': str(event), 'AS_SHOT': os.path.join(out, f'{tag}_load') + ':300'}, 60)
        results['load'] = code == 0 and os.path.exists(os.path.join(out, f'{tag}_load_300.png'))
        if a.quick:
            print(f'{tag}: {"PASS" if results["load"] else "FAIL"}  load={"ok" if results["load"] else "FAIL"}',
                  flush=True)
            failures += not results['load']
            continue

        clicks, drags, end = monkey_input(240)
        code, log = run(exe, out, {'AS_EVENT': str(event), 'AS_CLICKS': clicks, 'AS_DRAG': drags,
                                   'AS_SHOT': os.path.join(out, f'{tag}_monkey') + f':{end}'}, 240)
        results['monkey'] = code == 0

        code, log = run(exe, out, {'AS_EVENT': str(event), 'AS_AUTOWIN': '100',
                                   'AS_SHOT': os.path.join(out, f'{tag}_win') + ':900'}, 90)
        results['win'] = code == 0 and 'view 50' in log

        ok = all(results.values())
        failures += not ok
        detail = ' '.join(f'{k}={"ok" if v else "FAIL"}' for k, v in results.items())
        print(f'{tag}: {"PASS" if ok else "FAIL"}  {detail}', flush=True)
    print(f'screenshots: {out}')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
