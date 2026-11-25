"""Makes the showcase's 3D models, so all of them are the engine's own.

Needs numpy. The results are kept in the repository: run this only to change them.

    python tools/make_models.py
"""
import json
import math
import os
import struct

import numpy as np

MEDIA = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'NexgenRedux-Media', 'media', 'global', 'model')


# --- Geometry: lists of places, normals, colours, uvs and triangles ---

class Mesh:
    def __init__(self):
        self.p, self.n, self.c, self.j, self.w, self.t = [], [], [], [], [], []

    def add(self, p, n, color, tris, bones=None):
        base = len(self.p)
        for i in range(len(p)):
            self.p.append([float(x) for x in p[i]])
            length = math.sqrt(sum(x * x for x in n[i])) or 1
            self.n.append([float(x) / length for x in n[i]])
            self.c.append(list(color[i]) if isinstance(color[0], (list, tuple)) else list(color))
            weights = bones(p[i]) if callable(bones) else [(bones or 0, 1.0)]
            weights = (list(weights) + [(0, 0.0)] * 4)[:4]
            self.j.append([int(b) for b, _ in weights])
            self.w.append([float(w) for _, w in weights])
        self.t += [[base + a for a in tri] for tri in tris]

    def flat(self):
        # Each triangle its own colour and direction, for a faceted look.
        out = Mesh()
        for a, b, c in self.t:
            pa, pb, pc = (np.array(self.p[i]) for i in (a, b, c))
            n = np.cross(pb - pa, pc - pa)
            if np.linalg.norm(n) < 1e-9:
                continue
            for i in (a, b, c):
                out.p.append(self.p[i])
                out.n.append((n / np.linalg.norm(n)).tolist())
                out.c.append(self.c[a])
                out.j.append(self.j[i])
                out.w.append(self.w[i])
            out.t.append([len(out.p) - 3, len(out.p) - 2, len(out.p) - 1])
        return out


def rotation(x=0, y=0, z=0):
    x, y, z = (math.radians(v) for v in (x, y, z))
    rx = np.array([[1, 0, 0], [0, math.cos(x), -math.sin(x)], [0, math.sin(x), math.cos(x)]])
    ry = np.array([[math.cos(y), 0, math.sin(y)], [0, 1, 0], [-math.sin(y), 0, math.cos(y)]])
    rz = np.array([[math.cos(z), -math.sin(z), 0], [math.sin(z), math.cos(z), 0], [0, 0, 1]])
    return ry @ rx @ rz


def lathe(profile, around):
    """Turned about y: (radius, height) from the bottom up, outside to the right."""
    p, n, t = [], [], []
    for k, (r, y) in enumerate(profile):
        before, after = profile[max(k - 1, 0)], profile[min(k + 1, len(profile) - 1)]
        dr, dy = after[0] - before[0], after[1] - before[1]
        for i in range(around + 1):
            a = i / around * 2 * math.pi
            p.append((math.sin(a) * r, y, math.cos(a) * r))
            n.append((math.sin(a) * dy, -dr, math.cos(a) * dy))
    for k in range(len(profile) - 1):
        for i in range(around):
            a, b = k * (around + 1) + i, (k + 1) * (around + 1) + i
            t += [[a, a + 1, b + 1], [a, b + 1, b]]
    return p, n, t


