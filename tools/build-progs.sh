#!/bin/sh
# Compile the game code (qcsrc/, LibreQuake's LibreProgsDeluxe with our
# changes) into build/qc/progs.dat with gmqcc (0.3.6 tested; Debian and
# Ubuntu package it as gmqcc). The file list is read from qcsrc/progs.src,
# LibreQuake's own (FTEQCC's #includelist form), for the single-player
# build.
set -eu
cd "$(dirname "$0")/.."
command -v gmqcc >/dev/null || { echo "gmqcc not found (apt install gmqcc)" >&2; exit 1; }
mkdir -p build/qc
python3 -I tools/progs-list.py qcsrc/progs.src ../../qcsrc/ > build/qc/progs.src
cd build/qc
gmqcc -std=fteqcc -funtyped-nil -fassign-function-types \
      -O2 -Wno-unused-variable > gmqcc.log 2>&1 || { cat gmqcc.log >&2; exit 1; }
python3 -I ../../tools/progs-pad.py progs.dat
grep -E "warning|error" gmqcc.log | grep -v "unused" >&2 || true
echo "build/qc/progs.dat: $(wc -c < progs.dat) bytes"
