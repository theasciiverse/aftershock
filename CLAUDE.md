# Aftershock

A first-person shooter drawn in the terminal: the GPL engine of a 1996
shooter (TyrQuake, by way of libretro's tyrquake core and Raul Dias's
OmaQuake terminal frontend) filled only with free content, LibreQuake
(BSD-3-Clause art, maps, sounds and music; GPL-2.0+ game code). No id
Software data is needed, shipped or searched for, ever.

## OPEN BLOCKER: licence, waiting on Raul Dias

OmaQuake is GPL-2.0-only and links chafa (LGPL-3.0), which cannot be
distributed together. David is writing to Raul about it. Until that is
answered: **no tags, no releases, no release workflow runs**, and the repo
stays private. Local builds for testing are fine. Do not contact Raul or
anyone else about it. Our own half-block backend (`--video=half`, the
default, src/av_half.c) uses no library, so a build without chafa
(`make CHAFA=0`) is the one we could ship if the answer is no.

Local tags `v0.1.0` to `v0.1.2` (OmaQuake's) and `v0.47` on (TyrQuake's)
come from the upstream remotes: never `git push --tags`. Our own versions
are 0.x.0 and are not tagged while the blocker stands (ROADMAP.md says
what each one holds).

## How to play and test

    sh tools/fetch-librequake.sh && sh tools/build-data.sh && make -j4
    ./aftershock                     # half blocks, sound on; --video=sextant, --no-sound

Tests and recordings: a scratch `XDG_STATE_HOME` and `HOME` in the same
command, `--no-sound` (sound is on otherwise) and the 4G scope. `AFTERSHOCK_CMD`
('map lq_e0m1') and `AFTERSHOCK_CMD_INGAME` ('god') drive the console;
`--log` holds the console, every key and the player's state once a second.

## What it is made of

- `src/oq_*.c`: OmaQuake (Raul Dias, GPL-2.0-only), remote `upstream`,
  history kept. Our changes in them are marked `Aftershock:`.
- `src/av_*.c`: ours. `av_half.c` the half-block picture, `av_overlay.c`
  the text drawn over it (HUD, messages).
- `third_party/tyrquake`: libretro/tyrquake as a subtree, with our changes.
- `qcsrc/`: the game code, LibreQuake's LibreProgsDeluxe (GPL-2.0+) with
  our changes, compiled with gmqcc.
- `game/aftershock/`: our files over LibreQuake (configs, text table,
  debris models, colormap fix); `tools/build-data.sh` packs them with the
  compiled progs.dat into `data/aftershock/pak0.pak`.
- `data/` (not in git): `tools/fetch-librequake.sh` puts LibreQuake
  v0.09-beta (pinned SHA-256) in `data/id1`, `tools/build-data.sh` builds
  `data/aftershock`.
- The `tyrquake-base` branch holds the first start on plain desktop
  TyrQuake (before David switched the base to OmaQuake, 2026-11-15).

## Identity rules (David, 2026-11-15)

- The title is **Aftershock**. Never "Quake", id's logos, the Q symbol, id's
  monster, place or weapon names, or id's story texts in the title, menus,
  HUD, messages, endings or store text. Use LibreQuake's own names, or ours.
  Check LibreQuake's QuakeC and texts for leftover id names, including
  uncompiled branches, and replace anything that would ship.
- Credit plainly (credits screen, README, PATCHES.md, store draft): built on
  TyrQuake (Kevin Shanahan and contributors) from id Software's GPL source
  release, the libretro tyrquake core, OmaQuake (Raul Dias); content by the
  LibreQuake project. Naming Quake in a credits or history line is fine.
- Family-friendly by default: debris and sparks instead of blood and gibs,
  obituaries without "disemboweled" or "gutted". Optional switch for the
  original effects (`av_gore 1`), default off. No sexual content anywhere.
- No em-dashes or en-dashes in anything we write.

## Decisions (and conservative choices made while David was away)

- 2026-11-15: base switched from plain TyrQuake to OmaQuake (David).
- Sound and music are on by default (David, 2026-11-18; until then off
  unless asked for with `--sound`, which still works). `--no-sound` silences.
- Mouse: OmaQuake's behaviour is unchanged (evdev when readable, else the
  terminal's own reports). The game is fully playable on the keyboard.
- Keyboard-first bindings in `game/aftershock/default.cfg`; the core's own
  first-boot bind overrides were removed.
- The game code (qcsrc/) is ours, built with gmqcc 0.3.6 (David installed
  it 2026-11-16) by tools/build-progs.sh. Identity and family-friendly
  text lives there (localized_text.qc, aftershock.qc); av_text.txt is only
  for map and engine text. id's #else branches are removed.
