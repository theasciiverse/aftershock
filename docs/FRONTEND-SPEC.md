# Aftershock frontend: behaviour spec for the rewrite

Written 2026-11-27 for a rewrite of the terminal frontend from this spec
alone. It describes what the frontend does, how it must behave and why,
and the facts learnt the hard way (timings, terminal quirks, engine
quirks). It contains no code from the current files.

## Why

The current frontend is OmaQuake's (Raul Dias, GPL-2.0-only by its
README): the twelve `src/oq_*.c` files with their headers, the `Makefile`
it started from, `tests/oq_evdev_test.c` and `docs/DESIGN.md`. A
GPL-2.0-only program cannot be shipped linked with chafa (LGPL-3.0+), and
Raul has not answered. The engine (`third_party/tyrquake`, GPL-2.0 "or any
later version"), LibreQuake's game code (GPL-2.0+) and our own `av_*.c`
files all allow GPL-3.0. Once the OmaQuake files are replaced by our own,
Aftershock is ours to license as GPL-2.0-or-later, and a build with chafa
linked is distributed under GPL-3.0-or-later.

## Clean-room rule

Whoever writes the new code works from this spec, the libretro API
(`third_party/tyrquake/libretro-common/include/libretro.h`), the engine
sources, our own `src/av_*.c` files, and public documentation (kitty
keyboard protocol, xterm control sequences, Linux input and ALSA docs,
chafa and libcaca API docs). They do **not** open the OmaQuake files
listed above, `docs/OmaQuake-README.md`, or the diffs of Raul Dias's
commits (`git log --author=Raul`, `git show` on them). Commit messages
are fine to read; this spec already carries what they say.

Our own changes inside the `oq_*` files (marked `Aftershock:`) are
described here as behaviour; rewrite them too rather than copying.

## Layout of the new frontend

Suggested names (adjust if something reads better, keep the `av_`
prefix): `av_main.c` (command line, modes, game loop), `av_term.c` (the
terminal), `av_keys.c` (bytes to key and pointer events), `av_look.c`
(pointer motion to view motion), `av_evdev.c` (raw pointer from
/dev/input), `av_host.c` (libretro host), `av_render.c` (presentation
thread), `av_pictures.c` (backend table and the Picture setting),
`av_sound.c` (ALSA), `av_data.c` (data search), `av_chafa.c`,
`av_caca.c`, each with a header where needed. Our existing `av_half.c`,
`av_overlay.c` and `av_pad.c` move to the new interfaces (they call
today's `oq_term_write_raw`, `oq_retro_log`, `oq_retro_key`,
`oq_retro_mouse_move` and implement the backend struct).

Engine hooks that stay as they are (in `third_party/tyrquake/common`):
`aftershock_text_input()` (nonzero while a menu, the console or a message
line has the keyboard), `aftershock_textui()` (the text record, including
the current `picture`), `aftershock_picture_arg` and `aftershock_pictures`.

New files carry a short header naming the file's purpose and
`SPDX-License-Identifier: GPL-2.0-or-later`. Build clean under
`-Wall -Wextra -std=gnu99`. House style from `CLAUDE.md`: 4 spaces, K&R,
comments say why.

## Hard rules (unchanged)

- **stdout is the picture.** Nothing may print to stdout or stderr while
  the terminal is in game mode. Diagnostics go to the `--log` file, or to
  stderr only after the terminal is restored. This includes ALSA (give it
  a silent error handler) and the engine's own log (routed to the file).
- **One thread writes to the terminal**: the render thread, once it is
  running. Resize clears and backend switches happen there too. Anything
  that writes to the terminal outside it (setup, pointer mode switches)
  happens before the render thread starts or after it stops.
- **Every exit path restores the terminal and releases a grabbed pointer**:
  normal exit, `atexit`, SIGINT and SIGTERM (which only set a quit flag
  the loop checks). A grabbed mouse left on is the user's whole desktop
  pointer frozen.

## Command line

Usage line: `aftershock [options] [data folder holding aftershock/ and id1/]`.
A positional argument is the data folder (or a pak file). Unknown options
print the usage to stderr and exit 2. `--help` prints it and exits 0.

| Option | Meaning | Default |
|---|---|---|
| `--video=NAME` | backend by name: `half`, `sextant`, `braille`, `chafa`, `caca` (those compiled in); naming one also sets the Picture setting | the saved Picture setting, else `half` |
| `--symbols=ascii\|block\|fine` | glyph set, for chafa and caca | `fine` |
| `--color=mono\|16\|256\|true` | colour depth | `true` |
| `--demo` | test pattern instead of the game | |
| `--find-pak` | print the data path a normal run would load and exit 0; on failure the usual "looked in" report on stderr and exit 1. Runs before any terminal setup | |
| `--keytest` | input diagnostics screen | |
| `--frames=N` | stop after N frames; 0 means no limit (demo defaults to 600) | 0 |
| `--cell=WxH` | cell size in pixels, used when the terminal does not report one | 10x20 |
| `--res=WxH\|auto` | engine resolution | auto |
| `--fps=N` | presentation cap, 0 presents every frame | 30 |
| `--cells=WxH` | cap the canvas size in cells, 0x0 fills the terminal | fill |
| `--log=PATH` | session log | none |
| `--sound` / `--no-sound` | sound on (default) / the audio device is never opened | on |
| `--no-pad` | do not open a gamepad | |
| `--mouse=auto\|evdev\|term\|none` | pointer source | auto |
| `--mouse-dev=PATH` | evdev node to use instead of searching | |
| `--mouse-list` | list /dev/input/event* with a verdict each, exit 0 (plain stdout, before terminal setup) | |
| `--no-mouse` | same as `--mouse=none` | |
| `--mouse-sens=F` | pointer pixels (or device counts) to engine counts | 2.0 |
| `--mouse-edge=F` | steering band, fraction of each side; 0 turns it off | 0.15 |
| `--mouse-turn=F` | steering rate at the very edge, degrees per second | 220 |
| `--mouse-invert` | invert pitch | |

A bad `--symbols`, `--color`, `--mouse`, `--cells` or `--cell` value is a
usage error (exit 2). An unknown `--video` name prints
`aftershock: no such video backend 'X' (have: a,b,c)` and exits 2.

On exit (after restoring the terminal) print one line to stderr naming
the backend and whether key releases were available, and a second line
with the pointer note (see Pointer) when there is one.

## Environment

- `AFTERSHOCK_DATA`: data folder or pak, used when no positional argument.
- `XDG_STATE_HOME` (else `$HOME/.local/state`, else `.`): handed to the
  engine as save and system directory; the engine adds `aftershock/`.
  Empty counts as unset.
- `XDG_DATA_HOME`, `HOME`: data search (below).
- `AFTERSHOCK_ALSA_DEVICE`: ALSA PCM name, default `default`; `null`
  plays into ALSA's bit bucket. Also accept the old `OMAQUAKE_ALSA_DEVICE`
  when the new one is unset, so existing scripts keep working.
- `AFTERSHOCK_CMD`, `AFTERSHOCK_CMD_INGAME`: read by the engine, not the
  frontend; nothing to do.

## Data search

A **data folder** counts only when it has both `aftershock/` with a
`pak0.pak` (or `PAK0.PAK`) in it and `id1/` with a `pak0.pak` (or
`PAK0.PAK`). The result handed to the engine is the path of
`aftershock/pak0.pak`.

An explicit choice (positional argument, else `AFTERSHOCK_DATA`) is
honoured or refused, never replaced by a search hit: a regular file is
taken as the pak as it is; otherwise it must be a data folder.

Without one, try in order and take the first data folder found:
1. `<dir of the binary>/../data` (the binary is `build/aftershock` in a
   checkout), from `/proc/self/exe`
2. `<dir of the binary>/data`
3. `./data`
4. `$XDG_DATA_HOME/aftershock` if that variable is set and not empty,
   else `$HOME/.local/share/aftershock`
5. `/usr/share/aftershock`

Never search any Quake, QuakeSpasm or Steam location: no id Software data
is ever picked up.

On failure print to stderr `aftershock: no game data at 'X'` (explicit) or
`aftershock: could not find the game data`, then `looked in:` and each
folder tried on its own indented line, then:

    A data folder holds aftershock/pak0.pak and LibreQuake in id1/.
    In a checkout: sh tools/fetch-librequake.sh && sh tools/build-data.sh

and exit 2 (1 under `--find-pak`). `--demo` and `--keytest` need no data.

## Start-up order

1. Parse the command line. `--mouse-list` and `--find-pak` answer and exit
   here.
2. Look up the backend. Tell the engine which pictures exist
   (`aftershock_pictures`, a bit per Picture number). If `--video` was
   given, set `aftershock_picture_arg` to its Picture number; otherwise
   read the saved one (below) and, if it is compiled in, use it.
3. Find the data (not for `--demo` / `--keytest`).
4. Choose the pointer source (before the terminal is touched, so an error
   goes to a plain stderr). `--demo` forces no pointer.
5. Set up the terminal.
6. `--keytest`, `--demo`, or the game. Then shut the backend down,
   restore the terminal, print the exit lines.

**Saved picture**: read `<state dir>/aftershock/config.cfg` line by line
for `av_picture "N"` or `av_picture N` (N may be written as a float); the
last one wins. Missing file or no line: none saved.

## Terminal

Refuse to start unless both stdin and stdout are ttys
(`aftershock: stdin/stdout must be a terminal`, exit 1).

**Raw mode**: save the termios; clear IXON, ICRNL, BRKINT, INPCK, ISTRIP;
clear OPOST; clear ECHO, ICANON, ISIG, IEXTEN; VMIN 0, VTIME 0; apply with
TCSAFLUSH. Install the restore with `atexit`, SIGINT and SIGTERM set the
quit flag, SIGWINCH is ignored (size is polled).

Because OPOST is off, a bare LF no longer returns to column 0: any output
that separates rows with LF must send CR LF, or explicit cursor moves.
Forgetting this tears the picture into diagonal bands that scroll.

**Enter**: alternate screen (`CSI ?1049h`), hide the cursor (`CSI ?25l`),
autowrap off (`CSI ?7l`, so a glyph in the last column cannot wrap and
scroll), clear (`CSI 2J`).

**Kitty keyboard protocol**, after entering the alternate screen and
never before: the flag stack is per screen, so flags pushed on the main
screen are silently gone on the alternate one, and the program then
believes it has key releases while getting legacy bytes; every key sticks
down. Query `CSI ?u`; if the terminal answers, push flags 11
(disambiguate 1, report event types 2, report all keys as escape codes 8)
with `CSI >11u`, then query again and require bit 2 (event types) in the
answer. Without it, pop again (`CSI <u`) and use the legacy path.

**Queries** are written followed by a Primary Device Attributes request
(`CSI c`) as a sentinel: every terminal answers DA, so once a reply ending
in `c` has arrived the wanted answer has either come or never will. Read
with a 200 ms poll timeout per read. A kitty flags reply is
`CSI ? <digits> u` (the DA reply also starts `CSI ?` but ends in `c`).

**Text area in pixels**: query `CSI 14t`, answer `CSI 4 ; height ; width t`.
Do it during setup, while stdin is still ours; afterwards the input parser
owns stdin and a late reply would be decoded as keys. Cache it; later
needs scale it by the change in cells.

**Size**: TIOCGWINSZ on stdout, 80x24 when it fails or reports zero. The
size function also says whether it changed since the last call.

**Restore**, idempotent, in this order: close the evdev pointer (ungrab),
turn off pointer and focus reporting, pop the kitty flags if pushed, then
`CSI ?7h`, `CSI ?25h`, `CSI 0m`, `CSI ?1049l`, then the saved termios.

**Pointer reporting** (terminal source): `CSI ?1003h` (every motion, not
only drags, so looking needs no button held) and `CSI ?1016h` (SGR pixel
coordinates; only in this mode do terminals such as Ghostty stop
coalescing motion to one report per cell, and cell resolution is useless
for aiming). Plus focus reporting `CSI ?1004h`. The evdev source wants
focus reporting only. Disabling sends the matching `l` forms for whichever
were on.

**Writing**: loop on `write` until all bytes are out, retry on EINTR,
give up on other errors. Three ways to write a frame:
- raw: bytes exactly as given (our half-block family positions every cell
  itself);
- at the top left: `CSI H`, then the text with each LF sent as CR LF, in
  one write;
- at (row, col), 1-based, for a canvas smaller than the terminal: a
  cursor move to (row, col) and, for every following line, a cursor move
  to (row+n, col) instead of the LF. (1,1) is the plain case above.

## Keyboard input

Read all of stdin each poll into a buffer of at least 16 KB (pointer
reports under any-motion tracking arrive fast; a 1 KB buffer overflowed
and wedged), parse, keep any incomplete tail for the next poll. Keep
reading until stdin is empty, not just until the buffer is full, or aiming
falls behind. If a full buffer could not be parsed at all (one absurd
unterminated sequence), drop it, or every key after it is stuck.

Keys go to the engine as libretro `RETROK_*` codes with `RETROKMOD_*`
modifiers.

**Order of recognition** at an ESC:
1. `ESC [ I` / `ESC [ O`: focus in / out (mode 1004). Three bytes.
2. `ESC [ <`: an SGR mouse report (below). Must be tested before generic
   CSI: the generic path would take its `M`/`m` final byte as a cursor key
   and a minus sign as a private marker.
3. `ESC [ M` plus three bytes: X10 mouse report from a terminal without
   mode 1016. Consume all six bytes, do not act on them (cell resolution,
   capped at 223).
4. `ESC [`: CSI (below).
5. A lone ESC at the end of the buffer: with the kitty protocol on, Escape
   itself arrives as `CSI 27 u`, so a lone ESC is almost always the start
   of a sequence split across reads. Wait for the rest, but at most 50 ms;
   then take it as Escape (press and release). Treating it as Escape at
   once bounced the player out of menus on nearly every key. Without the
   protocol, a lone ESC is Escape.
6. `ESC O x`: SS3, which legacy terminals send for the arrows and F1 to
   F4 in application cursor mode. Map like the CSI finals below. An
   incomplete `ESC O` waits for more.
Reset the lone-ESC timer whenever the buffer does not start with ESC.

**CSI parsing**: parameters separated by `;`, sub-parameters by `:`;
private markers such as `?` and `>` skipped; the final byte is 0x40 to
0x7E. Incomplete: wait.
- Final `u` (kitty): first parameter is the Unicode key code; the second
  is the modifier field, 1-based (subtract 1, then bit 0 shift, 1 alt,
  2 ctrl, 3 meta); its sub-parameter is the event type: 1 press (also when
  absent), 2 repeat, 3 release. Drop repeats: the engine tracks its own
  repeat, so forwarding them double-fires.
- Final `~`: 1 Home, 2 Insert, 3 Delete, 4 End, 5 PageUp, 6 PageDown,
  15 F5, 17 F6, 18 F7, 19 F8, 20 F9, 21 F10, 23 F11, 24 F12.
- Other finals: A Up, B Down, C Right, D Left, H Home, F End, P F1, Q F2,
  R F3, S F4. Modifier and event type are read the same way as for `u`.

**Code points to keys**: A to Z and a to z to the letter keys, 0 to 9 to
the digit keys, 27 Escape, 13 Return, 9 Tab, 127 Backspace, 32 Space,
96 backquote; kitty's private codes 57441 LShift, 57442 LCtrl, 57443 LAlt,
57447 RShift, 57448 RCtrl, 57449 RAlt; any other code below 127 maps to the
same number (RETROK is ASCII there); anything else is unknown and dropped.

**Plain bytes** (outside escapes): 0x1C (Ctrl-\) quits (legacy path only);
other control codes below 32 except Tab, CR and ESC are Ctrl plus the
letter (byte 1 is Ctrl-A); everything else through the code point table.

**Frontend keys**, caught from the decoded event so they work under the
kitty protocol too (there Ctrl-\ arrives as `CSI 92;5u`, never as 0x1C):
- Ctrl-G toggles release of the pointer (see Pointer). Not passed on.
- F10, Ctrl-Q and Ctrl-\ quit. Not passed on.

**Without key releases** (legacy terminals) holds are synthesised, with
doom-ascii's timings (David's choice; the earlier flat 260 ms made walking
stutter in terminals with a long repeat delay):
- While the engine reports text input (`aftershock_text_input()`), every
  press is sent as press then release at once, so typing a letter twice
  gives two letters.
