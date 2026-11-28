#!/usr/bin/env python3
"""Draw Aftershock's console background, game/aftershock/gfx/conback.lmp.

usage: mkconback.py [palette source pak] [out.lmp] [preview.png]
       (defaults: data/id1/pak0.pak, game/aftershock/gfx/conback.lmp)

The picture is ours, drawn here from nothing but code: dark rock split by
cracks from an epicentre, a seismograph trace with the main shock and the
aftershocks dying away, and the name in carved block letters cut by a
fault, the lower half slipped sideways. It is 320x200 in the game's
palette (LibreQuake's gfx/palette.lmp), the size and format the engine
expects; the engine shows it behind the menu when no level is running and
while a level loads. The same seed always gives the same bytes.

Needs Pillow and NumPy."""
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

W, H = 320, 200
SEED = 1996

# Our own block letters, 5 wide and 7 high; diagonals two cells thick so
# they still join at this size.
FONT = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "R": ["11110", "10001", "10001", "11110", "10110", "10011", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "K": ["10001", "10011", "10110", "11100", "10110", "10011", "10001"],
}


def palette(pak):
    """gfx/palette.lmp from a .pak, as a 256x3 array."""
    with open(pak, "rb") as f:
        magic, off, ln = struct.unpack("<4sii", f.read(12))
        if magic != b"PACK":
            sys.exit(pak + ": not a pak")
        f.seek(off)
        d = f.read(ln)
        for i in range(0, ln, 64):
            name = d[i:i + 56].split(b"\0")[0].decode()
            if name == "gfx/palette.lmp":
                o, l = struct.unpack("<ii", d[i + 56:i + 64])
                f.seek(o)
                return np.frombuffer(f.read(l), np.uint8).reshape(256, 3)
    sys.exit(pak + ": no gfx/palette.lmp")


