# Writes palette.png, the one texture the landscape demo uses for every colour.
#   Rows 0..191:   terrain. u = height (-30 m .. 700 m), v = slope (flat at the top, steep at row 191).
#   Rows 192..255: solid swatches, a quarter of the width each, 32 px tall:
#     row 192: dark foliage | light foliage | bark | rock
#     row 224: water        | (spare, neutral grey ×3)
# Run: python3 example/landscape/makePalette.py   (keep in sync with world.cpp's height range and swatch UVs)
import os
import numpy as np
from PIL import Image

W, H = 1024, 256        # wide, so a 1 m beach still gets its own texels (730 m / 1024 px)
img = np.zeros((H, W, 3))

def lerp(a, b, t):
    return np.asarray(a) + (np.asarray(b) - np.asarray(a)) * t

def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)

sand, lushGrass, dryGrass = (194, 178, 128), (74, 112, 45), (118, 128, 62)
rockGrass, rock, snow = (112, 108, 88), (118, 112, 104), (236, 240, 245)
cliff = (96, 90, 84)
for x in range(W):
    h = -30 + (x + 0.5) / W * 730.0            # metres
    for y in range(192):
        s = (y + 0.5) / 192.0                  # 0 flat .. 1 steep
        c = lerp(sand, lushGrass, smooth(0.3, 1.5, h))
        c = lerp(c, dryGrass, smooth(100, 200, h))
        c = lerp(c, rockGrass, smooth(200, 300, h))
        c = lerp(c, rock, smooth(300, 380, h))
        c = lerp(c, cliff, smooth(0.45, 0.7, s))                     # steep: bare rock at any height
        c = lerp(c, snow, smooth(390, 440, h) * (1 - smooth(0.55, 0.8, s)))   # snow, except on the sheerest faces
        img[y, x] = c

swatches = {(0, 192): (34, 70, 32), (1, 192): (58, 98, 40), (2, 192): (92, 64, 44),
            (3, 192): (128, 124, 118), (0, 224): (40, 78, 104),
            (1, 224): (110, 106, 100), (2, 224): (110, 106, 100), (3, 224): (110, 106, 100)}   # spare: neutral
for (col, row), c in swatches.items():
    img[row:row + 32, col * W // 4:(col + 1) * W // 4] = c

Image.fromarray(img.astype(np.uint8)).save(os.path.join(os.path.dirname(__file__), "palette.png"))
