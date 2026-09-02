#!/usr/bin/env python3
"""topng.py - render buffers out of a 4 MB GS memory image (real-GS layout).

    topng.py <vram.bin> [spec ...]
    topng.py -c A.bin B.bin [spec ...]     # numeric diff of one/more buffers

spec is  tbp:bw:w:h[:name[:psm]]
    tbp   buffer base in 256-byte blocks (FRAME fbp*32, DISPFB fbp*32)
    bw    buffer width in 64-pixel units
    psm   32 (default), 24, 16, 16s

With no specs it uses the OSDSYS System-Config set:
    scr0 0, scr1 2240, wb3 6720, wb4 8960, all 640x224 PSMCT32, bw 10.
Writes <vram-stem>_<name>.png and <..>_<name>_a.png (alpha), 2x vertical
like osdbits/tools/gsmem.py does.

Swizzle provenance: 32/24-bit tables verified against the 1998 model +
retail dumps (see gsreplay notes); 16-bit tables are the ones probe.c
verifies; the 16S block table is TRANSCRIBED FROM DOCS AND UNVERIFIED.
"""
import sys

blockTable32 = [
    [0, 1, 4, 5, 16, 17, 20, 21],
    [2, 3, 6, 7, 18, 19, 22, 23],
    [8, 9, 12, 13, 24, 25, 28, 29],
    [10, 11, 14, 15, 26, 27, 30, 31]]
columnTable32 = [
    [0, 1, 4, 5, 8, 9, 12, 13],
    [2, 3, 6, 7, 10, 11, 14, 15],
    [16, 17, 20, 21, 24, 25, 28, 29],
    [18, 19, 22, 23, 26, 27, 30, 31],
    [32, 33, 36, 37, 40, 41, 44, 45],
    [34, 35, 38, 39, 42, 43, 46, 47],
    [48, 49, 52, 53, 56, 57, 60, 61],
    [50, 51, 54, 55, 58, 59, 62, 63]]

blockTable16 = [
    [0, 2, 8, 10], [1, 3, 9, 11], [4, 6, 12, 14], [5, 7, 13, 15],
    [16, 18, 24, 26], [17, 19, 25, 27], [20, 22, 28, 30], [21, 23, 29, 31]]
# UNVERIFIED (GS User's Manual layout; cross-check against shot.png)
blockTable16S = [
    [0, 2, 16, 18], [1, 3, 17, 19], [8, 10, 24, 26], [9, 11, 25, 27],
    [4, 6, 20, 22], [5, 7, 21, 23], [12, 14, 28, 30], [13, 15, 29, 31]]
columnTable16 = [
    [0, 2, 8, 10, 16, 18, 24, 26, 1, 3, 9, 11, 17, 19, 25, 27],
    [4, 6, 12, 14, 20, 22, 28, 30, 5, 7, 13, 15, 21, 23, 29, 31],
    [32, 34, 40, 42, 48, 50, 56, 58, 33, 35, 41, 43, 49, 51, 57, 59],
    [36, 38, 44, 46, 52, 54, 60, 62, 37, 39, 45, 47, 53, 55, 61, 63],
    [64, 66, 72, 74, 80, 82, 88, 90, 65, 67, 73, 75, 81, 83, 89, 91],
    [68, 70, 76, 78, 84, 86, 92, 94, 69, 71, 77, 79, 85, 87, 93, 95],
    [96, 98, 104, 106, 112, 114, 120, 122, 97, 99, 105, 107, 113, 115, 121, 123],
    [100, 102, 108, 110, 116, 118, 124, 126, 101, 103, 109, 111, 117, 119, 125, 127]]


