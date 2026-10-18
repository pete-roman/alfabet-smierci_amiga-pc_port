"""Play the whole game from the walkthrough, closed loop.

    py tools/playthrough.py [--from N] [--to M] [--load FILE]

Steps come from tests/walkthrough.json (derived from the scripts, see docs/WALKTHROUGH.md).
The engine runs with ALF_CONTROL=1: commands go in on stdin, and "state" answers with
the location, the script line the game is on and whether it waits for a right click.
So the bot right-clicks through speech only while the game really waits for it, and
clicks a dialogue row only once the option list is up.

After every step the location is checked against the next step's location, the state
is quick-saved to work/play/step_NNN.sav (resume with --from N+1 --load that file) and
a line goes to work/play/log.txt. On a mismatch the run stops with a screenshot.
"""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
from engine_path import executable

EXE = executable()
OUT = ROOT / "work" / "play"
SAVE = OUT / "current.sav"
NEUTRAL = (495, 300)            # outside every scene and UI rect: re-arms item clicks
ARROW_UP, ARROW_DOWN = (544, 218), (598, 218)
ENDING_LOC = 0x3D
FLAG_RE = re.compile(r"\$(7C[0-9A-F]{3})=(\d+)")
SLOT_X, SLOT_Y = 571, (43, 68, 92, 116, 140, 164)
DIALOGUE_TABLE = 0x1D790
DIALOGUE_MIN_Y = 0x192
BUSY_CALLS = ("delay", "fade", "decrunch", "load_disk", "ensure_disk", "show_frame", "wait_")


def loc_id(name):
    return int(name.split("_")[1], 16)


class Game:
    lmb_only = False          # --lmb-only: move speech on with the left button, as a player would

    def __init__(self, load=None):
        env = dict(os.environ, ALF_CONTROL="1", ALF_SKIP_INTRO="1", ALF_FAST="1", ALF_WINDOWED="1",
                   ALF_SAVE=str(SAVE))
        self.p = subprocess.Popen([str(EXE)], cwd=EXE.parent, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=open(OUT / "engine.log", "w"), text=True, env=env, bufsize=1)
        self.lines = {}
        self.state(300)
        self.rclicks("idle")
        if load:
            shutil.copy(load, SAVE)
            self.send("0 qload")
            self.state(50)
            self.rclicks("idle")

    def send(self, *cmds):
        for c in cmds:
            self.p.stdin.write(c + "\n")
        self.p.stdin.flush()

    def state(self, delay=8):
        self.send(f"{delay} state")
        line = self.p.stdout.readline()
        m = re.match(r"state frame (\d+) loc (\w+) where (\S+) rwait (\d) table (\w+) miny (\d+) lmbpoll (\d+)", line)
        if not m:
            raise RuntimeError(f"engine gone: {line!r}")
        return {"frame": int(m[1]), "loc": int(m[2], 16), "where": m[3], "rwait": m[4] == "1",
                "table": int(m[5], 16), "miny": int(m[6]), "lmbpoll": int(m[7])}

    def line_text(self, where):
        f, _, n = where.rpartition(":")
        if f not in self.lines:
            path = ROOT / "scripts" / f
            self.lines[f] = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
        lines = self.lines[f]
        n = int(n) if n.isdigit() else 0
        return lines[n - 1].strip() if 0 < n <= len(lines) else ""

    def busy(self, st):
        """Cutscenes, fades and disk loads do not look at the left button; rooms do, every frame."""
        if st["lmbpoll"] > 2:
            return True
        text = self.line_text(st["where"])
        return text.startswith("call ") and any(b in text for b in BUSY_CALLS)

    def options_up(self, st):
        return (st["table"] == DIALOGUE_TABLE and st["miny"] == DIALOGUE_MIN_Y and not st["rwait"]
                and st["lmbpoll"] <= 2)

    def rclicks(self, until, limit=4000):
        """Right-click while the game waits for it; stop when `until` holds."""
        calm = 0
        st = self.state(4)
        for _ in range(limit):
            if st["rwait"]:
                if self.lmb_only:
                    self.send(f"0 lclick {NEUTRAL[0]} {NEUTRAL[1]}", "4 lup")
                else:
                    self.send("0 rclick", "4 rup")
                calm = 0
            elif self.options_up(st):          # a list is up: the next step picks a row
                return st
            elif until == "idle" and not self.busy(st) and not self.options_up(st):
                calm += 1
                if calm >= 4:
                    return st
            elif st["loc"] == ENDING_LOC:
                # the last card of the ending: the game stops (halt) until a click or key
                calm += 1
                if calm >= 15:
                    return st
            else:
                calm = 0
            st = self.state(8)
        raise RuntimeError(f"rclicks({until}) did not settle, last {st}")

    def click(self, x, y):
        self.send(f"0 lclick {x} {y}", "3 lup")
        self.state(8)

    def click1(self, x, y):
        self.send(f"0 lclick {x} {y}", "1 lup", f"2 move {NEUTRAL[0]} {NEUTRAL[1]}")
        self.state(6)

    def move(self, x, y):
        self.send(f"0 move {x} {y}")
        self.state(4)

    def inv(self):
        self.send("0 inv")
        line = self.p.stdout.readline().split()
        if line[:2] != ["inv", "scroll"]:
            raise RuntimeError(f"bad inv reply {line}")
        return int(line[2]) // 4, [int(x, 16) for x in line[4:]]

    def select_item(self, item):
        """Click the item's slot, scrolling the six-slot window to it first. The slot numbers
        in the walkthrough come from a simulation; the real inventory is the authority."""
        want = int(item.lstrip("$"), 16)
        top, items = self.inv()
        if want not in items:
            raise RuntimeError(f"item ${want:05X} not in inventory {[hex(x) for x in items]}")
        i = items.index(want)
        for _ in range(40):
            if top <= i < top + 6:
                break
            self.click1(*(ARROW_UP if i < top else ARROW_DOWN))
            top, items = self.inv()
        else:
            raise RuntimeError("could not scroll the inventory to the item")
        self.move(*NEUTRAL)
        self.click(SLOT_X, SLOT_Y[i - top])

    def peek(self, addr):
        self.send(f"0 peek {addr:X}")
        line = self.p.stdout.readline().split()
        if line[:1] != ["peek"]:
            raise RuntimeError(f"bad peek reply {line}")
        return int(line[2])

    def check_flags(self, sets):
        """The walkthrough's "sets" field lists state bytes/words a step must set ($7Cxxx=v).
        Returns the ones that do not hold."""
        wrong = []
        for addr, v in FLAG_RE.findall(sets or ""):
            a, v = int(addr, 16), int(v)
            hi = self.peek(a)
            word = hi * 256 + self.peek(a + 1) if a % 2 == 0 else None
            if hi != v and word != v:
                wrong.append(f"${addr}={v} (is {hi})")
        return wrong

    def fire(self):
        self.send("0 fire", "10 fireup")
        self.state(8)

    def shot(self, path):
        self.send(f"0 shot {path}")
        self.state(2)

    def quick_save(self, dest):
        self.send("0 qsave")
        self.state(4)
        shutil.copy(SAVE, dest)

    def close(self):
        try:
            self.send("0 quit")
            self.p.stdin.close()
            self.p.wait(15)
        except Exception:
            self.p.kill()


