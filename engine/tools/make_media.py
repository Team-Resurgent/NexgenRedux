"""Makes the showcase's sounds, music and test images, so all of them are the engine's own.

Needs numpy, Pillow and ffmpeg. The results are kept in the repository: run this only to change them.

    python tools/make_media.py
"""
import os
import subprocess
import sys
import wave

import numpy as np

RATE = 44100
MEDIA = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'NexgenRedux-Media', 'media')
rng = np.random.default_rng(20)


def seconds(n):
    return np.arange(int(n * RATE)) / RATE


def tone(freq, length, shape='sine', vibrato=0.0):
    t = seconds(length)
    f = freq if np.isscalar(freq) else np.interp(t, np.linspace(0, length, len(freq)), freq)
    phase = 2 * np.pi * np.cumsum(np.full(len(t), f) if np.isscalar(f) else f) / RATE
    phase = phase + vibrato * np.sin(2 * np.pi * 5 * t)
    if shape == 'sine':
        return np.sin(phase)
    if shape == 'tri':
        return 2 / np.pi * np.arcsin(np.sin(phase))
    if shape == 'saw':
        # A few harmonics only, so it doesn't alias.
        return sum(np.sin(phase * k) / k for k in range(1, 9)) * 0.6
    return sum(np.sin(phase * k) / k for k in range(1, 12, 2)) * 0.9


def fall(length, decay, attack=0.003):
    t = seconds(length)
    return np.minimum(t / attack, 1) * np.exp(-t * decay) * np.minimum((length - t) / 0.01, 1)


def noise(length):
    return rng.uniform(-1, 1, int(length * RATE))


def smooth(x, width):
    return np.convolve(x, np.ones(width) / width, 'same')


def save_wav(path, x, peak=0.8):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    x = np.asarray(x)
    x = x / max(np.abs(x).max(), 1e-9) * peak
    data = (np.clip(x, -1, 1) * 32767).astype('<i2')
    with wave.open(path, 'wb') as f:
        f.setnchannels(1 if data.ndim == 1 else 2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(data.tobytes())


def compress(wav, out, *args):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav, '-map_metadata', '-1', '-bitexact', *args, out], check=True)
    os.remove(wav)


