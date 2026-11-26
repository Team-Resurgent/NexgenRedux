"""Makes the pieces that the scene editor builds with: caves, which join up, and platforms; and pictures.

Needs numpy. The results are kept in the repository: run this only to change them.

    python tools/make_pieces.py

Cave pieces have their floor at 0 and their ends 4 metres from the middle, or 8 for the room, so that they meet on a grid
of 4 metres. Every end is the same arch.
"""
import math
import os
import struct
import zlib

import numpy as np

from make_models import MEDIA, Mesh, ball, box, lathe, matter, put, rotation, rough, save

# The arch, from one foot over to the other, and then back along the floor.
ARCH = [(2.2, 0), (2.5, 0.9), (2.4, 1.9), (1.8, 2.8), (0.9, 3.3), (0, 3.45), (-0.9, 3.3), (-1.8, 2.8), (-2.4, 1.9), (-2.5, 0.9), (-2.2, 0)]
FLOOR = [(-1.1, 0), (0, 0), (1.1, 0)]
ROCK = (0.56, 0.51, 0.47)
EARTH = (0.50, 0.43, 0.36)


def ring():
    return [(x, y) for x, y in ARCH + FLOOR]


def shade(rng, base, spread=0.05):
    v = rng.uniform(-spread, spread)
    return [base[0] + v, base[1] + v, base[2] + v * 0.9, 1]


def tube(path, rng, closed_end=False):
    """path: (middle, the way right, how rough) at each section. Facets, seen from the inside."""
    m = Mesh()
    sections = []
    for middle, right, rough in path:
        s = []
        for x, y in ring():
            high = max(0, y) / 3.45
            k = 1 + rng.uniform(-0.10, 0.12) * rough * (1 if y > 0.1 else 0)
            lift = rng.uniform(-0.06, 0.06) * rough if y <= 0 and abs(x) < 2 else 0
            s.append(np.array(middle) + np.array(right) * (x * k) + np.array([0, y * (1 + (k - 1) * high) + lift, 0]))
        sections.append(s)
    n = len(ring())

    def face(a, b, c, inside):
        normal = np.cross(b - a, c - a)
        if np.dot(normal, inside - a) < 0:
            b, c, normal = c, b, -normal
        low = max(a[1], b[1], c[1]) - inside[1] < -1.2
        color = shade(rng, EARTH if low else ROCK)
        m.add([a, b, c], [normal] * 3, color, [(0, 1, 2)])
    for (sa, pa), (sb, pb) in zip(zip(sections, path), zip(sections[1:], path[1:])):
        inside = (np.array(pa[0]) + np.array(pb[0])) / 2 + np.array([0, 1.6, 0])
        for i in range(n):
            k = (i + 1) % n
            face(sa[i], sb[i], sa[k], inside)
            face(sa[k], sb[i], sb[k], inside)
    if closed_end:
        last = sections[-1]
        middle = sum(last) / n
        inside = np.array(path[-2][0]) + np.array([0, 1.6, 0])
        for i in range(n):
            face(last[i], last[(i + 1) % n], middle, inside)
    return m


def straight(steps, fall=0.0, length=8.0):
    out = []
    for i in range(steps + 1):
        t = i / steps
        # Level at both ends, so that the next piece goes on from it.
        ease = t * t * (3 - 2 * t)
        out.append(((0, -fall * ease, length / 2 - length * t), (1, 0, 0), 0 if i in (0, steps) else 1))
    return out


def bend(steps):
    out = []
    for i in range(steps + 1):
        a = math.pi / 2 * i / steps
        # About a corner, from the side towards +z to the side towards +x.
        out.append(((4 - 4 * math.cos(a), 0, 4 - 4 * math.sin(a)), (math.cos(a), 0, math.sin(a)), 0 if i in (0, steps) else 1))
    return out


