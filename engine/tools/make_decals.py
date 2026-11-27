"""Draws the pictures that decals lay over things: a scorch mark, a hole and a splash of paint, into global/picture.

    python tools/make_decals.py

Needs numpy. The splash is white, so a decal's colour makes it any colour.
"""
import math
import os
import struct
import zlib

import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'NexgenRedux-Media', 'media', 'global', 'picture')
SIZE = 128


def save(name, rgba):
    rgba = np.clip(rgba * 255 + 0.5, 0, 255).astype(np.uint8)
    rows = b''.join(b'\x00' + rgba[y].tobytes() for y in range(rgba.shape[0]))
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', rgba.shape[1], rgba.shape[0], 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')
    path = os.path.join(OUT, 'decal-%s.png' % name)
    open(path, 'wb').write(png)
    print(name, len(png), 'bytes')


def grid():
    y, x = np.mgrid[0:SIZE, 0:SIZE].astype(np.float64)
    x = (x + 0.5) / SIZE * 2 - 1
    y = (y + 0.5) / SIZE * 2 - 1
    return x, y, np.sqrt(x * x + y * y), np.arctan2(y, x)


def noise(rng, scale):
    # Smooth noise: a few octaves of random grids, made larger.
    total = np.zeros((SIZE, SIZE))
    for octave in range(4):
        cells = scale * 2 ** octave
        small = rng.random((cells + 1, cells + 1))
        at = np.linspace(0, cells, SIZE, endpoint=False)
        i = at.astype(int)
        f = at - i
        f = f * f * (3 - 2 * f)
        rows = small[i][:, i] * (1 - f)[None, :] + small[i][:, i + 1] * f[None, :]
        rows2 = small[i + 1][:, i] * (1 - f)[None, :] + small[i + 1][:, i + 1] * f[None, :]
        total += (rows * (1 - f)[:, None] + rows2 * f[:, None]) / 2 ** octave
    return total / 1.875


def scorch(rng):
    x, y, r, a = grid()
    edge = 0.62 + 0.22 * noise(rng, 3) + 0.06 * np.sin(a * 7)
    alpha = np.clip((edge - r) / 0.35, 0, 1) ** 1.3
    soot = 0.05 + 0.12 * noise(rng, 6)
    rgba = np.zeros((SIZE, SIZE, 4))
    rgba[..., 0] = soot * 1.1
    rgba[..., 1] = soot
    rgba[..., 2] = soot * 0.9
    rgba[..., 3] = alpha * (0.75 + 0.25 * noise(rng, 8))
    return rgba


def hole(rng):
    x, y, r, a = grid()
    rgba = np.zeros((SIZE, SIZE, 4))
    # Dark in the middle, a pale bruised rim, and cracks running out.
    middle = np.clip((0.2 - r) / 0.05, 0, 1)
    rim = np.exp(-((r - 0.27) / 0.08) ** 2) * (0.6 + 0.4 * noise(rng, 6))
    cracks = np.zeros((SIZE, SIZE))
    for i in range(7):
        angle = rng.random() * math.tau
        length = 0.45 + rng.random() * 0.45
        wobble = a + 0.15 * np.sin(r * 18 + i)
        d = np.abs(np.angle(np.exp(1j * (wobble - angle))))
        cracks = np.maximum(cracks, np.clip(1 - d * r * 40, 0, 1) * np.clip((length - r) / 0.2, 0, 1) * (r > 0.2))
    shade = 0.08 * middle + 0.55 * (1 - middle)
    rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = shade * (1 - 0.6 * cracks)
    rgba[..., 3] = np.clip(middle * 0.95 + rim * 0.55 + cracks * 0.8, 0, 1) * np.clip((0.95 - r) / 0.1, 0, 1)
    return rgba


def splash(rng):
    x, y, r, a = grid()
    edge = 0.45 + 0.16 * noise(rng, 3) + 0.05 * np.sin(a * 11 + 1)
    blob = np.clip((edge - r) / 0.03, 0, 1)
    # Drops thrown out from it.
    for i in range(14):
        angle = rng.random() * math.tau
        far = 0.55 + rng.random() * 0.33
        size = 0.02 + rng.random() * 0.05
        dx, dy = x - math.cos(angle) * far, y - math.sin(angle) * far
        blob = np.maximum(blob, np.clip((size - np.sqrt(dx * dx + dy * dy)) / 0.015, 0, 1))
    rgba = np.ones((SIZE, SIZE, 4))
    shade = 0.82 + 0.18 * noise(rng, 5)
    rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = shade
    rgba[..., 3] = blob * 0.95
    return rgba


def main():
    rng = np.random.default_rng(21)
    save('scorch', scorch(rng))
    save('hole', hole(rng))
    save('splash', splash(rng))


if __name__ == '__main__':
    main()