def ball(radius, around=12, squash=1.0):
    rows = max(4, around // 2)
    profile = []
    for i in range(rows + 1):
        a = -math.pi / 2 + math.pi * i / rows
        profile.append((max(math.cos(a), 1e-4) * radius, math.sin(a) * radius * squash))
    p, _, t = lathe(profile, around)
    return p, [(x, y / (squash * squash), z) for x, y, z in p], t


def box(w, h, d, bevel=0.0):
    p, n, t = [], [], []
    if bevel <= 0:
        for axis in range(3):
            for sign in (-1, 1):
                u, v = (axis + 1) % 3, (axis + 2) % 3
                half = [w / 2, h / 2, d / 2]
                base = len(p)
                for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    q = [0, 0, 0]
                    q[axis], q[u], q[v] = sign * half[axis], a * half[u], b * half[v]
                    p.append(q)
                    m = [0, 0, 0]
                    m[axis] = sign
                    n.append(m)
                t += [[base, base + 1, base + 2], [base, base + 2, base + 3]] if sign > 0 else [[base, base + 2, base + 1], [base, base + 3, base + 2]]
        return p, n, t
    # A ball's directions pushed out to a smaller box, so the edges are round.
    q, m, t = ball(1, 12)
    inner = (w / 2 - bevel, h / 2 - bevel, d / 2 - bevel)
    sign = lambda x: 1 if x >= 0 else -1
    for (x, y, z) in q:
        p.append((sign(x) * inner[0] + x * bevel, sign(y) * inner[1] + y * bevel, sign(z) * inner[2] + z * bevel))
    q, t = list(q), list(t)
    for up in (-1, 1):
        # The poles have come apart: the flat between them.
        base = len(p)
        for x, z in ((-1, -1), (-1, 1), (1, 1), (1, -1)):
            p.append((x * inner[0], up * h / 2, z * inner[2]))
            q.append((0, up, 0))
        t += [[base, base + 1, base + 2], [base, base + 2, base + 3]] if up > 0 else [[base, base + 2, base + 1], [base, base + 3, base + 2]]
    return p, q, t


def smooth(p, tris):
    n = np.zeros((len(p), 3))
    q = np.array(p, float)
    for a, b, c in tris:
        f = np.cross(q[b] - q[a], q[c] - q[a])
        for i in (a, b, c):
            n[i] += f
    return n.tolist()


def placed(shape, at=(0, 0, 0), turn=None, size=(1, 1, 1)):
    p, n, t = shape
    m = turn if turn is not None else np.eye(3)
    s = np.array(size, float)
    return [(m @ (np.array(q) * s) + np.array(at)).tolist() for q in p], [(m @ (np.array(q) / s)).tolist() for q in n], t


def put(mesh, shape, color, at=(0, 0, 0), turn=None, size=(1, 1, 1), bones=None):
    p, n, t = placed(shape, at, turn, size)
    mesh.add(p, n, color, t, bones)


# --- Files ---

class Writer:
    def __init__(self):
        self.data, self.views, self.accessors = b'', [], []

    def put(self, array, kind, component, target=None, limits=False):
        raw = array.tobytes()
        self.data += b'\0' * (-len(self.data) % 4)
        view = {'buffer': 0, 'byteOffset': len(self.data), 'byteLength': len(raw)}
        if target:
            view['target'] = target
        self.data += raw
        self.views.append(view)
        acc = {'bufferView': len(self.views) - 1, 'componentType': component, 'count': len(array), 'type': kind}
        if component == 5121 and kind == 'VEC4' and target and array.dtype == np.uint8 and limits == 'unit':
            acc['normalized'] = True
        if limits is True:
            acc['min'], acc['max'] = array.min(0).tolist() if array.ndim > 1 else [float(array.min())], array.max(0).tolist() if array.ndim > 1 else [float(array.max())]
        self.accessors.append(acc)
        return len(self.accessors) - 1


def save(name, parts, bones=None, animations=None):
    """parts: (mesh, material). bones: (name, parent, place). animations: name -> (length, {bone: {'rotation'|'translation': [(time, value)]}})."""
    w = Writer()
    materials, primitives = [], []
    for mesh, material in parts:
        attributes = {
            'POSITION': w.put(np.array(mesh.p, np.float32), 'VEC3', 5126, 34962, True),
            'NORMAL': w.put(np.array(mesh.n, np.float32), 'VEC3', 5126, 34962),
            'COLOR_0': w.put((np.clip(np.array(mesh.c), 0, 1) * 255 + 0.5).astype(np.uint8), 'VEC4', 5121, 34962, 'unit'),
        }
        if bones:
            attributes['JOINTS_0'] = w.put(np.array(mesh.j, np.uint8), 'VEC4', 5121, 34962)
            attributes['WEIGHTS_0'] = w.put(np.array(mesh.w, np.float32), 'VEC4', 5126, 34962)
        index = w.put(np.array(mesh.t, np.uint16).reshape(-1), 'SCALAR', 5123, 34963)
        primitives.append({'attributes': attributes, 'indices': index, 'material': len(materials)})
        materials.append(material)
    doc = {'asset': {'version': '2.0', 'generator': 'Nexgen make_models.py'}, 'scene': 0, 'scenes': [{'nodes': [0]}], 'meshes': [{'name': name, 'primitives': primitives}], 'materials': materials}
    if bones:
        nodes = [{'name': name, 'mesh': 0, 'skin': 0}]
        worlds = []
        for i, (bone, parent, place) in enumerate(bones):
            node = {'name': bone, 'translation': list(map(float, place))}
            kids = [k + 1 for k, b in enumerate(bones) if b[1] == i]
            if kids:
                node['children'] = kids
            nodes.append(node)
            worlds.append(np.array(place, float) + (worlds[parent] if parent >= 0 else 0))
        doc['scenes'][0]['nodes'] = [0, 1]
        inverse = []
        for at in worlds:
            m = np.eye(4, dtype=np.float32)
            m[3, :3] = -at
            inverse.append(m)
        doc['skins'] = [{'joints': list(range(1, len(bones) + 1)), 'inverseBindMatrices': w.put(np.array(inverse).reshape(-1, 16), 'MAT4', 5126), 'skeleton': 1}]
        doc['nodes'] = nodes
        doc['animations'] = []
        names = [b[0] for b in bones]
        for title, tracks in (animations or {}).items():
            samplers, channels = [], []
            for bone, paths in tracks.items():
                for path, keys in paths.items():
                    times = w.put(np.array([k[0] for k in keys], np.float32), 'SCALAR', 5126, None, True)
                    values = w.put(np.array([k[1] for k in keys], np.float32), 'VEC4' if path == 'rotation' else 'VEC3', 5126)
                    channels.append({'sampler': len(samplers), 'target': {'node': names.index(bone) + 1, 'path': path}})
                    samplers.append({'input': times, 'output': values, 'interpolation': 'LINEAR'})
            doc['animations'].append({'name': title, 'samplers': samplers, 'channels': channels})
    else:
        doc['nodes'] = [{'name': name, 'mesh': 0}]
    w.data += b'\0' * (-len(w.data) % 4)
    doc['buffers'] = [{'byteLength': len(w.data)}]
    doc['bufferViews'], doc['accessors'] = w.views, w.accessors
    text = json.dumps(doc, separators=(',', ':')).encode()
    text += b' ' * (-len(text) % 4)
    os.makedirs(MEDIA, exist_ok=True)
    path = os.path.join(MEDIA, name + '.glb')
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 28 + len(text) + len(w.data)))
        f.write(struct.pack('<II', len(text), 0x4E4F534A) + text)
        f.write(struct.pack('<II', len(w.data), 0x004E4942) + w.data)
    print(name, sum(len(m.p) for m, _ in parts), 'vertices', sum(len(m.t) for m, _ in parts), 'triangles', os.path.getsize(path), 'bytes')