def room(rng, r=8.0, doors=(0, 1, 2, 3), w=3.4):
    """Eight sides, those of the doors open; a dome over it. Doors face +z, +x, -z and -x: 0 to 3."""
    m = Mesh()

    def face(a, b, c, color, inside=np.array([0, 2.5, 0])):
        a, b, c = np.array(a, float), np.array(b, float), np.array(c, float)
        normal = np.cross(b - a, c - a)
        if np.dot(normal, inside - a) < 0:
            b, c, normal = c, b, -normal
        m.add([a, b, c], [normal] * 3, color, [(0, 1, 2)])

    def quad(a, b, c, d, base):
        color = shade(rng, base)
        face(a, b, c, color)
        face(a, c, d, shade(rng, base))
    top = 3.8
    corners = []
    for q in range(4):
        a = q * math.pi / 2
        c, s = math.cos(a), math.sin(a)
        at = lambda x, y, z: (x * c + z * s, y, -x * s + z * c)
        # The side with the way out: rock from the arch to the edge.
        outer = [(w, 0), (w, 1.5), (w, 3.0), (w, top), (1.7, top), (0, top), (-1.7, top), (-w, top), (-w, 3.0), (-w, 1.5), (-w, 0)]
        if q in doors:
            for i in range(len(ARCH) - 1):
                p, q2, o, o2 = ARCH[i], ARCH[i + 1], outer[i], outer[i + 1]
                quad(at(p[0], p[1], r), at(o[0], o[1], r), at(o2[0], o2[1], r), at(q2[0], q2[1], r), ROCK)
        else:
            # A wall, bulging out a little.
            for i in range(4):
                for k in range(3):
                    x0, x1, y0, y1 = -w + 2 * w * i / 4, -w + 2 * w * (i + 1) / 4, top * k / 3, top * (k + 1) / 3
                    out = lambda x, y: r + 0.5 * math.cos(x / w * math.pi / 2) * math.sin(y / top * math.pi)
                    quad(at(x0, y0, out(x0, y0)), at(x1, y0, out(x1, y0)), at(x1, y1, out(x1, y1)), at(x0, y1, out(x0, y1)), ROCK)
        # And the slanting side after it.
        a0, a1 = (w, r), (r, w)
        for i in range(4):
            for k in range(3):
                t0, t1, y0, y1 = i / 4, (i + 1) / 4, top * k / 3, top * (k + 1) / 3
                mix = lambda t: (a0[0] + (a1[0] - a0[0]) * t, a0[1] + (a1[1] - a0[1]) * t)
                bulge = lambda t, y: 1 + (0.06 * math.sin(t * math.pi) * math.sin(y / top * math.pi))
                pt = lambda t, y: at(mix(t)[0] * bulge(t, y), y, mix(t)[1] * bulge(t, y))
                quad(pt(t0, y0), pt(t1, y0), pt(t1, y1), pt(t0, y1), ROCK)
        corners += [at(w, 0, r), at(r, 0, w)]
    flat = [(x, z) for x, _, z in corners]
    # Floor and dome, from the rim in to the middle.
    rim = sorted(flat + [tuple(np.add(flat[i], flat[(i + 1) % 8]) / 2) for i in range(8)], key=lambda p: math.atan2(p[1], p[0]))
    levels = [(1.0, 0), (0.6, 0), (0.25, 0)]
    for up in (False, True):
        heights = [top, top + 0.5, top + 0.8] if up else [0, 0, 0]
        bump = lambda p, k: (rng.uniform(-0.15, 0.15) if up else rng.uniform(-0.05, 0.05)) if k else 0
        rings = [[(p[0] * f, heights[k] + bump(p, k), p[1] * f) for p in rim] for k, (f, _) in enumerate(levels)]
        base = ROCK if up else EARTH
        for k in range(2):
            for i in range(len(rim)):
                j = (i + 1) % len(rim)
                quad(rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i], base)
        middle = (0, heights[-1] + (0.2 if up else 0), 0)
        for i in range(len(rim)):
            face(rings[2][i], rings[2][(i + 1) % len(rim)], middle, shade(rng, base))
    return m


