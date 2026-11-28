#!/usr/bin/env python3
"""Write game/aftershock/credits.txt, the text of the Credits screen.

usage: make-credits.py [LibreQuake CREDITS file] [out]
       (defaults: data/id1/docs/CREDITS, game/aftershock/credits.txt)

The head is written here; the LibreQuake team is read from LibreQuake's
own CREDITS file (fetched by tools/fetch-librequake.sh), names and what
each person made, nothing else from it (no e-mail, accounts or links).
Lines are at most 40 characters, the width of the menus; a line starting
with '#' is a heading. Run it again after LibreQuake is updated."""
import sys
import textwrap

WIDTH = 40

HEAD = """\
#Aftershock
A shooter in the terminal

#Engine
TyrQuake
by Kevin Shanahan and contributors,
from id Software's GPL source
release of Quake

The libretro tyrquake core
by the libretro team and contributors

#In the terminal
OmaQuake by Raul Dias
first brought this engine
to the terminal

The terminal frontend, pictures,
text HUD and menus, keyboard play
and family-friendly changes
by the Aftershock authors

#World
The LibreQuake project
maps, art, sounds and music,
and the game code

#The LibreQuake team
"""

TAIL = """
#Thanks
To everyone who kept this engine
and its worlds free to play,
and to you, for playing.
"""


def people(path):
    """The CREDITS file's records, as (name, contributions)."""
    out, rec = [], {}

    def flush():
        name = rec.get("N")
        screen = rec.get("S")
        if name == "RealName":          # the file's template
            return
        if name and screen and screen != name:
            shown = "%s (%s)" % (name, screen)
        else:
            shown = name or screen
        if shown:
            out.append((shown, rec.get("C", "")))

    for raw in open(path, encoding="utf-8", errors="replace"):
        line = raw.strip()
        if len(line) > 2 and line[1] == ":" and line[0].isalpha():
            key, val = line[0], line[2:].strip()
            if key == "N" and rec:
                flush()
                rec = {}
            elif key == "S" and "S" in rec:
                flush()
                rec = {}
            rec[key] = val
        elif not line and rec:
            flush()
            rec = {}
    if rec:
        flush()
    return out


def ascii_only(s):
    # The menus draw the engine's font: plain ASCII only.
    return s.encode("ascii", "replace").decode().replace("?", "")


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "data/id1/docs/CREDITS"
    dst = sys.argv[2] if len(sys.argv) > 2 else "game/aftershock/credits.txt"
    lines = HEAD.splitlines()
    for name, what in people(src):
        for part in textwrap.wrap(ascii_only(name), WIDTH):
            lines.append(part)
        what = ascii_only(what).strip()
        if what:
            # One case for the list: "Models, Textures" reads as
            # "models, textures"; names like QuakeC and README stay.
            what = " ".join(w.lower() if w[:1].isupper() and w[1:].islower()
                            else w for w in what.split())
            for part in textwrap.wrap(what, WIDTH):
                lines.append(part)
        lines.append("")
    lines += TAIL.splitlines()[1:]
    for l in lines:
        assert len(l.lstrip("#")) <= WIDTH, l
    with open(dst, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("%s: %d lines" % (dst, len(lines)))


main()
