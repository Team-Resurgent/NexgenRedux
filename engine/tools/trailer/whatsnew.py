"""Writes engine/tools/trailer/whatsnew.json, the sequel trailer, on the retro synthwave by usefulpix (140 beats a minute,
2:31), played as it is and faded out at its end. Its sections, by where the drums come in:

    0 to 18.15       quiet: the logo and the opening
    18.15 to 73.01   loud, 128 beats: in motion, and the plane
    73.01 to 89.15   loudest: the console homes, and the original Xbox
    89.15 to 128.01  gentler: threads, sound, and the rest
    128.01 to 144.15 loud again: the demos, and still evolving
    144.15 on        its tail: the logo, then where to find it, as the music fades
"""
import json

BPM = 140.0
BEAT = 60 / BPM

def clip(name, at, beats, way='cut', **more):
    assert at >= 2.5 or name.startswith('og-'), name   # every recording begins with the menu fading out
    s = {'clip': name, 'from': at, 'beats': beats, 'in': way}
    s.update(more)
    return s

def feature(name, at, beats, way, caption, sub, **more):
    return clip(name, at, beats, way, caption=caption, captionSub=sub, **more)

def until(shot, t):
    """The last of a section: it lasts until the section ends, whatever that is in beats."""
    shot = dict(shot)
    shot.pop('beats', None)
    shot['to'] = t
    return shot

LENS = dict(focus=[1.0, 0.35])     # the Lens demo's panel is at its left: kept out of the picture

opening = [
    {'to': 18.15 - 30 * BEAT, 'logo': 'build/trailer/team-resurgent.png', 'logoSize': 0.62, 'logoY': 0.42, 'logoIn': 1, 'logoFade': 4,
     'zoom': [1.0, 1.0], 'title': 'P  R  E  S  E  N  T  S', 'titleSize': 38, 'titleY': 0.83, 'textIn': 5, 'textOut': 11.5, 'titleColor': '0xE848DF', 'titleDrift': 0},
    clip('explore.mp4', 3, 8, 'fade', speed=0.55, grade='dark', zoom=[1.3, 1.15], inBeats=3, title='WE SAID', sub='constantly evolving', titleSize=96, textIn=1.5, textOut=7.5),
    clip('lens.mp4', 3, 8, 'dissolve', speed=0.5, grade='warm', zoom=[1.66, 1.62], inBeats=3, title='WE MEANT IT', titleSize=96, textIn=1.5, textOut=7.5, **LENS),
    clip('topple.mp4', 8, 4, 'dissolve', speed=0.5, grade='cold', zoom=[1.35, 1.25], focus=[0.55, 0.65]),
    clip('moves.mp4', 4, 4, 'cut', speed=0.6, zoom=[1.4, 1.3], focus=[0.5, 0.55]),
    {'image': 'build/trailer/splash.png', 'fit': 0.84, 'fitY': 0.35, 'zoom': [1.1, 1.0], 'sweep': True, 'sweepAt': 1.5, 'sweepBeats': 4, 'beats': 6, 'in': 'flash', 'inBeats': 0.5,
     'mark': "WHAT'S NEW", 'markSize': 130, 'markY': 0.775, 'markIn': 1},
]
loud = [
    {'beats': 6, 'in': 'flash', 'inBeats': 0.5, 'title': 'IN MOTION', 'titleSize': 84, 'textIn': 0.3},
    feature('moves.mp4', 4, 8, 'glitch', 'ANIMATION, MIXED', 'Idle, walk and run, blended by speed, with root motion', zoom=[1.25, 1.15]),
    feature('moves.mp4', 10, 6, 'glitch', 'LAYERS AND LOOKS', 'A wave over walking; heads that turn to look', zoom=[1.5, 1.6], focus=[0.6, 0.5]),
    feature('topple.mp4', 8, 8, 'zoom', 'RAGDOLLS', 'Knocked over, thrown about, and up again', zoom=[1.3, 1.15]),
    feature('tumble.mp4', 4, 6, 'slide', 'JOINTS IN 3D', 'Hinges, limits, and things held by a bone'),
    feature('lens.mp4', 3, 6, 'glitch', 'GOD RAYS AND BOKEH', 'Occlusion, reflections and auto exposure', zoom=[1.66, 1.62], **LENS),
    feature('lens.mp4', 11, 6, 'glitch', 'DECALS', 'Paint that lands on whatever it hits', zoom=[1.62, 1.7], focus=[1.0, 0.6]),
    feature('pictures.mp4', 4, 6, 'wipe', 'PICTURES TO DRAW INTO', 'The world from above, drawn as a map each frame'),
    feature('og-topple.mp4', 3, 6, 'flash', 'ON THE ORIGINAL XBOX', 'Ragdolls, on 733 MHz and 64 MB', fit=1),
    feature('lights.mp4', 4, 6, 'flash', 'LIGHTS ON THE PLANE', 'And the shadows that walls cast'),
    feature('tiles.mp4', 4, 6, 'glitch', 'TERRAINS', 'Tiles that join up as they are painted'),
    feature('ways.mp4', 4, 6, 'slide', 'PATHS AND NOISE', 'Ways to follow, and ways round a maze'),
    feature('contraptions.mp4', 4, 6, 'zoom', '2D PHYSICS', 'Polygons, hills, one-way platforms and joints', zoom=[1.3, 1.4], focus=[0.35, 0.75]),
    feature('machines.mp4', 6, 6, 'glitch', 'WHEELS, PULLEYS, GEARS', 'Sprung wheels, a lift, a train of gears', zoom=[1.2, 1.3], focus=[0.6, 0.6]),
    feature('sketch.mp4', 4, 6, 'wipe', 'PATHS AND MASKS', 'Polygons, meshes, ellipses and masks'),
    feature('curves.mp4', 4, 8, 'glitch', 'CURVES AND BATCHES', 'Bezier curves, and sprites by the thousand'),
    feature('og-curves.mp4', 4, 8, 'flash', '60 FRAMES A SECOND', 'Fifteen hundred sprites, on the original Xbox', fit=1),
] + [clip(n, f, 2, 'glitch') for n, f in [('text.mp4', 4), ('life.mp4', 6), ('video.mp4', 6), ('synth.mp4', 8), ('sounds.mp4', 6),
                                          ('threads.mp4', 8), ('tiles.mp4', 8), ('lights.mp4', 9), ('machines.mp4', 9)]]