- Otherwise a fresh press goes to the engine and is held for 550 ms,
  enough to bridge the terminal's first auto-repeat delay. Each further
  press of the same key while held (the terminal's auto-repeat) extends
  the hold to 100 ms from now and sends nothing. When a hold runs out the
  release is sent. Up to 16 keys held at once.
- Expire holds every loop iteration.

Every key the engine gets is also logged: `key <code> down|up mods=<n>`.

## Pointer

### Choosing the source

`--mouse=none`: no pointer. `term`: the terminal's reports. `evdev`: a
raw device, and failure is an error (`aftershock: mouse: <reason>`,
exit 2), never a silent switch. `auto`: try evdev, else the terminal.
Naming `--mouse-dev` also makes evdev required unless the mode is `term`
or `none`; with those two, say on stderr that `--mouse-dev` is ignored.

Opening happens before terminal setup; the **grab** happens just before
the loop (so `--demo` and `--keytest` without a pointer never grab). If
the grab fails: an error when evdev was required, otherwise close the
device and fall back to the terminal source. Then calibrate (text area
size, below) and enable the reporting the source needs.

Keep a one-line **pointer note** for the log (written once the log is
open) and for stderr at exit, e.g. evdev path and name, "grabbed, 1:1 (no
edge band)", plus a remark when `--mouse-edge`/`--mouse-turn` were given
but do not apply; or "terminal SGR-pixels (modes 1003/1016), edge band
0.15 at 220 deg/s".