def effects():
    def save(name, x, peak=0.8):
        save_wav(os.path.join(MEDIA, 'global', 'sound', name + '.wav'), x, peak)

    save('move', tone(660, 0.05, 'tri') * fall(0.05, 60), 0.35)
    save('accept', np.concatenate([tone(660, 0.05, 'tri') * fall(0.05, 30), tone(990, 0.12, 'tri') * fall(0.12, 25)]), 0.5)
    save('back', np.concatenate([tone(520, 0.05, 'tri') * fall(0.05, 30), tone(390, 0.12, 'tri') * fall(0.12, 25)]), 0.5)
    save('pop', tone([420, 900], 0.09) * fall(0.09, 28))
    save('hit', (tone([300, 180], 0.12, 'square') * 0.6 + noise(0.12) * 0.4) * fall(0.12, 32), 0.7)
    save('wall', tone([220, 160], 0.09, 'tri') * fall(0.09, 35), 0.6)
    save('coin', np.concatenate([tone(988, 0.07, 'square') * fall(0.07, 10), tone(1319, 0.3, 'square') * fall(0.3, 10)]), 0.4)
    save('boom', (smooth(noise(0.8), 40) * 3 + tone([110, 35], 0.8)) * fall(0.8, 6), 0.9)
    save('whoosh', smooth(noise(0.35), 12) * np.sin(np.pi * seconds(0.35) / 0.35) ** 2, 0.6)
    save('win', np.concatenate([tone(f, 0.14, 'square') * fall(0.14, 9) for f in (523, 659, 784)] + [tone(1047, 0.5, 'square') * fall(0.5, 6)]), 0.5)
    save('lose', np.concatenate([tone(f, 0.2, 'tri') * fall(0.2, 6) for f in (392, 349, 311)] + [tone(262, 0.6, 'tri', 0.4) * fall(0.6, 4)]), 0.6)
    save('kick', tone([170, 45, 40], 0.28) * fall(0.28, 14, 0.001), 0.95)
    save('snare', (noise(0.2) * 0.8 + tone(190, 0.2) * 0.5) * fall(0.2, 20, 0.001), 0.7)
    save('hat', np.diff(noise(0.07), prepend=0) * fall(0.07, 60, 0.001), 0.4)
    save('bell', sum(tone(440 * r, 1.4) * fall(1.4, d) * a for r, d, a in ((1, 3, 1), (2.76, 5, 0.5), (5.4, 8, 0.25))), 0.6)
    save('pluck', tone(220, 0.6, 'saw') * fall(0.6, 8), 0.6)

    # For going on and on: the end runs into the beginning.
    def loop(x, over=0.25):
        n = int(over * RATE)
        ramp = np.linspace(0, 1, n)
        x = x.copy()
        x[:n] = x[:n] * np.sqrt(ramp) + x[-n:] * np.sqrt(1 - ramp)
        return x[:-n]

    t = seconds(2.25)
    buzz = sum(np.sin(2 * np.pi * 55 * k * t + np.sin(2 * np.pi * 7 * t) * 0.15 * k) / k for k in (1, 2, 3, 4, 6))
    save('engine', loop(buzz * 0.5 + smooth(noise(2.25), 18) * 0.9), 0.6)
    crackle = smooth(noise(4.25), 60) * 1.6
    for _ in range(70):
        at = rng.integers(0, len(crackle) - 2000)
        crackle[at:at + 1200] += np.diff(noise(0.04), prepend=0)[:1200] * np.exp(-np.arange(1200) / 120) * rng.uniform(0.2, 1)
    save('fire', loop(crackle), 0.5)
    t = seconds(6.25)
    swell = 0.35 + 0.65 * np.sin(np.pi * t / 6) ** 2
    save('waves', loop(smooth(noise(6.25), 9) * swell + smooth(noise(6.25), 90) * 2 * swell), 0.5)
    save('step', (smooth(noise(0.09), 14) * 1.5 + tone([140, 70], 0.09)) * fall(0.09, 48), 0.45)
    save('splash', smooth(noise(0.7), 5) * fall(0.7, 7, 0.01), 0.6)
    save('boost', (smooth(noise(1.1), 10) * 1.2 + tone([120, 420], 1.1, 'saw') * 0.35) * np.sin(np.pi * np.minimum(seconds(1.1) / 1.1, 1)) ** 1.3, 0.6)
    save('gate', np.concatenate([tone(784, 0.06, 'tri') * fall(0.06, 10), tone(1175, 0.35, 'tri') * fall(0.35, 8)]), 0.5)
    save('case', (np.diff(noise(0.12), prepend=0) * 0.7 + tone([900, 500], 0.12, 'tri') * 0.4) * fall(0.12, 38, 0.001), 0.5)
    # Without noise, so the others stay as they were.
    save('laser', tone([1800, 520], 0.09, 'square') * fall(0.09, 30, 0.001) * 0.6 + tone([3600, 1040], 0.09) * fall(0.09, 45, 0.001) * 0.3, 0.32)
    save('zap', tone([480, 1150], 0.16, 'tri', 0.3) * fall(0.16, 14, 0.002), 0.4)


NOTE = {n: i for i, n in enumerate(('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'))}


def hz(name):
    return 440 * 2 ** ((NOTE[name[:-1]] + 12 * (int(name[-1]) + 1) - 69) / 12)


class Track:
    """Notes are added round the end, so the loop has no seam."""

    def __init__(self, length):
        self.data = np.zeros((int(length * RATE), 2))

    def add(self, at, x, volume=1.0, pan=0.0):
        start = int(at * RATE)
        index = (start + np.arange(len(x))) % len(self.data)
        self.data[index, 0] += x * volume * np.cos((pan + 1) * np.pi / 4)
        self.data[index, 1] += x * volume * np.sin((pan + 1) * np.pi / 4)