loudest = [
    clip('home.mp4', 3, 8, 'flash', inBeats=0.5, title='CONSOLE HOMES', sub='Three of them, written in script', titleSize=84, textIn=1, textOut=7.5),
    feature('hub.mp4', 3, 6, 'slide', 'HUB', 'Bands of light, and tiles in a row'),
    feature('channels.mp4', 3, 6, 'slide', 'CHANNELS', 'Cards on a polished floor'),
    feature('og-channels.mp4', 3, 8, 'flash', 'ON THE ORIGINAL XBOX', 'All three, at 60 frames a second', fit=1),
] + [clip(n, f, 1, 'glitch') for n, f in [('lens.mp4', 5), ('topple.mp4', 11), ('curves.mp4', 6), ('channels.mp4', 8), ('hub.mp4', 8), ('machines.mp4', 8),
                                          ('threads.mp4', 9), ('tumble.mp4', 12)]]
loudest.append(until(clip('home.mp4', 10, 1, 'glitch'), 89.15))
gentle = [
    feature('threads.mp4', 6, 10, 'fade', 'SCRIPT THREADS', 'The Mandelbrot set, a row at a time, on threads of its own', speed=0.5, inBeats=2),
    feature('synth.mp4', 8, 10, 'dissolve', 'SOUND YOU MAKE', 'Written as it plays, through distortion, chorus and echo', speed=0.6, inBeats=2),
    feature('video.mp4', 4, 8, 'dissolve', 'VIDEO', 'MPEG-1 films with their sound, on every platform'),
    feature('sounds.mp4', 4, 8, 'dissolve', 'AUDIO EFFECTS', 'Reverb, echo, filters and more, on every bus'),
    feature('text.mp4', 4, 8, 'dissolve', 'RICH TEXT', 'Colours, bold and underline, marked in the words'),
    feature('life.mp4', 6, 8, 'dissolve', 'PIXELS', 'Read, written and saved by scripts'),
    feature('system.mp4', 4, 8, 'dissolve', 'THE WINDOW AND THE SYSTEM', 'Full screen, the clipboard, a clock, Windows 11 title bars', zoom=[1.2, 1.3]),
    feature('explore.mp4', 10, 8, 'dissolve', 'DEBUG AND PROFILE', 'Breakpoints, and where the time goes, in VS Code', grade='dark'),
    feature('og-machines.mp4', 4, 8, 'dissolve', 'AND MORE', 'Zip files, gzip, packed bytes, image fonts, justified text', fit=1),
]
gentle.append(until({'image': 'build/trailer2/x360-home.png', 'zoom': [1.05, 1.15], 'sweep': True, 'sweepAt': 1, 'sweepBeats': 4, 'in': 'dissolve', 'inBeats': 1,
                     'caption': 'XBOX 360', 'captionSub': 'Every new demo there too'}, 128.01))