def matter(glow=False, two_sided=False, see=False, alpha=1.0):
    m = {'pbrMetallicRoughness': {'baseColorFactor': [1, 1, 1, alpha], 'metallicFactor': 0}}
    if glow:
        m['emissiveFactor'] = [1, 1, 1]
    if two_sided:
        m['doubleSided'] = True
    if see:
        m['alphaMode'] = 'BLEND'
    return m


# --- Things that grow ---

def rough(shape, amount, rng):
    """Pushes each place in or out; places that are the same stay the same."""
    p, _, t = shape
    moved = {}
    out = []
    for q in p:
        key = tuple(round(v, 3) for v in q)
        if key not in moved:
            moved[key] = 1 + rng.uniform(-amount, amount)
        out.append([v * moved[key] for v in q])
    return out, smooth(out, t), t


def pine(rng):
    m = Mesh()
    put(m, lathe([(0.24, -0.4), (0.17, 1.0), (0.12, 2.4)], 7), (0.36, 0.25, 0.16, 1))
    for i in range(4):
        low, size = 0.9 + i * 1.05, 1.75 - i * 0.36
        green = (0.12 + i * 0.03, 0.32 + i * 0.045, 0.16 + i * 0.02, 1)
        put(m, rough(lathe([(0.02, low + 0.22), (size, low), (size * 0.5, low + 0.75), (0.01, low + 1.7)], 10), 0.05, rng), green)
    return m.flat()


def oak(rng):
    m = Mesh()
    put(m, lathe([(0.42, -0.4), (0.25, 0.5), (0.2, 1.6), (0.14, 2.6)], 8), (0.4, 0.29, 0.19, 1))
    for at, size, shade in (((0, 3.2, 0), 1.5, 0.0), ((0.95, 2.7, 0.35), 1.05, -0.03), ((-0.85, 2.8, -0.35), 1.1, 0.02), ((0.1, 2.6, -0.95), 0.95, -0.02), ((-0.2, 3.0, 0.95), 1.0, 0.04), ((0.1, 4.3, 0.1), 0.95, 0.07)):
        put(m, rough(ball(size, 9, 0.85), 0.09, rng), (0.26 + shade, 0.46 + shade * 1.4, 0.16 + shade, 1), at)
    return m.flat()


def bush(rng):
    m = Mesh()
    for at, size in (((0, 0.35, 0), 0.6), ((0.5, 0.25, 0.2), 0.42), ((-0.45, 0.25, 0.1), 0.45), ((0.05, 0.25, -0.45), 0.4)):
        put(m, rough(ball(size, 8, 0.8), 0.1, rng), (0.24 + rng.uniform(0, 0.05), 0.42, 0.18, 1), at)
    return m.flat()


