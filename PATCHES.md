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
- **Braille picture** (`--video=braille`, `av_half.c`): 2x4 dots a cell in
  two colours, the dots square on a 1:2 cell; shares the sextants' code.
- **Picture setting**: half blocks, sextants, Braille, chafa (fine, blocks,
  ASCII) or caca, those compiled in, chosen in Options, Video (`menu.c`, cvar
  `av_picture` in `av_textui.c`, saved with the settings). The frontend
  switches backends between frames on the render thread
  (`oq_render_switch`), opens in the saved one (`saved_picture` in
  `oq_main.c` reads config.cfg), and `--video` sets it (`libretro.c`).
  The numbers are in `oq_present.c`'s table: append only.
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
- **Sound and music on by default**, `--no-sound` for silence (oq_main.c;
  our first builds were silent unless `--sound`).
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

- `common/av_family.c` (new), hooked into `cl_parse.c` and `cl_main.c`:
  the `av_gore` cvar (0, family-friendly, by default; the game code reads
  it), and the text table `av_text.txt`, which rewrites what the player
  reads from outside the game code: the maps' own level names and
  messages, and the engine's lines.
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
- `libretro.c`, `draw.c`: the engine's width is a whole number of 8
  columns, and the console background fills the last pixels of a row one at
  a time. A terminal 319 columns wide asked for a 638 wide picture, and the
  console, drawn 4 pixels at a time, wrote past the framebuffer's end; glibc
  aborted a second after starting.
- `r_main.c`: the view model is drawn only when it is an alias model
  (LibreQuake's demo2 crashed the renderer).
- `libretro.c`: the console goes to the frontend's log; the first-boot bind
  and gamma overrides are gone (the game's default.cfg rules);
  `AFTERSHOCK_CMD` and `AFTERSHOCK_CMD_INGAME` hand the console a command
  line at start or once in a level (tests, the preview);
  `aftershock_text_input()` tells the frontend when the keyboard is typing;
  only `/id1/` is recognised in the data path, so a folder named like a
  mission pack switches nothing.

## Game code (qcsrc/)

LibreQuake v0.09-beta's LibreProgsDeluxe (GPL-2.0+), imported unchanged in
its own commit, then:

- **Built with gmqcc** (GMQCC 0.3.6) by `tools/build-progs.sh`:
  `-std=fteqcc -funtyped-nil -fassign-function-types -O2`, the file list
  read from LibreQuake's own `progs.src` (`tools/progs-list.py`). Two
  FTEQCC-only constructs were rewritten: the epilogue table (a struct
  array) is a function, `IntermissionEpilogue`, and five casts between
  function types are plain assignments. gmqcc places a local vector in
  the last two globals, one past the end, which the engine rejects;
  `tools/progs-pad.py` adds three spare globals. Same monster and secret
  totals as LibreQuake's own build on every map compared.
- **One set of texts** (`localized_text.qc`): LibreQuake's names; id's
  original texts (the `#else` branches) are removed here and in
  `client.qc`, `items.qc`, `weapons.qc` and the camera code, so nothing
  depends on `__LIBREQUAKE__` any more. New episode endings of our own.
  Family-friendly obituaries (flattened, knocked flat, bowled over by a
  Grave Hound, blown away, cut down, started over). The pentagram item is
  the Aegis; the nailguns are the Spike Gun and the Spike Cannon, their
  ammunition spikes; two typos fixed.
- **Monster names**: each monster's `netname` is LibreQuake's name.
- **`aftershock.qc`** (new): with `av_gore 0`, `ThrowGib` and `ThrowHead`
  throw stone debris (`progs/av_*.mdl`), `SpawnBlood` throws sparks
  (palette 226 instead of 73), thrown flesh is stone, and the gib sounds
  (`player/udeath.wav`, `player/gib.wav`, `zombie/z_gib.wav`) are thuds
  (`player/land2.wav`, `weapons/tink1.wav`). `av_gore 1` brings back the
  originals at once.

## Game data (game/aftershock, packed into data/aftershock/pak0.pak)

- `quake.rc`, `default.cfg`: our start-up, keyboard-first bindings,
  gamma 0.65, `viewsize 120`, the FitzQuake network protocol
  (`sv_protocol 666`, for LibreQuake's maps with more than 256 models).
- `gfx/colormap.lmp`: LibreQuake's, with the one byte the engine's size check
  expects (16385).
- `progs/av_rubble1..3.mdl`, `progs/av_pile.mdl`: the debris
  (`tools/mkdebris.py`).
- `av_text.txt`: the text table, for map and engine text.
- `progs.dat`: built from `qcsrc/` by `tools/build-data.sh`, not stored.

## Removed

- OmaQuake's Omarchy bar plugin (`omarchy-plugin/`, `manifest.json`,
  `preview.png`, which carry id's logo), its Arch packaging for the id
  shareware data (`packaging/`), and its shareware key settings
  (`share/autoexec.cfg`, `config.cfg`). They are in OmaQuake's history.

## Not changed

- OmaQuake's chafa and libcaca backends, mouse handling (evdev or the
  terminal's reports) and ALSA sound.
- LibreQuake's maps, models, textures, sounds and music.
