# Roadmap

Versions stay 0.x.0 until the whole game is in. No tags or releases while
the licence question in CLAUDE.md is open.

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
- [ ] HUD, centre messages and notify lines as real terminal text
- [ ] Menus and console as terminal text
- [ ] Identity: Aftershock in menus, title, messages and endings; no id
      names or texts shown
- [ ] Family-friendly by default: debris and sparks, clean obituaries,
      `av_gore 1` for the originals
- [ ] Credits screen

## 0.3.0 and on
- [ ] LibreQuake's episodes 1 to 4 played through; maps that fail fixed or
      left out
- [ ] Our own sextant or octant mode, if it beats half blocks
- [ ] Gamepad (Linux /dev/input, as in doom-ascii)
- [ ] 100x30 .cast preview
- [ ] Release workflow (Linux static, macOS, Windows), once the licence is
      settled
- [ ] QuakeC rebuilt from source once a QuakeC compiler is installed
