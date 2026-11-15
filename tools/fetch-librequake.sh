#!/bin/sh
# Fetch LibreQuake v0.09-beta (art, maps, sounds, music: BSD-3-Clause;
# game code: GPL-2.0-or-later) from its official GitHub release, check it
# against the SHA-256 we pinned, and unpack it into data/id1 with its
# licence files.  Nothing else from the archive is run.
set -eu
cd "$(dirname "$0")/.."
VER=v0.09-beta
URL=https://github.com/lavenderdotpet/LibreQuake/releases/download/$VER/full.zip
SUM=623e463b35811216244f9ba15e0c45abc1765288650b5e3d36a899b35e4bcf3d
mkdir -p data
if [ ! -f data/librequake-full.zip ] || ! echo "$SUM  data/librequake-full.zip" | sha256sum -c --quiet - 2>/dev/null; then
    curl -fL -o data/librequake-full.zip.part "$URL"
    mv data/librequake-full.zip.part data/librequake-full.zip
fi
echo "$SUM  data/librequake-full.zip" | sha256sum -c --quiet -
rm -rf data/unpack data/id1
mkdir -p data/unpack
unzip -q data/librequake-full.zip -d data/unpack
mv data/unpack/full/id1 data/id1
rm -rf data/unpack
# LibreQuake's own engine configs name other engines; ours are in data/aftershock.
rm -f data/id1/style_*.cfg data/id1/zoom*.cfg data/id1/dev.cfg
echo "LibreQuake $VER in data/id1"