### Calibration

Cell size in pixels = text area pixels / cells, from the `CSI 14t` answer;
if there was none or it gives less than 1, use `--cell`. The look model's
extent is cells times cell size. On a resize, recompute it from the new
cell count and the cached cell size: asking the terminal again would be a
write from the wrong thread.

### Look model

The engine reads relative mouse counts. One count is worth 0.066 degrees
with stock settings (sensitivity 3 times m_yaw 0.022); the engine
multiplies the count as an integer, so scaling must happen before it, in
the frontend. Default sensitivity 2.0 counts per pixel is about 0.13
degrees per pixel.

**Terminal source** (absolute pixel positions, no pointer lock, no warp):
- Each report: if there is a previous position less than 150 ms old, add
  the difference (each axis clamped to plus or minus 500 pixels, anything
  bigger is a mis-associated report) times the sensitivity to an
  accumulator, Y negated when inverted. Always remember the new position
  and time. The 150 ms resync stops "left on one side, came back on the
  other" from reading as one huge flick.
- Steering band: within the outer `edge` fraction of each side (at least
  1 pixel), a factor t runs 0 at the band's inner edge to plus or minus 1
  at the window edge (clamped), squared with its sign kept (t times |t|),
  so entering the band is a nudge. Each frame add factor times
  (turn / 0.066) times the seconds since the last frame (clamped to 0 to
  0.1 s so a stall is not paid back as a lurch) to each axis. 1:1 motion
  and the band are added, not switched, so a turn carries on smoothly as
  the pointer runs out of room. Edge 0 turns the band off.
