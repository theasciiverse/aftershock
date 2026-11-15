#!/usr/bin/env python3
"""Pack a folder into a Quake-format .pak (the format the engine reads).

usage: mkpak.py SRC_DIR OUT.pak
Files are stored under their path relative to SRC_DIR, sorted, so the same
input always gives the same bytes."""
import os, struct, sys

def main():
    src, out = sys.argv[1], sys.argv[2]
    files = []
    for root, _, names in os.walk(src):
        for n in names:
            full = os.path.join(root, n)
            rel = os.path.relpath(full, src).replace(os.sep, "/")
            if len(rel) > 55:
                sys.exit("name too long for a pak: " + rel)
            files.append((rel, full))
    files.sort()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out + ".part", "wb") as f:
        f.write(b"PACK" + b"\0" * 8)
        table = []
        for rel, full in files:
            data = open(full, "rb").read()
            table.append((rel, f.tell(), len(data)))
            f.write(data)
        diroff = f.tell()
        for rel, off, ln in table:
            f.write(rel.encode().ljust(56, b"\0") + struct.pack("<ii", off, ln))
        dirlen = f.tell() - diroff
        f.seek(4)
        f.write(struct.pack("<ii", diroff, dirlen))
    os.replace(out + ".part", out)
    print("%s: %d files" % (out, len(files)))

main()
