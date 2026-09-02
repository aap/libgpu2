#!/usr/bin/env python3
"""hwprep.py - turn a gsprep output dir into a bundle the TOOL runner eats.

    hwprep.py <outdir-from-gsprep>     (writes into <outdir>/hw/)

Real-hardware replay of a GS dump (tools/ps2/gsrun.c on a DTL-T10000):
the runner seeds VRAM, streams the GIF records, applies the priv-reg
snapshots, and reads VRAM back -- same contract as gsreplay, with the
silicon as the model.

Writes:
    hw/vram_hw.bin    4 MB, vram.bin pushed through the inverse PSMCT32
                      swizzle as one 1024x1024 raster image: uploading it
                      with LoadImage (BW=16) makes hardware VRAM land
                      byte-identical to vram.bin's raw layout.  The same
                      LUT maps the runner's StoreImage readback to raw
                      (tools/gscmp.py).
    hw/hwstream.bin   "GSHW" u32 nrec, u32 smode2, u32 flags, then
                      16-byte-aligned records:
                        u8 type, u8 path/field, u16 0, u32 nqw, u64 0
                        + nqw*16 payload
                      type 0 GIF data; type 1 vsync; type 2 priv regs,
                      condensed to 4 qw: PMODE SMODE2 DISPFB1 DISPLAY1
                      DISPFB2 DISPLAY2 BGCOLOR 0.

The GIF stream is passed UNGATED: no MTBA clearing, no 0x11-0x13
dropping -- hardware is the referee for the 1998-dialect questions.
"""
import struct, sys, os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import topng


def lut32():
    """word address of every pixel of a 1024x1024 PSMCT32 image at
    tbp 0, bw 16 -- a bijection onto the 1M words of VRAM"""
    y, x = np.mgrid[0:1024, 0:1024]
    blk = np.asarray(topng.blockTable32)[(y % 32) >> 3, (x % 64) >> 3]
    col = np.asarray(topng.columnTable32)[y % 8, x % 8]
    lut = ((y >> 5) * 16 + (x >> 6)) * 2048 + blk * 64 + col
    assert np.bincount(lut.ravel(), minlength=1 << 20).min() == 1
    return lut.ravel()


# priv-reg byte offsets inside the 8 KB page image (= real 0x12000000 map)
PRIV = [0x00, 0x20, 0x70, 0x80, 0x90, 0xa0, 0xe0]  # PMODE SMODE2 DISPFB1
                                                   # DISPLAY1 DISPFB2
                                                   # DISPLAY2 BGCOLOR

def prep(outdir, hw=None):
    hw = hw or os.path.join(outdir, 'hw')
    os.makedirs(hw, exist_ok=True)

    vram = np.fromfile(os.path.join(outdir, 'vram.bin'), dtype='<u4')
    assert vram.size == 1 << 20
    vram[lut32()].tofile(os.path.join(hw, 'vram_hw.bin'))

    d = open(os.path.join(outdir, 'stream.bin'), 'rb').read()
    assert d[:4] == b'GSR1'
    nrec = struct.unpack_from('<I', d, 4)[0]
    p, out, smode2 = 8, [], None
    for _ in range(nrec):
        typ, path = d[p], d[p + 1]
        nqw = struct.unpack_from('<I', d, p + 4)[0]
        p += 8
        if typ == 0:
            out.append(struct.pack('<BBHIQ', 0, path, 0, nqw, 0))
            out.append(d[p:p + nqw * 16])
            p += nqw * 16
        elif typ == 1:
            out.append(struct.pack('<BBHIQ', 1, path, 0, 0, 0))
        elif typ == 2:
            regs = [struct.unpack_from('<Q', d, p + o)[0] for o in PRIV]
            if smode2 is None:
                smode2 = regs[1]
            out.append(struct.pack('<BBHIQ', 2, 0, 0, 4, 0))
            out.append(struct.pack('<8Q', *regs, 0))
            p += nqw * 16
        else:
            sys.exit(f'bad record type {typ} at {p - 8}')
    assert p == len(d), (p, len(d))

    with open(os.path.join(hw, 'hwstream.bin'), 'wb') as f:
        f.write(struct.pack('<4sIII', b'GSHW', nrec,
                            (smode2 or 0) & 0xffffffff,
                            1 if smode2 is not None else 0))
        f.write(b''.join(out))
    print(f'{hw}: {nrec} records, smode2='
          f'{"none" if smode2 is None else hex(smode2)}')


if __name__ == '__main__':
    prep(sys.argv[1])
