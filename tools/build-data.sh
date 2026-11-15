#!/bin/sh
# Build data/aftershock/pak0.pak from game/aftershock (our files over
# LibreQuake). Run tools/fetch-librequake.sh first for data/id1.
set -eu
cd "$(dirname "$0")/.."
python3 -I tools/mkpak.py game/aftershock data/aftershock/pak0.pak
