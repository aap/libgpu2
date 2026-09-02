#!/usr/bin/env python3
"""gscmp.py - compare a TOOL hardware readback against a model snapshot.

    gscmp.py <model-snap.bin> <hwvram.bin> [tbp:bw:w:h[:name[:psm]] ...]

<model-snap.bin> is gsreplay's raw 4 MB VRAM (real-GS layout).
<hwvram.bin> is gsrun's StoreImage readback (1024x1024 PSMCT32 raster);
it is converted back to raw layout through the same LUT hwprep.py used,
written next to it as hwvram_raw.bin, and compared byte-for-byte.
Optional buffer specs (topng syntax) render per-pixel diff maps.
"""
import sys, os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import topng
from hwprep import lut32


def main():
    snapfn, hwfn = sys.argv[1], sys.argv[2]
    specs = sys.argv[3:]

    img = np.fromfile(hwfn, dtype='<u4')
    assert img.size == 1 << 20, f'{hwfn}: want 4 MB'
    raw = np.empty(1 << 20, dtype='<u4')
    raw[lut32()] = img
    rawfn = os.path.join(os.path.dirname(hwfn) or '.', 'hwvram_raw.bin')
    raw.tofile(rawfn)

    snap = np.fromfile(snapfn, dtype='<u4')
    assert snap.size == 1 << 20, f'{snapfn}: want 4 MB'
    dw = int((snap != raw).sum())
    db = int((snap.view(np.uint8) != raw.view(np.uint8)).sum())
    print(f'words differing: {dw}/1048576 ({100 * dw / (1 << 20):.3f}%)  '
          f'bytes: {db}/4194304')
    if dw:
        pages = np.unique((snap != raw).nonzero()[0] >> 11)
        print(f'pages touched: {pages.size}/512  first: '
              + ' '.join(hex(p) for p in pages[:16]))
    if specs:
        topng.compare(snapfn, rawfn, specs)
    sys.exit(0 if dw == 0 else 1)


if __name__ == '__main__':
    main()
