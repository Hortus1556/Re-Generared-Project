"""Films screens and patients with AS_FILM and checks their motion.

usage: python tools/motion_sweep.py [--out <folder>] [--steps N] [screens|patients|all] [patient numbers...]

Runs build-dev with a scratch save (AS_SAVE_DIR), films each case (every
display step between logic passes) and runs tools/motion_check.py on it.
Patients are played with the same tool strokes as test_patients.py's monkey
test, so wounds, effects and tool animations all get exercised.
"""
import argparse
import os
import subprocess
import sys
import tempfile

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(PORT, 'tools'))
from test_patients import monkey_input  # noqa: E402

# name: (event, clicks, drags, first frame, last frame[, saved progress])
SCREENS = {
    'splash': (1, '', '', 1, 120),
    'menu': (2, '', '', 1, 120),
    'patients': (3, '', '', 1, 80),
    'story': (115, '', '', 1, 80),
    'card': (115, '60:850:745,100:800:735', '', 96, 130),
    # A beaten patient opens its file page, which slides in.
    'file': (3, '40:160:280', '', 38, 60, 5),
    # The credits open and close (the hand wipes the water away).
    'credits': (2, '80:960:750,130:888:697', '', 128, 150),
    'lose': (6, '', '', 1, 120),
    'win': (7, '', '', 1, 120),
}


def film(exe, out, name, event, clicks, drags, first, last, steps, progress=None):
    csv = os.path.join(out, name)
    save = os.path.join(out, 'save_' + name)
    os.makedirs(save, exist_ok=True)
    with open(os.path.join(save, 'RMS.txt'), 'w') as f:
        f.write('PLAYER1_NAME\tPlayer 1\n' + (f'PATIENT_PROCESS\t{progress}\n' if progress else ''))
    env = dict(os.environ)
    env.update({'AS_SAVE_DIR': save, 'AS_EVENT': str(event),
                'AS_FILM': f'{csv}:{first}:{last}:{steps}'})
    if clicks:
        env['AS_CLICKS'] = clicks
    if drags:
        env['AS_DRAG'] = drags
    try:
        subprocess.run([exe, '-d'], cwd=os.path.dirname(exe), env=env, capture_output=True, timeout=600)
    except subprocess.TimeoutExpired:
        print(f'{name}: timeout')
        return
    r = subprocess.run([sys.executable, os.path.join(PORT, 'tools', 'motion_check.py'), csv + '.csv', '--top', '8'],
                       capture_output=True, text=True)
    print(f'### {name}\n{r.stdout}', flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('what', nargs='?', default='all')
    ap.add_argument('patients', nargs='*', type=int)
    ap.add_argument('--out', default=os.path.join(tempfile.gettempdir(), 'as_motion'))
    ap.add_argument('--steps', type=int, default=4)
    ap.add_argument('--exe', default=os.path.join(PORT, 'build-dev', 'AmateurSurgeon.exe'))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    exe = os.path.abspath(a.exe)
    if a.what in ('screens', 'all'):
        for name, case in SCREENS.items():
            film(exe, a.out, name, *case[:5], a.steps, *case[5:])
    if a.what in ('patients', 'all'):
        for n in a.patients or range(1, 31):
            clicks, drags, end = monkey_input(60)
            film(exe, a.out, f'op{n:02d}', 9 + n, clicks, drags, 30, end, a.steps)


if __name__ == '__main__':
    main()
