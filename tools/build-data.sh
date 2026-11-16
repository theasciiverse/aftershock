#!/bin/sh
# Build data/aftershock/pak0.pak: our files over LibreQuake (game/aftershock)
# plus the game code compiled from qcsrc/ (tools/build-progs.sh).
# Run tools/fetch-librequake.sh first for data/id1.
set -eu
cd "$(dirname "$0")/.."
sh tools/build-progs.sh
rm -rf build/pak
mkdir -p build/pak
cp -r game/aftershock/. build/pak/
cp build/qc/progs.dat build/pak/progs.dat
python3 -I tools/mkpak.py build/pak data/aftershock/pak0.pak
