#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""The opening (app.c): a scene in big pixels, and a maned wolf walking into it, from two pictures.

  opening.py [--preview DIR]     writes opening.h; --preview: the scene and the wolf, as PNGs

art/opening-scene.webp, the cerrado at dusk: the wolf coming into it at its left and its title are taken out (the
wolf's place mirrored from the scene on his right, the title's filled with the sky across: app.c writes the name in
its own letters), then a square of it comes down to 80 x 80 pixels (each 3 x 3 on the screen). art/opening.webp gives
the wolf: his body, head and tail inside an outline drawn here (on its 240 screen, a third of it here), a little
darker for the dusk; his legs are drawn by app.c, each two bones from hip or shoulder to foot, walking, and settling
as this picture has them. Both share 32 colours. Needs numpy and Pillow."""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance

HERE = Path(__file__).resolve().parent.parent
N = 80                                    # the pixels a side (3 x 3 on the screen each)
COLOURS = 32
CLEAR = 255                               # a sprite's pixel that is not his
# the scene (600 x 670): the wolf coming in at its left, its title; the square kept (from this row)
SCENE_WOLF = [(0, 248), (40, 258), (84, 244), (98, 249), (106, 272), (114, 247), (127, 254), (126, 298), (139, 328),
              (150, 352), (148, 370), (133, 380), (104, 383), (78, 394), (52, 404), (40, 416), (58, 450), (88, 494),
              (113, 518), (126, 540), (112, 552), (78, 550), (58, 522), (28, 482), (0, 474)]
SCENE_TITLE = [(150, 55), (440, 55), (440, 130), (150, 130)]
SCENE_TOP = 35
# the wolf's picture: his body, head and tail on its 240 screen (a third of it here)
BODY = [(34, 172), (35, 159), (43, 143), (51, 135), (63, 124), (78, 115), (90, 108), (111, 104), (129, 102), (145, 92),
        (160, 86), (167, 84), (178, 65), (186, 65), (188, 77), (191, 67), (198, 67), (197, 90), (190, 93), (197, 101),
        (205, 106), (204, 109), (195, 112), (181, 113), (176, 121), (169, 129), (169, 142), (164, 152), (150, 154),
        (135, 153), (120, 153), (100, 155), (88, 160), (75, 165), (65, 166), (55, 169), (45, 176), (37, 177)]
DUSK = (0.86, 0.80, 0.84)                 # his colours, a little darker and redder: the scene's light
# his legs as that picture has them (here, in its 80 pixels): hip or shoulder, knee, foot. Far hind, far fore, near
# hind, near fore
LEGS = [("far hind", (31, 54), (33, 60), (37, 66)), ("far fore", (44, 51), (43, 58), (41, 66)),
        ("near hind", (24, 53), (21, 59), (19, 66)), ("near fore", (52, 52), (57, 58), (63, 66))]
FEET = 66                                 # where they stand in it
GROUND = 67                               # where they stand in the scene
STOP = -12                                # where he stops: clear of the sun, his tail at the scene's edge


def poly_mask(pts, w, h, scale=1.0):
    m = Image.new("L", (w, h), 0)
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


def scene_of(src):
    """the scene without the wolf coming in and without its title; its square, 80 pixels a side"""
    im = np.asarray(Image.open(src).convert("RGB")).astype(float)
    h, w = im.shape[:2]
    wolf, title = poly_mask(SCENE_WOLF, w, h), poly_mask(SCENE_TITLE, w, h)
    out = im.copy()
    for y in range(h):                   # where he was: each row mirrored from the scene on his right
        xs = np.where(wolf[y])[0]
        if len(xs):
            x1 = xs.max() + 1
            for x in xs:
                out[y, x] = im[y, min(w - 1, x1 + 2 + (x1 - 1 - x))]
    for y in range(h):                   # the title: the sky across it, from either side
        xs = np.where(title[y])[0]
        if len(xs):
            a, b = im[y, xs.min() - 4:xs.min() - 1].mean(axis=0), im[y, xs.max() + 2:xs.max() + 5].mean(axis=0)
            t = (xs - xs.min() + 1) / (len(xs) + 1)
            out[y, xs] = a * (1 - t[:, None]) + b * t[:, None]
    sq = Image.fromarray(out.round().astype(np.uint8)).crop((0, SCENE_TOP, w, SCENE_TOP + w))
    return sq.resize((N, N), Image.BOX)


def rgb565(c):
    return (int(c[0]) >> 3) << 11 | (int(c[1]) >> 2) << 5 | int(c[2]) >> 3


def c_list(a, per=32):
    flat = [str(v) for v in np.asarray(a).ravel()]
    return ",\n".join("    " + ", ".join(flat[i:i + per]) for i in range(0, len(flat), per))


def vivid(im):
    return ImageEnhance.Contrast(ImageEnhance.Color(im).enhance(1.15)).enhance(1.08)


def build(scene_src, wolf_src, out, preview):
    scene = np.asarray(vivid(scene_of(scene_src)))
    wolf_pic = np.asarray(vivid(Image.open(wolf_src).convert("RGB").resize((N, N), Image.BOX))).astype(float)
    wolf_pic = np.clip(wolf_pic * DUSK, 0, 255).astype(np.uint8)
    # his body: inside the outline, but not its picture's purple sky (his fur has little blue; his white chest is bright)
    r, g, bl = (wolf_pic[..., i].astype(int) for i in range(3))
    body = poly_mask(BODY, N, N, N / 240) & ~((bl > 50) & (bl * 5 > r * 2) & (g < 110))
    ys, xs = np.where(body)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    # one palette for the scene and him
    both = Image.fromarray(np.concatenate([scene, wolf_pic], axis=1))
    q = both.quantize(COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    idx = np.asarray(q)
    scene_i, wolf_i = idx[:, :N], idx[:, N:]
    pal = np.array(q.getpalette()[:COLOURS * 3]).reshape(-1, 3)
    body_pix = np.where(body[y0:y1, x0:x1], wolf_i[y0:y1, x0:x1], CLEAR)
    # his legs' colours, as in his picture (and as dark): the near ones black below their rust fur, the far darker
    legs = np.clip(np.array([(22, 18, 18), (52, 34, 28), (150, 48, 12), (96, 36, 18)]) * DUSK, 0, 255)
    pal = np.vstack([pal, legs])
    lines = ["/* SPDX-License-Identifier: MPL-2.0 */",
             "/* generated by tools/opening.py from art/opening-scene.webp and art/opening.webp: the opening (app.c),",
             " * 80 x 80 pixels (3 x 3 on the screen each) of 32 colours: the scene, the wolf's body, his legs as his",
             " * picture has them. OP_NEAR, OP_FAR: his legs' colours (OP_NEAR_FUR, OP_FAR_FUR: at their top);",
             " * OP_CLEAR: not his */",
             f"#define OP_N {N}",
             f"#define OP_NEAR {COLOURS}u",
             f"#define OP_FAR {COLOURS + 1}u",
             f"#define OP_NEAR_FUR {COLOURS + 2}u",
             f"#define OP_FAR_FUR {COLOURS + 3}u",
             f"#define OP_CLEAR {CLEAR}u",
             f"#define OP_DY {GROUND - FEET}                  /* his picture's ground to the scene's */",
             f"#define OP_STOP {STOP}               /* where he stops, from where his picture has him */",
             f"static const uint16_t OP_PAL[{len(pal)}] = {{",
             c_list([rgb565(c) for c in pal], 12), "};",
             f"static const uint8_t OP_SCENE[{N * N}] = {{   /* the scene */", c_list(scene_i), "};",
             f"#define OP_BODY_X {x0}",
             f"#define OP_BODY_Y {y0}",
             f"#define OP_BODY_W {x1 - x0}",
             f"#define OP_BODY_H {y1 - y0}",
             f"static const uint8_t OP_BODY[{(x1 - x0) * (y1 - y0)}] = {{   /* his body, head and tail */",
             c_list(body_pix, x1 - x0), "};",
             "/* his legs in his picture: hip or shoulder, knee, foot (far hind, far fore, near hind, near fore) */",
             "static const int8_t OP_LEG[4][6] = {"]
    for name, h, k, f in LEGS:
        lines.append(f"    {{{h[0]}, {h[1]}, {k[0]}, {k[1]}, {f[0]}, {f[1]}}},   /* {name} */")
    lines.append("};\n")
    out.write_text("\n".join(lines))
    print(f"opening: {2 * len(pal) + N * N + body_pix.size + 24} B of flash -> {out}")
    if preview:
        preview.mkdir(parents=True, exist_ok=True)
        pal_rgb = np.vstack([pal, [[255, 0, 255]] * (256 - len(pal))]).astype(np.uint8)
        Image.fromarray(pal_rgb[scene_i]).resize((480, 480), Image.NEAREST).save(preview / "scene.png")
        bp = pal_rgb[wolf_i].copy()
        bp[~body] = bp[~body] // 4
        Image.fromarray(bp).resize((480, 480), Image.NEAREST).save(preview / "body.png")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--scene", type=Path, default=HERE / "art" / "opening-scene.webp")
    ap.add_argument("--wolf", type=Path, default=HERE / "art" / "opening.webp")
    ap.add_argument("--out", type=Path, default=HERE / "opening.h")
    ap.add_argument("--preview", type=Path)
    a = ap.parse_args()
    build(a.scene, a.wolf, a.out, a.preview)


if __name__ == "__main__":
    main()
