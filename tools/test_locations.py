"""Smoke-test every location of the port: warp in, fill the inventory, click around.

    py tools/test_locations.py [loc_XX ...]        -> work/loctest/report.txt + screenshots

For each location routine the engine starts with ALF_SKIP_INTRO, jumps into the location
(ALF_WARP), gets every item into the inventory (ALF_ITEMS) and is driven by random clicks
on the scene, the verb buttons, the inventory slots and the dialogue lines. The log is
checked for unimplemented routines, script errors, stack overflows and hangs.
"""
import pathlib
import random
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
from engine_path import executable

EXE = executable()
OUT = ROOT / "work" / "loctest"

VERB_BUTTONS = [(545, 260), (598, 260), (545, 310), (598, 310), (545, 360)]   # look talk use open take
SLOTS = [(570, 43), (570, 68), (570, 92), (570, 116), (570, 140), (570, 164)]
ARROWS = [(545, 218), (598, 218)]


def items():
    names = set()
    for f in (ROOT / "scripts").glob("*.alf"):
        names.update(re.findall(r"l\[\$1932A\] = \$(7B[0-9A-F]+)", f.read_text(encoding="utf-8")))
    return sorted(names)


def locations():
    out = []
    for f in sorted((ROOT / "scripts").glob("loc_*.alf")):
        out += re.findall(r"^routine (loc_\w+) @", f.read_text(encoding="utf-8"), re.M)
    return out


def script_for(loc, seed, frames):
    rnd = random.Random(seed)
    ev = [f"250 shot {OUT.as_posix()}/{loc}_enter.bmp"]
    t = 300
    while t < frames:
        r = rnd.random()
        if r < 0.45:
            x, y = rnd.randint(16, 480), rnd.randint(32, 370)
        elif r < 0.6:
            x, y = rnd.choice(VERB_BUTTONS)
        elif r < 0.7:
            x, y = rnd.choice(SLOTS)
        elif r < 0.75:
            x, y = rnd.choice(ARROWS)
        else:
            x, y = rnd.randint(16, 620), rnd.randint(400, 500)    # dialogue lines / text panel
        ev.append(f"{t} move {x} {y}")
        if rnd.random() < 0.8:
            ev += [f"{t + 2} lclick {x} {y}", f"{t + 4} lup"]
        else:
            ev += [f"{t + 2} rclick", f"{t + 5} rup"]
        t += rnd.randint(10, 40)
    ev += [f"{frames} shot {OUT.as_posix()}/{loc}_end.bmp", f"{frames + 2} quit"]
    return "\n".join(ev) + "\n"


def run(loc, item_list, frames=4000, seed=1):
    auto = OUT / f"{loc}.txt"
    auto.write_text(script_for(loc, seed, frames))
    env = dict(**__import__("os").environ, ALF_SKIP_INTRO="1", ALF_WARP=loc, ALF_ITEMS=",".join(item_list[:24]),
               ALF_AUTOPLAY=str(auto), ALF_FAST="1", ALF_WINDOWED="1", ALF_TRACE="500")
    try:
        p = subprocess.run([str(EXE)], cwd=EXE.parent, env=env, capture_output=True, text=True, timeout=240)
        log = p.stdout + p.stderr
        exit_code = p.returncode
        timeout = False
    except subprocess.TimeoutExpired as e:
        log = (e.stdout or b"").decode(errors="replace") + (e.stderr or b"").decode(errors="replace")
        timeout = True
        exit_code = None
    (OUT / f"{loc}.log").write_text(log, encoding="utf-8")
    problems = sorted({l for l in log.splitlines() if not l.startswith(("tick", "scripts:"))})
    if exit_code not in (None, 0):
        problems.append(f"process exit {exit_code}")
    locs = re.findall(r"loc \$([0-9A-F]{2})", log)
    expected = loc.split("_")[1].upper()
    if expected not in locs:
        problems.append(f"target location {loc} never reported")
    return timeout, problems, sorted(set(locs))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    item_list = items()
    todo = sys.argv[1:] or locations()
    lines = []
    failed = False
    for loc in todo:
        timeout, problems, visited = run(loc, item_list)
        status = "TIMEOUT" if timeout else ("PROBLEMS" if problems else "ok")
        failed |= status != "ok"
        line = f"{loc:12s} {status:8s} visited {' '.join(visited)}"
        print(line, flush=True)
        lines.append(line)
        for p in problems[:8]:
            print("    " + p, flush=True)
            lines.append("    " + p)
    (OUT / "report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
