#!/bin/sh
# Lays out the av archive in stage/: the start script at the top, the
# binary in lib/, the game data in data/ (found by the binary from lib/),
# and the licences, credits and notes.
#
# Windows (build/aftershock.exe): no start script. av starts aftershock.exe
# from the top of the install folder, the program finds data/ beside
# itself and keeps its state and session log in %LOCALAPPDATA%\aftershock.
set -eu
cd "$(dirname "$0")/.."
if [ -f build/aftershock.exe ]; then
    rm -rf stage && mkdir -p stage/data stage/licenses
    cp build/aftershock.exe stage/aftershock.exe
else
    rm -rf stage && mkdir -p stage/lib stage/data stage/licenses
    cp asciiverse/aftershock stage/aftershock
    cp build/aftershock stage/lib/aftershock
fi
cp -r data/id1 data/aftershock stage/data/
cp README.md PATCHES.md UPSTREAM.md stage/
cp COPYING stage/licenses/GPL-2.0.txt
cp LICENSES/* asciiverse/licenses/* stage/licenses/