def mouth(rng, mine=False):
    """The way down, in a mound of rock; or of earth, with timber round it."""
    # Smooth where it comes through the land, and no wider than the opening in it, which is 4 metres.
    n = 16
    narrow = lambda t: 0.78 + 0.22 * min(1, max(0, (t - 0.55) / 0.45))
    path = [(at, (narrow(i / n), 0, 0), rough if i / n > 0.75 else 0) for i, (at, _, rough) in enumerate(straight(n, 5))]
    inside = tube(path, rng)
    m = Mesh()
    # The land is open for 6 metres: what is left of that beside the arch, and beyond it, is ground.
    grass = (0.30, 0.42, 0.24)

    def wide(t):
        high = 5 * t * t * (3 - 2 * t)       # the land's height over the floor there
        for (x0, y0), (x1, y1) in zip(ARCH[:5], ARCH[1:6]):
            if y0 <= high <= y1:
                return (x0 + (x1 - x0) * (high - y0) / (y1 - y0)) * narrow(t) + 0.03
        return 0.0
    for i in range(12):
        t0, t1 = i / n, (i + 1) / n
        z0, z1 = 4 - 8 * t0, 4 - 8 * t1
        inner = max(wide(t0), wide(t1))      # never inside the arch, which bends between
        for side in (-1, 1):
            a, b = (side * inner, 0.05, z0), (side * inner, 0.05, z1)
            c, d = (side * 2.3, 0.05, z1), (side * 2.3, 0.05, z0)
            points = [a, d, c, b] if side > 0 else [a, b, c, d]
            m.add(points, [(0, 1, 0)] * 4, shade(rng, grass, 0.02), [(0, 1, 2), (0, 2, 3)])
    # A mound of rock over it: a face round the way in, then back and down into the land, with grass on top.
    outline = [(4.6, -0.7), (4.1, 0.8), (3.6, 2.1), (2.7, 3.3), (1.4, 4.15), (0, 4.5), (-1.4, 4.15), (-2.7, 3.3), (-3.6, 2.1), (-4.1, 0.8), (-4.6, -0.7)]
    rows = []
    for i in range(8):
        t = i / 7 * 0.82
        high = 5 * t * t * (3 - 2 * t)
        f = max(0, (3.45 - high) / 3.45) ** 0.75
        row = []
        for k, (x, y) in enumerate(outline):
            foot = k in (0, len(outline) - 1)
            bump = 0 if foot else 0.22
            row.append(np.array([x * (0.72 + 0.28 * f) + rng.uniform(-bump, bump), (y / 4.5 * max(0, 4.5 - high) if y > 0 else y) - 0.3 * (i == 7) + rng.uniform(-bump, bump) * f * 0.5,
                                 4.25 - 8 * t + (rng.uniform(-0.25, 0.25) if i else rng.uniform(-0.2, 0.25))]))
        rows.append(row)
    # And down into the land behind.
    rows.append([np.array([q[0] * 0.9, -0.5, q[2] - 3.2]) for q in rows[-1]])

    def stone(a, b, c, out):
        normal = np.cross(b - a, c - a)
        if np.dot(normal, out) < 0:
            b, c, normal = c, b, -normal
        up = normal[1] / (np.linalg.norm(normal) or 1)
        m.add([a, b, c], [normal] * 3, shade(rng, (0.30, 0.42, 0.22) if up > 0.8 else (0.40, 0.42, 0.30) if up > 0.62 else (0.42, 0.33, 0.24) if mine else (0.50, 0.46, 0.41), 0.045), [(0, 1, 2)])
    middle = np.array([0, -4, 0])
    for a, b in zip(rows, rows[1:]):
        for k in range(len(outline) - 1):
            out = (a[k] + b[k + 1]) / 2 - middle
            out[2] = 0
            if b is rows[-1]:
                out = np.array([0, 0.6, -1])
            stone(a[k], b[k], a[k + 1], out)
            stone(a[k + 1], b[k], b[k + 1], out)
    # The face: from the arch out to the mound, with a lip between that stands forward.
    arch = [np.array([x * narrow(0), y, 4.0]) for x, y in ARCH]
    lip = [np.array([x * narrow(0) * 1.28 + rng.uniform(-0.12, 0.12), (y * 1.1 if y else -0.5) + rng.uniform(-0.1, 0.1) * (y > 0), 4.55 + rng.uniform(-0.15, 0.2)]) for x, y in ARCH]
    for inner, outer in ((arch, lip), (lip, rows[0])):
        for k in range(len(ARCH) - 1):
            stone(inner[k], outer[k], inner[k + 1], np.array([0, 0.2, 1]))
            stone(inner[k + 1], outer[k], outer[k + 1], np.array([0, 0.2, 1]))
    if mine:
        wood, dark = (0.46, 0.32, 0.18, 1), (0.36, 0.24, 0.14, 1)
        for side in (-1, 1):
            put(m, box(0.36, 3.5, 0.36), wood, (side * 1.95, 1.75, 4.95))
            put(m, box(0.3, 3.3, 0.3), dark, (side * 1.8, 1.4, 3.9))
            put(m, box(0.16, 1.3, 0.16), dark, (side * 1.5, 3.0, 4.95), rotation(0, 0, side * 45))
        put(m, box(5.0, 0.42, 0.42), wood, (0, 3.6, 4.95))
        put(m, box(3.9, 0.3, 0.3), dark, (0, 3.0, 3.9))
        for side in (-0.55, 0.55):
            put(m, box(0.1, 0.12, 2.6), (0.35, 0.35, 0.38, 1), (side, 0.1, 5.2))
        for i in range(4):
            put(m, box(1.7, 0.08, 0.22), dark, (0, 0.03, 4.2 + i * 0.65))
    for x, z, size in ((-3.4, 5.2, 0.55), (3.2, 5.0, 0.7), (-4.2, 4.0, 0.4), (4.3, 4.6, 0.35), (2.6, 5.6, 0.3)):
        p, n2, t2 = rough(ball(size, 7, 0.75), 0.18, rng)
        put(m, (p, n2, t2), shade(rng, (0.48, 0.45, 0.41), 0.05), (x, size * 0.35, z))
    return inside, m


