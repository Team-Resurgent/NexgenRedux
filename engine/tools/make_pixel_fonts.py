"""Makes two pixel fonts, shared by every screen, from one 5x7 design drawn here: pixel.png, an image font (glyphs side by side
with a column of one colour between them), and chunky.fnt with chunky.png, a BMFont of the same letters drawn twice
the size, with a shadow and a gradient.

    python make_pixel_fonts.py NexgenRedux-Media/media/global/font
"""
import struct
import sys
import zlib

GLYPHS = {
    ' ': ['.....'] * 7,
    'A': ['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'B': ['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.'],
    'C': ['.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.'],
    'D': ['####.', '#...#', '#...#', '#...#', '#...#', '#...#', '####.'],
    'E': ['#####', '#....', '#....', '####.', '#....', '#....', '#####'],
    'F': ['#####', '#....', '#....', '####.', '#....', '#....', '#....'],
    'G': ['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.####'],
    'H': ['#...#', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'I': ['.###.', '..#..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    'J': ['..###', '...#.', '...#.', '...#.', '...#.', '#..#.', '.##..'],
    'K': ['#...#', '#..#.', '#.#..', '##...', '#.#..', '#..#.', '#...#'],
    'L': ['#....', '#....', '#....', '#....', '#....', '#....', '#####'],
    'M': ['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'],
    'N': ['#...#', '##..#', '#.#.#', '#..##', '#...#', '#...#', '#...#'],
    'O': ['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
    'P': ['####.', '#...#', '#...#', '####.', '#....', '#....', '#....'],
    'Q': ['.###.', '#...#', '#...#', '#...#', '#.#.#', '#..#.', '.##.#'],
    'R': ['####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#'],
    'S': ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'],
    'T': ['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'],
    'U': ['#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
    'V': ['#...#', '#...#', '#...#', '#...#', '#...#', '.#.#.', '..#..'],
    'W': ['#...#', '#...#', '#...#', '#.#.#', '#.#.#', '#.#.#', '.#.#.'],
    'X': ['#...#', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '#...#'],
    'Y': ['#...#', '#...#', '.#.#.', '..#..', '..#..', '..#..', '..#..'],
    'Z': ['#####', '....#', '...#.', '..#..', '.#...', '#....', '#####'],
    '0': ['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'],
    '1': ['..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    '2': ['.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####'],
    '3': ['####.', '....#', '....#', '.###.', '....#', '....#', '####.'],
    '4': ['...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.'],
    '5': ['#####', '#....', '####.', '....#', '....#', '#...#', '.###.'],
    '6': ['..##.', '.#...', '#....', '####.', '#...#', '#...#', '.###.'],
    '7': ['#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...'],
    '8': ['.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.'],
    '9': ['.###.', '#...#', '#...#', '.####', '....#', '...#.', '.##..'],
    '.': ['.', '.', '.', '.', '.', '.', '#'],
    ',': ['..', '..', '..', '..', '..', '.#', '#.'],
    '!': ['#', '#', '#', '#', '#', '.', '#'],
    '?': ['.###.', '#...#', '....#', '...#.', '..#..', '.....', '..#..'],
    ':': ['.', '.', '#', '.', '.', '#', '.'],
    '-': ['....', '....', '....', '####', '....', '....', '....'],
    '+': ['.....', '..#..', '..#..', '#####', '..#..', '..#..', '.....'],
    '=': ['....', '....', '####', '....', '####', '....', '....'],
    "'": ['#', '#', '.', '.', '.', '.', '.'],
    '(': ['.#', '#.', '#.', '#.', '#.', '#.', '.#'],
    ')': ['#.', '.#', '.#', '.#', '.#', '.#', '#.'],
    '/': ['....#', '....#', '...#.', '..#..', '.#...', '#....', '#....'],
    '%': ['##..#', '##..#', '...#.', '..#..', '.#...', '#..##', '#..##'],
}
ORDER = ' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?:-+=\'()/%'


def png(path, width, height, pixels):
    rows = b''.join(b'\x00' + bytes(pixels[y * width * 4:(y + 1) * width * 4]) for y in range(height))

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(rows, 9)))
        f.write(chunk(b'IEND', b''))


def image_font(folder):
    # One pixel of the separator before each glyph and after the last; glyphs white, so text colours tint them.
    widths = [len(GLYPHS[c][0]) for c in ORDER]
    width = sum(widths) + len(widths) + 1
    height = 7
    pixels = [0] * (width * height * 4)
    separator = (255, 0, 255, 255)

    def put(x, y, color):
        pixels[(y * width + x) * 4:(y * width + x) * 4 + 4] = color

    x = 0
    for c in ORDER:
        for y in range(height):
            put(x, y, separator)
        x += 1
        for y, row in enumerate(GLYPHS[c]):
            for i, bit in enumerate(row):
                put(x + i, y, (255, 255, 255, 255) if bit == '#' else (0, 0, 0, 0))
        x += len(GLYPHS[c][0])
    for y in range(height):
        put(x, y, separator)
    png(folder + '/pixel.png', width, height, pixels)


def bitmap_font(folder):
    # Each glyph at twice the size, a gradient from gold to orange, and a dark shadow one step down and right.
    scale, columns, cell_w, cell_h = 2, 10, 14, 18
    rows = (len(ORDER) + columns - 1) // columns
    width, height = 256, 128
    assert columns * cell_w <= width and rows * cell_h <= height
    pixels = [0] * (width * height * 4)
    lines = ['info face="Chunky" size=14 bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=1 padding=0,0,0,0 spacing=0,0',
             'common lineHeight=18 base=14 scaleW=%d scaleH=%d pages=1 packed=0' % (width, height),
             'page id=0 file="chunky.png"',
             'chars count=%d' % len(ORDER)]
    for n, c in enumerate(ORDER):
        glyph = GLYPHS[c]
        gw = len(glyph[0]) * scale + 1
        gh = 7 * scale + 1
        ox, oy = (n % columns) * cell_w, (n // columns) * cell_h
        for layer in (0, 1):
            for y, row in enumerate(glyph):
                for i, bit in enumerate(row):
                    if bit != '#':
                        continue
                    for dy in range(scale):
                        for dx in range(scale):
                            px, py = ox + i * scale + dx + (1 - layer), oy + y * scale + dy + (1 - layer)
                            t = (y * scale + dy) / (7 * scale - 1)
                            color = (40, 20, 60, 255) if layer == 0 else (255, int(220 - 110 * t), int(90 - 70 * t), 255)
                            pixels[(py * width + px) * 4:(py * width + px) * 4 + 4] = color
        lines.append('char id=%d x=%d y=%d width=%d height=%d xoffset=0 yoffset=0 xadvance=%d page=0 chnl=15' % (ord(c), ox, oy, gw, gh, gw + 1 if c != ' ' else 8))
    png(folder + '/chunky.png', width, height, pixels)
    with open(folder + '/chunky.fnt', 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    image_font(out)
    bitmap_font(out)