- Losing focus drops the position and the accumulators, so nothing done
  while away arrives as one jump.
- Measured with the old code (checks worth repeating): 180 px at
  sensitivity 2.0 turned 23.9 degrees (23.76 predicted); edge steering
  206 degrees per second (205.7 predicted). Read the angle back from a
  save file's v_angle.

**evdev source**: device counts times sensitivity (Y inverted when asked)
straight into the accumulators; no band, no resync, no extent.

**Each frame** hand the engine the whole counts of each accumulator
(toward zero), keep the fraction (a slow steer is a fraction of a count a
frame and would otherwise never move), clamp what is handed over to
plus or minus 30000.

### Terminal reports

SGR: `ESC [ < b ; x ; y M` (press or motion) or `m` (release). x and y can
be **negative** (a drag outside the window keeps reporting) and must be
read signed, or the view snaps across the map. In b: bit 32 means motion;
bits 4, 8, 16 are modifiers and ignored; 64 to 67 are wheel up, down,
left, right; low bits 3 mean no button; else 0 left, 1 middle, 2 right.
A malformed report that runs into a new ESC ends just before that ESC (so
the next key survives); one with any other stray byte is dropped through
that byte.

Every report updates the look position. Only non-motion reports are
button transitions (a drag repeats the held button on each motion
report). There is no hold-to-look button: looking is always on, and every
button stays free for binding.