def grass(rng):
    # Thin blades from one spot, each bending outwards: dark at the root, light at the tip. Their normals
    # point up, so a tuft is lit as the land under it is.
    m = Mesh()
    for i in range(18):
        a = rng.uniform(0, math.tau)
        out = rng.uniform(0.02, 0.12)
        tall = rng.uniform(0.3, 0.6)
        lean = rng.uniform(0.08, 0.22)
        wide = rng.uniform(0.025, 0.04)
        bx, bz = math.cos(a) * out, math.sin(a) * out
        sx, sz = -math.sin(a) * wide, math.cos(a) * wide
        dx, dz = math.cos(a), math.sin(a)
        mid = (bx + dx * lean * 0.35, tall * 0.55, bz + dz * lean * 0.35)
        tip = (bx + dx * lean, tall, bz + dz * lean)
        pts = [(bx - sx, 0, bz - sz), (bx + sx, 0, bz + sz), (mid[0] + sx * 0.6, mid[1], mid[2] + sz * 0.6), (mid[0] - sx * 0.6, mid[1], mid[2] - sz * 0.6), tip]
        g = rng.uniform(-0.04, 0.04)
        root, middle, top = (0.17, 0.3 + g, 0.1, 1), (0.3, 0.48 + g, 0.17, 1), (0.52, 0.7 + g, 0.3, 1)
        m.add(pts, [(0, 1, 0)] * 5, [root, root, middle, middle, top], [[0, 1, 2], [0, 2, 3], [3, 2, 4]])
    return m


def rock(rng):
    m = Mesh()
    put(m, rough(ball(1, 9, 0.72), 0.2, rng), (0.45, 0.44, 0.43, 1), (0, 0.35, 0))
    out = m.flat()
    for i in range(0, len(out.p), 3):
        # Moss where it faces up.
        c = (0.36, 0.45, 0.27, 1) if out.n[i][1] > 0.75 else tuple(0.44 + rng.uniform(-0.04, 0.04) for _ in range(1))[0:1] * 3 + (1,)
        for k in range(3):
            out.c[i + k] = list(c)
    return out


def crystal(rng):
    m = Mesh()
    for at, size, lean in (((0, 0, 0), 1.0, (0, 0, 0)), ((0.35, 0, 0.15), 0.6, (12, 40, -24)), ((-0.32, 0, 0.1), 0.7, (8, 0, 20)), ((0.02, 0, -0.35), 0.5, (-24, 0, 4))):
        put(m, lathe([(0.17, -0.1), (0.24, 0.75), (0.001, 1.25)], 6), (0.3, 0.85, 1, 1), at, rotation(*lean), (size, size, size))
    return m.flat()


def mushroom(rng):
    m = Mesh()
    put(m, lathe([(0.12, -0.1), (0.08, 0.2), (0.07, 0.4)], 8), (0.92, 0.88, 0.8, 1))
    put(m, lathe([(0.06, 0.38), (0.34, 0.36), (0.28, 0.52), (0.12, 0.62), (0.001, 0.64)], 12), (0.85, 0.22, 0.16, 1))
    for a in range(5):
        x, z = math.sin(a * 1.256) * 0.2, math.cos(a * 1.256) * 0.2
        put(m, ball(0.05, 6, 0.6), (1, 0.97, 0.9, 1), (x, 0.555, z))
    return m


# --- Things that are built ---