def cap(rng):
    """Closes an end that nothing is on: a rough wall."""
    m = Mesh()
    edge = [np.array([x, y, 0.0]) for x, y in ring()]
    middle = [np.array([x * 0.5 + rng.uniform(-0.2, 0.2), 1.6 + (y - 1.6) * 0.5 + rng.uniform(-0.2, 0.2), -0.7 + rng.uniform(-0.3, 0.3)]) for x, y in ring()]
    centre = np.array([0, 1.6, -0.9])

    def face(a, b, c):
        normal = np.cross(b - a, c - a)
        if normal[2] < 0:
            b, c, normal = c, b, -normal
        m.add([a, b, c], [normal] * 3, shade(rng, ROCK), [(0, 1, 2)])
    n = len(edge)
    for i in range(n):
        k = (i + 1) % n
        face(edge[i], middle[i], edge[k])
        face(edge[k], middle[i], middle[k])
        face(middle[i], centre, middle[k])
    return m


# --- To stand on ---

def solid(w, h, d, color, top=None, bevel=0.08):
    m = Mesh()
    put(m, box(w, h, d, bevel), color, (0, h / 2, 0))
    if top:
        put(m, box(w * 0.94, 0.06, d * 0.94, 0.02), top, (0, h, 0))
    return m


def ramp():
    m = Mesh()
    w, h, d = 1.5, 2.0, 2.5
    p = [(-w, 0, d), (w, 0, d), (w, 0, -d), (-w, 0, -d), (w, h, -d), (-w, h, -d)]
    stone, side = (0.62, 0.58, 0.52, 1), (0.5, 0.47, 0.43, 1)

    def face(points, color):
        a, b, c = (np.array(q, float) for q in points[:3])
        n = np.cross(b - a, c - a)
        m.add(points, [n] * len(points), color, [(0, 1, 2)] + ([(0, 2, 3)] if len(points) == 4 else []))
    face([p[0], p[1], p[4], p[5]], stone)
    face([p[2], p[3], p[5], p[4]], side)
    face([p[1], p[2], p[4]], side)
    face([p[3], p[0], p[5]], side)
    face([p[3], p[2], p[1], p[0]], side)
    return m


