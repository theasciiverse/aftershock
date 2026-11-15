# Changes from upstream

Aftershock is OmaQuake 0.1.2 (Raul Dias, GPL-2.0-only,
github.com/rsd/omaquake) with the libretro tyrquake core (GPL-2.0+) carried
as a subtree, played with LibreQuake v0.09-beta (BSD-3-Clause data, GPL-2.0+
game code) instead of id Software's data. `main` starts at OmaQuake's
`master` with its history; UPSTREAM.md has every remote and base.

AnsiQuake (Ben Morris, github.com/pixelbadger/AnsiQuake; its README says
GPLv2, the repository has no LICENSE file) was read as a reference. No code
was taken from it.

## Frontend (src/)

- **Half-block picture** (`src/av_half.c`, new, the default `--video=half`):
  truecolour upper half blocks, two square pixels per cell, each the average
  of the engine pixels it covers. The engine renders at the grid's own aspect
  times a whole number (`av_half_resolution`), so nothing is stretched or
  wasted. Only changed cells are sent, colours and cursor moves only when they
  differ, inside synchronised output (`CSI ? 2026`). Uses no library.
  `oq_term_write_raw` (oq_term.c) writes its bytes unchanged.
- **Sextant picture** (`--video=sextant`, same file): 2x3 sub-pixels per
  cell in two colours (split at the cell's mean brightness), drawn with the
  sextant characters; same diffing and text overlay.
- **Text overlay** (`src/av_overlay.c`, new): terminal text laid over the
  picture cells (HUD and messages).
- **`make CHAFA=0`** builds without chafa (LGPL-3.0), the configuration that
  can be shipped with this GPL-2.0-only program; libcaca (WTFPL) may stay.
- **Data search** (`oq_paths.c`): only Aftershock's own data folders
  (`data/` beside the binary or in the checkout, `$XDG_DATA_HOME/aftershock`,
  `/usr/share/aftershock`), each needing `aftershock/pak0.pak` and
  LibreQuake's `id1/`. OmaQuake's search of Quake, QuakeSpasm and Steam
  folders is gone, so id data is never picked up. `AFTERSHOCK_DATA` replaces
  `OMAQUAKE_PAK`.
- **Saves and settings** in `$XDG_STATE_HOME/aftershock` (oq_main.c).
- **Sound off unless `--sound`** (oq_main.c).
- **Plain-terminal keys** (oq_input.c): without the kitty protocol a fresh
  press holds 550 ms and each auto-repeat 100 ms (doom-ascii's timings; were
  260 ms flat, which made walking stutter in terminals with a long repeat
  delay). In menus, the console and message lines every press is a whole
  keystroke, so typed letters are no longer swallowed.
- **Bigger engine memory**: the frontend answers the core's memory query so
  it takes a 128 MB hunk for LibreQuake's larger maps (oq_retro.c).
- The binary is `build/aftershock`; `./aftershock` starts it with a session
  log in the state folder.

## Engine (third_party/tyrquake)

Every change is marked `Aftershock:` in a comment.

- `common/av_family.c` (new), hooked into `cl_parse.c`, `r_part.c`,
  `cl_main.c`, `model.c`, `snd_mem.c`:
  - `av_gore 0` (default): blood particles become sparks, gib and head
    models load as stone debris (`progs/av_*.mdl`, made by
    `tools/mkdebris.py`), blood trails become smoke, squelching gib sounds
    become thuds. `av_gore 1` restores the originals.
  - The text table `av_text.txt` rewrites what the player reads (prints,
    centre messages, level names, endings).
- `common/av_textui.c` (new), hooked into `draw.c`, `menu.c`, `screen.c`:
  with `av_textui 1` (default) the engine records the characters of its
  menus, console and centre messages instead of drawing them, puts text
  labels (and the AFTERSHOCK title, and a help page of our own) in place of
  the menu pictures, does not draw the status bar, plaques or level summary,
  and fills in the player's state for the frontend. `Draw_LastCachePicName`
  (draw.c) names the picture a menu is about to draw.
- `menu.c`: our own quit prompts (id's were jokes, not all for children);
  the Host a game list holds LibreQuake's playable maps by their own names;
  the mission-pack lists point at it. "No Quake servers found" says "No
  servers found".
- `keys.c`: `` ` `` and `~` run their binding in the console again, so they
  close it (libretro had made them typed characters).
- `model.c`, `bspfile.h`: the BSP2 format as compilers write it today
  ("BSP2", float bounds in nodes and leafs) loads; the core only knew the
  older "2PSB" layout under that name. LibreQuake's lq_e2m4 needs it.
- `alias_model.c`: skins up to 4096 wide (LibreQuake's two bosses).
- `pr_cmds.c`: `objerror` removes the entity and the level goes on, as in
  FitzQuake (lq_e3m4 has a nameless path corner).
- `r_main.c`: the view model is drawn only when it is an alias model
  (LibreQuake's demo2 crashed the renderer).
- `libretro.c`: the console goes to the frontend's log; the first-boot bind
  and gamma overrides are gone (the game's default.cfg rules);
  `AFTERSHOCK_CMD` and `AFTERSHOCK_CMD_INGAME` hand the console a command
  line at start or once in a level (tests, the preview);
  `aftershock_text_input()` tells the frontend when the keyboard is typing;
  only `/id1/` is recognised in the data path, so a folder named like a
  mission pack switches nothing.

## Game data (game/aftershock, packed into data/aftershock/pak0.pak)

- `quake.rc`, `default.cfg`: our start-up, keyboard-first bindings,
  gamma 0.65, `viewsize 120`, the FitzQuake network protocol
  (`sv_protocol 666`, for LibreQuake's maps with more than 256 models).
- `gfx/colormap.lmp`: LibreQuake's, with the one byte the engine's size check
  expects (16385).
- `progs/av_rubble1..3.mdl`, `progs/av_pile.mdl`: the debris.
- `av_text.txt`: the text table.

## Removed

- OmaQuake's Omarchy bar plugin (`omarchy-plugin/`, `manifest.json`,
  `preview.png`, which carry id's logo), its Arch packaging for the id
  shareware data (`packaging/`), and its shareware key settings
  (`share/autoexec.cfg`, `config.cfg`). They are in OmaQuake's history.

## Not changed

- OmaQuake's chafa and libcaca backends, mouse handling (evdev or the
  terminal's reports) and ALSA sound.
- LibreQuake's maps, models, textures, sounds, music and compiled game code.
