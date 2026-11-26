"""Makes the teaser trailer from trailer.json: footage, pictures, words and music, cut on the beat.

    python tools/trailer/make_trailer.py [trailer.json] [--preview]

Needs ffmpeg and Python 3 (and Pillow, for the light that sweeps across pictures). Everything that can be changed is in
the .json: what each shot shows, for how many beats, the words on it and how it comes in. Each shot is made once
and kept beside the output, so after a change only the shots that changed are made again. --preview makes it at
half the size, quickly, for trying things.

Shots (in order; times are within the music as it is cut, beats are the music's):
    "beats" or "seconds": how long; or "to": the time it lasts until
    "clip": a video, "from" seconds into it, "speed" (0.5 is slow motion), "zoom" [start, end], "shake": seconds,
        "fit": 1 to show all of it within the letterbox, with a blurred copy round it (for 4:3 footage)
    "image": a picture, with "zoom" [start, end] and "sweep": true for a light that passes across it; "fit" and
        "fitY" (0 top, 1 bottom) as for clips
    "focus": [x, y], where in the picture the crop is centred (0 to 1), to keep a corner out of the shot
    "logo": a picture with transparency, on black, "logoSize" of the height (0.6), coming in at "logoIn" beats
    "mosaic": a list of videos, each { "file", "from" }, shown together in a grid of "columns"
    neither: black
    "grade": "dark", "mono", "night", "warm", "cold" or "vivid" (default, slightly richer than as recorded)
    "in": how it comes in: "cut", "flash", "fade", "glitch", "zoom", "slide", "circle", "dissolve", "blur", "wipe"
    "inBeats": how long that takes, in beats
    "title" / "sub": large words in the middle, with a glow; "titleSize", "titleY" (0 top, 1 bottom)
    "caption" / "captionSub": a label at the bottom left
    "counter": counts up to this number, with "counterSuffix" after it
    "textIn" / "textOut": when the words come and go, in beats from the start of the shot
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
CODE = hashlib.sha1(open(os.path.abspath(__file__), 'rb').read()).hexdigest()

TRANSITIONS = {'flash': 'fadewhite', 'fade': 'fade', 'zoom': 'zoomin', 'slide': 'smoothleft', 'circle': 'circleopen',
               'dissolve': 'dissolve', 'blur': 'hblur', 'wipe': 'wipeleft', 'black': 'fadeblack', 'glitch': 'pixelize'}

GRADES = {
    'vivid': 'eq=contrast=1.06:saturation=1.15',
    'dark': 'eq=brightness=-0.10:contrast=1.12:saturation=0.55',
    'mono': 'hue=s=0,eq=contrast=1.25:brightness=-0.04',
    'night': 'eq=brightness=-0.02:contrast=1.1:saturation=0.9,colorbalance=bs=0.08:bm=0.04',
    'warm': 'eq=contrast=1.08:saturation=1.2,colorbalance=rs=0.06:rm=0.04:bs=-0.05',
    'cold': 'eq=contrast=1.1:saturation=0.9,colorbalance=bs=0.08:bm=0.05:rs=-0.04',
}


def ffmpeg(args, cwd):
    subprocess.run(['ffmpeg', '-v', 'error', '-y'] + args, cwd=cwd, check=True)


class Trailer:
    def __init__(self, path, preview):
        self.path = os.path.abspath(path)
        self.base = os.path.dirname(self.path)
        self.cfg = json.load(open(path, encoding='utf-8'))
        scale = 0.5 if preview else 1.0
        self.W = int(self.cfg['size'][0] * scale) // 2 * 2
        self.H = int(self.cfg['size'][1] * scale) // 2 * 2
        self.k = self.H / 1080          # sizes in the .json are for 1080 lines
        self.fps = self.cfg.get('fps', 30)
        m = self.cfg['music']
        self.beat = 60.0 / m['bpm']
        out = self.find(self.cfg['output'])
        self.output = out if not preview else os.path.splitext(out)[0] + '-preview.mp4'
        self.work = os.path.join(os.path.dirname(self.output), 'trailer-shots' + ('-preview' if preview else ''))
        os.makedirs(self.work, exist_ok=True)
        for name, key in (('font.ttf', 'font'), ('bold.ttf', 'fontBold')):
            shutil.copyfile(self.find(self.cfg[key]), os.path.join(self.work, name))
        self.makeSweep()

    def find(self, name):
        """A file, as written: absolute, or from the .json's folder, or from the repository."""
        for p in (name, os.path.join(self.base, name), os.path.join(ROOT, name)):
            if os.path.exists(p):
                return os.path.abspath(p)
        return os.path.join(ROOT, name)

    def footage(self, name):
        folder = self.cfg.get('footage', '')
        for p in (os.path.join(self.find(folder), name) if folder else None, self.find(name)):
            if p and os.path.exists(p):
                return p
        raise SystemExit('no footage called %s' % name)

    def makeSweep(self):
        """A soft band of light, leaning, for passing across pictures."""
        from PIL import Image, ImageFilter
        w, h = int(self.W * 0.5), self.H
        band = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        px = band.load()
        for x in range(w):
            d = abs(x - w / 2) / (w / 2)
            a = int(max(0.0, 1 - d) ** 2.2 * 120)
            for y in range(h):
                px[x, y] = (255, 246, 230, a)
        band = band.rotate(-14, resample=Image.BICUBIC, expand=False).filter(ImageFilter.GaussianBlur(6))
        band.save(os.path.join(self.work, 'sweep.png'))

    def wordmark(self, text, size):
        """Words made to look like the logo's: slanted, metal from gold to olive, a blue edge under them, a bright bevel
        along their tops and a blue glow round them. Made once, as a picture."""
        from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont
        name = 'mark-%s-%d.png' % (hashlib.sha1(text.encode()).hexdigest()[:8], size)
        path = os.path.join(self.work, name)
        if os.path.exists(path):
            return name
        font = ImageFont.truetype(os.path.join(self.work, 'bold.ttf'), size)
        try:
            font.set_variation_by_name('Bold')
        except Exception:
            pass
        pad = size
        box = font.getbbox(text)
        w, h = box[2] - box[0] + pad * 2, box[3] - box[1] + pad * 2
        mask = Image.new('L', (w, h), 0)
        ImageDraw.Draw(mask).text((pad - box[0], pad - box[1]), text, font=font, fill=255)
        # Leaning forward, as the logo does.
        lean = 0.22
        mask = mask.transform((w, h), Image.AFFINE, (1, lean, -lean * h * 0.5, 0, 1, 0), resample=Image.BICUBIC)
        out = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        # A glow, then the blue edge beneath, then the metal face.
        glow = mask.filter(ImageFilter.GaussianBlur(size * 0.18))
        out.paste(Image.new('RGBA', (w, h), (40, 150, 255, 255)), (0, 0), glow.point(lambda v: int(v * 0.75)))
        depth = max(2, size // 14)
        for d in range(depth, 0, -1):
            edge = ImageChops.offset(mask, d // 2, d)
            shade = 150 + int(80 * (1 - d / depth))
            out.paste(Image.new('RGBA', (w, h), (20, int(shade * 0.75), 255, 255)), (0, 0), edge)
        face = Image.new('RGBA', (w, h))
        px = face.load()
        top, bottom = pad - size * 0.05, pad + (box[3] - box[1])
        stops = [(0.0, (250, 244, 190)), (0.35, (214, 196, 92)), (0.55, (150, 150, 64)), (0.62, (226, 220, 150)), (1.0, (120, 132, 58))]
        for y in range(h):
            t = min(1.0, max(0.0, (y - top) / max(1.0, bottom - top)))
            for n in range(len(stops) - 1):
                if stops[n][0] <= t <= stops[n + 1][0]:
                    a = (t - stops[n][0]) / (stops[n + 1][0] - stops[n][0])
                    c = tuple(int(stops[n][1][k] + (stops[n + 1][1][k] - stops[n][1][k]) * a) for k in range(3))
                    break
            for x in range(w):
                px[x, y] = c + (255,)
        out.paste(face, (0, 0), mask)
        # The bevel: where the letters' tops are, lighter.
        bevel = ImageChops.subtract(mask, ImageChops.offset(mask, 0, max(1, size // 40))).filter(ImageFilter.GaussianBlur(1))
        out.paste(Image.new('RGBA', (w, h), (255, 255, 240, 255)), (0, 0), bevel.point(lambda v: int(v * 0.8)))
        out.crop(out.getbbox()).save(path)
        return name

    def feathered(self, image, height):
        """A picture at a height, its edges fading out, and the colour round its edge to put it on."""
        from PIL import Image, ImageFilter
        name = 'soft-%s-%d.png' % (hashlib.sha1(image.encode()).hexdigest()[:8], height)
        im = Image.open(self.find(image)).convert('RGB')
        w = int(im.width * height / im.height) // 2 * 2
        im = im.resize((w, height), Image.LANCZOS)
        edge = [im.getpixel((x, y)) for x in range(0, w, 8) for y in (2, height - 3)] + [im.getpixel((x, y)) for y in range(0, height, 8) for x in (2, w - 3)]
        colour = tuple(sorted(c[k] for c in edge)[len(edge) // 2] for k in range(3))
        path = os.path.join(self.work, name)
        if not os.path.exists(path):
            border = int(height * 0.08)
            mask = Image.new('L', (w, height), 0)
            mask.paste(255, (border, border, w - border, height - border))
            mask = mask.filter(ImageFilter.GaussianBlur(border * 0.6))
            im.putalpha(mask)
            im.save(path)
        return name, '0x%02x%02x%02x' % colour

    # --- The timeline ---

    def plan(self):
        shots = self.cfg['shots']
        t = 0.0
        for s in shots:
            if 'to' in s:
                length = s['to'] - t
            elif 'beats' in s:
                length = s['beats'] * self.beat
            else:
                length = s.get('seconds', 2)
            s['_start'] = t
            s['_length'] = length
            t += length
        self.total = t
        # Half of each coming in is before the shot's start, half after.
        frame = 1.0 / self.fps
        for i, s in enumerate(shots):
            way = s.get('in', 'cut') if i > 0 else 'cut'
            d = s.get('inBeats', 0.5) * self.beat if way not in ('cut', 'glitch') else frame
            if way == 'glitch':
                d = 2 * frame
            s['_half'] = d / 2
            s['_way'] = way
        for i, s in enumerate(shots):
            after = shots[i + 1]['_half'] if i + 1 < len(shots) else 0
            s['_from'] = s['_start'] - s['_half']
            s['_render'] = s['_length'] + s['_half'] + after + 3 * frame
        return shots

    # --- One shot ---

    def text(self, name, content):
        p = os.path.join(self.work, name)
        open(p, 'w', encoding='utf-8').write(content)
        return name

    def fade(self, s, a, b, f=0.35):
        """Alpha for words that come in at a and go at b, local to the file."""
        lead = s['_half']
        a, b = a + lead, b + lead
        return "'min(1,max(0,(t-%.3f)/%.3f))*min(1,max(0,(%.3f-t)/%.3f))'" % (a, f, b, f)

    def words(self, s, key, i, src):
        """Filters for the words on a shot, as a chain that takes [v] and gives [out]."""
        beat, k, W, H = self.beat, self.k, self.W, self.H
        a = s.get('textIn', 0.25) * beat
        b = s['textOut'] * beat if 'textOut' in s else s['_length'] - 0.15 * beat
        parts = []
        layers = []
        if 'title' in s:
            size = int(s.get('titleSize', 120) * k)
            y = s.get('titleY', 0.42)
            f = self.text('t%s.txt' % key, s['title'])
            drift = "'%.1f-%.2f*t'" % (H * y - size / 2, 6 * k)
            draw = "drawtext=fontfile=bold.ttf:textfile=%s:fontsize=%d:fontcolor=%s:x=(w-text_w)/2:y=%s:alpha=%s" % (
                f, size, s.get('titleColor', 'white'), drift, self.fade(s, a, b))
            layers.append(draw)
            if 'sub' in s:
                g = self.text('s%s.txt' % key, s['sub'])
                ssize = int(s.get('subSize', 44) * k)
                layers.append("drawtext=fontfile=font.ttf:textfile=%s:fontsize=%d:fontcolor=%s:x=(w-text_w)/2:y=%d:alpha=%s" % (
                    g, ssize, s.get('subColor', '0xd8e4ff'), int(H * y + size * 0.62), self.fade(s, a + 0.4 * beat, b)))
        if 'counter' in s:
            n = int(s['counter'])
            g = self.text('c%s.txt' % key, '%%{eif:min(%d,floor(%d*pow(min(1,max(0,(t-%.3f)/%.3f)),0.6))):d}%s' % (
                n, n, s['_half'] + a, max(0.1, s.get('counterBeats', 4) * beat), s.get('counterSuffix', '')))
            size = int(s.get('counterSize', 220) * k)
            layers.append("drawtext=fontfile=bold.ttf:textfile=%s:expansion=normal:fontsize=%d:fontcolor=white:x=(w-text_w)/2:y=%d:alpha=%s" % (
                g, size, int(H * s.get('counterY', 0.3)), self.fade(s, a, b)))
        if layers:
            # The words, and the same words blurred under them for a glow.
            parts.append("color=c=black@0:s=%dx%d:r=%d:d=%.3f,format=rgba,%s,split[w%d][g%d]" % (W, H, self.fps, s['_render'], ','.join(layers), i, i))
            parts.append("[g%d]gblur=sigma=%.1f,colorchannelmixer=aa=0.85[h%d]" % (i, 22 * k, i))
            parts.append("[%s][h%d]overlay=format=auto[v2]" % (src, i))
            parts.append("[v2][w%d]overlay=format=auto[v3]" % i)
            last = 'v3'
        else:
            last = src
        if 'caption' in s:
            f = self.text('k%s.txt' % key, s['caption'])
            size = int(46 * k)
            x, y = int(W * 0.06), int(H * 0.72)
            al = self.fade(s, a, b)
            cap = "drawbox=x=%d:y=%d:w=%d:h=%d:color=0x46a0ff@0.9:t=fill:enable='between(t,%.3f,%.3f)'" % (
                x - int(18 * k), y + int(6 * k), int(6 * k), int(size * (1.9 if 'captionSub' in s else 1.05)), s['_half'] + a, s['_half'] + b)
            cap += ",drawtext=fontfile=bold.ttf:textfile=%s:fontsize=%d:fontcolor=white:x=%d:y=%d:alpha=%s:shadowcolor=black@0.6:shadowx=2:shadowy=2" % (f, size, x, y, al)
            if 'captionSub' in s:
                g = self.text('q%s.txt' % key, s['captionSub'])
                cap += ",drawtext=fontfile=font.ttf:textfile=%s:fontsize=%d:fontcolor=0xd0dcf0:x=%d:y=%d:alpha=%s:shadowcolor=black@0.6:shadowx=2:shadowy=2" % (
                    g, int(28 * k), x, y + int(size * 1.15), self.fade(s, a + 0.25 * beat, b))
            parts.append("[%s]%s[v4]" % (last, cap))
            last = 'v4'
        parts.append("[%s]null[out]" % last)
        return parts

    def zoomCrop(self, s):
        W, H = self.W, self.H
        z0, z1 = s.get('zoom', [1.0, 1.06])
        L = max(s['_render'], 0.1)
        shake = s.get('shake', 0)
        # Bigger, then cut to the frame: the push-in, and room to shake.
        zoom = "scale=w='trunc(%d*(%f+%f*t/%f)/2)*2':h=-2:eval=frame" % (W, z0, z1 - z0, L)
        jx = "+%.1f*sin(t*83)*max(0,1-t/%.2f)" % (14 * self.k, shake) if shake else ''
        jy = "+%.1f*sin(t*61+1)*max(0,1-t/%.2f)" % (10 * self.k, shake) if shake else ''
        fx, fy = s.get('focus', [0.5, 0.5])
        return "%s,crop=%d:%d:'(iw-%d)*%.3f%s':'(ih-%d)*%.3f%s'" % (zoom, W, H, W, fx, jx, H, fy, jy)

    def render(self, i, s):
        W, H, fps = self.W, self.H, self.fps
        # The shot as written, and this script as it is: a change to either makes it again.
        recipe = json.dumps({k: v for k, v in s.items()}, sort_keys=True) + '|%d|%d|%d|' % (W, H, fps) + CODE
        key = hashlib.sha1(recipe.encode()).hexdigest()[:12]
        out = os.path.join(self.work, 'shot-%s.mp4' % key)
        if os.path.exists(out):
            return out
        inputs = []
        L = s['_render']
        speed = s.get('speed', 1.0)
        cover = "scale=%d:%d:force_original_aspect_ratio=increase,crop=%d:%d,setsar=1" % (W, H, W, H)
        grade = GRADES.get(s.get('grade', 'vivid'), GRADES['vivid'])
        chain = []
        if 'clip' in s:
            start = max(0.0, s.get('from', 0) - s['_half'] * speed)
            inputs += ['-ss', '%.3f' % start, '-t', '%.3f' % (L * speed + 0.5), '-i', self.footage(s['clip'])]
            if s.get('fit'):
                # All of it, within the letterbox, and a blurred copy round it.
                fh = int(H * (1 - 2 * self.cfg.get('look', {}).get('letterbox', 0.11)) * s.get('fit', 1.0)) // 2 * 2
                chain.append("[0:v]setpts=(PTS-STARTPTS)/%f,fps=%d,split[fa][fb]" % (speed, fps))
                chain.append("[fa]%s,gblur=sigma=%.1f,eq=brightness=-0.22:saturation=0.7[bg]" % (cover, 30 * self.k))
                chain.append("[fb]scale=-2:%d,setsar=1[fg]" % fh)
                chain.append("[bg][fg]overlay=(W-w)/2:(H-h)/2,%s,%s[src]" % (self.zoomCrop(s), grade))
            else:
                chain.append("[0:v]setpts=(PTS-STARTPTS)/%f,fps=%d,%s,%s,%s[src]" % (speed, fps, cover, self.zoomCrop(s), grade))
        elif 'image' in s:
            inputs += ['-loop', '1', '-t', '%.3f' % (L + 0.2), '-framerate', str(fps), '-i', self.find(s['image'])]
            look = GRADES.get(s.get('grade'), 'null') if s.get('grade') else 'null'
            if s.get('fit'):
                # On its own border's colour, its edges melting into it, so it has no frame.
                fh = int(H * (1 - 2 * self.cfg.get('look', {}).get('letterbox', 0.11)) * s['fit']) // 2 * 2
                soft, colour = self.feathered(s['image'], fh)
                inputs[:] = ['-loop', '1', '-t', '%.3f' % (L + 0.2), '-framerate', str(fps), '-i', soft]
                chain.append("color=c=%s:s=%dx%d:r=%d:d=%.3f,format=rgba[bg]" % (colour, W, H, fps, L + 0.2))
                chain.append("[bg][0:v]overlay=(W-w)/2:(H-h)*%.3f,format=yuv420p,%s,%s[pic]" % (s.get('fitY', 0.5), self.zoomCrop(s), look))
            else:
                chain.append("[0:v]fps=%d,%s,%s,%s[pic]" % (fps, cover, self.zoomCrop(s), look))
            if s.get('sweep'):
                inputs += ['-loop', '1', '-t', '%.3f' % (L + 0.2), '-framerate', str(fps), '-i', 'sweep.png']
                t0 = s['_half'] + s.get('sweepAt', 0.5) * self.beat
                d = s.get('sweepBeats', 4) * self.beat
                chain.append("[pic][1:v]overlay=x='-w+(W+w)*(t-%.3f)/%.3f':y=0:enable='between(t,%.3f,%.3f)'[src]" % (t0, d, t0, t0 + d))
            else:
                chain.append("[pic]null[src]")
        elif 'logo' in s:
            # On black, growing a little as it fades in.
            inputs += ['-f', 'lavfi', '-t', '%.3f' % (L + 0.2), '-i', 'color=c=black:s=%dx%d:r=%d' % (W, H, fps)]
            inputs += ['-loop', '1', '-t', '%.3f' % (L + 0.2), '-framerate', str(fps), '-i', self.find(s['logo'])]
            lh = int(H * s.get('logoSize', 0.6)) // 2 * 2
            t0 = s['_half'] + s.get('logoIn', 0.3) * self.beat
            chain.append("[1:v]format=rgba,scale=-2:%d,fade=in:st=%.3f:d=%.3f:alpha=1[lg]" % (lh, t0, s.get('logoFade', 1.5) * self.beat))
            chain.append("[0:v][lg]overlay=(W-w)/2:(H-h)*%.3f:format=auto,format=yuv420p,%s[src]" % (s.get('logoY', 0.5), self.zoomCrop(s)))
        elif 'mosaic' in s:
            cols = s.get('columns', 4)
            tiles = s['mosaic']
            rows = (len(tiles) + cols - 1) // cols
            tw, th = W // cols // 2 * 2, H // rows // 2 * 2
            names = []
            for n, tile in enumerate(tiles):
                inputs += ['-ss', '%.3f' % tile.get('from', 0), '-t', '%.3f' % (L + 0.5), '-i', self.footage(tile['file'])]
                # Each comes in a moment after the last.
                late = n * s.get('stagger', 0.06)
                chain.append("[%d:v]fps=%d,scale=%d:%d:force_original_aspect_ratio=increase,crop=%d:%d,setsar=1,%s,pad=%d:%d:3:3:black,fade=in:st=%.3f:d=0.25[m%d]" % (
                    n, fps, tw - 6, th - 6, tw - 6, th - 6, grade, tw, th, s['_half'] + late, n))
                names.append('[m%d]' % n)
            layout = '|'.join('%d_%d' % ((n % cols) * tw, (n // cols) * th) for n in range(len(tiles)))
            chain.append("%sxstack=inputs=%d:layout=%s:fill=black,scale=%d:%d,%s,eq=brightness=%.2f[src]" % (
                ''.join(names), len(tiles), layout, W, H, self.zoomCrop(s), s.get('dim', -0.12)))
        else:
            inputs += ['-f', 'lavfi', '-t', '%.3f' % (L + 0.2), '-i', 'color=c=black:s=%dx%d:r=%d' % (W, H, fps)]
            chain.append("[0:v]null[src]")
        if s.get('in') == 'glitch':
            # Colours torn apart, and noise, for a moment as it comes in.
            g = s['_half'] + 0.2
            chain.append("[src]rgbashift=rh=%d:bh=-%d:enable='lt(t,%.3f)',noise=alls=45:allf=t:enable='lt(t,%.3f)'[v]" % (int(20 * self.k), int(20 * self.k), g, g))
        else:
            chain.append("[src]null[v]")
        if 'mark' in s:
            mark = self.wordmark(s['mark'], int(s.get('markSize', 150) * self.k))
            inputs += ['-loop', '1', '-t', '%.3f' % (L + 0.2), '-framerate', str(fps), '-i', mark]
            n = sum(1 for x in inputs if x == '-i') - 1
            t0 = s['_half'] + s.get('markIn', 0.5) * self.beat
            grow = "scale=w='trunc(iw*(1.25-0.25*min(1,max(0,(t-%.3f)/0.35)))/2)*2':h=-2:eval=frame" % t0
            fadein = "fade=in:st=%.3f:d=0.3:alpha=1" % t0
            chain.append("[%d:v]format=rgba,%s,%s[mk]" % (n, grow, fadein))
            chain.append("[v][mk]overlay=x=(W-w)/2:y=%d-h/2:format=auto[vm]" % int(H * s.get('markY', 0.77)))
            chain += self.words(s, key, i, 'vm')
        else:
            chain += self.words(s, key, i, 'v')
        graph = ';'.join(chain)
        script = os.path.join(self.work, 'graph-%s.txt' % key)
        open(script, 'w', encoding='utf-8').write(graph)
        frames = int(round(L * fps))
        ffmpeg(inputs + ['-filter_complex_script', os.path.basename(script), '-map', '[out]', '-an', '-frames:v', str(frames),
                         '-r', str(fps), '-pix_fmt', 'yuv420p', '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '14', os.path.basename(out)], self.work)
        return out

    # --- All of it ---

    def music(self):
        m = self.cfg['music']
        parts = m.get('parts', [[0, self.total]])
        chain = []
        for n, (a, b) in enumerate(parts):
            chain.append("[%d:a]atrim=%.3f:%.3f,asetpts=PTS-STARTPTS[p%d]" % (1, a, b, n))
        last = 'p0'
        for n in range(1, len(parts)):
            chain.append("[%s][p%d]acrossfade=d=%.3f:c1=tri:c2=tri[q%d]" % (last, n, m.get('joinSeconds', 0.04), n))
            last = 'q%d' % n
        # It may end before the pictures do, which then go on in silence.
        end = min(m.get('end', self.total), self.total)
        chain.append("[%s]atrim=0:%.3f,afade=t=out:st=%.3f:d=%.3f,apad=whole_dur=%.3f[aout]" % (last, end, end - m.get('fadeOut', 1.5), m.get('fadeOut', 1.5), self.total))
        return chain

    def make(self):
        shots = self.plan()
        files = []
        for i, s in enumerate(shots):
            files.append(self.render(i, s))
            print('shot %2d  %6.2f s  %5.2f s  %s' % (i, s['_start'], s['_length'], s.get('title') or s.get('caption') or s.get('clip') or s.get('image') or ''), flush=True)
        # The shots, one after another, in groups: each group goes on from what the one before made, so that only so
        # many pictures are being read at once.
        group = 20
        so_far = None
        for first in range(1, len(files), group):
            last_shot = min(first + group, len(files))
            inputs = ['-i', so_far if so_far else os.path.basename(files[0])]
            for f in files[first:last_shot]:
                inputs += ['-i', os.path.basename(f)]
            chain = ["[%d:v]settb=AVTB,fps=%d,format=yuv420p[s%d]" % (k, self.fps, k) for k in range(last_shot - first + 1)]
            last = 's0'
            for k, i in enumerate(range(first, last_shot), start=1):
                s = shots[i]
                way = TRANSITIONS.get(s['_way'], 'fade') if s['_way'] not in ('cut',) else 'fade'
                chain.append("[%s][s%d]xfade=transition=%s:duration=%.4f:offset=%.4f[x%d]" % (last, k, way, max(s['_half'] * 2, 1.0 / self.fps), s['_start'] - s['_half'], k))
                last = 'x%d' % k
            chain.append("[%s]null[vout]" % last)
            part = 'joined-%d.mp4' % first
            open(os.path.join(self.work, 'join.txt'), 'w', encoding='utf-8').write(';'.join(chain))
            ffmpeg(inputs + ['-filter_complex_script', 'join.txt', '-map', '[vout]', '-an', '-r', str(self.fps), '-pix_fmt', 'yuv420p',
                             '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '10', part], self.work)
            so_far = part
        look = self.cfg.get('look', {})
        bar = int(self.H * look.get('letterbox', 0.11))
        post = ["trim=0:%.3f" % self.total]
        if bar:
            post.append("drawbox=x=0:y=0:w=iw:h=%d:color=black:t=fill,drawbox=x=0:y=ih-%d:w=iw:h=%d:color=black:t=fill" % (bar, bar, bar))
        if look.get('vignette', True):
            post.append("vignette=angle=PI/5")
        if look.get('grain', 7):
            post.append("noise=alls=%d:allf=t" % look.get('grain', 7))
        post.append("fade=t=in:st=0:d=0.6,fade=t=out:st=%.3f:d=%.3f" % (self.total - look.get('fadeOut', 1.2), look.get('fadeOut', 1.2)))
        chain = ["[0:v]settb=AVTB,fps=%d,format=yuv420p,%s[vout]" % (self.fps, ','.join(post))]
        # The music is the input after the pictures.
        chain += self.music()
        script = os.path.join(self.work, 'final.txt')
        open(script, 'w', encoding='utf-8').write(';'.join(chain))
        ffmpeg(['-i', so_far, '-i', self.find(self.cfg['music']['file']), '-filter_complex_script', 'final.txt', '-map', '[vout]', '-map', '[aout]', '-r', str(self.fps), '-pix_fmt', 'yuv420p',
                '-c:v', 'libx264', '-preset', 'slow', '-crf', '18', '-maxrate', '18M', '-bufsize', '36M', '-c:a', 'aac', '-b:a', '256k', '-movflags', '+faststart', self.output], self.work)
        print('made %s, %.1f seconds' % (self.output, self.total))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    path = args[0] if args else os.path.join(HERE, 'trailer.json')
    Trailer(path, '--preview' in sys.argv).make()


if __name__ == '__main__':
    main()