Button numbering used by both sources: left 0, middle 1, right 2, wheel
up 3, down 4, left 5, right 6. They reach the engine as MOUSE1 and up, so
binds work whichever source is live.

### Focus and release

- Focus out: release left, middle and right in the engine (their
  releases will never arrive while away), deactivate the look model.
- evdev: ungrab on focus out, grab again on focus in unless released by
  the player. Ctrl-G toggles released: ungrab or grab. Keep draining the
  device while unfocused or released, but discard what comes, so nothing
  queued arrives as one lurch.
- Terminals that never report focus count as focused.

### evdev device selection

Reading the wrong event device is keylogging. Rules:
- Classify a node by its capability bitmaps through ioctls only; open it
  O_RDONLY|O_NONBLOCK|O_CLOEXEC, never `read()` it unless it is a
  pointer, and close it after classifying.
- **Keyboard first, and final**: a node reporting KEY_A, KEY_Z or
  KEY_ENTER is a keyboard and is never read, whatever else it can do.
  This applies to a node named with `--mouse-dev` too.
- A **pointer** has EV_REL with REL_X and REL_Y and EV_KEY with BTN_LEFT.
  Wheel-only or button-only nodes are not pointers.
- A **keyboard's own pointer**: a pointer whose name (EVIOCGNAME) is
  exactly the name of a keyboard node is that keyboard's pointer
  interface and is rejected. Real case: a Razer BlackWidow shows
  "Razer Razer BlackWidow X Chroma" as both keyboard and pointer, while a
  mouse's key node has a suffixed name ("SINOWEALTH Game Mouse" pointer,
  "SINOWEALTH Game Mouse Keyboard" keyboard). No capability test can tell
  them apart.
- Scan `/dev/input/event<N>` nodes in numeric order (event2 before
  event10). Collect keyboard names (up to 32) first, then score each
  remaining pointer: +100 if the name contains "mouse" or "trackball"
  (case-insensitive), +10 for a horizontal wheel; highest wins, ties go
  to the lower number.
- Verdicts for `--mouse-list` and errors: pointer; keyboard - never
  opened; keyboard's pointer - rejected; not a pointer; unreadable. List
  lines: path, verdict, name (or "(no name)").
- Reading: drain without blocking, at most 32 reads of up to 64 events a
  frame. REL_X/REL_Y add to the deltas; REL_WHEEL positive is wheel up,
  negative down; REL_HWHEEL positive is right, negative left; BTN_LEFT,
  BTN_RIGHT, BTN_MIDDLE value 1/0 are press/release, value 2 ignored.
  A read error other than EAGAIN or EINTR (unplugged) closes the device
  and play goes on without it.
- The grab (EVIOCGRAB) makes the compositor stop seeing the device: the
  desktop pointer freezes and stays hidden. The kernel drops it when the
  descriptor closes, so even a crash returns the mouse; we still ungrab
  explicitly on every exit path.
- Not Linux: evdev is a stub that always fails, `auto` lands on the
  terminal.

## libretro host

The core is linked statically (`STATIC_LINKING=1`), so call the `retro_*`
functions directly.

