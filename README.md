# libgpu2 decompilation

Reconstructing buildable C++ source for Sony's 1998 behavioural reference
model of the PlayStation 2 Graphics Synthesizer (`libgpu2` Ver1.12.0, the
renderer behind the SKY EE simulator): 23 unstripped ELF32 i386 objects,
g++ 2.7.2.3, recovered from a PS2 toolchain ISO.

The originals live in `/u/aap/othersrc/libgpu2` (surveys: `FINDINGS.md`,
`re/`, `port/`) and, as build inputs, in `orig/` here.  **Neither `orig/`
nor `dumps/` nor any other Sony-derived data is ever committed** — this
repo carries only reconstructed source, tools, and documentation.

## Layout

    src/        reconstructed source, one .c per original object (C++ in
                .c files, as Sony had it; original tree was rooted gpu2u/)
    include/    reconstructed headers
    orig/       [not committed] the 1998 objects + archive + libgpu2.h
    dumps/      [not committed] PCSX2 GS dumps used as test inputs
    out/        [not committed] replay/render output
    tools/      the oracle harness (see below)
    test/       differential + behaviour tests, growing per object
    doc/        ABI.md (g++ 2.7 ABI facts), STRUCTS.md (layout evidence),
                compiler.md (era-compiler bring-up)

## The oracle pipeline

One command, PCSX2 GS dump in, PNG out (builds the harness on first use):

    tools/gs2png dumps/whatever.gs           # -> whatever/snap000_end_fb1.png
    tools/gs2png dump.gs --vsync             # a PNG per frame
    tools/gs2png dump.gs --spec tbp:bw:w:h[:name[:psm]]   # manual buffers

`gs2png` unpacks the dump (`gsprep.py`), replays the GIF stream through the
1998 model (`gsreplay`, snapshotting the 4 MB VRAM in real-GS layout), reads
PMODE/DISPFB/DISPLAY/SMODE2 out of the dump's privileged-register records to
find what was on screen, and renders it (`topng.py`).  The dump's own
embedded screenshot lands beside the output as `shot.png` for comparison.

Pieces (all in `tools/`, shared history with
`ps2rev/osdsys/osdbits/tools/gsreplay/` where the harness was built):

    gsprep.py    dump.gs -> vram.bin / stream.bin / state blobs / shot.png
    gsreplay.c   the replayer; -s/-e snapshot specs, -Z/-x experiment knobs,
                 register gate for the model's fatal-exit design
    topng.py     VRAM -> PNG (PSMCT32/24/16; 16S table present, unverified)
    probe.c      behaviour test suite (0 failures against the model)
    swz.c fmt.c  derive/verify the model's in-page swizzle vs retail
    regprobe.c   measure which registers are fatal
    build.sh     clang -m32 + hand ld link against orig/lib

## Decompilation workflow

Replace one object at a time; originals fill the gaps:

    REPLACE="addrconv slong" tools/build.sh    # hybrid archive
    tools/gs2png dumps/... -f                  # oracle replay
    tools/topng.py -c out/A/snap.bin out/B/snap.bin   # byte-diff buffers

Verification tiers:
1. **Differential**: link Sony's object (symbols prefixed via objcopy) and
   ours into one test binary, compare per function.  For leaves.
2. **Oracle replay**: hybrid archive, replay the dump corpus, demand
   byte-identical VRAM snapshots.
3. **Byte-matching**: compile with the era g++ 2.7.2.3 (`tools/gcc272/`,
   see doc/compiler.md) and diff objects.  Required for per-object swaps
   of the virtual-dispatch pipeline classes — the modern-compiler tiers
   cannot reproduce the 1998 C++ ABI (doc/ABI.md).

Attack order: leaf arithmetic (addrconv, slong, div, txm_div, texfunc,
clut, dbg, drawprim) → pre1/pre3 → memory/bitblt/memif → dda → pcalc, txm
→ gpu2reg, gpu2vec, pcrtc/xif → gpu2, libgpu2.

Status (doc/MATCHING.md has the per-object scoreboard):

- **addrconv** done: 100% byte-identical (.text/.data/.rodata/relocs), differential + oracle clean.
- **libgpu2** done: 7/9 functions byte-exact, rest allocation-shape only; differential + oracle clean.
- **pre1/pre3** done: 8/16 functions byte-identical, rest instruction-identical or a few insns short (suspected compiler-mod artifacts); 711k-snapshot differential + oracle clean, including a full Ridge Racer V dump.

Long-term goal beyond a buildable library: a native amd64 build and an
interactive GS debugger on top of it — live framebuffer view, register
state, stepping through primitives (the virtual-Put tap architecture is
exactly the hook point for this).

## Ground rules

- Struct layouts come from evidence only: `__builtin_new` sizes, ctor
  stores, member-access offsets (doc/STRUCTS.md cites instructions).
  The DeepSeek-era generated material in othersrc (`FINDINGS.md`, `re/`)
  is leads, not truth — four of its claims are already disproven
  (doc/STRUCTS.md §"prior-claim corrections").
- Behaviour claims get measured against the model before being written
  down; keep observation and inference marked apart.
- x87: the objects were compiled for 387.  80-bit excess precision is
  observable behaviour; modern builds must force x87 (`-mno-sse`) until
  tier-3 verification exists.
