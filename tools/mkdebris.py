#!/usr/bin/env python3
"""Write small stone-debris models in the engine's .mdl format.

Aftershock throws debris where the original throws gibs and heads.  Each
model is an irregular stone (a jittered icosahedron) with a grey and brown
stone skin from the palette's grey and brown ramps, and the smoke-trail flag,
so flying debris leaves dust instead of a red trail.

usage: mkdebris.py OUTDIR"""
import math, os, random, struct, sys

EF_GRENADE = 2  # smoke trail

def icosahedron():
    t = (1 + 5 ** 0.5) / 2
    v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0),
         (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
         (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
         (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
         (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    return v, f

def stone(seed, radius, squash):
    rnd = random.Random(seed)
    v, f = icosahedron()
    out = []
    for x, y, z in v:
        n = math.sqrt(x * x + y * y + z * z)
        r = radius * (0.65 + 0.5 * rnd.random())
        out.append((x / n * r, y / n * r, z / n * r * squash))
    return out, f

def skin(seed, w=16, h=16):
    rnd = random.Random(seed)
    # palette 0..15 is the grey ramp, 16..31 a brown ramp
    px = bytearray()
    for i in range(w * h):
        if rnd.random() < 0.7:
            px.append(5 + rnd.randrange(6))
        else:
            px.append(19 + rnd.randrange(6))
    return bytes(px)

def write_mdl(path, verts, faces, seed):
    sw = sh = 16
    mins = [min(v[i] for v in verts) for i in range(3)]
    maxs = [max(v[i] for v in verts) for i in range(3)]
    scale = [(maxs[i] - mins[i]) / 255.0 or 1.0 for i in range(3)]
    radius = max(math.sqrt(x * x + y * y + z * z) for x, y, z in verts)
    hdr = b"IDPO" + struct.pack("<i", 6)
    hdr += struct.pack("<3f", *scale) + struct.pack("<3f", *mins)
    hdr += struct.pack("<f", radius) + struct.pack("<3f", 0, 0, 0)
    hdr += struct.pack("<iiiiiiii", 1, sw, sh, len(verts), len(faces), 1,
                       0, EF_GRENADE)
    hdr += struct.pack("<f", radius)
    body = struct.pack("<i", 0) + skin(seed, sw, sh)
    # one texture coordinate per vertex, spread over the skin
    rnd = random.Random(seed + 1)
    for _ in verts:
        body += struct.pack("<iii", 0, rnd.randrange(sw), rnd.randrange(sh))
    for a, b, c in faces:
        body += struct.pack("<iiii", 1, a, b, c)
    # one frame
    q = [tuple(int(round((v[i] - mins[i]) / scale[i])) for i in range(3))
         for v in verts]
    body += struct.pack("<i", 0)
    body += bytes([0, 0, 0, 0, 255, 255, 255, 0])
    body += b"debris".ljust(16, b"\0")
    for x, y, z in q:
        body += bytes([x, y, z, 0])
    with open(path, "wb") as f:
        f.write(hdr + body)

def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    # small chunks for flying debris, a bigger pile for what a body leaves
    for i, (r, sq) in enumerate([(3.0, 0.8), (4.0, 0.7), (5.0, 0.9)]):
        v, f = stone(10 + i, r, sq)
        write_mdl(os.path.join(out, "av_rubble%d.mdl" % (i + 1)), v, f, 10 + i)
    v, f = stone(42, 9.0, 0.5)
    write_mdl(os.path.join(out, "av_pile.mdl"), v, f, 42)
    print("debris models in", out)

main()
