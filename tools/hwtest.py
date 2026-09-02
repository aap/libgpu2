#!/usr/bin/env python3
"""hwtest.py - a TOOL test suite over many dumps, with short names.

    hwtest.py add [out-dir ...]   add entries (default: every out/ dir
                                  that has a model snap000_end.bin)
    hwtest.py compare             diff every entry's TOOL readback
                                  against the model, render hw PNGs
    hwtest.py status              list entries and what they have

Builds hwtest/ next to out/:
    index.txt        entry names, one per line -- gsrun.elf iterates it
    <entry>/         hwstream.bin + vram_hw.bin  (the gsrun bundle)
      name.txt       the original long out/ name
      model.bin ->   the model's snap000_end.bin      (symlink)
      shot.png ->    PCSX2's own render, if present   (symlink)
      hwvram.bin     written back by the TOOL

One dsedb invocation runs the whole suite:
    dsedb -r run tools/ps2/gsrun.elf host0:hwtest
then `hwtest.py compare` prints one line per frame and writes
<entry>/hw_<buf>.png (+ diff stats) using the display buffer the
dump's own last priv-reg snapshot points at.
"""
import os, re, struct, sys

import numpy as np

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import topng
from hwprep import prep, lut32

ROOT = os.path.dirname(TOOLS)
OUT = os.path.join(ROOT, 'out')
HWT = os.path.join(ROOT, 'hwtest')

PSM = {0: '32', 1: '24', 2: '16', 10: '16s'}


def entries():
    idx = os.path.join(HWT, 'index.txt')
    if not os.path.exists(idx):
        return []
    return [l.strip() for l in open(idx) if l.strip()]


def longname(e):
    return open(os.path.join(HWT, e, 'name.txt')).read().strip()


def shorttag(name):
    m = re.search(r'(\d{6})$', name)
    return m.group(1) if m else re.sub(r'[^A-Za-z0-9]+', '', name)[:10]


def add(dirs):
    os.makedirs(HWT, exist_ok=True)
    have = {longname(e): e for e in entries()}
    es = entries()
    for d in dirs:
        d = d.rstrip('/')
        name = os.path.basename(d)
        snap = os.path.join(d, 'snap000_end.bin')
        if not os.path.exists(snap):
            print(f'skip (no model snap): {name}')
            continue
        if name in have:
            continue
        e = f'{len(es):03d}_{shorttag(name)}'
        ed = os.path.join(HWT, e)
        prep(d, ed)
        open(os.path.join(ed, 'name.txt'), 'w').write(name + '\n')
        os.symlink(os.path.relpath(snap, ed), os.path.join(ed, 'model.bin'))
        shot = os.path.join(d, 'shot.png')
        if os.path.exists(shot):
            os.symlink(os.path.relpath(shot, ed), os.path.join(ed, 'shot.png'))
        es.append(e)
        have[name] = e
    with open(os.path.join(HWT, 'index.txt'), 'w') as f:
        f.write(''.join(e + '\n' for e in es))
    print(f'{len(es)} entries in {HWT}')


def dispspec(e):
    """display-buffer topng spec from the entry's last priv snapshot"""
    d = open(os.path.join(HWT, e, 'hwstream.bin'), 'rb').read()
    p, last = 16, None
    while p + 16 <= len(d):
        typ, nqw = d[p], struct.unpack_from('<I', d, p + 4)[0]
        p += 16
        if typ == 2:
            last = struct.unpack_from('<8Q', d, p)
        p += nqw * 16
    if last is None:
        return None
    pmode, smode2 = last[0], last[1]
    dispfb, disp = (last[4], last[5]) if pmode >> 1 & 1 else (last[2], last[3])
    fbp, fbw, psm = dispfb & 0x1ff, dispfb >> 9 & 0x3f, dispfb >> 15 & 0x1f
    magh, magv = disp >> 23 & 0xf, disp >> 27 & 3
    w = (int(disp >> 32 & 0xfff) + 1) // (magh + 1)
    h = (int(disp >> 44 & 0x7ff) + 1) // (magv + 1)
    if (smode2 & 1) and (smode2 >> 1 & 1):
        h //= 2
    return f'{fbp * 32}:{fbw}:{w}:{h}:disp:{PSM.get(psm, "32")}'


def compare():
    lut = lut32()
    ndone = nsame = 0
    for e in entries():
        ed = os.path.join(HWT, e)
        hwfn = os.path.join(ed, 'hwvram.bin')
        if not os.path.exists(hwfn):
            print(f'{e}: no TOOL readback yet')
            continue
        img = np.fromfile(hwfn, dtype='<u4')
        if img.size != 1 << 20:
            print(f'{e}: partial readback ({img.size} words) -- '
                  'suite still writing it, skipping')
            continue
        raw = np.empty(1 << 20, dtype='<u4')
        raw[lut] = img
        rawfn = os.path.join(ed, 'hwvram_raw.bin')
        raw.tofile(rawfn)
        model = np.fromfile(os.path.join(ed, 'model.bin'), dtype='<u4')
        dw = int((model != raw).sum())
        ndone += 1
        nsame += dw == 0
        print(f'{e}: {dw:7d}/1048576 words differ ({100 * dw / (1 << 20):6.3f}%)')
        spec = dispspec(e)
        if spec:
            topng.render(rawfn, [spec], stem=os.path.join(ed, 'hw'))
            if dw:
                topng.compare(os.path.join(ed, 'model.bin'), rawfn, [spec])
    print(f'-- {ndone} frames run, {nsame} bit-identical to the model')


def status():
    for e in entries():
        ed = os.path.join(HWT, e)
        hw = 'hw:yes' if os.path.exists(os.path.join(ed, 'hwvram.bin')) else 'hw:no '
        print(f'{e}  {hw}  {longname(e)}')


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'status'
    if cmd == 'add':
        dirs = sys.argv[2:] or sorted(
            os.path.join(OUT, n) for n in os.listdir(OUT))
        add(dirs)
    elif cmd == 'compare':
        compare()
    elif cmd == 'status':
        status()
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