def stairs():
    m = Mesh()
    for i in range(8):
        put(m, box(3, 0.25 * (i + 1), 0.5), (0.6 + 0.02 * (i % 2), 0.56 + 0.02 * (i % 2), 0.5, 1), (0, 0.125 * (i + 1), 1.75 - i * 0.5))
    return m


def bridge():
    m = Mesh()
    wood, dark = (0.5, 0.35, 0.2, 1), (0.36, 0.25, 0.15, 1)
    for i in range(20):
        z = -4.75 + i * 0.5
        put(m, box(2.8, 0.14, 0.46), tuple(c * (0.9 + 0.1 * (i * 7 % 3)) for c in wood[:3]) + (1,), (0, 0.07, z))
    for side in (-1.3, 1.3):
        put(m, box(0.2, 0.25, 10), dark, (side, -0.1, 0))
        put(m, box(0.1, 0.1, 10), dark, (side, 1.05, 0))
        for i in range(6):
            put(m, box(0.14, 1.1, 0.14), dark, (side, 0.55, -4.9 + i * 1.96))
    return m


def column():
    m = Mesh()
    put(m, lathe([(0.55, 0), (0.55, 0.3), (0.4, 0.4), (0.36, 3.5), (0.52, 3.62), (0.52, 3.9), (0, 3.9)], 10), (0.66, 0.63, 0.58, 1))
    return m


# --- Pictures: for things, and all round the sky ---

def png(folder, name, pixels):
    h, w, _ = pixels.shape
    rows = np.concatenate([np.zeros((h, 1), np.uint8), (np.clip(pixels, 0, 1) * 255 + 0.5).astype(np.uint8).reshape(h, -1)], 1)
    # Each pixel less the one before: far smaller for what is smooth.
    plain = rows[:, 1:].astype(int)
    less = plain.copy()
    less[:, 3:] -= plain[:, :-3]
    rows[:, 1:] = (less & 255).astype(np.uint8)
    rows[:, 0] = 1
    chunk = lambda kind, data: struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    folder = os.path.join(os.path.dirname(MEDIA), folder)
    os.makedirs(folder, exist_ok=True)
    data = chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows.tobytes(), 9)) + chunk(b'IEND', b'')
    with open(os.path.join(folder, name + '.png'), 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + data)
    print(name, w, h, len(data), 'bytes')


def wander(w, h, cells, rng, across=True):
    """Smooth noise that meets itself at its sides."""
    g = rng.random((cells + 1, cells * 2 if across else cells))
    y, x = np.mgrid[0:h, 0:w]
    fy, fx = y / h * cells, x / w * g.shape[1]
    iy, ix = fy.astype(int), fx.astype(int)
    ty, tx = fy - iy, fx - ix
    ty, tx = ty * ty * (3 - 2 * ty), tx * tx * (3 - 2 * tx)
    right, low = (ix + 1) % g.shape[1], np.minimum(iy + 1, cells)
    return (g[iy, ix] * (1 - tx) + g[iy, right] * tx) * (1 - ty) + (g[low, ix] * (1 - tx) + g[low, right] * tx) * ty


