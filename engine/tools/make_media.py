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
    chip(os.path.join(MEDIA, 'showcase', 'cracktro', 'asset', 'chip.ogg'))
    sys.exit(0)
