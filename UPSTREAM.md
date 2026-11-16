# Upstreams

| Part | Where | Remote here | Base |
|---|---|---|---|
| Terminal frontend | OmaQuake, Raul Dias, https://github.com/rsd/omaquake (GPL-2.0-only) | `upstream` | `main` starts at its `master`, 118e52e (v0.1.2), history kept |
| Engine | libretro tyrquake core, https://github.com/libretro/tyrquake (GPL-2.0+) | `libretro-tyrquake` | subtree at `third_party/tyrquake`, squashed from e57bb11 (the commit OmaQuake's submodule pinned) |
| Engine lineage | TyrQuake, Kevin Shanahan, https://disenchant.net/tyrquake/ (mirror https://github.com/sezero/tyrquake) | `tyrquake` | branch `tyrquake-base`: our first start on desktop TyrQuake 6531579 |
| Game code | LibreQuake's LibreProgsDeluxe (qcsrc/, GPL-2.0+) | none | imported from the v0.09-beta source archive, sha256 a4c86aab43c62b11454237c4c0976c1737743e7aafa2f1c3499675505e0cf9cc, then ours |
| Content | LibreQuake, https://github.com/lavenderdotpet/LibreQuake (BSD-3-Clause data, GPL-2.0+ QuakeC) | none; fetched | release v0.09-beta, `full.zip`, SHA-256 623e463b35811216244f9ba15e0c45abc1765288650b5e3d36a899b35e4bcf3d |
| Reference only | AnsiQuake, Ben Morris, https://github.com/pixelbadger/AnsiQuake (README says GPLv2, no LICENSE file) | none | read, no code taken |

Pulling from upstream:

    git fetch upstream && git merge upstream/master
    git subtree pull --prefix third_party/tyrquake libretro-tyrquake master --squash