def addr32(x, y, tbp, bw):
    """word address of a PSMCT32/24/Z32 pixel"""
    page = tbp // 32 + (y // 32) * bw + (x // 64)
    px, py = x % 64, y % 32
    return page * 2048 + blockTable32[py // 8][px // 8] * 64 + \
        columnTable32[py % 8][px % 8]


def addr16(x, y, tbp, bw, s=False):
    """halfword address of a PSMCT16(S) pixel"""
    page = tbp // 32 + (y // 64) * bw + (x // 64)
    px, py = x % 64, y % 64
    blk = (blockTable16S if s else blockTable16)[py // 8][px // 16]
    return page * 4096 + blk * 128 + columnTable16[py % 8][px % 16]


def grab(mem, tbp, bw, w, h, psm='32', x0=0, y0=0):
    """returns RGBA8888 bytes, row-major"""
    out = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            o = (y * w + x) * 4
            if psm in ('32', '24'):
                a = addr32(x0 + x, y0 + y, tbp, bw) * 4
                out[o:o + 4] = mem[a:a + 4]
                if psm == '24':
                    out[o + 3] = 255
            else:
                a = addr16(x0 + x, y0 + y, tbp, bw, psm == '16s') * 2
                v = mem[a] | mem[a + 1] << 8
                out[o] = (v & 31) << 3
                out[o + 1] = (v >> 5 & 31) << 3
                out[o + 2] = (v >> 10 & 31) << 3
                out[o + 3] = (v >> 15) * 255
    return bytes(out)


def topng(raw, w, h, name, alpha=False):
    from PIL import Image
    img = Image.new('RGB', (w, h)); px = img.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = raw[(y * w + x) * 4:(y * w + x) * 4 + 4]
            px[x, y] = (a, a, a) if alpha else (r, g, b)
    img.resize((w, h * 2), Image.NEAREST).save(name)


DEFAULT = ['0:10:640:224:scr0', '2240:10:640:224:scr1',
           '6720:10:640:224:wb3', '8960:10:640:224:wb4']


def parse(s):
    f = s.split(':')
    tbp, bw, w, h = (int(x) for x in f[:4])
    name = f[4] if len(f) > 4 and f[4] else 'b%d' % tbp
    psm = f[5] if len(f) > 5 else '32'
    if psm not in ('32', '24', '16', '16s'):
        sys.exit('bad psm %r (want 32/24/16/16s)' % psm)
    return tbp, bw, w, h, name, psm


def render(vramfile, specs, stem=None):
    mem = open(vramfile, 'rb').read()
    stem = stem or vramfile.rsplit('.', 1)[0]
    made = []
    for spec in specs:
        tbp, bw, w, h, name, psm = parse(spec)
        raw = grab(mem, tbp, bw, w, h, psm)
        topng(raw, w, h, '%s_%s.png' % (stem, name))
        topng(raw, w, h, '%s_%s_a.png' % (stem, name), alpha=True)
        made.append('%s_%s.png' % (stem, name))
        if psm == '16s':
            print('warning: %s uses the unverified PSMCT16S table' % name)
    return made


def compare(fa, fb, specs):
    ma = open(fa, 'rb').read(); mb = open(fb, 'rb').read()
    for spec in specs:
        tbp, bw, w, h, name, psm = parse(spec)
        ra, rb = grab(ma, tbp, bw, w, h, psm), grab(mb, tbp, bw, w, h, psm)
        n = w * h
        diff = sum(1 for i in range(n)
                   if ra[i * 4:i * 4 + 3] != rb[i * 4:i * 4 + 3])
        tot = sum(abs(ra[i] - rb[i]) for i in range(0, n * 4) if i % 4 != 3)
        print('%-6s tbp=%-5d rgb-differing px %6d/%d (%5.1f%%)  '
              'mean |d| %6.3f' % (name, tbp, diff, n, 100.0 * diff / n,
                                  tot / (n * 3.0)))


def main():
    a = sys.argv[1:]
    if a and a[0] == '-c':
        compare(a[1], a[2], a[3:] or DEFAULT)
        return
    if not a:
        sys.exit(__doc__)
    for f in render(a[0], a[1:] or DEFAULT):
        print(f)


if __name__ == '__main__':
    main()
