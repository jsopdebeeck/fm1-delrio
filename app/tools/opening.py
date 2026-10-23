#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""The opening (app.c): a picture in big pixels, and its maned wolf walking into it, from art/opening.webp.

  opening.py [--preview DIR]     writes opening.h; --preview: the picture, the scene without him and his body, as PNGs

The picture comes down to 80 x 80 pixels of 32 colours (each 3 x 3 on the screen). The wolf is cut from it: his body,
head and tail inside an outline drawn here (the screen's 240 coordinates, a third of them here); his legs are taken
out and drawn by app.c, each two bones from its hip or shoulder to its foot, walking. Behind him the scene is filled
in, a row at a time, from the scene on either side: the sky is in horizontal bands. Where he stands at the end, the
picture itself. Needs numpy and Pillow."""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance

HERE = Path(__file__).resolve().parent.parent
N = 80                                    # the picture's pixels a side (3 x 3 on the screen each)
COLOURS = 32
CLEAR = 255                               # a sprite's pixel that is not his
# his body, head and tail, on the 240 screen (a third of it here)
BODY = [(34, 172), (35, 159), (43, 143), (51, 135), (63, 124), (78, 115), (90, 108), (111, 104), (129, 102), (145, 92),
        (160, 86), (167, 84), (178, 65), (186, 65), (188, 77), (191, 67), (198, 67), (197, 90), (190, 93), (197, 101),
        (205, 106), (204, 109), (195, 112), (181, 113), (176, 121), (169, 129), (169, 142), (164, 152), (150, 154),
        (135, 153), (120, 153), (100, 155), (88, 160), (75, 165), (65, 166), (55, 169), (45, 176), (37, 177)]
# his legs as the picture has them (here, in its 80 pixels): hip or shoulder, knee, foot. Far hind, far fore, near
# hind, near fore; and the corridors on the 240 screen where they are, to take them out of the scene
LEGS = [
    ("far hind", (31, 54), (33, 60), (37, 66), [(86, 154), (103, 154), (117, 201), (103, 201), (91, 176)]),
    ("far fore", (44, 51), (43, 58), (41, 66), [(125, 148), (141, 148), (137, 176), (135, 201), (117, 201), (123, 170)]),
    ("near hind", (24, 53), (21, 59), (19, 66), [(60, 158), (82, 158), (76, 182), (67, 201), (51, 201), (57, 182)]),
    ("near fore", (52, 52), (57, 58), (63, 66), [(144, 147), (167, 147), (183, 174), (197, 197), (195, 201), (177, 201),
                                                (165, 178), (149, 165)]),
]


def poly_mask(pts, n, scale):
    m = Image.new("L", (n, n), 0)
    ImageDraw.Draw(m).polygon([(x * scale, y * scale) for x, y in pts], fill=1)
    return np.asarray(m).astype(bool)


def grow(m, n):
    """the mask n pixels wider (its edge pixels are half his)"""
    for _ in range(n):
        g = m.copy()
        g[1:] |= m[:-1]
        g[:-1] |= m[1:]
        g[:, 1:] |= m[:, :-1]
        g[:, :-1] |= m[:, 1:]
        m = g
    return m


def fill(rgb, hole):
    """the scene where he stood: each row's gap from the scene on either side of it (the middle of 3 pixels just past
    its edge), the one blending into the other across"""
    out = rgb.astype(float).copy()
    n = rgb.shape[0]
    for y in range(n):
        row, x = hole[y], 0
        while x < n:
            if not row[x]:
                x += 1
                continue
            x1 = x
            while x1 < n and row[x1]:
                x1 += 1
            left = [out[y, k] for k in range(x - 1, -1, -1) if not row[k]][1:4]
            right = [out[y, k] for k in range(x1, n) if not row[k]][1:4]
            a = np.median(left or right, axis=0)
            b = np.median(right or left, axis=0)
            for k in range(x, x1):
                t = (k - x + 1) / (x1 - x + 1)
                out[y, k] = a * (1 - t) + b * t
            x = x1
    return out.round().astype(np.uint8)


def rgb565(c):
    return (int(c[0]) >> 3) << 11 | (int(c[1]) >> 2) << 5 | int(c[2]) >> 3


def c_list(a, per=32):
    flat = [str(v) for v in np.asarray(a).ravel()]
    return ",\n".join("    " + ", ".join(flat[i:i + per]) for i in range(0, len(flat), per))


def build(src, out, preview):
    big = Image.open(src).convert("RGB")
    small = big.resize((N, N), Image.BOX)
    small = ImageEnhance.Contrast(ImageEnhance.Color(small).enhance(1.15)).enhance(1.08)
    pic = np.asarray(small)
    # what is his: the outline (his body), and the legs' corridors' dark pixels and fur, found on the 240 picture
    fine = np.asarray(big.resize((240, 240), Image.BOX)).astype(int)
    r, g, b = fine[..., 0], fine[..., 1], fine[..., 2]
    yy = np.mgrid[0:240, 0:240][0]
    legs240 = np.zeros((240, 240), bool)
    for _, _, _, _, corridor in LEGS:
        legs240 |= poly_mask(corridor, 240, 1) & (((r < 95) & (g < 70) & (b < 70)) |
                                                 ((r > 110) & (b < 50) & (g < 85) & (r - g > 70) & (yy < 166)))
    legs = Image.fromarray(legs240.astype(np.uint8) * 255).resize((N, N), Image.BOX)
    body = grow(poly_mask(BODY, N, N / 240), 1)
    wolf = grow(body | (np.asarray(legs) > 40), 1)
    scene = fill(pic, wolf)
    both = Image.fromarray(np.concatenate([pic, scene], axis=1))
    q = both.quantize(COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    idx = np.asarray(q)
    pic_i, scene_i = idx[:, :N], idx[:, N:]
    pal = np.array(q.getpalette()[:COLOURS * 3]).reshape(-1, 3)
    ys, xs = np.where(body)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    body_pix = np.where(body[y0:y1, x0:x1], pic_i[y0:y1, x0:x1], CLEAR)
    # the legs' colours, as in the picture: the near ones black below their rust fur, the far ones darker
    pal = np.vstack([pal, [(22, 18, 18), (52, 34, 28), (150, 48, 12), (96, 36, 18)]])
    lines = ["/* SPDX-License-Identifier: MPL-2.0 */",
             "/* generated by tools/opening.py from art/opening.webp: the opening (app.c), 80 x 80 pixels of 32 colours",
             " * (3 x 3 on the screen each): the picture, the scene without the wolf, his body; his legs as the picture",
             " * has them. OP_NEAR, OP_FAR: his legs' colours (OP_NEAR_FUR, OP_FAR_FUR: at their top); OP_CLEAR: not his */",
             f"#define OP_N {N}",
             f"#define OP_NEAR {COLOURS}u",
             f"#define OP_FAR {COLOURS + 1}u",
             f"#define OP_NEAR_FUR {COLOURS + 2}u",
             f"#define OP_FAR_FUR {COLOURS + 3}u",
             f"#define OP_CLEAR {CLEAR}u",
             f"static const uint16_t OP_PAL[{len(pal)}] = {{",
             c_list([rgb565(c) for c in pal], 12), "};",
             f"static const uint8_t OP_PIC[{N * N}] = {{   /* the picture */", c_list(pic_i), "};",
             f"static const uint8_t OP_SCENE[{N * N}] = {{   /* the scene without him */", c_list(scene_i), "};",
             f"#define OP_BODY_X {x0}",
             f"#define OP_BODY_Y {y0}",
             f"#define OP_BODY_W {x1 - x0}",
             f"#define OP_BODY_H {y1 - y0}",
             f"static const uint8_t OP_BODY[{(x1 - x0) * (y1 - y0)}] = {{   /* his body, head and tail */",
             c_list(body_pix, x1 - x0), "};",
             "/* his legs in the picture: hip or shoulder, knee, foot (far hind, far fore, near hind, near fore) */",
             "static const int8_t OP_LEG[4][6] = {"]
    for name, h, k, f, _ in LEGS:
        lines.append(f"    {{{h[0]}, {h[1]}, {k[0]}, {k[1]}, {f[0]}, {f[1]}}},   /* {name} */")
    lines.append("};\n")
    out.write_text("\n".join(lines))
    size = 2 * len(pal) + 2 * N * N + body_pix.size + 24
    print(f"opening: {size} B of flash -> {out}")
    if preview:
        preview.mkdir(parents=True, exist_ok=True)
        pal_rgb = np.vstack([pal, [[255, 0, 255]] * (256 - len(pal))]).astype(np.uint8)
        for name, a in (("picture", pic_i), ("scene", scene_i)):
            Image.fromarray(pal_rgb[a]).resize((480, 480), Image.NEAREST).save(preview / f"{name}.png")
        bp = pal_rgb[pic_i].copy()
        bp[~body] = bp[~body] // 4
        Image.fromarray(bp).resize((480, 480), Image.NEAREST).save(preview / "body.png")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", type=Path, default=HERE / "art" / "opening.webp")
    ap.add_argument("--out", type=Path, default=HERE / "opening.h")
    ap.add_argument("--preview", type=Path)
    a = ap.parse_args()
    build(a.src, a.out, a.preview)


if __name__ == "__main__":
    main()
