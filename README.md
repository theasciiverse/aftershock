# Aftershock

A first-person shooter drawn in the terminal. Cultists, hounds and things
from beyond the veil, five episodes of dark fortresses, bases and ruins,
and every frame of it made of characters.

Aftershock runs the GPL engine of a 1996 first-person shooter (TyrQuake,
through libretro's tyrquake core) with a terminal frontend of its own, and
is filled only with free content from the LibreQuake project.
No id Software data is needed, shipped or looked for.

## Build and play (Linux)

    sh tools/fetch-librequake.sh    # LibreQuake v0.09-beta into data/id1 (240 MB)
    sh tools/build-data.sh          # game code (gmqcc) and our files into data/aftershock
    make -j4                        # builds build/aftershock
    ./aftershock

Needs a C compiler, make, Python 3 and gmqcc (GMQCC 0.3.6, `apt install
gmqcc`), which compiles the game code in `qcsrc/`. chafa, libcaca and ALSA are optional
(`make backends` says which were found); `make CHAFA=0` leaves chafa out.

Play in a terminal with 24-bit colour, at least 100x30 (bigger is better),
ideally one with the kitty keyboard protocol (Ghostty, kitty, WezTerm, foot,
Alacritty, iTerm2): then every key press and release is exact. Elsewhere,
and inside tmux or screen, releases are guessed and it still plays.

    ./aftershock                    # half blocks, with sound and music
    ./aftershock --no-sound         # silently
    ./aftershock --video=chafa --symbols=fine    # chafa's modes, see below
    ./aftershock --help

Settings, saves and the session log go to `$XDG_STATE_HOME/aftershock`
(usually `~/.local/state/aftershock`).

## Controls

Made for the keyboard; a mouse is optional.

| | |
|---|---|
| Arrows | walk and turn |
| W A S D, or `,` `.` | walk and step sideways |
| Ctrl, F or Enter | fire |
| Space | jump (swim up in water) |
| Shift | walk slowly (you always run) |
| Page Up, Page Down | look up, down; End or V looks ahead again |
| 1 to 8, Q or `]`, `[` | weapons |
| Esc | menu; `` ` `` console; Tab scores; P pause |
| F6, F9 | quick save, quick load |
| F10, Ctrl-Q | quit at once |

A gamepad on Linux (read from /dev/input, untested so far): left stick
walks, right stick looks, right trigger fires, left trigger or A jumps,
X and Y (or the bumpers) change weapons, Start is the menu; in menus the
d-pad moves, A chooses and B goes back. `--no-pad` leaves it alone.

Shots are lifted onto what you face, so you rarely need to look up or down.

## Pictures

Every picture below is also a setting, **Picture** in Options, Video
(those the build has): it switches at once and is remembered, and the game
opens in it next time. Naming one with `--video` sets it too.

- `--video=half` (the first default): truecolour half blocks, two square pixels per
  character, only changed cells sent. Ours, no library.
- `--video=sextant`: ours too, sharper: each character is 2x3 sub-pixels
  in two colours (sextant characters, U+1FB00 on, which most current
  terminals draw themselves). Same speed as half blocks.
- `--video=braille`: ours as well: each character is 2x4 dots (Braille,
  U+2800 on), square on the screen, in two colours with the second filling
  between the dots.
- `--video=chafa --symbols=fine|block|ascii`: the chafa library's modes,
  sub-cell glyphs, block shades, or plain letters.
- `--video=caca`: libcaca, 16 colours.

## Credits

- Engine: TyrQuake by Kevin Shanahan and contributors, from id Software's
  GPL source release of Quake; the libretro tyrquake core by the libretro
  team and contributors.
- Content: the LibreQuake project (github.com/lavenderdotpet/LibreQuake),
  art, maps, sounds and music under BSD-3-Clause, game code under GPL-2.0+.
  Its contributors are listed in data/id1/docs/CREDITS.
- Aftershock: the terminal frontend, the half-block picture, text HUD,
  keyboard scheme, identity and family-friendly changes. PATCHES.md lists
  every change.

## History

Aftershock began in November 2026 on OmaQuake by Raul Dias
(github.com/rsd/omaquake), which brought this engine to the terminal, and
its history is kept in this repository. OmaQuake is
GPL-2.0-only, which cannot be shipped with chafa (LGPL-3.0-or-later), so
on 2026-11-27 its files were replaced by a frontend of our own, written
from a description of how it must behave (docs/FRONTEND-SPEC.md) without
reading OmaQuake's code. Much of how Aftershock plays in a terminal was
first worked out in OmaQuake.

## Licence

The program is GPL-2.0-or-later, see COPYING. A build with chafa linked
(the default when chafa is installed) is distributed under GPL-3.0-or-later,
as chafa is LGPL-3.0-or-later, see LICENSES/GPL-3.0.txt; `make CHAFA=0`
builds without it. LibreQuake's content is BSD-3-Clause, see LICENSES/.