**Order**: open the log, set the environment, video, audio sample, audio
batch, input poll and input state callbacks, `retro_init`, then
`retro_load_game` with the pak path (the core reads its options and
registers the keyboard callback inside it, so the environment callback
must be in place first). After loading, select
`RETRO_DEVICE_KEYBOARD` on port 0: the core leaves it on the joypad,
and only on the keyboard device does it read the mouse at all (deltas and
buttons). Then `retro_get_system_av_info` for the frame rate (fallback
60; the core reports 60, not 72) and sample rate, then open sound.

**Environment answers**:
- SET_PIXEL_FORMAT: accept RGB565 only.
- SET_KEYBOARD_CALLBACK: keep the callback; keys are delivered through it
  (down, keycode, character 0, modifiers).
- GET_LOG_INTERFACE: a logger that writes to the log file only (tag each
  line once with its level, `[N] `; the engine prints lines in pieces) and
  discards everything when there is no log.
- GET_VARIABLE: `tyrquake_resolution` the chosen resolution,
  `tyrquake_framerate` "auto", `tyrquake_sound_samplerate` "auto",
  `tyrquake_compute_rendering` "disabled" (keeps the software
  rasterizer), `tyrquake_rumble` "disabled", `tyrquake_invert_y_axis`
  "disabled", `tyrquake_analog_deadzone` "15"; others unknown.
- GET_VARIABLE_UPDATE: false. GET_SAVE_DIRECTORY and
  GET_SYSTEM_DIRECTORY: the state dir. GET_CAN_DUPE: true.
  GET_AUDIO_VIDEO_ENABLE: video and audio (bits 1 and 2).
- SET_MESSAGE: log it.
- GET_MEMORY_STATUS: report 512 MB total and free; the core takes a
  quarter, a 128 MB hunk, which LibreQuake's larger maps need (the
  default 64 MB is not enough).
- SET_GEOMETRY, SET_INPUT_DESCRIPTORS, SET_CONTROLLER_INFO,
  SET_PERFORMANCE_LEVEL: accept. Everything else: decline.

**Video**: a NULL frame is a dupe, ignore it. Ask the presenter first
whether it wants this frame (the `--fps` throttle) and skip the colour
expansion when it does not. Expand RGB565 to RGB888 replicating the high
bits into the low ones (so full scale is 255, not 248). Log the size when
it changes (`video: WxH`).

**Input state**: port 0 only. Mouse X/Y: the pending deltas, cleared on
read. Left, middle, right: held state. The four wheel directions: latched
on press, reported for exactly one read and cleared (so the engine sees a
press and then the release that ends the bound command). Keyboard device:
answer the **arrow keys** with whether they are held. The core polls the
arrows every frame and releases any arrow reported as up, so without this
mirror a held arrow is released at once and turning stops (measured: a
held left arrow turned 29.5 degrees with the mirror, 1.4 without).
Input poll does nothing.

**Audio**: the core uses the batch callback but may use the single-sample
one; both feed the sound sink, the batch one always reports every frame
consumed.

**Shutdown**: unload, deinit, close sound (logging frames dropped), free,
close the log.

## Sound

ALSA, optional at build time (pkg-config `alsa`); without it every call
is a stub, init fails with "built without ALSA support", and the game is
silent.

- Install a silent ALSA error handler before anything else: the default
  one prints to stderr, which is the picture.
- Open the PCM (`AFTERSHOCK_ALSA_DEVICE`, see Environment) for playback,
  **non-blocking**. Signed 16-bit, interleaved, 2 channels, the core's
  rate, software resampling allowed (a card that wants 48 kHz must not
  stop sound), 80 ms latency (measured: drops are the same at 40 and
  160 ms, because what overflows is a loop stall such as a level load).
- Writes never wait: what does not fit is dropped and counted. On an
  underrun or suspend, recover silently, at most twice per write.
- Close by dropping, not draining.
- Sound failing to open is not fatal: log `audio: disabled (<reason>)`
  and play on. On success log `audio: <rate> Hz on '<device>'`.

## Game loop and timing

Each iteration: poll input, expire synthesised holds, pointer work
(source, focus, look model, hand the counts to the engine), poll the
gamepad (`av_pad_poll(focused)`), run one engine frame. Stop on quit or
after `--frames`. Pace to the core's frame rate with a monotonic clock:
sleep until the next tick; if behind, restart the schedule from now
rather than trying to catch up.

**Presenting is throttled, not the loop**: the engine makes exactly one
frame's worth of audio per run, so slowing the loop starves the sound
card (choppy sound and a low frame rate were one bug). The `--fps` check:
a frame is wanted when the next present time has come; the next time
becomes the old one plus the period, or now plus the period if we have
fallen more than a period behind.

**Before the loop**: fit the canvas, clear, resize the backend, start the
pointer (writes to the terminal), start the render thread, choose the
resolution, start the engine, init keys, open the pad (unless
`--no-pad`), log the pointer note.

**Canvas fit**: the terminal size, capped by `--cells`; for chafa and caca
reserve the overlay's bottom rows (`av_overlay_reserved_rows()`); our
half-block family lays out its own text and gets the whole terminal.
Minimum 1x1. Centre it: origin column `1 + (term cols - cols) / 2`, row
likewise.