def tune(path):
    beat = 60 / 112
    bars = [('A2', 'A3 C4 E4 A4'), ('F2', 'F3 A3 C4 F4'), ('C2', 'G3 C4 E4 G4'), ('G2', 'G3 B3 D4 G4'),
            ('A2', 'A3 C4 E4 A4'), ('F2', 'F3 A3 C4 F4'), ('D2', 'A3 D4 F4 A4'), ('E2', 'G#3 B3 E4 G#4')] * 2
    melody = 'E5 . . D5 C5 . A4 . | C5 . . A4 F4 . . . | G4 . C5 . E5 . D5 . | D5 . . B4 G4 . . . | ' \
             'E5 . . D5 C5 . A4 . | C5 . D5 . F5 . E5 . | D5 . . F5 A5 . G5 . | E5 . . . G#4 . B4 .'
    melody = melody.replace('|', '').split() * 2
    track = Track(len(bars) * 4 * beat)
    for i, (bass, chord) in enumerate(bars):
        at = i * 4 * beat
        notes = chord.split()
        for n in notes[:3]:
            pad = tone(hz(n), beat * 4.4, 'tri', 0.1) * np.minimum(1, np.minimum(seconds(beat * 4.4) * 3, (beat * 4.4 - seconds(beat * 4.4)) * 3))
            track.add(at, pad, 0.09, 0.4 if n == notes[0] else -0.4)
        for k in range(8):
            length = 0.22
            track.add(at + k * beat / 2, tone(hz(bass) * (2 if k % 4 == 3 else 1), length, 'saw') * fall(length, 7), 0.3)
        for k in range(16):
            n = notes[(0, 1, 2, 3, 2, 1, 3, 2)[k % 8]]
            track.add(at + k * beat / 4, tone(hz(n) * 2, 0.2, 'square') * fall(0.2, 18), 0.07 if i >= 4 else 0.04, 0.6 * np.sin(k))
        for k in range(4):
            track.add(at + k * beat, tone([170, 45, 40], 0.28) * fall(0.28, 14, 0.001), 0.55)
            track.add(at + (k + 0.5) * beat, np.diff(noise(0.06), prepend=0) * fall(0.06, 60, 0.001), 0.12, 0.3)
            if k % 2:
                track.add(at + k * beat, (noise(0.2) * 0.7 + tone(190, 0.2) * 0.5) * fall(0.2, 20, 0.001), 0.28)
    for k, n in enumerate(melody):
        if n != '.' and k >= 32:
            for delay, volume in ((0, 0.2), (beat * 0.75, 0.07)):
                track.add(k * beat / 2 + delay, tone(hz(n), 0.6, 'tri', 0.15) * fall(0.6, 4), volume, 0.2 if delay == 0 else -0.5)
    wav = path + '.wav'
    save_wav(wav, track.data, 0.85)
    compress(wav, path, '-c:a', 'libvorbis', '-q:a', '3')


def ambient(path):
    track = Track(24)
    for i, chord in enumerate(('D3 A3 E4', 'B2 F#3 D4', 'G2 D3 B3', 'A2 E3 C#4')):
        for n in chord.split():
            t = seconds(9)
            swell = np.sin(np.pi * t / 9) ** 2
            track.add(i * 6, (tone(hz(n), 9, 'tri', 0.2) + tone(hz(n) * 1.004, 9) * 0.7) * swell, 0.2, rng.uniform(-0.6, 0.6))
    for i in range(30):
        n = rng.choice(['D5', 'E5', 'F#5', 'A5', 'B5', 'D6'])
        track.add(rng.uniform(0, 24), tone(hz(n), 1.6) * fall(1.6, 3, 0.01), 0.06, rng.uniform(-0.9, 0.9))
    wav = path + '.wav'
    save_wav(wav, track.data, 0.7)
    compress(wav, path, '-c:a', 'libmp3lame', '-b:a', '96k')


def pulse(freq, length, width=0.5, vibrato=0.0):
    """A pulse wave from its harmonics, so that it doesn't alias."""
    t = seconds(length)
    phase = 2 * np.pi * freq * t + vibrato * np.sin(2 * np.pi * 6 * t)
    count = max(1, min(14, int(RATE / 2 / freq) - 1))
    return sum(np.sin(np.pi * k * width) / k * np.cos(phase * k) for k in range(1, count + 1)) * 0.6


