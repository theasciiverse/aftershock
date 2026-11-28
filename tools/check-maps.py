#!/usr/bin/env python3
"""Check LibreQuake's episode maps by machine: they load, run, and can be left.

usage: check-maps.py [--maps lq_e1m1,lq_e2m3,...] [--frames N] [--jobs N]
                     [--no-run] [--report FILE]

For every map of episodes 1 to 4 (from data/id1's paks):

1. Static: the map file parses (BSP29, "BSP2" or "2PSB"), has a player
   start and at least one exit (trigger_changelevel), and every exit names
   a map that exists in the game data.
2. Reachable: a flood fill of the player-sized hull (hull 1) from the start
   on a 16-unit grid reaches each exit's touch box. It ignores gravity,
   doors and lifts (doors are their own models, so their openings are
   open here), so it proves the exit is not sealed off from the start,
   not that a person can walk there.
3. Runs: the game loads the map (god mode, monsters blind with notarget)
   and plays it for a while; the session log must show the level running,
   no engine or game code error, and the program must end cleanly.
4. Leaves: for each exit, the player is put in its touch box with
   `setpos` while holding fire; the intermission must start and the next
   map must load.

Everything runs as CLAUDE.md asks: --no-sound, a scratch XDG_STATE_HOME
and HOME, a real pty (script), inside a 4G memory scope. The report is
plain text, one block per map.
"""
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PAKS = [os.path.join(ROOT, "data/id1/pak0.pak"),
        os.path.join(ROOT, "data/id1/pak1.pak")]
EPISODES = ["lq_e%dm%d" % (e, m) for e in range(1, 5) for m in range(1, 9)]

CONTENTS_SOLID = -2
CONTENTS_SKY = -6
HULL1_MINS = np.array([-16.0, -16.0, -24.0])
HULL1_MAXS = np.array([16.0, 16.0, 32.0])
GRID = 16.0


# ---- game data ------------------------------------------------------------

def read_paks(paths):
    files = {}
    for p in paths:
        with open(p, "rb") as f:
            magic, off, ln = struct.unpack("<4sii", f.read(12))
            f.seek(off)
            d = f.read(ln)
            for i in range(0, ln, 64):
                name = d[i:i + 56].split(b"\0")[0].decode("latin-1")
                o, l = struct.unpack("<ii", d[i + 56:i + 64])
                files[name] = (p, o, l)
    return files


def pak_file(files, name):
    p, o, l = files[name]
    with open(p, "rb") as f:
        f.seek(o)
        return f.read(l)


# ---- BSP ------------------------------------------------------------------

class Bsp:
    def __init__(self, data):
        self.data = data
        magic = data[:4]
        if magic == b"BSP2":
            self.kind, wide = "BSP2", True
        elif magic == b"2PSB":
            self.kind, wide = "2PSB", True
        else:
            (ver,) = struct.unpack("<i", magic)
            if ver != 29:
                raise ValueError("unknown BSP version %r" % magic)
            self.kind, wide = "BSP29", False
        self.lumps = [struct.unpack("<ii", data[4 + 8 * i:12 + 8 * i])
                      for i in range(15)]
        self.entities = parse_entities(self.lump(0).split(b"\0")[0]
                                       .decode("latin-1"))
        planes = np.frombuffer(self.lump(1), np.dtype(
            [("n", "<f4", 3), ("d", "<f4"), ("t", "<i4")]))
        self.pn = planes["n"].astype(np.float64)
        self.pd = planes["d"].astype(np.float64)
        if wide:
            cn = np.frombuffer(self.lump(9), np.dtype(
                [("p", "<i4"), ("c", "<i4", 2)]))
            self.cplane = cn["p"].astype(np.int64)
            self.cchild = cn["c"].astype(np.int64)
        else:
            cn = np.frombuffer(self.lump(9), np.dtype(
                [("p", "<i4"), ("c", "<u2", 2)]))
            self.cplane = cn["p"].astype(np.int64)
            c = cn["c"].astype(np.int64)
            # As the engine reads them: past the last clipnode means contents.
            self.cchild = np.where(c > 0xfff0, c - 0x10000, c)
        models = np.frombuffer(self.lump(14), np.dtype(
            [("mins", "<f4", 3), ("maxs", "<f4", 3), ("org", "<f4", 3),
             ("head", "<i4", 4), ("vis", "<i4"), ("ff", "<i4"),
             ("nf", "<i4")]))
        self.models = models

    def lump(self, i):
        o, l = self.lumps[i]
        return self.data[o:o + l]

    def hull1_contents(self, pts):
        """Contents of hull 1 (the player's box) at each point."""
        node = np.full(len(pts), int(self.models["head"][0][1]), np.int64)
        out = np.zeros(len(pts), np.int64)
        live = np.arange(len(pts))
        for _ in range(4096):
            if not len(live):
                break
            n = node[live]
            p = self.cplane[n]
            d = (pts[live] * self.pn[p]).sum(1) - self.pd[p]
            child = np.where(d < 0, self.cchild[n, 1], self.cchild[n, 0])
            done = child < 0
            out[live[done]] = child[done]
            node[live[~done]] = child[~done]
            live = live[~done]
        return out