def skies(rng):
    w, h = 1024, 512
    up = 1 - np.mgrid[0:h, 0:w][0] / (h / 2)   # 1 above, 0 level, -1 below
    up = up[..., None]
    low, top, ground = np.array([0.80, 0.88, 0.94]), np.array([0.24, 0.48, 0.82]), np.array([0.55, 0.60, 0.58])
    day = np.where(up > 0, low + (top - low) * np.clip(up, 0, 1) ** 0.6, low + (ground - low) * np.clip(-up * 4, 0, 1))
    n = sum(wander(w, h, c, rng) / 2 ** k for k, c in enumerate((6, 12, 24, 48))) / 1.875
    cloud = np.clip((n - 0.5) * 5, 0, 1)[..., None] * np.clip(up * 6, 0, 1) * np.clip((1 - up) * 3, 0, 1)
    png('picture', 'sky-clouds', day * (1 - cloud * 0.85) + np.array([1, 1, 1]) * cloud * 0.85)
    night = np.array([0.02, 0.025, 0.06]) + np.array([0.05, 0.06, 0.12]) * np.clip(1 - np.abs(up), 0, 1) ** 4
    milky = np.exp(-((up[..., 0] - 0.45 - 0.25 * np.sin(np.mgrid[0:h, 0:w][1] / w * 2 * math.pi)) * 5) ** 2) * n
    night = night + milky[..., None] * np.array([0.10, 0.09, 0.14])
    for _ in range(1500):
        y, x = int(rng.integers(0, h)), int(rng.integers(0, w))
        night[y, x] += np.array([1, 0.97, 0.92]) * rng.random() ** 3 * 1.2
    png('picture', 'sky-stars', np.where(up < -0.02, np.array([0.02, 0.025, 0.05]), night))


