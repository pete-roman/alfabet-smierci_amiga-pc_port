"""Run the port in real time with the intro and measure the timing at key moments.

    py tools/realtime_check.py         -> work/realtime/log.txt (every sample), report on stdout

Real time = no ALF_FAST: the main thread paces frames at 50 Hz and the audio device pulls
samples at 48 kHz. Every half second (25 frames) the engine reports its frame counter,
the wall clock, the music position and the samples sent to the audio device. Checked:
  - frames per wall second (PAL 50 Hz),
  - samples per wall second (48000: the music really plays in real time),
  - music rows per second against the module's own speed/tempo,
  - the intro's key moments (logos, title, story pages) and the handover to the game,
  - a location change in the game (fade out, picture, fade in) in frames.
"""
import os
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
from engine_path import executable

EXE = executable()
OUT = ROOT / "work" / "realtime"
RE = re.compile(r"state frame (\d+) loc (\w+) where (\S+) rwait (\d) table (\w+) miny (\d+) lmbpoll (\d+) "
                r"music (-?\d+) (-?\d+) (-?\d+) (\d+) ms (\d+) speed (\d+) tempo ([\d.]+) title (\d)")
KEYS = ("frame", "loc", "where", "rwait", "table", "miny", "lmbpoll", "mod", "order", "row", "samples", "ms",
        "speed", "tempo", "title")


class Port:
    def __init__(self):
        env = dict(os.environ, ALF_CONTROL="1", ALF_WINDOWED="1")
        env.pop("ALF_FAST", None)
        env.pop("ALF_SKIP_INTRO", None)
        self.p = subprocess.Popen([str(EXE)], cwd=EXE.parent, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True, env=env, bufsize=1)
        self.log = open(OUT / "log.txt", "w", encoding="utf-8")
        self.samples = []

    def send(self, *cmds):
        for c in cmds:
            self.p.stdin.write(c + "\n")
        self.p.stdin.flush()

    def state(self, delay=25):
        self.send(f"{delay} state")
        m = RE.match(self.p.stdout.readline())
        if not m:
            raise RuntimeError("engine gone")
        st = dict(zip(KEYS, m.groups()))
        for k in KEYS:
            if k == "loc" or k == "table":
                st[k] = int(st[k], 16)
            elif k == "tempo":
                st[k] = float(st[k])
            elif k != "where":
                st[k] = int(st[k])
        self.samples.append(st)
        self.log.write(" ".join(f"{k}={st[k]}" for k in KEYS) + "\n")
        return st

    def click(self, x, y):
        self.send(f"0 lclick {x} {y}", "3 lup")

    def close(self):
        self.send("0 quit")
        self.p.stdin.close()
        try:
            self.p.wait(15)
        except subprocess.TimeoutExpired:
            self.p.kill()


def in_story_wait(where):
    """intro_sub_03504 (intro.alf lines 178-205): a story page waits for a click. The logo and
    title waits (intro_sub_03552) can be skipped by a click too, so the bot leaves those alone."""
    f, _, n = where.partition(":")
    return f == "intro.alf" and n.isdigit() and 178 <= int(n) <= 205


def rate(a, b, key):
    return (b[key] - a[key]) * 1000.0 / max(1, b["ms"] - a["ms"])


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    g = Port()
    events = []

    def event(name, st):
        events.append((name, st))
        print(f"{name:34s} frame {st['frame']:6d}  wall {st['ms'] / 1000:7.1f} s  music module {st['mod']}"
              f" order {st['order']} row {st['row']}", flush=True)

    try:
        st = g.state(5)
        event("start", st)
        seen_title = seen_story = False
        while not st["where"].startswith(("loc_", "common")) or st["loc"] != 0x04:
            st = g.state()
            if st["title"] and not seen_title:
                seen_title = True
                event("title picture", st)
            if in_story_wait(st["where"]) and st["lmbpoll"] <= 2:
                if not seen_story:
                    seen_story = True
                    event("first story page waits", st)
                g.click(320, 200)
            if st["frame"] > 50 * 60 * 6:
                raise RuntimeError("intro did not end in 6 minutes")
        event("game started (loc_04)", st)
        while st["lmbpoll"] > 2:
            st = g.state()
        event("station waits for the player", st)
        g.send("0 move 495 300")
        play_from = g.state(250)                       # 5 s of station music
        play_to = g.state(250)
        g.click(242, 335)                              # to the town
        clicked = g.state(1)
        st = clicked
        while not (st["loc"] == 0x05 and st["lmbpoll"] <= 2):
            st = g.state(5)
        event("town waits for the player", st)
        fade = [s for s in g.samples if s is not None and s["frame"] >= clicked["frame"]]
    finally:
        g.close()

    s0, s1 = g.samples[1], g.samples[-1]
    fps = rate(s0, s1, "frame")
    sps = rate(s0, s1, "samples")
    rows = (play_to["order"] - play_from["order"]) * 64 + (play_to["row"] - play_from["row"])
    rows_per_s = rows * 1000.0 / max(1, play_to["ms"] - play_from["ms"])
    expected = play_from["tempo"] / (2.5 * play_from["speed"]) if play_from["speed"] else 0
    print()
    print(f"frames per wall second:  {fps:.2f}  (PAL 50)")
    print(f"audio samples per second: {sps:.0f}  (48000)")
    print(f"station music: {rows_per_s:.2f} rows/s, module speed {play_from['speed']} tempo {play_from['tempo']}"
          f" -> expected {expected:.2f} rows/s")
    print(f"location change: click to town waiting for the player in {st['frame'] - clicked['frame']} frames"
          f" ({(st['ms'] - clicked['ms']) / 1000:.1f} s)")
    (OUT / "report.txt").write_text("\n".join(f"{n}: frame {s['frame']} wall {s['ms']}" for n, s in events) + "\n")


if __name__ == "__main__":
    main()
