#!/bin/sh
# Lays out the av archive in stage/: the start script at the top, the
# binary in lib/, the game data in data/ (found by the binary from lib/),
# and the licences, credits and notes.
set -eu
cd "$(dirname "$0")/.."
rm -rf stage && mkdir -p stage/lib stage/data stage/licenses
cp asciiverse/aftershock stage/aftershock
cp build/aftershock stage/lib/aftershock
cp -r data/id1 data/aftershock stage/data/
cp README.md PATCHES.md UPSTREAM.md stage/
cp COPYING stage/licenses/GPL-2.0.txt
cp LICENSES/* asciiverse/licenses/* stage/licenses/