- The text UI replaces menu pictures by name (av_textui.c, `piclabels`); a
  picture it does not know is drawn as before.
- Gamepad support (src/av_pad.c) was written without a pad to try it on.
- Saves and settings: `$XDG_STATE_HOME/aftershock` (else
  `~/.local/state/aftershock`). Tests and smoke runs always set a scratch
  `XDG_STATE_HOME` and `HOME` in the same command, and pass `--no-sound`.
- Every build, test or recording runs under
  `systemd-run --user --scope -p MemoryMax=4G` with `-j4`.

# Rules inherited from OmaQuake (still true here)

## stdout is the picture

This program's stdout **is** the rendered game frame, written as raw escape
sequences. Never `printf`/`puts`/write anything to stdout from any code path
that can run while the terminal is in presentation mode: one stray log line
corrupts the frame mid-escape-sequence. Diagnostics go to `--log=PATH`
(`oq_retro.c`'s `log_printf`) or to `stderr` only *after* `oq_term_shutdown()`
has restored the terminal. Same rule applies to anything ALSA might try to
print: see the silent error handler in `src/oq_audio.c`; do not remove it.

## Never run the game without silencing audio

Any command that launches `omaquake` in a non-interactive context (tests,
benchmarking, CI, this agent's own tool calls) **must** pass `--no-sound` or
set `OMAQUAKE_ALSA_DEVICE=null`. Without one of those it opens the default
ALSA device and plays sound out of the user's speakers.

## The binary requires a real tty

`oq_term_init()` refuses to start unless both stdin and stdout are ttys.
Test it via a pty, not by piping:

    script -qe -c "stty cols 200 rows 50; ./build/omaquake --demo --frames=60" /dev/null

**`COLUMNS`/`LINES` environment variables do not resize a pty.** Only `stty`
(or an ioctl) does. Forgetting this silently benchmarks/tests an 80x24
terminal regardless of what `COLUMNS`/`LINES` say, which has produced
misleading numbers before: always set size with `stty` inside the `script`
command, and confirm by checking what the program itself reports.

## Threading: writes to the terminal happen on the render thread only

`src/oq_render.c` is the only place that should be writing frame data to
stdout, from `render_loop()`. The engine thread (`retro_run()`,
`oq_retro.c`, `oq_main.c`'s game loop) must never write to the terminal
directly: two threads writing to the same fd can interleave mid escape
sequence and corrupt the frame. Even the clear-on-resize goes through
`oq_render_reconfigure()` so it executes on the render thread, not inline
in `video_sink()`.

## Building and checking what got compiled in

    git submodule update --init   # third_party/tyrquake
    make engine                    # builds the static libretro core
    make                            # builds ./build/omaquake
    make backends                   # reports which of chafa/caca/alsa were found

chafa, libcaca and ALSA are all optional and detected via `pkg-config`.
`make backends` is the source of truth for what's actually linked in: a
missing pkg-config file silently drops a backend, it does not fail the
build. Verify what's compiled in before assuming a feature (or its absence)
is real.

## third_party/tyrquake is ours to change (Aftershock)

OmaQuake kept the engine as an untouched submodule. Aftershock needs engine
changes (key handling, the text HUD, family-friendly effects), so it is a git
subtree of libretro/tyrquake (remote `libretro-tyrquake`, base commit in
UPSTREAM.md). Every change there is listed in PATCHES.md and marked
`Aftershock:` in a comment.

## Code style

- 4-space indentation, K&R brace style.
- Must compile clean under `-Wall -Wextra -std=gnu99` (see `Makefile`
  `CFLAGS`): no new warnings.
- Comments explain *why*, not *what*: see any of `src/oq_*.c` for the
  house style. Non-obvious constraints (threading, protocol quirks, ALSA
  timing) get a comment at the point a future change would plausibly get
  it wrong.

## This IS a git repository

The session's environment banner may claim otherwise: it is captured before
`git init` ran and is stale. `git log` works and the history is worth reading:
the commit messages carry the reasoning and the measured numbers behind most
of the non-obvious code here, and several of them document bugs that are easy
to reintroduce. Check with `git rev-parse --is-inside-work-tree` rather than
trusting the banner. (An agent has already been caught by this and skipped the
history entirely.)

## Verify, don't assume

Several bugs in this project's history survived review because the output
*looked* plausible (a frame that renders but is silently the wrong colours,
a keybind that's silently dropped, a canvas that's silently capped to
80x24). Prefer running the actual binary: via the `script`/`stty` pattern
above: over reasoning from source alone when a change touches terminal
I/O, input decoding, or timing.