def parse_entities(text):
    ents, cur = [], None
    for m in re.finditer(r'"([^"]*)"|([{}])', text):
        if m.group(2) == "{":
            cur, key = {}, None
        elif m.group(2) == "}":
            ents.append(cur)
            cur = None
        elif cur is not None:
            if key is None:
                key = m.group(1)
            else:
                cur[key] = m.group(1)
                key = None
    return ents


def vec(s):
    return np.array([float(x) for x in s.split()[:3]])


def reach(bsp, start):
    """Flood fill of hull 1 from start, through teleporters; returns the
    grid's origin and the reached cells (None if the start is in solid)."""
    wmins = bsp.models["mins"][0].astype(np.float64) - 32
    wmaxs = bsp.models["maxs"][0].astype(np.float64) + 32
    # The grid goes through the start point, so the start is a cell.
    lo = start - np.ceil((start - wmins) / GRID) * GRID
    shape = tuple(int(x) for x in np.ceil((wmaxs - lo) / GRID) + 1)
    free = np.zeros(shape, bool)
    xs = lo[0] + GRID * np.arange(shape[0])
    ys = lo[1] + GRID * np.arange(shape[1])
    for k in range(shape[2]):
        z = lo[2] + GRID * k
        gx, gy = np.meshgrid(xs, ys, indexing="ij")
        pts = np.stack([gx.ravel(), gy.ravel(),
                        np.full(gx.size, z)], 1)
        c = bsp.hull1_contents(pts)
        free[:, :, k] = ((c != CONTENTS_SOLID) &
                         (c != CONTENTS_SKY)).reshape(gx.shape)
    from scipy import ndimage
    labels, _ = ndimage.label(free)

    def label_at(p):
        """The free region at p, or at the nearest free cell close by."""
        i = np.round((p - lo) / GRID).astype(int)
        for r in range(3):
            sl = tuple(slice(max(i[a] - r, 0), i[a] + r + 1) for a in range(3))
            sub = labels[sl]
            if sub.size and sub.max() > 0:
                return int(sub[sub > 0][0])
        return 0

    first = label_at(start)
    if not first or not free[tuple(int(round(x)) for x in (start - lo) / GRID)]:
        return lo, None
    # Teleporters join regions: a teleporter whose touch box lies in a
    # reached region brings its destination's region in too.
    dests = {}
    for e in bsp.entities:
        if e.get("classname") == "info_teleport_destination" and \
                "targetname" in e and "origin" in e:
            # The game raises the destination 27 units (triggers.qc).
            dests[e["targetname"]] = vec(e["origin"]) + [0, 0, 27]
    teles = [(touch_box(bsp, e), dests.get(e.get("target")))
             for e in bsp.entities
             if e.get("classname") == "trigger_teleport" and
             e.get("model", "").startswith("*") and
             not int(e.get("spawnflags", "0") or 0) & 512]
    have = {first}
    grew = True
    while grew:
        grew = False
        mask = np.isin(labels, list(have))
        for box, dest in teles:
            if dest is None:
                continue
            if reached_point(lo, mask, box) is None:
                continue
            l = label_at(dest)
            if l and l not in have:
                have.add(l)
                grew = True
    return lo, np.isin(labels, list(have))