def chip(path):
    """Four channels, as a home computer would: arpeggios for chords, a lead, a bass and noise."""
    step = 60 / 138 / 4
    bars = [('A', 'A4 C5 E5'), ('A', 'A4 C5 E5'), ('F', 'F4 A4 C5'), ('G', 'G4 B4 D5'),
            ('A', 'A4 C5 E5'), ('C', 'G4 C5 E5'), ('D', 'F4 A4 D5'), ('E', 'G#4 B4 E5')] * 2
    lead = ('E5 . E5 G5 A5 . G5 E5 D5 . E5 . . . . . | E5 . E5 G5 A5 . C6 . B5 . A5 . G5 . E5 . | '
            'F5 . F5 A5 C6 . A5 F5 C6 . B5 . A5 . G5 . | G5 . G5 B5 D6 . B5 G5 E5 . . . D5 . E5 . | '
            'A5 . . C6 . . E6 . D6 . C6 . A5 . . . | G5 . . C6 . . E6 . D6 . C6 . G5 . . . | '
            'F5 . A5 . D6 . A5 . F5 . A5 . D6 . F6 . | E6 . . . B5 . G#5 . E5 . G#5 . B5 . E6 .').replace('|', ' ').split() * 2
    track = Track(len(bars) * 16 * step)
    for i, (root, chord) in enumerate(bars):
        notes = chord.split()
        at = i * 16 * step
        for k in range(16):
            octave = 3 if k % 4 == 2 else 2
            track.add(at + k * step, pulse(hz(root + str(octave)), step * 0.95, 0.25) * fall(step * 0.95, 9, 0.002), 0.34, -0.15)
            # A chord is one note after another, too fast to tell apart.
            for j in range(3):
                length = step / 3
                track.add(at + k * step + j * length, pulse(hz(notes[j]), length, 0.5) * np.minimum(1, (length - seconds(length)) * 400), 0.13, 0.5)
            if k % 2 == 0:
                track.add(at + k * step, noise(0.03) * fall(0.03, 90, 0.001), 0.10, 0.2)
            if k % 8 == 4:
                track.add(at + k * step, noise(0.14) * fall(0.14, 26, 0.001), 0.30)
            if k % 8 == 0 or k == 10:
                track.add(at + k * step, pulse(90, 0.12) * fall(0.12, 30, 0.001) + tone([220, 50, 45], 0.12) * fall(0.12, 20, 0.001), 0.5)
    for k, n in enumerate(lead):
        if n != '.':
            # The second time round a thinner sound, and an echo throughout.
            width = 0.5 if k < len(lead) // 2 else 0.2
            for delay, volume in ((0, 0.24), (step * 3, 0.08)):
                track.add(k * step + delay, pulse(hz(n), step * 2.4, width, 0.25) * fall(step * 2.4, 5, 0.004), volume, 0 if delay == 0 else -0.6)
    wav = path + '.wav'
    save_wav(wav, track.data, 0.85)
    compress(wav, path, '-c:a', 'libvorbis', '-q:a', '3')


