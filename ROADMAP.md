# Roadmap

Versions stay 0.x.0 until the whole game is in. No tags or releases until
David says (see CLAUDE.md).

## 0.1.0, first light
- [x] OmaQuake's frontend on the libretro tyrquake core, LibreQuake's data,
      our own game folder over it, no id data searched for
- [x] Our half-block backend: truecolour, changed cells only, synchronised
      output, sized to the terminal, the default
- [x] Keyboard: kitty key releases, plus doom-ascii's hold timings and real
      typing on plain terminals
- [x] Keyboard-first bindings with autoaim
- [x] LibreQuake's start map and episode 0 load and play
- [x] Sound off unless `--sound`
- [x] Saves and settings in `$XDG_STATE_HOME/aftershock`, session log
- [x] `make CHAFA=0`: the shippable build, no chafa linked

## 0.2.0, readable and ours
- [x] HUD, centre messages and notify lines as real terminal text
- [x] Menus and console as terminal text
- [x] Identity: Aftershock in menus, title, messages and endings; no id
      names or texts shown (text table, our own quit prompts and level
      lists)
- [x] Family-friendly by default: debris and sparks, clean obituaries,
      `av_gore 1` for the originals
- [x] Credits on the Help page, in the README and the store draft
- [x] Gamepad (Linux /dev/input), untested with a real pad
- [x] Our own sextant mode
- [x] Every real LibreQuake map loads (BSP2, FitzQuake protocol,
      non-fatal object errors)

## 0.3.0 and on
- [ ] LibreQuake's episodes 1 to 4 played through; maps that fail fixed or
      left out
- [ ] Play episodes 1 to 4 through by hand and fix what turns up
- [ ] An octant mode (Unicode 16) once terminals draw octants widely
- [x] 100x30 .cast preview, a fight
- [ ] A credits screen of its own in the menu
- [ ] Title graphics (conback) of our own
- [x] Our own terminal frontend, written clean room from
      docs/FRONTEND-SPEC.md: the program is GPL-2.0-or-later
- [ ] Release workflow (Linux static, macOS, Windows), when David says
- [x] QuakeC built from source with gmqcc; texts and gore defaults in it