def touch_box(bsp, ent):
    """The player origins whose box touches this trigger's box."""
    m = int(ent["model"][1:])
    tmins = bsp.models["mins"][m].astype(np.float64)
    tmaxs = bsp.models["maxs"][m].astype(np.float64)
    if "origin" in ent:
        tmins = tmins + vec(ent["origin"])
        tmaxs = tmaxs + vec(ent["origin"])
    return tmins - HULL1_MAXS, tmaxs - HULL1_MINS


def reached_point(lo, region, box):
    """A reached grid point inside box, nearest its centre, or None."""
    if region is None:
        return None
    bmin, bmax = box
    i0 = np.maximum(np.ceil((bmin - lo) / GRID).astype(int), 0)
    i1 = np.minimum(np.floor((bmax - lo) / GRID).astype(int),
                    np.array(region.shape) - 1)
    if (i1 < i0).any():
        return None
    sub = region[i0[0]:i1[0] + 1, i0[1]:i1[1] + 1, i0[2]:i1[2] + 1]
    idx = np.argwhere(sub)
    if not len(idx):
        return None
    pts = lo + GRID * (idx + i0)
    centre = (bmin + bmax) / 2
    return pts[np.argmin(((pts - centre) ** 2).sum(1))]


def static_check(files, name):
    r = {"map": name, "problems": [], "exits": []}
    data = pak_file(files, "maps/%s.bsp" % name)
    bsp = Bsp(data)
    r["format"] = bsp.kind
    world = bsp.entities[0]
    r["title"] = world.get("message", "").replace("\\n", " ").strip()
    # LibreQuake v0.09-beta stands in for unfinished levels with a short
    # untitled corridor leading straight on (a dozen entities, no monsters).
    r["placeholder"] = (not r["title"] and len(bsp.entities) <= 15 and
                        not any(e.get("classname", "").startswith("monster_")
                                for e in bsp.entities))
    starts = [e for e in bsp.entities
              if e.get("classname") == "info_player_start"]
    if not starts:
        r["problems"].append("no info_player_start")
        return r, bsp, None
    start = vec(starts[0]["origin"])
    r["start"] = start
    for e in bsp.entities:
        if e.get("classname") != "trigger_changelevel":
            continue
        ex = {"to": e.get("map", ""), "flags": int(e.get("spawnflags", "0")
                                                    or 0)}
        if not ex["to"]:
            r["problems"].append("an exit names no map")
        elif ex["to"] != "start" and "maps/%s.bsp" % ex["to"] not in files:
            r["problems"].append("exit to %s, which is not in the data"
                                 % ex["to"])
        ex["box"] = touch_box(bsp, e)
        # Spawnflag 512 (not on Normal) removes it in single player at the
        # skill the checks run at; such exits are for deathmatch.
        ex["solo"] = not (ex["flags"] & 512)
        r["exits"].append(ex)
    if not any(ex["solo"] for ex in r["exits"]):
        r["problems"].append("no exit in single player (trigger_changelevel)")
    return r, bsp, start


# ---- running the game -----------------------------------------------------

