#!/usr/bin/env python3
"""Turn LibreQuake's FTEQCC progs.src (#includelist, #ifdef __QW__) into the
plain list gmqcc reads: the output name, then one file per line, for the
single-player (not __QW__) build.

usage: progs-list.py qcsrc/progs.src PREFIX"""
import sys

src, prefix = sys.argv[1], sys.argv[2]
files, stack = [], []
for line in open(src):
    l = line.split("//")[0].strip()
    if not l:
        continue
    if l.startswith("#ifdef __QW__"):
        stack.append(False)
    elif l.startswith("#ifndef __QW__"):
        stack.append(True)
    elif l.startswith("#else"):
        stack[-1] = not stack[-1]
    elif l.startswith("#endif"):
        stack.pop()
    elif l.startswith("#"):
        continue
    elif all(stack):
        files.append(prefix + l)
print("progs.dat")
print("\n".join(files))
