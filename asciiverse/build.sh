#!/bin/sh
# Builds Aftershock for a release, on Linux (glibc; on the oldest runner,
# so it runs on glibc 2.35 and newer). Leaves build/aftershock and the game
# data in data/ (LibreQuake in data/id1, ours in data/aftershock).
#
# The binary needs only libc and libm: chafa and GLib are linked in from
# their static archives (asciiverse/static-libs.sh), libasound is loaded
# when sound starts (src/av_sound.c) and the game plays silently without
# it. No libcaca: its picture is for local builds.
#
# GMQCC names the compiler for the game code (tools/build-progs.sh looks
# for gmqcc on PATH); the release builds it from a pinned commit.
set -eu
cd "$(dirname "$0")/.."
sh tools/fetch-librequake.sh
sh tools/build-data.sh
make -j4 CHAFA_STATIC=1 CACA=0
make backends CACA=0
strip build/aftershock