def surfaces(rng):
    n = 256
    y, x = np.mgrid[0:n, 0:n]
    grain = sum(wander(n, n, c, rng, False) / 2 ** k for k, c in enumerate((8, 32))) / 1.5

    def blocks(high, wide, shift, gap, colors, joint):
        row = y // high
        sx = (x + row * shift) % n
        column = sx // wide
        edge = (y % high < gap) | (sx % wide < gap)
        pick = rng.random((n // high + 1, n // wide + 1))[row, column][..., None]
        a, b = np.array(colors[0]), np.array(colors[1])
        out = (a + (b - a) * pick) * (0.88 + grain[..., None] * 0.24)
        return np.where(edge[..., None], np.array(joint) * (0.9 + grain[..., None] * 0.2), out)
    png('picture', 'bricks', blocks(32, 64, 32, 3, ((0.58, 0.27, 0.20), (0.70, 0.36, 0.26)), (0.66, 0.62, 0.56)))
    png('picture', 'tiles', blocks(64, 64, 0, 3, ((0.74, 0.74, 0.72), (0.86, 0.85, 0.82)), (0.45, 0.45, 0.45)))
    png('picture', 'stones', blocks(64, 128, 64, 4, ((0.46, 0.46, 0.47), (0.60, 0.59, 0.57)), (0.30, 0.30, 0.30)))
    boards = blocks(32, 256, 0, 2, ((0.46, 0.31, 0.18), (0.60, 0.42, 0.26)), (0.24, 0.16, 0.10))
    png('picture', 'planks', boards * (0.92 + 0.08 * np.sin(x / 9.0 + 5 * grain * 3))[..., None])



# --- Roads, as tiles of 8 metres: their sides at x or z of 4 ---

ASPHALT, KERB, LINE, SIDE = (0.24, 0.25, 0.27), (0.75, 0.74, 0.70), (0.93, 0.92, 0.85), (0.56, 0.55, 0.52)


def tile(kind, rng):
    """Seen from above: a grid of cells, each road, kerb or pavement; and white lines down the middle."""
    m = Mesh()
    n = 32
    cell = 8 / n

    def what(x, z):
        # x, z from -4 to 4: how far from the middle of each way it goes.
        ways = {'straight': [(0, 1)], 'end': [(0, 1)], 'bend': [(1, 0), (0, 1)], 't': [(0, 1), (1, 0)], 'cross': [(0, 1), (1, 0)]}[kind]
        near = 9
        for wx, wz in ways:
            if kind == 'bend':
                # About the corner at x 4, z 4.
                near = min(near, abs(math.hypot(x - 4, z - 4) - 4))
            elif (wx, wz) == (0, 1):
                near = min(near, abs(x) if (kind != 'end' or z > -1.0) else math.hypot(x, z + 1.0))
            elif kind == 't':
                near = min(near, abs(z) if x > 0 else 9)
            else:
                near = min(near, abs(z))
        return near
    kinds = [[None] * n for _ in range(n)]
    for i in range(n):
        for k in range(n):
            x, z = -4 + (i + 0.5) * cell, -4 + (k + 0.5) * cell
            d = what(x, z)
            line = d < 0.1 and kind not in ('end', 'cross') and int(z + 8) % 2 == 0 and (kind != 't' or x < -2.8 or x > 2.8 or True)
            kinds[i][k] = LINE if line else ASPHALT if d < 2.8 else KERB if d < 3.05 else SIDE
    high = lambda c: 0.0 if c in (ASPHALT, LINE) else 0.12
    # Runs of the same along z, each one quad.
    for i in range(n):
        k = 0
        while k < n:
            e = k
            while e + 1 < n and kinds[i][e + 1] == kinds[i][k]:
                e += 1
            c, y = kinds[i][k], high(kinds[i][k])
            x0, x1, z0, z1 = -4 + i * cell, -4 + (i + 1) * cell, -4 + k * cell, -4 + (e + 1) * cell
            m.add([(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)], [(0, 1, 0)] * 4, c + (1,), [(0, 2, 1), (0, 3, 2)])
            k = e + 1
    # Where the kerb steps up.
    for i in range(n):
        for k in range(n):
            for di, dk in ((1, 0), (0, 1)):
                if i + di < n and k + dk < n and high(kinds[i][k]) != high(kinds[i + di][k + dk]):
                    x, z = -4 + (i + di) * cell, -4 + (k + dk) * cell
                    a2, b2 = ((x, z), (x, z + cell)) if di else ((x, z), (x + cell, z))
                    m.add([(a2[0], 0, a2[1]), (b2[0], 0, b2[1]), (b2[0], 0.12, b2[1]), (a2[0], 0.12, a2[1])], [(0, 1, 0)] * 4, KERB + (1,), [(0, 1, 2), (0, 2, 3)])
    # Underneath, so that it isn't seen through from below a slope.
    m.add([(-4, -0.3, -4), (4, -0.3, -4), (4, -0.3, 4), (-4, -0.3, 4)], [(0, -1, 0)] * 4, SIDE + (1,), [(0, 1, 2), (0, 2, 3)])
    return m



def png_alpha(folder, name, pixels):
    # As png(), with alpha: colour type 6.
    h, w, _ = pixels.shape
    raw = (np.clip(pixels, 0, 1) * 255 + 0.5).astype(np.uint8).reshape(h, -1)
    rows = np.concatenate([np.zeros((h, 1), np.uint8), raw], 1)
    chunk = lambda kind, data: struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    folder = os.path.join(os.path.dirname(MEDIA), folder)
    os.makedirs(folder, exist_ok=True)
    data = chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows.tobytes(), 9)) + chunk(b'IEND', b'')
    with open(os.path.join(folder, name + '.png'), 'wb') as f:
        f.write(bytes([137]) + b'PNG' + bytes([13, 10, 26, 10]) + data)


