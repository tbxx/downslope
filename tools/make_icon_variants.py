#!/usr/bin/env python3
"""Generate icon variants for design review.

    .venv/bin/python tools/make_icon_variants.py
"""

from PIL import Image, ImageDraw, ImageFilter
import math
import pathlib

SIZE = 1024
ACCENT = (224, 70, 38)
GLOW   = (255, 110, 60)
BG_TOP = (38, 36, 48)
BG_BOT = (10, 10, 14)
SQUIRCLE_R = int(SIZE * 0.2237)


def vertical_gradient(top, bot):
    strip = Image.new("RGB", (1, SIZE))
    for y in range(SIZE):
        t = y / (SIZE - 1)
        strip.putpixel((0, y), tuple(int(a + (b - a) * t) for a, b in zip(top, bot)))
    return strip.resize((SIZE, SIZE)).convert("RGBA")


def draw_arrow(draw, start, end, shaft_w, head_w, head_l, color):
    sx, sy = start
    ex, ey = end
    dx, dy = ex - sx, ey - sy
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    px, py = -uy, ux
    shx, shy = ex - ux * head_l, ey - uy * head_l
    sw, hw = shaft_w / 2, head_w / 2
    draw.polygon([
        (sx + px * sw, sy + py * sw),
        (shx + px * sw, shy + py * sw),
        (shx - px * sw, shy - py * sw),
        (sx - px * sw, sy - py * sw),
    ], fill=color)
    draw.ellipse((sx - sw, sy - sw, sx + sw, sy + sw), fill=color)
    draw.polygon([
        (shx + px * hw, shy + py * hw),
        (ex, ey),
        (shx - px * hw, shy - py * hw),
    ], fill=color)


def apply_squircle(img):
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, SIZE, SIZE), radius=SQUIRCLE_R, fill=255)
    img.putalpha(mask)
    return img


def render(name, *, start, end, shaft, head_w, head_l,
           echoes=(), glow_radius=42, glow_extra=(30, 50, 30),
           highlight=True):
    bg = vertical_gradient(BG_TOP, BG_BOT)

    # echoes
    echo_layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ed = ImageDraw.Draw(echo_layer)
    main_len = math.hypot(end[0] - start[0], end[1] - start[1])
    mid = ((start[0] + end[0]) / 2, (start[1] + end[1]) / 2)
    u = (1 / math.sqrt(2), 1 / math.sqrt(2))
    for (ox, oy), a, sw, hw, hl, lf in echoes:
        cx, cy = mid[0] + ox, mid[1] + oy
        half = main_len / 2 * lf
        es = (cx - u[0] * half, cy - u[1] * half)
        ee = (cx + u[0] * half, cy + u[1] * half)
        draw_arrow(ed, es, ee, sw, hw, hl, (*ACCENT, a))

    # glow
    glow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    draw_arrow(gd, start, end,
               shaft + glow_extra[0],
               head_w + glow_extra[1],
               head_l + glow_extra[2],
               (*GLOW, 200))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=glow_radius))

    # main arrow
    main = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    md = ImageDraw.Draw(main)
    draw_arrow(md, start, end, shaft, head_w, head_l, (*ACCENT, 255))

    out = Image.alpha_composite(bg, echo_layer)
    out = Image.alpha_composite(out, glow)
    out = Image.alpha_composite(out, main)

    if highlight:
        hl = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        hd = ImageDraw.Draw(hl)
        draw_arrow(hd, start, end,
                   max(20, shaft - 60),
                   max(40, head_w - 100),
                   max(30, head_l - 40),
                   (255, 195, 165, 90))
        hl = hl.filter(ImageFilter.GaussianBlur(radius=10))
        out = Image.alpha_composite(out, hl)

    out = apply_squircle(out)
    dest = pathlib.Path(__file__).parent.parent / "Assets" / "variants" / f"icon_{name}.png"
    dest.parent.mkdir(parents=True, exist_ok=True)
    out.save(dest)
    print(f"wrote {dest}")


# ---- Variants ----------------------------------------------------------------

# A: "Bold" — uniformly scaled up from the current design, keeps echoes + highlight
render("A_bold",
       start=(200, 200), end=(860, 860),
       shaft=160, head_w=380, head_l=260,
       echoes=[
           ((-70,  -70), 120, 105, 290, 200, 0.85),
           ((-140,-140),  65,  75, 220, 155, 0.70),
           ((-210,-210),  30,  50, 165, 115, 0.55),
       ])

# B: "Chunky head" — moderate shaft, very aggressive arrowhead, only 2 echoes
render("B_chunky_head",
       start=(220, 220), end=(820, 820),
       shaft=140, head_w=460, head_l=340,
       echoes=[
           ((-80,  -80),  95, 90, 340, 250, 0.80),
           ((-160,-160),  40, 60, 240, 175, 0.62),
       ],
       glow_extra=(40, 80, 50), glow_radius=55,
       highlight=False)

# C: "Edge-to-edge" — corner-to-corner, no echoes, all about the arrow itself
render("C_edge",
       start=(160, 160), end=(900, 900),
       shaft=190, head_w=440, head_l=290,
       echoes=(),
       glow_extra=(35, 60, 35), glow_radius=50)

# D: "Solid icon" — bold, no echoes, no highlight, strong outer glow, very iconic
render("D_solid",
       start=(220, 220), end=(840, 840),
       shaft=170, head_w=400, head_l=270,
       echoes=(),
       glow_extra=(50, 90, 60), glow_radius=70,
       highlight=False)
