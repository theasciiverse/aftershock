#!/usr/bin/env python3
"""Pad a progs.dat's globals with a few zeros.

gmqcc places a function's local vector in the last two global slots, so its
third component falls one past the end; the engine (rightly) rejects
statements that reach past the globals. Three spare globals at the end make
every such vector fit. Only the globals lump is touched; it is moved to the
end of the file if it is not there already.

usage: progs-pad.py progs.dat"""
import struct, sys

p = sys.argv[1]
d = bytearray(open(p, "rb").read())
h = list(struct.unpack("<15i", d[:60]))
ofs_globals, num_globals = h[12], h[13]
glob = d[ofs_globals:ofs_globals + num_globals * 4] + bytes(12)
if ofs_globals + num_globals * 4 == len(d):
    d = d[:ofs_globals] + glob
else:
    h[12] = len(d)
    d = d + glob
h[13] = num_globals + 3
d[:60] = struct.pack("<15i", *h)
open(p, "wb").write(d)
print("%s: globals %d -> %d" % (p, num_globals, num_globals + 3))