def wisp():
    # A little glowing spirit, from four sides (in front, its left, behind, its right) across, and two frames down
    # as it flickers: 64 pixels a picture, drawn four times larger and made smaller for soft edges.
    big = 4
    cell = 64 * big
    sheet = np.zeros((2 * cell, 4 * cell, 4))
    y, x = np.mgrid[0:cell, 0:cell].astype(float) / big
    for frame in range(2):
        for side in range(4):
            a = side * math.pi / 2
            sway = (1 if frame else -1) * 1.5
            # A drop: round above, drawn out into a flickering tail below.
            cx = 32 + sway * (y - 24) / 40 * (y > 24)
            body_r = np.where(y < 30, 17, 17 - (y - 30) * 0.42)
            wave = np.sin(y * 0.45 + frame * 2.2) * 2.2 * (y > 34)
            inside = ((x - cx - wave) ** 2 + ((y - 30) * (y < 30)) ** 2 < body_r ** 2) & (y > 12) & (y < 62)
            r = np.sqrt((x - 32) ** 2 + (y - 28) ** 2) / 20
            glow = np.clip(1.1 - r * 0.55, 0.45, 1)
            col = np.stack([0.55 * glow + 0.2, 0.85 * glow + 0.1, np.full_like(glow, 1.0)], -1)
            # Eyes, round with it: two in front, one from the side, none behind.
            for e in (-1, 1):
                ex = 32 + math.sin(a) * 9 + math.cos(a) * e * 6.5
                facing = math.cos(a) * 1 + 0.0
                seen = math.cos(a) > -0.2 and abs(math.sin(a) * 9 + math.cos(a) * e * 6.5) < 13 and not (math.cos(a) < 0.1 and e * math.sin(a) > 0)
                if seen:
                    eye = ((x - ex) / (2.4 if abs(math.cos(a)) > 0.5 else 1.6)) ** 2 + ((y - 25) / (4.2 - frame * 0.8)) ** 2 < 1
                    col[eye] = (0.08, 0.1, 0.25)
            pix = np.concatenate([col, inside[..., None].astype(float)], -1)
            sheet[frame * cell:(frame + 1) * cell, side * cell:(side + 1) * cell] = pix
    small = sheet.reshape(2 * 64, big, 4 * 64, big, 4)
    alpha = small[..., 3].mean(axis=(1, 3))
    color = (small[..., :3] * small[..., 3:4]).sum(axis=(1, 3)) / np.maximum(small[..., 3:4].sum(axis=(1, 3)), 1e-6)
    color[alpha == 0] = (0.6, 0.85, 1)
    png_alpha('picture', 'sprite-wisp', np.concatenate([color, alpha[..., None]], -1))

def main():
    rng = np.random.default_rng(11)
    two = matter(two_sided=True)
    two['name'] = 'indoors'      # no sun
    out = matter(two_sided=True)
    save('cave-tunnel', [(tube(straight(6), rng), two)])
    save('cave-bend', [(tube(bend(6), rng), two)])
    save('cave-slope', [(tube(straight(8, 5), rng), two)])
    save('cave-end', [(tube([((0, 0, 4 - i), (1 - max(0, i - 3) * 0.22, 0, 0), 0 if i == 0 else 1) for i in range(6)], rng, True), two)])
    save('cave-room', [(room(rng), two)])
    save('cave-cross', [(room(rng, 4.0, (0, 1, 2, 3), 2.6), two)])
    save('cave-fork', [(room(rng, 4.0, (0, 1, 2), 2.6), two)])
    save('cave-hall', [(room(rng, 8.0, (0, 2), 3.4), two)])
    save('cave-tunnel-2', [(tube(straight(6), np.random.default_rng(23)), two)])
    save('cave-bend-2', [(tube(bend(6), np.random.default_rng(29)), two)])
    save('cave-mouth', list(zip(mouth(rng), (two, out))))
    save('cave-mouth-mine', list(zip(mouth(rng, True), (two, out))))
    save('cave-cap', [(cap(rng), two)])
    save('platform', [(solid(4, 0.5, 4, (0.45, 0.4, 0.36, 1), (0.42, 0.6, 0.32, 1)), matter())])
    save('platform-round', [((lambda m: (put(m, lathe([(0, 0), (1.5, 0), (2, 0.35), (2, 0.5), (0, 0.5)], 20), (0.5, 0.52, 0.58, 1)), m)[1])(Mesh()), matter())])
    save('ramp', [(ramp(), matter())])
    save('stairs', [(stairs(), matter())])
    save('bridge', [(bridge(), matter())])
    save('column', [(column(), matter())])
    save('wall', [(solid(4, 2.5, 0.4, (0.6, 0.55, 0.48, 1)), matter())])
    for kind in ('straight', 'bend', 't', 'cross', 'end'):
        save('road-' + kind, [(tile(kind, rng), matter(two_sided=True))])
    skies(rng)
    surfaces(rng)


if __name__ == '__main__':
    main()