def house():
    m, glass = Mesh(), Mesh()
    wall, wood, roof, stone = (0.88, 0.82, 0.7, 1), (0.36, 0.24, 0.15, 1), (0.62, 0.24, 0.18, 1), (0.5, 0.49, 0.47, 1)
    put(m, box(6.3, 0.9, 5.3), stone, (0, 0, 0))
    put(m, box(6, 2.6, 5), wall, (0, 1.7, 0))
    for x in (-3, 3):
        for z in (-2.5, 2.5):
            put(m, box(0.3, 2.7, 0.3), wood, (x, 1.75, z))
    for z in (-2.52, 2.52):
        put(m, box(6, 0.22, 0.12), wood, (0, 2.95, z))
    # The gables and the roof.
    for x, sign in ((-3, -1), (3, 1)):
        p = [(x, 3.0, -2.5), (x, 3.0, 2.5), (x, 5.0, 0)]
        m.add(p, [(sign, 0, 0)] * 3, wall, [[0, 1, 2]] if sign < 0 else [[0, 2, 1]])
    for z, tip in ((-1, 38.66), (1, -38.66)):
        put(m, box(7, 0.22, 3.9), roof, (0, 3.98, z * 1.47), rotation(x=-tip * 1.0 if z > 0 else -tip))
    put(m, box(0.8, 2.2, 0.8), stone, (1.8, 4.9, -0.9))
    put(m, box(1.1, 2.0, 0.14), wood, (0, 1.4, 2.55))
    put(m, ball(0.07, 8), (0.9, 0.75, 0.3, 1), (0.38, 1.4, 2.66))
    put(m, box(1.8, 0.5, 1.0), stone, (0, 0.1, 3.0))
    put(m, box(1.8, 0.25, 0.8), stone, (0, 0.0, 3.8))
    lit = (1, 0.82, 0.45, 1)
    for x, z, turned in ((-1.9, 2.53, 0), (1.9, 2.53, 0), (-1.5, -2.53, 0), (1.5, -2.53, 0), (-3.03, 0, 90), (3.03, 0, 90)):
        put(m, box(1.15, 1.15, 0.1), wood, (x, 1.8, z), rotation(y=turned))
        put(glass, box(0.95, 0.95, 0.14), lit, (x, 1.8, z), rotation(y=turned))
        put(m, box(0.08, 0.95, 0.17), wood, (x, 1.8, z), rotation(y=turned))
        put(m, box(0.95, 0.08, 0.17), wood, (x, 1.8, z), rotation(y=turned))
    return [(m, matter()), (glass, matter(glow=True))]


def lantern():
    m, lamp = Mesh(), Mesh()
    iron = (0.16, 0.17, 0.2, 1)
    put(m, lathe([(0.22, -0.2), (0.2, 0.15), (0.09, 0.3), (0.06, 2.4), (0.1, 2.5), (0.2, 2.6), (0.21, 2.64)], 8), iron)
    put(m, lathe([(0.26, 3.1), (0.28, 3.12), (0.001, 3.42)], 8), iron)
    for a in range(4):
        x, z = math.sin(a * math.pi / 2 + 0.785) * 0.2, math.cos(a * math.pi / 2 + 0.785) * 0.2
        put(m, box(0.035, 0.5, 0.035), iron, (x, 2.86, z))
    put(lamp, lathe([(0.001, 2.63), (0.15, 2.64), (0.18, 2.9), (0.13, 3.1), (0.001, 3.11)], 10), (1, 0.85, 0.5, 1))
    return [(m, matter()), (lamp, matter(glow=True))]


def well():
    m = Mesh()
    stone, wood = (0.52, 0.51, 0.5, 1), (0.42, 0.28, 0.17, 1)
    put(m, lathe([(1.0, -0.4), (1.0, 0.8), (0.75, 0.8), (0.75, -0.2)], 14), stone)
    put(m, lathe([(0.76, 0.1), (0.001, 0.1)], 14), (0.15, 0.3, 0.42, 1))
    for x in (-0.85, 0.85):
        put(m, box(0.16, 2.2, 0.16), wood, (x, 1.5, 0))
    put(m, lathe([(0.09, -1), (0.09, 1)], 8), wood, (0, 1.9, 0), rotation(z=90))
    for z, tip in ((-0.5, -40), (0.5, 40)):
        put(m, box(2.4, 0.1, 1.35), (0.6, 0.25, 0.19, 1), (0, 2.75, z), rotation(x=tip))
    put(m, lathe([(0.16, 1.15), (0.2, 1.45), (0.17, 1.45), (0.13, 1.17)], 10), wood)
    return m


