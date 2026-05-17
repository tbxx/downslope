#!/usr/bin/env python3
"""Generate Assets/icon.png — a stylized ↘ for Downslope.

Run from the repo root:
    .venv/bin/python tools/make_icon.py
"""

from PIL import Image, ImageDraw, ImageFilter
import math
import pathlib

SIZE = 1024
# Palette matches the plugin editor: travertine cream background, Le Corbusier red.
ACCENT = (178, 34, 34)            # Le Corbusier red — matches editor accent
ECHO_INK = (26, 26, 26)           # subtle charcoal echoes (drawing-paper feel)
BG_TOP = (242, 239, 229)          # warm cream, slight lift at top
BG_BOT = (221, 215, 199)          # darker travertine at bottom
SQUIRCLE_RADIUS_FRACTION = 0.2237 # macOS app icon corner radius


def vertical_gradient(size, top, bot):
    strip = Image.new("RGB", (1, size))
    for y in range(size):
        t = y / (size - 1)
        strip.putpixel((0, y), tuple(int(a + (b - a) * t) for a, b in zip(top, bot)))
    return strip.resize((size, size)).convert("RGBA")


def draw_arrow(draw, start, end, shaft_w, head_w, head_l, color):
    sx, sy = start
    ex, ey = end
    dx, dy = ex - sx, ey - sy
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L          # forward unit vector
    px, py = -uy, ux                 # perpendicular unit vector
    shx, shy = ex - ux * head_l, ey - uy * head_l   # shaft → head transition
    sw, hw = shaft_w / 2, head_w / 2

    # shaft (rectangle)
    draw.polygon([
        (sx  + px * sw, sy  + py * sw),
        (shx + px * sw, shy + py * sw),
        (shx - px * sw, shy - py * sw),
        (sx  - px * sw, sy  - py * sw),
    ], fill=color)

    # rounded cap at the start
    draw.ellipse((sx - sw, sy - sw, sx + sw, sy + sw), fill=color)

    # arrowhead (triangle)
    draw.polygon([
        (shx + px * hw, shy + py * hw),
        (ex, ey),
        (shx - px * hw, shy - py * hw),
    ], fill=color)


def main():
    bg = vertical_gradient(SIZE, BG_TOP, BG_BOT)

    # Bold main arrow — same generous geometry as the chosen variant ("A_bold").
    main_start = (200, 200)
    main_end   = (860, 860)
    main_w, head_w, head_l = 160, 380, 260

    # Echo arrows behind the main one — charcoal hairlines for a drawing-paper
    # feel on the cream background (warm glow would read poorly here).
    echoes = [
        # (offset, alpha, shaft_w, head_w, head_l, length_factor)
        ((-70,  -70),  55, 105, 290, 200, 0.85),
        ((-140,-140),  32,  75, 220, 155, 0.70),
        ((-210,-210),  16,  50, 165, 115, 0.55),
    ]

    arrows = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    adraw = ImageDraw.Draw(arrows)
    for (ox, oy), a, sw, hw, hl, lf in echoes:
        cx = (main_start[0] + main_end[0]) / 2 + ox
        cy = (main_start[1] + main_end[1]) / 2 + oy
        main_len = math.hypot(main_end[0] - main_start[0], main_end[1] - main_start[1])
        half = main_len / 2 * lf
        u = (1 / math.sqrt(2), 1 / math.sqrt(2))
        es = (cx - u[0] * half, cy - u[1] * half)
        ee = (cx + u[0] * half, cy + u[1] * half)
        draw_arrow(adraw, es, ee, sw, hw, hl, (*ECHO_INK, a))

    # Soft drop shadow under the main arrow — gentle depth on cream paper.
    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow)
    draw_arrow(sdraw,
               (main_start[0] + 10, main_start[1] + 10),
               (main_end[0]   + 10, main_end[1]   + 10),
               main_w, head_w, head_l, (0, 0, 0, 60))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=18))

    # Main arrow.
    main_layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    mdraw = ImageDraw.Draw(main_layer)
    draw_arrow(mdraw, main_start, main_end, main_w, head_w, head_l, (*ACCENT, 255))

    # Compose: bg → echoes → shadow → main.
    out = Image.alpha_composite(bg, arrows)
    out = Image.alpha_composite(out, shadow)
    out = Image.alpha_composite(out, main_layer)

    # Squircle clip (macOS standard corner radius for app icons).
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, SIZE, SIZE),
        radius=int(SIZE * SQUIRCLE_RADIUS_FRACTION),
        fill=255,
    )
    out.putalpha(mask)

    dest = pathlib.Path(__file__).parent.parent / "Assets" / "icon.png"
    dest.parent.mkdir(parents=True, exist_ok=True)
    out.save(dest)
    print(f"wrote {dest} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
