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

    # Per-path GIF tag reassembly.  The dump interleaves records from the
    # four paths at packet granularity, and a tag's payload can continue in
    # a later record of the same path; the model keeps a tag state machine
    # per path, and the real GIF only switches paths at tag boundaries.  A
    # single-FIFO replay therefore must emit whole-tag chunks: buffer each
    # path, flush every time its pending tags complete, and drop a path's
    # residue whose payload never arrives (an in-flight transfer truncated
    # by the vsync-aligned dump start/end -- the model parks it forever,
    # contributing nothing).
    pbuf = {}   # path -> [bytearray pending, qwords still owed to open tag]
    nout = [0]  # records actually emitted (reframing merges and drops)
    def flush(path):
        buf, rem = pbuf[path]
        if not buf:
            return
        # scan complete tags from the front
        off, r = 0, 0
        while off < len(buf):
            if r == 0:
                if off + 16 > len(buf):
                    break
                lo, = struct.unpack_from('<Q', buf, off)
                nloop = lo & 0x7fff
                flg = lo >> 58 & 3
                nreg = lo >> 60 & 15 or 16
                r = (nloop * nreg if flg == 0 else
                     (nloop * nreg + 1) // 2 if flg == 1 else nloop)
                need = (r + 1) * 16
                if off + need > len(buf):
                    break               # tag's payload incomplete: keep
                off += need
                r = 0
            else:
                assert 0
        if off:
            nout[0] += 1
            out.append(struct.pack('<BBHIQ', 0, path, 0, off // 16, 0))
            out.append(bytes(buf[:off]))
            del buf[:off]
        pbuf[path][1] = r

    for _ in range(nrec):
        typ, path = d[p], d[p + 1]
        nqw = struct.unpack_from('<I', d, p + 4)[0]
        p += 8
        if typ == 0:
            buf = pbuf.setdefault(path, [bytearray(), 0])[0]
            buf += d[p:p + nqw * 16]
            flush(path)
            p += nqw * 16
        elif typ == 1:
            nout[0] += 1
            out.append(struct.pack('<BBHIQ', 1, path, 0, 0, 0))
        elif typ == 2:
            regs = [struct.unpack_from('<Q', d, p + o)[0] for o in PRIV]
            if smode2 is None:
                smode2 = regs[1]
            nout[0] += 1
            out.append(struct.pack('<BBHIQ', 2, 0, 0, 4, 0))
            out.append(struct.pack('<8Q', *regs, 0))
            p += nqw * 16
        else:
            sys.exit(f'bad record type {typ} at {p - 8}')
    assert p == len(d), (p, len(d))
    for path, (buf, rem) in sorted(pbuf.items()):
        if buf:
            print(f'  dropped path-{path} residue: {len(buf) // 16} qw '
                  f'buffered, open tag owed {rem} more qw')

    with open(os.path.join(hw, 'hwstream.bin'), 'wb') as f:
        f.write(struct.pack('<4sIII', b'GSHW', nout[0],
                            (smode2 or 0) & 0xffffffff,
                            1 if smode2 is not None else 0))
        f.write(b''.join(out))
    print(f'{hw}: {nout[0]} records ({nrec} in), smode2='
          f'{"none" if smode2 is None else hex(smode2)}')


if __name__ == '__main__':
    prep(sys.argv[1])
