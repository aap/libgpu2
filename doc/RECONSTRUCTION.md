# Reconstruction workflow

How the source in `src/` was produced and how every drop was verified.
The per-object results live in [MATCHING.md](MATCHING.md); this file is
the method.

## The oracle pipeline

The dump → replay → render chain in `tools/` doubles as the
decompilation oracle: replace an object, replay the corpus, demand
byte-identical VRAM.

Pieces (all in `tools/`, shared history with
`ps2rev/osdsys/osdbits/tools/gsreplay/` where the harness was built):

    gs2png       one command: PCSX2 dump -> PNG of the displayed frame
                 (--spec tbp:bw:w:h[:name[:psm]] overrides buffer
                 detection when a dump displays something unusual)
    gsprep.py    dump.gs -> vram.bin / stream.bin / state blobs / shot.png
    gsreplay.c   the replayer; -s/-e snapshot specs, -w the model's own
                 X11 display, -Z/-x experiment knobs, register gate for
                 the model's fatal-exit design
    topng.py     VRAM -> PNG (PSMCT32/24/16; 16S table present, unverified)
    probe.c      behaviour test suite (0 failures against the model)
    swz.c fmt.c  derive/verify the model's in-page swizzle vs retail
    regprobe.c   measure which registers are fatal
    build.sh     clang -m32 + hand ld link; REPLACE="..." hybrid archive,
                 OWN=1 links against our objects only (no 1998 members)
    build-modern.sh  the same sources under the host g++/clang, native
                 amd64 -- the everyday build (notes/modern-port.md)
    hwtest.py    build/compare suites for replay on a real DTL-T10000
    gcc272/      the era compiler (see compiler.md)

## Workflow

One object at a time; originals fill the gaps:

    REPLACE="addrconv slong" tools/build.sh    # hybrid archive
    tools/gs2png dumps/... -f                  # oracle replay
    tools/topng.py -c out/A/snap.bin out/B/snap.bin   # byte-diff buffers

Verification tiers, all three required per object:

1. **Differential**: link Sony's object (symbols prefixed via objcopy)
   and ours into one i386 test binary, drive both sides identically,
   compare every observable per call — millions of records per object,
   with deliberately-broken canary builds to prove the harness bites.
   Scripts: `test/run_<object>.sh`.
2. **Oracle replay**: hybrid archive, replay the dump corpus, demand
   byte-identical 4 MB VRAM snapshots (md5 of the end state).  For the
   three members the gsreplay link never pulls (gpu2reg, drawprim,
   gpu2vec) this is a *non-perturbation* test: swapping them in must
   change nothing, or a symbol leaked.
3. **Byte-matching**: compile with the era g++ 2.7.2.3 (`tools/gcc272/`,
   see compiler.md) and diff objects — .text per function, .rodata,
   .data, relocation sets, symbol tables.  Required for per-object
   swaps of the virtual-dispatch pipeline classes: modern compilers
   cannot reproduce the 1998 C++ ABI (ABI.md).  Accepted residual
   classes are defined in MATCHING.md; everything else must match.

Attack order (as executed): leaf arithmetic (addrconv, slong, div,
txm_div, texfunc, clut, dbg) → pre1/pre3 → memory/bitblt/memif → dda →
pcalc, txm → pcrtc/xif → gpu2 → gpu2reg/drawprim, gpu2vec.

## Ground rules

- Struct layouts come from evidence only: `__builtin_new` sizes, ctor
  stores, member-access offsets (STRUCTS.md cites instructions).
  Earlier machine-generated survey material in the reference tree is
  leads, not truth — five of its claims are disproven
  (STRUCTS.md §"prior-claim corrections").
- Behaviour claims get measured against the model before being written
  down; keep observation and inference marked apart.
- Original bugs are preserved, not fixed — the target is the 1998
  object, bug for bug (the per-object notes document each one found).
- x87: the objects were compiled for 387.  80-bit excess precision is
  observable behaviour; modern builds must force x87 (`-mno-sse`)
  wherever tier-3 verification does not exist.  (For the model itself
  this was then measured: its four runtime-built FP tables and all
  corpus replays are byte-identical under SSE too, so build-modern.sh
  asks gcc for `-mfpmath=387` as belt and braces and lets clang use its
  default -- notes/modern-port.md.)
- Sony-derived data (original objects, dumps, replay output) is never
  committed; the repo carries only reconstructed source, tools and
  documentation.