last = [clip(n, f, 1, 'glitch') for n, f in [('lens.mp4', 4), ('topple.mp4', 10), ('curves.mp4', 5), ('channels.mp4', 6), ('hub.mp4', 6), ('moves.mp4', 8),
                                             ('threads.mp4', 7), ('synth.mp4', 9), ('home.mp4', 8), ('lights.mp4', 7), ('tumble.mp4', 11), ('machines.mp4', 10)]]
last[0]['in'] = 'flash'
last.append({'mosaic': [{'file': n, 'from': f} for n, f in [('moves.mp4', 6), ('topple.mp4', 10), ('lens.mp4', 4), ('curves.mp4', 4), ('machines.mp4', 8), ('home.mp4', 4),
                                                           ('hub.mp4', 4), ('channels.mp4', 4), ('threads.mp4', 8), ('synth.mp4', 9), ('lights.mp4', 6), ('tiles.mp4', 6)]],
             'columns': 4, 'beats': 12, 'in': 'flash', 'inBeats': 0.5, 'counter': 83, 'counterSuffix': ' DEMOS', 'counterBeats': 6, 'title': ' ',
             'sub': 'Eighteen of them new since the last trailer', 'titleY': 0.62, 'textIn': 1})
last.append(until({'image': 'build/trailer/splash.png', 'fit': 0.84, 'fitY': 0.35, 'zoom': [1.08, 1.0], 'sweep': True, 'sweepAt': 1, 'sweepBeats': 6, 'in': 'flash', 'inBeats': 0.5,
                   'mark': 'STILL EVOLVING', 'markSize': 120, 'markY': 0.775, 'markIn': 1}, 144.15))
ending = [
    {'to': 150.2, 'in': 'fade', 'inBeats': 2, 'mark': 'NEXGEN  REDUX', 'markSize': 120, 'markY': 0.42, 'markIn': 0.5,
     'sub': 'Windows  ·  macOS  ·  Linux  ·  Original Xbox  ·  Xbox 360  ·  Xbox One / Series', 'subSize': 36, 'title': ' ', 'titleSize': 40, 'titleY': 0.52, 'textIn': 2},
    {'to': 155.5, 'in': 'fade', 'inBeats': 3, 'title': 'github.com/Team-Resurgent/NexgenRedux', 'titleSize': 60, 'titleY': 0.46,
     'sub': 'Open source, made for the community', 'subSize': 34, 'textIn': 1.5},
]

# Each section's own shots must end before its edge, leaving the last to reach it.
t = opening[0]['to']
for name, group, edge in [('opening', opening[1:], 18.15), ('loud', loud, 73.01), ('loudest', loudest, 89.15), ('gentle', gentle, 128.01), ('last', last, 144.15)]:
    for s in group:
        if 'beats' in s:
            t += s['beats'] * BEAT
        else:
            assert s['to'] > t + 0.3, (name, s['to'], t)
            t = s['to']
    print('%-8s ends at %.2f (its edge %.2f)' % (name, t, edge))
    assert abs(t - edge) < 0.02, name

shots = opening + loud + loudest + gentle + last + ending
cfg = {
    'output': 'build/nexgen-whats-new.mp4',
    'shots folder': 'whats-new-shots',
    'footage': 'build/trailer2/footage',
    'size': [1920, 1080],
    'fps': 30,
    'font': 'C:/Windows/Fonts/bahnschrift.ttf',
    'fontBold': 'C:/Windows/Fonts/bahnschrift.ttf',
    'music': {'file': 'C:/Users/eq2k/Downloads/usefulpix-retro-synthwave-background-soundtrack-341853.mp3', 'bpm': BPM,
              'parts': [[0, 150.86]], 'end': 150.86, 'fadeIn': 1, 'fadeOut': 6},
    'look': {'letterbox': 0.11, 'grain': 7, 'vignette': True, 'fadeOut': 2},
    'shots': shots,
}
import os
json.dump(cfg, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'whatsnew.json'), 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
print('shots', len(shots))
