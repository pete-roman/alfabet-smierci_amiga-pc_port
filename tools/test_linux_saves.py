"""Exercise all original save slots and quick save/load in an isolated user directory.

    ALF_EXE=/path/to/packaged/AlfabetSmierci python3 tools/test_linux_saves.py

Uses the real menu and disk I/O, with a changed train-rides counter as the state marker.
Game disks and existing player saves are never modified. Logs remain in work/native-saves/.
"""
import os
import pathlib
import re
import subprocess
import sys
import tempfile

from engine_path import ROOT, executable


def main():
    if not (sys.platform.startswith("linux") or sys.platform == "darwin"):
        raise SystemExit("This test supports Linux and macOS native builds.")
    out = ROOT / "work" / "native-saves"
    out.mkdir(parents=True, exist_ok=True)
    events = []
    frame = 300

    def event(action, delay=0):
        nonlocal frame
        frame += delay
        events.append(f"{frame} {action}")

    def click(x, y):
        event(f"lclick {x} {y}", 10)
        event("lup", 4)
        event("state", 600)

    # Switch from verbs to the original menu; these coordinates are game coordinates.
    click(598, 360)
    for slot, y in enumerate((43, 68, 92), 1):
        event(f"poke 7C0CF {slot}", 10)
        click(545, 260)  # Zapisz
        click(570, y)
    for slot, y in enumerate((43, 68, 92), 1):
        event("poke 7C0CF 0", 10)
        click(598, 260)  # Wczytaj
        click(570, y)
        event("peek 7C0CF", 10)
    event("qsave", 10)
    event("poke 7C0CF 0", 30)
    event("qload", 10)
    event("peek 7C0CF", 600)
    event("quit", 10)

    with tempfile.TemporaryDirectory(prefix="save test ", dir=out) as temporary:
        temporary = pathlib.Path(temporary)
        auto = temporary / "events.txt"
        auto.write_text("\n".join(events) + "\n", encoding="utf-8")
        env = {k: v for k, v in os.environ.items() if not k.startswith("ALF_")}
        if sys.platform == "darwin":
            home = temporary / "home"
            home.mkdir()
            env.update(HOME=str(home), CFFIXED_USER_HOME=str(home))
        else:
            env.update(XDG_DATA_HOME=str(temporary / "userdata"))
        env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                   ALF_SKIP_INTRO="1", ALF_FAST="1", ALF_WINDOWED="1",
                   ALF_AUTOPLAY=str(auto))
        result = subprocess.run([str(executable())], cwd=temporary, env=env,
                                capture_output=True, text=True, timeout=120)
        (out / "engine.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(f"game exited with {result.returncode}: see {out / 'engine.log'}")
        observed = [int(n) for n in re.findall(r"^peek 7C0CF (\d+)$", result.stdout, re.M)]
        if observed != [1, 2, 3, 3]:
            raise RuntimeError(f"save/load restored {observed}, expected [1, 2, 3, 3]")
        saves = list(temporary.rglob("stan_gry.adf"))
        quick = list(temporary.rglob("stan.sav"))
        if len(saves) != 1 or len(quick) != 1:
            raise RuntimeError("expected both save files in the isolated user directory")
        disk = saves[0].read_bytes()
        if len(disk) != 901120 or disk[:4] != b"STAN":
            raise RuntimeError("invalid save disk size or header")
        offset = 0x7C0CF - 0x7BBD4
        markers = [disk[0x1600 + slot * 0x69C + offset] for slot in range(3)]
        if markers != [1, 2, 3]:
            raise RuntimeError(f"save disk slots contain {markers}, expected [1, 2, 3]")
        state = quick[0].read_bytes()
        if len(state) != 0x69C or state[offset] != 3:
            raise RuntimeError("invalid quick save content")
    print("ok: original slots 1/2/3 and quick save/load, isolated per-user files")


if __name__ == "__main__":
    main()