def skimmer():
    """Something to race: pointed, with fins, and engines that glow. It faces +z."""
    hull, glow = Mesh(), Mesh()
    white, dark = (1, 1, 1, 1), (0.16, 0.17, 0.22, 1)
    # A body of rings along z: how wide, how tall, how high.
    rings = [(-1.5, 0.42, 0.2, 0.06), (-1.2, 0.62, 0.3, 0.08), (-0.4, 0.6, 0.36, 0.12), (0.5, 0.44, 0.27, 0.06), (1.5, 0.2, 0.12, 0.0), (2.0, 0.03, 0.02, -0.04)]
    n = 14
    p, t = [], []
    for z, w, h, up in rings:
        for i in range(n):
            a = i / n * 2 * math.pi
            c, s = math.cos(a), math.sin(a)
            p.append((c * w, up + s * h * (1 if s > 0 else 0.55), z))
    for k in range(len(rings) - 1):
        for i in range(n):
            a, b, c, d = k * n + i, k * n + (i + 1) % n, (k + 1) * n + i, (k + 1) * n + (i + 1) % n
            t += [[a, b, d], [a, d, c]]
    hull.add(p, smooth(p, t), white, t)
    back = len(p)
    put(hull, ball(0.36, 14, 0.75), (0.1, 0.16, 0.26, 1), (0, 0.32, 0.25), None, (0.8, 1, 1.5))
    for side in (-1, 1):
        # Wings, swept back, and what hangs from them.
        a = [(side * 0.4, 0.06, 0.7), (side * 1.35, -0.08, -0.5), (side * 1.4, -0.1, -1.25), (side * 0.5, 0.04, -1.1)]
        for y, up in ((0.045, 1), (-0.045, -1)):
            q = [(x, yy + y, z) for x, yy, z in a]
            face = [[0, 2, 1], [0, 3, 2]] if side * up > 0 else [[0, 1, 2], [0, 2, 3]]
            hull.add(q, [(0, up, 0)] * 4, (0.95, 0.95, 1, 1), face)
        put(hull, lathe([(0.02, -0.75), (0.2, -0.55), (0.22, 0.35), (0.12, 0.75), (0.001, 0.8)], 10), dark, (side * 1.3, -0.08, -0.65), rotation(x=90))
        put(glow, lathe([(0.001, -0.005), (0.16, 0), (0.001, 0.02)], 10), (0.4, 0.85, 1, 1), (side * 1.3, -0.08, -1.4), rotation(x=-90))
        # A fin.
        f = [(side * 0.38, 0.15, -0.55), (side * 0.55, 0.85, -1.45), (side * 0.5, 0.8, -1.75), (side * 0.42, 0.12, -1.4)]
        hull.add(f, [(side, 0.2, 0)] * 4, (0.9, 0.9, 0.95, 1), [[0, 1, 2], [0, 2, 3]] if side < 0 else [[0, 2, 1], [0, 3, 2]])
    put(glow, lathe([(0.001, 0), (0.3, 0.01), (0.001, 0.03)], 12), (0.4, 0.85, 1, 1), (0, 0.1, -1.53), rotation(x=-90), (1, 1, 0.6))
    return [(hull, matter(two_sided=True)), (glow, matter(glow=True, two_sided=True))]


def gate():
    """A ring to go through, 9 metres across, standing up and facing +z."""
    frame, light = Mesh(), Mesh()
    n = 28
    for mesh, ring, tube, color in ((frame, 4.6, 0.34, (0.3, 0.32, 0.4, 1)), (light, 4.28, 0.11, (1, 1, 1, 1))):
        p, q, t = [], [], []
        for i in range(n + 1):
            a = i / n * 2 * math.pi
            for k in range(9):
                b = k / 8 * 2 * math.pi
                d = (math.cos(a) * math.cos(b), math.sin(a) * math.cos(b), math.sin(b))
                p.append((math.cos(a) * ring + d[0] * tube, math.sin(a) * ring + d[1] * tube, d[2] * tube))
                q.append(d)
        for i in range(n):
            for k in range(8):
                a, b = i * 9 + k, (i + 1) * 9 + k
                t += [[a, b, b + 1], [a, b + 1, a + 1]]
        mesh.add(p, q, color, t)
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        put(frame, box(0.9, 0.9, 0.9, 0.2), (0.22, 0.23, 0.3, 1), (math.cos(a) * 4.6, math.sin(a) * 4.6, 0), rotation(z=math.degrees(a)))
    return [(frame, matter()), (light, matter(glow=True))]


def campfire():
    m = Mesh()
    for i in range(9):
        a = i / 9 * 2 * math.pi
        put(m, ball(0.2, 7, 0.8), (0.42, 0.41, 0.4, 1), (math.sin(a) * 0.75, 0.06, math.cos(a) * 0.75))
    for i in range(4):
        put(m, lathe([(0.08, -0.5), (0.08, 0.5)], 6), (0.3, 0.2, 0.13, 1), (0, 0.17, 0), rotation(x=62, y=i * 45))
    return m


# --- Someone to walk about ---

def quat(x=0, y=0, z=0):
    x, y, z = (math.radians(v) / 2 for v in (x, y, z))
    q = lambda ax, ay, az, aw: np.array([ax, ay, az, aw])
    def mul(a, b):
        return np.array([a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                         a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]])
    return mul(mul(q(0, math.sin(y), 0, math.cos(y)), q(math.sin(x), 0, 0, math.cos(x))), q(0, 0, math.sin(z), math.cos(z))).tolist()