def run_game(cmd, ingame, frames, label, keep_dir):
    """Run the game once; returns (exit status, log text)."""
    scratch = tempfile.mkdtemp(prefix="aftershock-check-")
    log = os.path.join(scratch, "session.log")
    env = dict(os.environ)
    env.update({
        "XDG_STATE_HOME": os.path.join(scratch, "state"),
        "HOME": os.path.join(scratch, "home"),
        "AFTERSHOCK_CMD": cmd,
        "AFTERSHOCK_CMD_INGAME": ingame,
        "AFTERSHOCK_ALSA_DEVICE": "null",
    })
    os.makedirs(env["XDG_STATE_HOME"])
    os.makedirs(env["HOME"])
    inner = ("stty cols 100 rows 30; exec %s --no-sound --no-mouse --no-pad "
             "--fps=0 --frames=%d --log=%s"
             % (os.path.join(ROOT, "build/aftershock"), frames, log))
    argv = ["systemd-run", "--user", "--scope", "-q", "-p", "MemoryMax=4G",
            "script", "-qe", "-c", inner, "/dev/null"]
    try:
        p = subprocess.run(argv, env=env, cwd=ROOT, stdin=subprocess.DEVNULL,
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=600)
        status = p.returncode
    except subprocess.TimeoutExpired:
        status = "timeout"
    text = open(log, encoding="utf-8", errors="replace").read() \
        if os.path.exists(log) else ""
    if keep_dir:
        shutil.copy(log, os.path.join(keep_dir, label + ".log")) \
            if os.path.exists(log) else None
    shutil.rmtree(scratch, ignore_errors=True)
    return status, text


ERROR_RE = re.compile(r"Host_Error|SV_Error|Sys_Error|PR_RunError|"
                      r"runaway loop|stack overflow|Bad entity|"
                      r"SZ_GetSpace|ED_Alloc: no free|Hunk_Alloc|"
                      r"error:|Error:|ERROR", re.I)
WARN_RE = re.compile(r"^\[\d\] (Couldn't load .*|Bad surface extents.*|"
                     r".*objerror.*|.*no free edicts.*|Mod_.*|"
                     r"Missing .*|.*not found.*|.*can't find (?!music/).*|"
                     r".* is not a field)$", re.M)
STATE_RE = re.compile(r'^state: level "([^"]*)" game=(\d+) .*intermission=(\d)'
                      r'.* health=(-?\d+)', re.M)


def states(text):
    out = []
    for line in text.splitlines():
        if not line.startswith("state:"):
            continue
        m = re.search(r'level "([^"]*)" game=(\d+)', line)
        im = re.search(r"intermission=(\d)", line)
        hm = re.search(r"health=(-?\d+)", line)
        out.append((m.group(1), int(m.group(2)), int(im.group(1)),
                    int(hm.group(1))))
    return out


def errors(text):
    found = []
    for line in text.splitlines():
        if line.startswith("state:"):
            continue
        # [3] is the engine's error level.
        if (line.startswith("[3]") or ERROR_RE.search(line)) and \
                "couldn't exec" not in line:
            found.append(line.strip())
    return found


def run_check(r, frames, keep_dir):
    name = r["map"]
    status, text = run_game("skill 1; map " + name, "god; notarget", frames,
                            name + "-run", keep_dir)
    st = states(text)
    levels = [s for s in st if s[1] == 1]
    r["run_status"] = status
    r["run_errors"] = errors(text)
    r["run_warnings"] = sorted(set(WARN_RE.findall(text)))
    r["run_seconds"] = len(levels)
    r["level_name"] = levels[-1][0] if levels else ""
    if status != 0:
        r["problems"].append("the program ended with %s" % status)
    if not levels:
        r["problems"].append("the level never ran")
    if r["run_errors"]:
        r["problems"].append("errors in the log")


def exit_check(r, ex, i, frames, keep_dir):
    p = ex.get("point")
    if p is None:
        bmin, bmax = ex["box"]
        p = (bmin + bmax) / 2
    pos = " ".join("%.0f" % v for v in p)
    status, text = run_game("skill 1; map " + r["map"],
                            "god; notarget; +attack; setpos " + pos,
                            frames, "%s-exit%d" % (r["map"], i), keep_dir)
    st = states(text)
    names = []
    for s in st:
        if s[1] == 1 and (not names or names[-1] != s[0]):
            names.append(s[0])
    ex["intermission"] = any(s[2] for s in st)
    ex["levels_seen"] = names
    ex["status"] = status
    ex["errors"] = errors(text)
    ex["left"] = text.count("Using protocol") >= 2
    if not ex["left"]:
        r["problems"].append("exit to %s did not load the next map"
                             % ex["to"])
    if ex["errors"]:
        r["problems"].append("errors while leaving to %s" % ex["to"])