def run_step(g, step, nxt):
    acts = step["actions"]
    items = [step.get("item"), step.get("item2")]
    # like a player: rest the pointer off every hotspot first. Some rooms re-arm their
    # hotspots (hover_mode = 1) only while the pointer is off them and no verb is chosen.
    g.move(*NEUTRAL)
    g.state(6)
    for i, a in enumerate(acts):
        d = a["do"]
        what = a.get("what", "")
        if what.startswith("inventory down") or what.startswith("inventory up"):
            continue                        # select_item scrolls by itself
        if what.startswith("inventory slot"):
            g.select_item(items.pop(0))
        elif d == "click":
            g.click(a["x"], a["y"])
        elif d == "click1":
            g.click1(a["x"], a["y"])
        elif d == "move":
            g.move(a["x"], a["y"])
        elif d == "fire":
            g.fire()
        elif d == "rclicks":
            later = acts[i + 1] if i + 1 < len(acts) else None
            row_next = later and later["do"] == "click" and "dialogue row" in later.get("what", "")
            g.rclicks("options" if row_next else "idle")
        else:
            raise RuntimeError(f"unknown action {d}")
    if not acts or acts[-1]["do"] != "rclicks":
        g.rclicks("idle")
    st = g.state(8)
    want = loc_id(nxt["loc"]) if nxt else None
    return st, want


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="first", type=int, default=1)
    ap.add_argument("--to", dest="last", type=int, default=10**9)
    ap.add_argument("--load", default=None)
    ap.add_argument("--lmb-only", action="store_true", help="speech moved on with the left button only")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    steps = json.loads((ROOT / "tests" / "walkthrough.json").read_text(encoding="utf-8"))
    by_n = {s["n"]: s for s in steps}
    log = open(OUT / "log.txt", "a", encoding="utf-8")

    def say(msg):
        print(msg, flush=True)
        log.write(msg + "\n")
        log.flush()

    Game.lmb_only = args.lmb_only
    g = Game(args.load)
    ok = True
    try:
        st = g.state(4)
        say(f"start at step {args.first}: loc {st['loc']:02X} ({st['where']})")
        for s in steps:
            n = s["n"]
            if n < args.first or n > args.last:
                continue
            if st["loc"] != loc_id(s["loc"]):
                say(f"step {n}: expected to be in {s['loc']} but in {st['loc']:02X}")
            st, want = run_step(g, s, by_n.get(n + 1))
            tag = f"step {n:3d} {s['loc']:9s} {s['verb']:10s} -> loc {st['loc']:02X} frame {st['frame']}"
            if want is not None and st["loc"] != want:
                g.shot((OUT / f"fail_{n:03d}.bmp").as_posix())
                say(tag + f"  MISMATCH: next step wants {by_n[n + 1]['loc']}")
                ok = False
                break
            wrong = g.check_flags(s.get("sets"))
            if wrong:
                g.shot((OUT / f"fail_{n:03d}.bmp").as_posix())
                say(tag + f"  FLAGS NOT SET: {', '.join(wrong)}")
                ok = False
                break
            g.quick_save(OUT / f"step_{n:03d}.sav")
            say(tag + "  ok")
        if ok:
            g.shot((OUT / "final.bmp").as_posix())
            say(f"done, final loc {g.state(4)['loc']:02X}")
            if g.state(4)["loc"] == ENDING_LOC:
                g.send("0 key", "5 keyup")    # past the last card: the port's end page
                g.state(300)
                g.shot((OUT / "port_end_page.bmp").as_posix())
                g.send("0 key", "5 keyup")    # and back to the title
                st = g.state(300)
                g.shot((OUT / "after_ending.bmp").as_posix())
                back = st["where"].startswith("intro.alf")
                say(f"key after the ending -> {st['where']} ({'title' if back else 'NOT the intro'})")
                ok = back
    except RuntimeError as e:
        say(f"stopped: {e}")
        ok = False
    finally:
        g.close()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