def smooth_noise(rng, w, h, cell):
    """Value noise: a coarse random grid, bilinearly upsampled."""
    gw, gh = w // cell + 2, h // cell + 2
    g = rng.random((gh, gw))
    ys = np.arange(h) / cell
    xs = np.arange(w) / cell
    y0 = ys.astype(int)
    x0 = xs.astype(int)
    fy = (ys - y0)[:, None]
    fx = (xs - x0)[None, :]
    fy = fy * fy * (3 - 2 * fy)
    fx = fx * fx * (3 - 2 * fx)
    a = g[y0][:, x0]
    b = g[y0][:, x0 + 1]
    c = g[y0 + 1][:, x0]
    d = g[y0 + 1][:, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def rock(rng):
    """Dark warm rock, lighter towards the bottom where the name sits."""
    n = (0.5 * smooth_noise(rng, W, H, 40) + 0.3 * smooth_noise(rng, W, H, 12)
         + 0.2 * smooth_noise(rng, W, H, 4))
    v = 0.10 + 0.16 * n
    v *= 0.75 + 0.45 * (np.arange(H)[:, None] / H)
    base = np.array([1.0, 0.62, 0.42])          # brown-red
    img = v[:, :, None] * base[None, None, :] * 255
    # A faint vignette keeps the edges darker.
    yy, xx = np.mgrid[0:H, 0:W]
    r = np.hypot((xx - W / 2) / (W / 2), (yy - H / 2) / (H / 2))
    img *= np.clip(1.15 - 0.35 * r, 0.6, 1.0)[:, :, None]
    return img


def crack_path(rng, x, y, angle, length, step=5.0, wobble=0.22):
    pts = [(x, y)]
    for _ in range(int(length / step)):
        angle += rng.normal(0, wobble)
        x += step * np.cos(angle)
        y += step * np.sin(angle)
        pts.append((x, y))
    return pts, angle


def cracks(img, rng, ex, ey):
    """Cracks from the epicentre, each splitting once or twice."""
    dark = Image.new("L", (W, H), 0)
    lite = Image.new("L", (W, H), 0)
    dd, ld = ImageDraw.Draw(dark), ImageDraw.Draw(lite)
    todo = []
    for k in range(9):
        a = k * 2 * np.pi / 9 + rng.normal(0, 0.25)
        todo.append((ex, ey, a, rng.uniform(80, 190), 2))
    while todo:
        x, y, a, length, width = todo.pop()
        pts, end = crack_path(rng, x, y, a, length)
        dd.line(pts, fill=255, width=width)
        ld.line([(px, py + width) for px, py in pts], fill=255, width=1)
        if width > 1 and length > 40:
            for _ in range(rng.integers(1, 3)):
                i = rng.integers(len(pts) // 3, len(pts))
                px, py = pts[i]
                todo.append((px, py, end + rng.choice([-1, 1]) *
                             rng.uniform(0.4, 1.0), length * 0.5, width - 1))
    d = np.asarray(dark, float)[:, :, None] / 255
    l = np.asarray(lite, float)[:, :, None] / 255
    img = img * (1 - 0.85 * d)
    img = img + l * (1 - d) * np.array([34, 20, 12])
    return img


def seismograph(img, rng, y0):
    """The trace: quiet, the main shock, then aftershocks dying away."""
    xs = np.arange(W)
    amp = 1.2 + 0.0 * xs
    for cx, a, w in ((34, 26, 9), (226, 13, 6), (262, 9, 5), (290, 6, 4),
                     (308, 4, 3)):
        amp = amp + a * np.exp(-((xs - cx) / w) ** 2)
    phase = np.cumsum(rng.uniform(0.9, 1.6, W))
    ys = y0 + amp * np.sin(phase) * rng.uniform(0.6, 1.0, W)
    trace = Image.new("L", (W, H), 0)
    glow = Image.new("L", (W, H), 0)
    td, gd = ImageDraw.Draw(trace), ImageDraw.Draw(glow)
    pts = list(zip(xs.tolist(), ys.tolist()))
    gd.line(pts, fill=255, width=3)
    td.line(pts, fill=255, width=1)
    t = np.asarray(trace, float)[:, :, None] / 255
    g = np.asarray(glow, float)[:, :, None] / 255
    amber = np.array([255, 176, 64])
    img = img * (1 - 0.5 * g) + g * 0.5 * amber * 0.45
    img = img * (1 - t) + t * amber
    return img


def logo(img, rng, text, y0, cell, gap):
    """The name in carved block letters, split by a fault line."""
    glyph_w = 5 * cell
    total = len(text) * glyph_w + (len(text) - 1) * gap
    x0 = (W - total) // 2
    mask = np.zeros((H, W), bool)
    for i, ch in enumerate(text):
        gx = x0 + i * (glyph_w + gap)
        for r, row in enumerate(FONT[ch]):
            for c, bit in enumerate(row):
                if bit == "1":
                    mask[y0 + r * cell:y0 + (r + 1) * cell,
                         gx + c * cell:gx + (c + 1) * cell] = True
    # The fault: a jagged line through the middle of the letters; below it
    # everything slips 2 pixels to the right and the gap shows as a crack.
    # Through the fifth row, clear of the bars in the fourth.
    fault = (y0 + int(4.5 * cell) +
             np.round(1.2 * np.sin(np.arange(W) / 7.0) +
                      rng.integers(-1, 1, W)).astype(int))
    yy = np.arange(H)[:, None]
    below = yy > fault[None, :]
    shifted = np.zeros_like(mask)
    shifted[:, 2:] = mask[:, :-2]
    mask = np.where(below, shifted, mask)
    mask &= np.abs(yy - fault[None, :]) > 0     # the crack itself

    # Carved stone: a warm light face, lit from the top left.
    up = np.zeros_like(mask)
    up[1:, :] = mask[:-1, :]
    left = np.zeros_like(mask)
    left[:, 1:] = mask[:, :-1]
    down = np.zeros_like(mask)
    down[:-1, :] = mask[1:, :]
    right = np.zeros_like(mask)
    right[:, :-1] = mask[:, 1:]
    edge_hi = mask & (~up | ~left)
    edge_lo = mask & (~down | ~right)
    grain = smooth_noise(rng, W, H, 3)
    shade = (0.78 + 0.22 * grain) * (1.05 - 0.35 * (yy - y0) / (7 * cell))
    face = np.clip(shade[:, :, None] * np.array([232, 150, 58]), 0, 255)
    # A drop shadow down and to the right first, then the face.
    sh = np.zeros_like(mask)
    sh[2:, 2:] = mask[:-2, :-2]
    sh &= ~mask
    img = np.where(sh[:, :, None], img * 0.35, img)
    img = np.where(mask[:, :, None], face, img)
    img = np.where(edge_lo[:, :, None], face * 0.55, img)
    img = np.where(edge_hi[:, :, None],
                   np.minimum(face * 1.25 + 20, 255), img)
    return img


def to_palette(img, pal):
    """Nearest colour of the game's palette, leaving out the fullbright
    rows (224 to 255), which some renderers treat specially."""
    usable = pal[:224].astype(float)
    flat = img.reshape(-1, 3)
    best = np.empty(len(flat), np.uint8)
    for s in range(0, len(flat), 8192):
        chunk = flat[s:s + 8192]
        dist = ((chunk[:, None, :] - usable[None, :, :]) ** 2).sum(2)
        best[s:s + 8192] = dist.argmin(1)
    return best.reshape(H, W)


def main():
    pak = sys.argv[1] if len(sys.argv) > 1 else "data/id1/pak0.pak"
    out = sys.argv[2] if len(sys.argv) > 2 else "game/aftershock/gfx/conback.lmp"
    preview = sys.argv[3] if len(sys.argv) > 3 else None
    rng = np.random.default_rng(SEED)
    pal = palette(pak)
    img = rock(rng)
    img = cracks(img, rng, 52, 120)
    img = seismograph(img, rng, 132)
    img = logo(img, rng, "AFTERSHOCK", 148, 5, 4)
    idx = to_palette(np.clip(img, 0, 255), pal)
    with open(out, "wb") as f:
        f.write(struct.pack("<ii", W, H))
        f.write(idx.tobytes())
    if preview:
        im = Image.frombytes("P", (W, H), idx.tobytes())
        im.putpalette(pal.tobytes())
        im.convert("RGB").resize((W * 3, H * 3), Image.NEAREST).save(preview)
    print("%s: %dx%d" % (out, W, H))


main()