def climb(path):
    """For a game that speeds up: five beats to the bar, going round and up, with bells over a low pulse."""
    beat = 60 / 132
    # Dorian, and each time round a step higher in the bass.
    bars = [('D', 'D4 F4 A4 C5'), ('D', 'D4 G4 A4 C5'), ('G', 'D4 G4 B4 D5'), ('G', 'E4 G4 B4 D5'),
            ('E', 'E4 G4 B4 D5'), ('F', 'F4 A4 C5 E5'), ('C', 'E4 G4 C5 E5'), ('A', 'C4 E4 G4 A4')] * 2
    bells = ('A5 . . D6 . C6 . . G5 . | A5 . . D6 . F6 . E6 . . | B5 . . D6 . G6 . . F6 . | E6 . D6 . . B5 . . . . | '
             'G5 . . B5 . E6 . . D6 . | C6 . . F6 . A6 . G6 . . | E6 . . G6 . C6 . . D6 . | A5 . . . . E5 . G5 . .').replace('|', ' ').split()
    track = Track(len(bars) * 5 * beat)
    for i, (root, chord) in enumerate(bars):
        notes = chord.split()
        at = i * 5 * beat
        for n in notes[:3]:
            length = beat * 5.3
            t = seconds(length)
            track.add(at, tone(hz(n), length, 'tri', 0.12) * np.minimum(1, np.minimum(t * 2.5, (length - t) * 2.5)), 0.075, 0.5 if n == notes[1] else -0.35)
        # Three and two.
        for k in (0, 1.5, 3, 4):
            track.add(at + k * beat, tone(hz(root + '2'), 0.5, 'saw') * fall(0.5, 5), 0.3 if k in (0, 3) else 0.2)
        for k in range(10):
            n = notes[(0, 2, 1, 3, 2, 0, 3, 1, 2, 3)[k]]
            track.add(at + k * beat / 2, pulse(hz(n) * 2, 0.16, 0.2) * fall(0.16, 16, 0.002), 0.10, 0.7 * np.cos(k * 1.3 + i))
        for k in (0, 3):
            track.add(at + k * beat, tone([150, 50, 42], 0.26) * fall(0.26, 13, 0.001), 0.5)
        track.add(at + 1.5 * beat, (noise(0.16) * 0.6 + tone(210, 0.16) * 0.4) * fall(0.16, 24, 0.001), 0.2)
        track.add(at + 4 * beat, (noise(0.16) * 0.6 + tone(210, 0.16) * 0.4) * fall(0.16, 24, 0.001), 0.24)
        for k in range(10):
            track.add(at + k * beat / 2, np.diff(noise(0.05), prepend=0) * fall(0.05, 70, 0.001), 0.10 if k % 2 == 0 else 0.06, 0.3)
    # Twice round: alone, and then with another above it.
    for turn in range(2):
        for k, n in enumerate(bells):
            if n == '.':
                continue
            at = (turn * len(bells) + k) * beat / 2
            ring = (tone(hz(n), 1.1) + tone(hz(n) * 2.756, 1.1) * 0.25 * fall(1.1, 9)) * fall(1.1, 4.2, 0.002)
            for delay, volume in ((0, 0.2), (beat * 1.5, 0.06)):
                track.add(at + delay, ring, volume, -0.2 if delay == 0 else 0.6)
            if turn and k % 5 == 0:
                track.add(at, tone(hz(n) * 1.5, 0.7, 'tri', 0.2) * fall(0.7, 4), 0.08, 0.4)
    wav = path + '.wav'
    save_wav(wav, track.data, 0.82)
    compress(wav, path, '-c:a', 'libvorbis', '-q:a', '3')


