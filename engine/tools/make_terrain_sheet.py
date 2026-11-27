"""The Tiles demo's sheet: grass, then water and a dirt path as terrains of 16 frames each, one for each set of sides that
join (up 1, right 2, down 4, left 8). 32 pixels a tile, 8 across. make_terrain_sheet.py out.png"""
import random, sys
from PIL import Image, ImageDraw

T = 32
COLS, ROWS = 8, 5
sheet = Image.new('RGBA', (T * COLS, T * ROWS), (0, 0, 0, 0))


def grass(seed):
    im = Image.new('RGBA', (T, T), (78, 140, 70, 255))
    d = ImageDraw.Draw(im)
    rnd = random.Random(seed)
    for _ in range(26):
        x, y = rnd.randrange(T), rnd.randrange(T)
        c = rnd.choice([(96, 162, 80), (66, 122, 60), (110, 172, 86)])
        d.line([(x, y), (x + rnd.choice([-1, 0, 1]), y - 2)], fill=c + (255,))
    return im


def region(d, joins, margin, radius, fill, outline=None):
    """The part of a tile a terrain covers: to the edge on sides that join, `margin` short of it on the rest, with the
    corners between two open sides rounded."""
    up, right, down, left = joins & 1, joins & 2, joins & 4, joins & 8
    box = (0 if left else margin, 0 if up else margin, T - 1 - (0 if right else margin), T - 1 - (0 if down else margin))
    corners = (not up and not left, not up and not right, not down and not right, not down and not left)
    x0, y0, x1, y1 = box
    centres = ((x0 + radius, y0 + radius), (x1 - radius, y0 + radius), (x1 - radius, y1 - radius), (x0 + radius, y1 - radius))
    for py in range(y0, y1 + 1):
        for px in range(x0, x1 + 1):
            inside = True
            for k in range(4):
                cx, cy = centres[k]
                beyond = (px < cx if k in (0, 3) else px > cx) and (py < cy if k in (0, 1) else py > cy)
                if corners[k] and beyond and (px - cx) ** 2 + (py - cy) ** 2 > radius * radius:
                    inside = False
            if inside:
                d.point((px, py), fill=fill)


def frame(at, im):
    sheet.paste(im, ((at % COLS) * T, (at // COLS) * T))


frame(0, grass(0))
for joins in range(16):
    # Water: a sandy shore, then the water, with a line of foam inside the shore.
    im = grass(joins + 1)
    d = ImageDraw.Draw(im)
    region(d, joins, 3, 10, (214, 196, 140, 255))
    region(d, joins, 7, 8, (58, 120, 196, 255))
    region(d, joins, 9, 7, (150, 200, 240, 255))
    region(d, joins, 10, 6, (52, 108, 182, 255))
    rnd = random.Random(100 + joins)
    for _ in range(4):
        x, y = rnd.randrange(10, 22), rnd.randrange(10, 22)
        d.line([(x, y), (x + 3, y)], fill=(110, 170, 225, 255))
    frame(1 + joins, im)

    # A path: dirt with a darker rim.
    im = grass(joins + 40)
    d = ImageDraw.Draw(im)
    region(d, joins, 4, 9, (120, 90, 58, 255))
    region(d, joins, 6, 7, (168, 130, 88, 255))
    rnd = random.Random(200 + joins)
    for _ in range(8):
        x, y = rnd.randrange(8, 24), rnd.randrange(8, 24)
        d.point((x, y), fill=(140, 108, 72, 255))
    frame(17 + joins, im)

sheet.save(sys.argv[1])
print('made', sys.argv[1], sheet.size)