**Resolution `auto`**: for the half-block family, `av_half_resolution()`
of the terminal minus the reserved rows. For the others, 2 pixels per
column by 4 per row, clamped to 320x200 at least and 1920x1200 at most.
(Measured: engine resolution is close to free up to 1920x1200 once
presentation has its own thread.)

**Every frame from the engine**: if the terminal size changed, refit the
canvas, update the look extent (terminal source), and hand the new
geometry to the render thread. If the engine's Picture setting
(`aftershock_textui()->picture`) differs from the one shown and is
compiled in: log `picture: N`, refit, hand the render thread the switch.
Then capture the overlay (`av_overlay_capture()`) and submit the frame.

**After the loop**: stop the render thread, close the pad, release the
evdev pointer before the slow part of shutdown, turn off pointer
reporting, shut the engine down.

## Render thread

The engine alone takes about one 60 Hz frame of CPU, so all conversion
and terminal output runs on a second thread.

- Two buffers. Submitting copies the frame into the pending buffer under
  a lock and wakes the thread; if the previous pending frame was never
  taken, it is overwritten and counted as dropped (newest wins; a stale
  frame is worth nothing). The engine never waits for the terminal.
- The thread waits for a frame, swaps buffers under the lock, takes the
  current config and any requested reconfigure or backend switch, and
  unlocks before drawing.
- Backend switch (Picture setting): shut the old backend down, clear the
  screen, init the new one with the config. Same backend with other
  symbols: treat as a reconfigure.
- Reconfigure: clear the screen, resize the backend.
- Draw the frame with the backend; for backends outside the half-block
  family, then draw the text overlay (`av_overlay_emit(cols, rows +
  reserved rows, origin col, origin row)`).
- Stop: mark not running, wake, join (draws a pending frame first), free.

Measured before this design: 11.8 fps at 400x100 cells, 1.1 MB of escape
codes a frame. After: about 61 fps at any size, no audio drops.

## Backends and the Picture setting

A backend has a name and init(config), shutdown, resize(config) and
frame(rgb888, w, h, stride). The config carries symbols, colour depth,
canvas cols and rows, cell pixel size, and the canvas origin (1-based).
**Initialise the config to zero with origin 1,1** before any use: an
unset origin once produced a cursor move to row 1600019804 and an empty
screen in `--demo`.

Compiled in, in this order for `--video` and the help text: half,
sextant, braille, chafa (if found), caca (if found).

**Picture numbers** are saved in config.cfg as `av_picture` and are the
Options, Video menu order, so never reorder, only append:
0 half, 1 sextant, 2 chafa fine, 3 chafa block, 4 chafa ASCII, 5 caca,
6 Braille. A number whose backend is not compiled in is unavailable.
Backend plus symbols to number: exact match first, else the first number
with that backend.

**chafa**: build a canvas of cols x rows with the cell pixel size (so a
4:3 picture is not squashed by 1:2 cells), symbols mode, a symbol map by
the symbols setting (ascii: ASCII plus space; block: block plus space;
fine: block, space, half, quadrant, sextant and octant), canvas mode by
colour (mono FG/BG, 16 indexed, 256 as indexed 240, true colour),
optimisations all (escape volume is what saturates the terminal), work
factor 0.15, dithering none in true colour and ordered otherwise (error
diffusion shimmers in motion). Terminal info from chafa's term database
detected from the environment. Rebuild on resize. Each frame: draw the
pixels as RGB8, print, strip trailing CR/LF (a newline on the bottom row
scrolls the picture up a line a frame), write at the origin. Written for
chafa 1.18 (no canvas alignment setters; those belong to placements).

**libcaca**: never `caca_create_display()` (its drivers take over the
tty). A detached canvas of cols x rows; a 24-bit dither recreated when
the frame's size or stride changes, with masks red 0x0000ff, green
0x00ff00, blue 0xff0000 (libcaca reads the three bytes little-endian, so
for R,G,B in memory red and blue look swapped), charset by symbols
(ascii "ascii", block "shades", fine "blocks"), colour mono "mono",
16 "16", 256 and true "full16", algorithm "fstein", antialias
"prefilter". Each frame: dither into the whole canvas, export as "utf8",
strip trailing CR/LF, write at the origin.

## Modes besides the game

**`--demo`**: a 320x200 test pattern, about 30 frames a second (33 ms
sleep), until `--frames` (default 600) or quit; checks for resize each
frame and resizes the backend. Top two thirds: eight vertical bars white,
yellow, cyan, green, magenta, red, blue, black (saturated primaries in a
known order, so a channel-order mistake shows at once). Bottom third: a
gradient that moves with the frame number (red rising across, green
falling, blue down the rows).