def covers(folder):
    """Two dozen made-up records: patterns, and names to go with them."""
    from PIL import Image, ImageDraw, ImageFont
    os.makedirs(folder, exist_ok=True)
    size = 256
    y, x = np.mgrid[0:size, 0:size] / size * 2 - 1
    r, a = np.hypot(x, y), np.arctan2(y, x)
    kinds = [lambda p: np.sin(12 * r - p * 6 + np.sin(a * (3 + p % 4)) * 2), lambda p: np.sin(x * (5 + p % 5) + np.sin(y * 4 + p) * 2), lambda p: np.sign(np.sin(a * (4 + p % 6) + r * 5)) * np.cos(r * 3),
             lambda p: np.sin(np.floor(x * (3 + p % 3)) * 2.3 + p) * np.cos(np.floor(y * (3 + p % 4)) * 1.7 + p), lambda p: 1 - 2 * np.clip(np.abs(y + 0.3 * np.sin(x * 5 + p)) * 4, 0, 1), lambda p: np.cos(18 * np.maximum(np.abs(x), np.abs(y)) + p)]
    words = [('Slow Comet', 'The Orbits'), ('Paper Moons', 'Ada Verne'), ('Glass Garden', 'Field Echo'), ('Night Ferry', 'Harbour Light'), ('Copper Sky', 'The Wiremen'), ('Small Hours', 'Nine to Noon'),
             ('Tidal', 'Low Country'), ('Signal Fires', 'Beacon Hill'), ('Static Bloom', 'Violet Hum'), ('Long Division', 'Primes'), ('Kite Weather', 'Open Field'), ('Saltwater', 'Pier Nine'),
             ('Half Light', 'Dusk Radio'), ('Mainframe', 'Punch Card'), ('Red Shift', 'Far Arms'), ('Soft Machines', 'Clockwork Sea'), ('Northern Line', 'Last Stop'), ('Echo Park', 'The Returns'),
             ('Feather', 'Weightless'), ('Ironwood', 'Forest Press'), ('Lantern', 'Moth Club'), ('Overcast', 'Grey Area'), ('Prism', 'Split Beam'), ('Home Again', 'The Orbits')]
    for i, (title, who) in enumerate(words):
        wave = kinds[i % len(kinds)](i * 1.37)
        wave = wave + 0.35 * kinds[(i + 2) % len(kinds)](i * 0.7)
        hue, other = i * 0.381966 % 1, (i * 0.381966 + 0.12 + (i % 3) * 0.14) % 1
        def paint(h, v):
            return np.stack([0.5 + 0.5 * np.cos(2 * np.pi * (h + p)) for p in (0, 0.33, 0.67)], -1) * v
        k = np.clip(wave * 0.5 + 0.5, 0, 1)[..., None]
        picture = paint(hue, 0.95) * k + paint(other, 0.28) * (1 - k)
        picture *= (1.08 - 0.45 * r ** 2)[..., None]
        image = Image.fromarray((np.clip(picture, 0, 1) * 255).astype('uint8'))
        draw = ImageDraw.Draw(image)
        try:
            big, small = ImageFont.truetype('DejaVuSans-Bold.ttf', 26), ImageFont.truetype('DejaVuSans.ttf', 15)
        except OSError:
            big, small = ImageFont.load_default(26), ImageFont.load_default(15)
        low = i % 2 == 0
        draw.rectangle((0, 196 if low else 0, 256, 256 if low else 60), fill=(12, 12, 16))
        draw.text((14, 202 if low else 8), title, font=big, fill=(250, 248, 240))
        draw.text((15, 234 if low else 38), who, font=small, fill=(190, 190, 200))
        image.save(os.path.join(folder, 'cover%02d.jpg' % i), quality=86)
    with open(os.path.join(folder, 'covers.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join('%s|%s' % w for w in words) + '\n')


def pictures():
    from PIL import Image
    folder = os.path.join(MEDIA, 'showcase', 'loading', 'asset')
    os.makedirs(folder, exist_ok=True)
    size = 512
    y, x = np.mgrid[0:size, 0:size] / size * 2 - 1
    for i in range(12):
        a, b, c, d = rng.uniform(1.5, 5, 4)
        warp = np.sin(a * x + 2 * np.sin(b * y + i)) + np.sin(c * y + 2 * np.cos(d * x + i * 2)) + np.sin(6 * np.hypot(x, y) - i)
        hue = i / 12
        image = np.stack([0.5 + 0.5 * np.cos(2 * np.pi * (hue + warp * 0.16 + p)) * (0.6 + 0.4 * np.sin(warp * 2)) for p in (0, 0.33, 0.67)], -1)
        Image.fromarray((np.clip(image, 0, 1) * 255).astype('uint8')).save(os.path.join(folder, 'picture%d.jpg' % i), quality=88)


if __name__ == '__main__':
    effects()
    music = os.path.join(MEDIA, 'showcase', 'music', 'asset')
    os.makedirs(music, exist_ok=True)
    tune(os.path.join(music, 'tune.ogg'))
    ambient(os.path.join(music, 'ambient.mp3'))
    pictures()
    shared = os.path.join(MEDIA, 'global', 'music')
    os.makedirs(shared, exist_ok=True)
    chip(os.path.join(shared, 'chip.ogg'))
    climb(os.path.join(shared, 'climb.ogg'))
    covers(os.path.join(MEDIA, 'showcase', 'cases', 'asset'))
    sys.exit(0)