def robot():
    # name, parent, place from the parent. It faces +z, its feet on the ground.
    bones = [('hips', -1, (0, 0.78, 0)), ('chest', 0, (0, 0.2, 0)), ('head', 1, (0, 0.46, 0)),
             ('arm.l', 1, (0.36, 0.3, 0)), ('hand.l', 3, (0.02, -0.3, 0)), ('arm.r', 1, (-0.36, 0.3, 0)), ('hand.r', 5, (-0.02, -0.3, 0)),
             ('leg.l', 0, (0.16, -0.06, 0)), ('foot.l', 7, (0, -0.34, 0)), ('leg.r', 0, (-0.16, -0.06, 0)), ('foot.r', 9, (0, -0.34, 0)), ('aerial', 2, (0, 0.36, 0))]
    at = []
    for _, parent, place in bones:
        at.append(np.array(place) + (at[parent] if parent >= 0 else 0))
    body, glow = Mesh(), Mesh()
    shell, dark, accent = (0.93, 0.94, 0.96, 1), (0.22, 0.25, 0.31, 1), (0.98, 0.52, 0.16, 1)

    def limb(top, bottom, r1, r2, color):
        # Bends smoothly where the two bones meet.
        a, b = at[top], at[bottom]
        length = float(a[1] - b[1])
        tail = 0.16
        profile = [(0.001, -length - tail), (r2, -length - tail + 0.04), (r2, -length - 0.03), (r2, -length + 0.04), (r1, -length * 0.45), (r1, -0.03), (0.001, 0.06)]
        def weights(q):
            t = min(max((a[1] - q[1] - length + 0.09) / 0.18, 0), 1)
            return [(top, 1 - t), (bottom, t)]
        put(body, lathe(profile, 12), color, (b[0], a[1], a[2]), bones=weights)

    put(body, box(0.4, 0.22, 0.3, 0.09), dark, at[0] + (0, 0, 0), bones=0)
    put(body, lathe([(0.001, -0.08), (0.2, -0.06), (0.3, 0.1), (0.31, 0.28), (0.22, 0.4), (0.001, 0.43)], 18), shell, at[1], None, (1, 1, 0.8), bones=1)
    put(body, lathe([(0.302, 0.08), (0.325, 0.12), (0.312, 0.16)], 18), accent, at[1], None, (1, 1, 0.8), bones=1)
    put(body, lathe([(0.09, 0.38), (0.09, 0.5)], 10), dark, at[1], bones=1)
    put(body, box(0.3, 0.32, 0.14, 0.06), accent, at[1] + (0, 0.2, -0.27), bones=1)
    put(body, box(0.58, 0.42, 0.46, 0.16), shell, at[2] + (0, 0.18, 0), bones=2)
    put(body, box(0.48, 0.28, 0.12, 0.055), dark, at[2] + (0, 0.17, 0.2), bones=2)
    for x in (-0.31, 0.31):
        put(body, lathe([(0.001, -0.04), (0.1, -0.03), (0.1, 0.03), (0.001, 0.04)], 12), accent, at[2] + (x, 0.18, 0), rotation(z=90), bones=2)
    put(body, lathe([(0.02, 0), (0.014, 0.2)], 6), dark, at[11] + (0, 0.02, 0), bones=11)
    put(glow, ball(0.055, 10), (1, 0.45, 0.2, 1), at[11] + (0, 0.25, 0), bones=11)
    for x in (-0.11, 0.11):
        put(glow, ball(0.062, 10), (0.35, 0.95, 1, 1), at[2] + (x, 0.18, 0.25), None, (1, 1.25, 0.5), bones=2)
    put(glow, ball(0.05, 10), (0.35, 0.95, 1, 1), at[1] + (0, 0.22, 0.24), None, (1, 1, 0.5), bones=1)
    for top, bottom in ((3, 4), (5, 6)):
        put(body, ball(0.115, 12), accent, at[top], bones=top)
        limb(top, bottom, 0.075, 0.085, dark)
        put(body, ball(0.1, 12), shell, at[bottom] + (0, -0.2, 0), bones=bottom)
    for top, bottom in ((7, 8), (9, 10)):
        limb(top, bottom, 0.1, 0.1, dark)
        put(body, box(0.2, 0.16, 0.34, 0.07), shell, at[bottom] + (0, -0.3, 0.05), bones=bottom)
        put(body, box(0.205, 0.05, 0.345, 0.02), accent, at[bottom] + (0, -0.355, 0.05), bones=bottom)

    def cycle(length, count, fn):
        return [(length * i / count, quat(**fn(2 * math.pi * i / count))) for i in range(count + 1)]

    def rise(length, count, fn):
        return [(length * i / count, (0, 0.78 + fn(2 * math.pi * i / count), 0)) for i in range(count + 1)]

    def gait(length, leg, knee, arm, elbow, lean, bob, sway):
        k = 16
        tracks = {
            'hips': {'translation': rise(length, k, lambda a: bob * (math.cos(2 * a) - 1) * 0.5), 'rotation': cycle(length, k, lambda a: dict(x=lean, y=sway * math.sin(a), z=sway * 0.4 * math.cos(a)))},
            'chest': {'rotation': cycle(length, k, lambda a: dict(x=lean * 0.4, y=-sway * 1.8 * math.sin(a)))},
            'head': {'rotation': cycle(length, k, lambda a: dict(x=-lean * 1.2, y=sway * math.sin(a), z=sway * 0.3 * math.cos(a)))},
            'aerial': {'rotation': cycle(length, k, lambda a: dict(x=-10 * math.sin(2 * a + 1) - lean))},
        }
        for side, phase in (('l', 0), ('r', math.pi)):
            tracks['leg.' + side] = {'rotation': cycle(length, k, lambda a, p=phase: dict(x=-leg * math.sin(a + p) - lean))}
            tracks['foot.' + side] = {'rotation': cycle(length, k, lambda a, p=phase: dict(x=knee * max(0, math.sin(a + p + 1.9)) ** 1.3 + 4))}
            tracks['arm.' + side] = {'rotation': cycle(length, k, lambda a, p=phase: dict(x=arm * math.sin(a + p), z=(7 if side == 'l' else -7)))}
            tracks['hand.' + side] = {'rotation': cycle(length, k, lambda a, p=phase: dict(x=-elbow - elbow * 0.4 * math.sin(a + p)))}
        return tracks

    def rest(length, extra=None):
        tracks = {
            'hips': {'translation': rise(length, 12, lambda a: 0.012 * math.sin(a) - 0.012)},
            'chest': {'rotation': cycle(length, 12, lambda a: dict(x=1.5 * math.sin(a)))},
            'head': {'rotation': cycle(length, 12, lambda a: dict(x=-2 * math.sin(a + 0.6), y=7 * math.sin(a)))},
            'aerial': {'rotation': cycle(length, 12, lambda a: dict(x=5 * math.sin(2 * a), z=5 * math.cos(a)))},
            'arm.l': {'rotation': cycle(length, 12, lambda a: dict(x=3 * math.sin(a), z=9 + 2 * math.sin(a)))},
            'arm.r': {'rotation': cycle(length, 12, lambda a: dict(x=3 * math.sin(a), z=-9 - 2 * math.sin(a)))},
            'hand.l': {'rotation': cycle(length, 12, lambda a: dict(x=-14))},
            'hand.r': {'rotation': cycle(length, 12, lambda a: dict(x=-14))},
        }
        tracks.update(extra or {})
        return tracks

    still = lambda length, **v: {'rotation': [(0, quat(**v)), (length, quat(**v))]}
    air = {'hips': still(0.6, x=8), 'chest': still(0.6, x=-6), 'head': still(0.6, x=-8), 'arm.l': still(0.6, x=-40, z=55), 'arm.r': still(0.6, x=-40, z=-55), 'hand.l': still(0.6, x=-30), 'hand.r': still(0.6, x=-30),
           'leg.l': still(0.6, x=-45), 'foot.l': still(0.6, x=60), 'leg.r': still(0.6, x=12), 'foot.r': still(0.6, x=38), 'aerial': still(0.6, x=20)}
    wave = rest(1.6, {'arm.r': {'rotation': cycle(1.6, 16, lambda a: dict(x=-8, z=-150 - 8 * math.sin(2 * a)))}, 'hand.r': {'rotation': cycle(1.6, 16, lambda a: dict(z=-18 * math.sin(2 * a) - 10))}, 'head': {'rotation': cycle(1.6, 16, lambda a: dict(z=6 * math.sin(a), y=-8))}})
    dance = gait(0.96, 12, 35, 50, 60, 0, 0.1, 16)
    save('robot', [(body, matter()), (glow, matter(glow=True))], bones, {
        'idle': rest(2.6), 'walk': gait(0.86, 32, 50, 28, 16, 3, 0.05, 7), 'run': gait(0.56, 50, 82, 52, 42, 12, 0.08, 10), 'jump': air, 'wave': wave, 'dance': dance})


def main():
    rng = np.random.default_rng(7)
    for name, make in (('pine', pine), ('oak', oak), ('bush', bush), ('rock', rock), ('mushroom', mushroom)):
        save(name, [(make(rng), matter())])
    save('crystal', [(crystal(rng), matter(glow=True))])
    save('house', house())
    save('lantern', lantern())
    save('well', [(well(), matter(two_sided=True))])
    save('skimmer', skimmer())
    save('gate', gate())
    save('campfire', [(campfire(), matter())])
    robot()
    save('grass', [(grass(np.random.default_rng(11)), matter(two_sided=True))])


if __name__ == '__main__':
    main()