**`--keytest`**: writes to stdout on purpose (it is a diagnostic screen,
CR LF line ends). Header: whether key releases are on ("YES - kitty
keyboard protocol active" / "NO - press-only fallback, holds are
synthesised"), the pointer note or "off (--mouse=none)", text area pixels
and pixels per cell, and "press keys (arrows, Enter, W, Ctrl) or move the
pointer; Ctrl-Q, Ctrl-\ or F10 quits". Then every 20 ms: each raw read as
hex bytes with the printable character, each decoded key (name, DOWN/UP,
modifiers in hex), mouse report, focus change, ignored X10 report, and
for the pointer the counts handed over with position and the two band
factors.

## Session log (`--log`)

Opened by the host, written and flushed line by line. Holds the engine's
log and console, `core: <fps> fps, <rate> Hz audio`, the audio lines,
`video: WxH`, `mouse: <note>`, `picture: N`, every key, and what our
`av_overlay.c` and `av_pad.c` write (player state about once a second,
pad lines). A log function usable by those files is part of the host's
interface.

## Build

Rewrite the `Makefile` too (it began as OmaQuake's). Needs:
- `build/aftershock` from our sources, the static core
  `third_party/tyrquake/tyrquake_libretro_unix.a` (always handed to the
  core's own make with `STATIC_LINKING=1`, which rebuilds only what
  changed), and the libretro-common sources the core leaves out when
  linking statically (its `Makefile.common` says which; today that is
  `file/retro_dirent.c`, `encodings/encoding_utf.c`,
  `string/stdstring.c`, `streams/file_stream.c`,
  `streams/file_stream_transforms.c`, `vfs/vfs_implementation.c`,
  `file/file_path.c`, `file/file_path_io.c`, `features/features_cpu.c`,
  `compat/fopen_utf8.c`, `compat/compat_strl.c`,
  `compat/compat_posix_string.c`, `compat/compat_strcasestr.c`,
  `compat/compat_snprintf.c`, `time/rtime.c`, `net/net_compat.c`,
  `net/net_socket.c`; a missing one shows up as a named link error),
  built without warnings.
- Include paths: libretro-common's `include`, and the core's `common`
  as a quote include (for `av_textui.h`).
- chafa, libcaca and ALSA found by pkg-config, each optional, each
  adding its file and a define. `CHAFA=0` leaves chafa out entirely.
  A stamp of the compiler flags so that a library appearing or vanishing
  rebuilds what depends on it (a stale object once left chafa missing
  from a binary that was built "with" it).
- Targets: `all`, `engine`, `backends` (reports which of chafa, caca,
  alsa were found), `test`, `clean`, `install`
  (`$(DESTDIR)$(PREFIX)/bin/aftershock`).
- `-Wall -Wextra -std=gnu99`, `-pthread`, `-lm`.

## Tests

A new `tests/av_evdev_test.c` (`make test`), since device selection is
the one part that can do real harm:
- every keyboard-capable node on the machine is classified as a keyboard;
- synthetic devices through `/dev/uinput` (skip with a note when not
  permitted): a pointer is accepted, and its motion, wheel and buttons
  arrive with the right numbering; a keyboard is refused, also when named
  explicitly; a device with both keys and pointer axes is refused, and
  auto-detection never lands on it; a pointer named exactly like a
  keyboard is rejected as that keyboard's pointer.

## Acceptance

Compare old and new builds before removing the old files. Under the
4 GB scope, with a scratch `XDG_STATE_HOME` and `HOME`, `--no-sound`, in a
pty set by `stty` (see CLAUDE.md):
- `make` and `make CHAFA=0` build with no warnings; `ldd` on the CHAFA=0
  binary shows no chafa.
- `--demo --frames=60` in each backend shows the bars in the right order.
- `--keytest` (kitty terminal and plain) decodes arrows, Enter, Escape,
  letters, Ctrl-letters, F keys, modifiers, and quits on F10, Ctrl-Q and
  Ctrl-\.
- The game starts `AFTERSHOCK_CMD='map lq_e0m1'`, walks with held keys
  (both paths), turns with held arrows, the menus work, typing in the
  console gives doubled letters when typed twice, the Picture menu
  switches backends live and the choice survives a restart, a resize
  refits the picture, quitting restores the terminal.
- Pointer: the look numbers above; evdev picks the right node on this
  machine (`--mouse-list`), Ctrl-G and focus loss release it.
- The preview recorder still works.

Then remove `src/oq_*`, `tests/oq_evdev_test.c`, `docs/DESIGN.md` and
`docs/OmaQuake-README.md`, and update README, PATCHES.md, UPSTREAM.md,
CLAUDE.md (licence section, file list, `AFTERSHOCK_ALSA_DEVICE`) and the
catalog draft (`asciiverse/aftershock.toml` licence:
`GPL-2.0-or-later AND BSD-3-Clause`, GPL-3.0-or-later for a build with
chafa). OmaQuake stays credited in README's history and in the git
history.