def check_map(files, name, args):
    try:
        r, bsp, start = static_check(files, name)
    except Exception as e:      # a map that does not even parse
        return {"map": name, "problems": ["does not parse: %s" % e],
                "exits": []}
    if start is not None and r["exits"]:
        lo, region = reach(bsp, start)
        r["reach_cells"] = int(region.sum()) if region is not None else 0
        if region is None:
            r["problems"].append("the start is inside solid (hull 1)")
        for ex in r["exits"]:
            ex["point"] = reached_point(lo, region, ex["box"])
            ex["reached"] = ex["point"] is not None
            if not ex["reached"] and ex["solo"]:
                r["problems"].append("exit to %s not reached from the start"
                                     % ex["to"])
    if not args.no_run:
        run_check(r, args.frames, args.keep_logs)
        for i, ex in enumerate(r["exits"]):
            if ex["solo"]:
                exit_check(r, ex, i, args.exit_frames, args.keep_logs)
    return r


def report(r):
    out = []
    ok = not r["problems"]
    out.append("%s  %s  %s" % (r["map"], "ok" if ok else "PROBLEMS",
                               r.get("title", "")))
    if r.get("placeholder"):
        out.append("  LibreQuake's placeholder for an unfinished level: an "
                   "untitled corridor on to the next")
    if "format" in r:
        out.append("  format %s, %d exit(s), %s reachable cells"
                   % (r["format"], len(r["exits"]), r.get("reach_cells", "?")))
    if "run_seconds" in r:
        out.append("  ran %d s as \"%s\", exit status %s"
                   % (r["run_seconds"], r["level_name"], r["run_status"]))
    for ex in r["exits"]:
        if not ex["solo"]:
            out.append("  exit to %s: deathmatch only, not checked" % ex["to"])
            continue
        line = "  exit to %s: %s" % (
            ex["to"], "reached" if ex.get("reached") else "NOT reached")
        if "left" in ex:
            line += ", %s, %s" % (
                "intermission" if ex["intermission"] else "no intermission",
                "next level loaded" if ex["left"] else "NEXT LEVEL NOT LOADED")
            if len(ex["levels_seen"]) > 1:
                line += " (\"%s\")" % ex["levels_seen"][-1]
        out.append(line)
    for w in r.get("run_warnings", []):
        out.append("  warning: %s" % w)
    for e in r.get("run_errors", []):
        out.append("  error: %s" % e)
    for ex in r["exits"]:
        for e in ex.get("errors", []):
            out.append("  error leaving to %s: %s" % (ex["to"], e))
    for p in r["problems"]:
        out.append("  PROBLEM: %s" % p)
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--maps")
    ap.add_argument("--frames", type=int, default=1800,
                    help="frames to run each map (60 a second)")
    ap.add_argument("--exit-frames", type=int, default=900)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--no-run", action="store_true")
    ap.add_argument("--keep-logs", help="folder for the session logs")
    ap.add_argument("--report")
    args = ap.parse_args()
    if args.keep_logs:
        os.makedirs(args.keep_logs, exist_ok=True)
    files = read_paks(PAKS)
    names = args.maps.split(",") if args.maps else \
        [n for n in EPISODES if "maps/%s.bsp" % n in files]
    with ThreadPoolExecutor(args.jobs) as pool:
        results = list(pool.map(lambda n: check_map(files, n, args), names))
    text = "\n\n".join(report(r) for r in results)
    bad = [r["map"] for r in results if r["problems"]]
    text += "\n\n%d maps, %d with problems%s\n" % (
        len(results), len(bad), (": " + ", ".join(bad)) if bad else "")
    if args.report:
        open(args.report, "w").write(text)
    print(text)
    return 1 if bad else 0


sys.exit(main())
