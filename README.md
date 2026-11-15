# Aftershock

A first-person shooter drawn in the terminal. Cultists, hounds and things
from beyond the veil, five episodes of dark fortresses, bases and ruins,
and every frame of it made of characters.

Aftershock runs the GPL engine of a 1996 first-person shooter (TyrQuake,
through libretro's tyrquake core and Raul Dias's OmaQuake terminal
frontend) and is filled only with free content from the LibreQuake project.
No id Software data is needed, shipped or looked for.

## Build and play (Linux)

    sh tools/fetch-librequake.sh    # LibreQuake v0.09-beta into data/id1 (240 MB)
    sh tools/build-data.sh          # our own files into data/aftershock
    make -j4                        # builds build/aftershock
    ./aftershock

Needs a C compiler and make. chafa, libcaca and ALSA are optional
(`make backends` says which were found); `make CHAFA=0` leaves chafa out.

Play in a terminal with 24-bit colour, at least 100x30 (bigger is better),
ideally one with the kitty keyboard protocol (Ghostty, kitty, WezTerm, foot,
Alacritty, iTerm2): then every key press and release is exact. Elsewhere,
and inside tmux or screen, releases are guessed and it still plays.

    ./aftershock                    # half blocks, sound off
    ./aftershock --sound            # with sound and music
    ./aftershock --video=chafa --symbols=fine    # OmaQuake's modes, see below
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

- `--video=half` (default): truecolour half blocks, two square pixels per
  character, only changed cells sent. Ours, no library.
- `--video=sextant`: ours too, sharper: each character is 2x3 sub-pixels
  in two colours (sextant characters, U+1FB00 on, which most current
  terminals draw themselves). Same speed as half blocks.
- `--video=chafa --symbols=fine|block|ascii`: OmaQuake's chafa modes,
  sub-cell glyphs, block shades, or plain letters.
- `--video=caca`: libcaca, 16 colours.

## Credits

- Engine: TyrQuake by Kevin Shanahan and contributors, from id Software's
  GPL source release of Quake; the libretro tyrquake core by the libretro
  team and contributors.
- Terminal frontend: OmaQuake by Raul Dias (github.com/rsd/omaquake).
- Content: the LibreQuake project (github.com/lavenderdotpet/LibreQuake),
  art, maps, sounds and music under BSD-3-Clause, game code under GPL-2.0+.
  Its contributors are listed in data/id1/docs/CREDITS.
- Aftershock: the half-block picture, text HUD, keyboard scheme, identity and
  family-friendly changes. PATCHES.md lists every change.

## Licence

The program is GPL-2.0-only (OmaQuake's licence; the engine is
GPL-2.0-or-later), see COPYING. LibreQuake's content is BSD-3-Clause, see
LICENSES/. OmaQuake's own README is in docs/OmaQuake-README.md.
