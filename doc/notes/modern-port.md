# The modern build

`tools/build-modern.sh` compiles the same `src/` and `include/` with the
host's own compiler, natively 64-bit.  Nothing forks: both builds read
one copy of every header, and the era objects stay byte-identical.

    tools/build-modern.sh              # amd64 (default)
    BITS=32 tools/build-modern.sh      # i386, next to build.sh's output
    CXX=clang++ CC=clang tools/build-modern.sh

## The constraint, and how it was held

Every edit below had to leave all 23 era-compiled objects unchanged, to
the byte.  The gate, run before the first edit and after every batch:
compile all 23 with `tools/gcc272/g++272` under
[build.sh](../../tools/build.sh)'s per-module flags and `cmp` against the
pre-edit reference.  **23/23 byte-identical at every checkpoint.**

That leaves three shapes of edit:

1. `#if __GNUC__ < 3` / `#if __GNUC__ >= 3` around code the era
   preprocessor can be told to skip (it reports `__GNUC__ == 2`).
2. Constant expressions that fold to the era value on their own:
   `(sizeof(void *) == 8) * N` is 0 everywhere but LP64, and
   `(__GNUC__ >= 3) * N` is 0 for the era compiler.  This is the form
   used inside `src/txm.c`, whose `assert` `__LINE__` is load bearing and
   so cannot absorb a three-line `#if` block.
3. Compiler flags and one force-included header (`include/modern.h`),
   which the era build never sees at all.

## Dialect: three fixes, and that is all

Out of 23 modules, 20 compiled with a 2024 g++ untouched.  The three that
did not:

* **`include/param.h` - ambiguous `operator<<`.**  `param.o` defines both
  `operator<<(const param&, int)` and `operator<<(const param&, const
  int&)`.  gcc 2.7 binds `p << 4` to the `int` one, and nothing in the
  archive ever calls the reference form (`nm` over all 23 objects: only
  `__ls__FRC5parami` is ever imported).  ISO C++ calls the pair
  ambiguous, so the reference *declaration* is now `#if __GNUC__ < 3`.
  `src/param.c` still defines both, unconditionally, so the era object is
  unchanged.
* **`memcpy` in `include/xif.h`, `strcpy` in `src/gpu2reg.c`.**  The
  era's libc5 `<stdio.h>`/`<stdlib.h>` leaked the string functions;
  today's do not.  Fixing it in the sources would move the `__LINE__`s
  that `xif.h`, `pcrtc.h`, `txm.h` and `gpu2vec.h` bake into their
  objects, so `include/modern.h` pulls `<string.h>` in from the command
  line instead (`-include`), where the era compiler cannot see it.

Everything else the modern compiler says about this code is
`-Wwrite-strings` (178 of them: 1998 C++ handed string literals to
`char *`), plus two `-Wmismatched-new-delete` in `gpu2reg.c` that are the
*original's* documented `delete`-on-`new[]` bug, preserved on purpose.

## The real portability bug: the vptr moved

g++ 2.7 puts a class's vptr **after** all its data members (doc/ABI.md);
every ABI since puts it first.  That is invisible while a class has one
definition -- but the reconstruction deliberately gives several TUs their
own *view* of a neighbouring class, because the 1998 sources did (the
per-object notes cite the evidence).  Several of those views omit the
neighbour's `virtual`, which under the era ABI still lands every member
at the right offset and under a modern one shifts them all by a pointer.

That is one silent bug, and it is total: at `-m32` the first build linked
and ran and rasterized *nothing*.  `PCalc::Put` read `p->send_type` four
bytes low, always saw 0, and took the `Primitive` arm where the era build
takes `Register`.  Three views were affected:

| view | canonical | fix |
| --- | --- | --- |
| `include/pcalc.h` `class Pre3` | `pre3.h` (virtual `Put`) | leading `era_vptr_hole` under `#if __GNUC__ >= 3` |
| `include/clut.h` `class MemIF` | `memif.h` (virtual) | same |
| `src/txm.c` `class DDA` | `dda.h` (virtual) | its leading `char m_000[]` hole grows instead |

With those three, `-m32` passes everything: probe 0 failures, all three
corpus dumps bit-identical.

## The 64-bit audit

On LP64 two things move: pointers widen, and `long long`/`double` go from
4-byte to 8-byte alignment inside structures.  In-memory layout changing
is fine -- the model addresses its VRAM with its own arithmetic, never
with host pointers -- as long as **every TU agrees on the same layout**.
The per-TU views are the only thing that cannot follow along on their
own, because their holes are written as 1998 byte offsets.

The audit was mechanical, and is worth re-running after any change to a
class layout: compile each TU `-m64 -g -fno-eliminate-unused-debug-types
-femit-class-debug-always`, run `pahole -C <class>` per (class, TU), and
compare member offsets against the canonical TU.  It found seven classes
disagreeing; five needed real fixes:

| TU | class | hole | LP64 growth |
| --- | --- | --- | --- |
| `src/txm.c` | `DDA` | `m_000` | +28 (2 pointers, 4 `DDAvalue`s) |
| `src/gpu2.c` | `DDA` | `m_008` | +28 |
| `src/gpu2.c` | `PCalc` | `m_ab8`, `m_bb4` | +4 each |
| `src/gpu2.c` | `Pre3` | `m_008` | +4 |
| `src/gpu2.c` | `Pre1` | `m_000` | +4 (allocation size only) |
| `include/dda.h` | `PCalc` | `m_000` | +12 |
| `src/dbg.c` | `Pre3` / `PCalc` / `DDA` | `m_head`, `m_dda` | +8 / +16 / +40 |

`src/gpu2.c` matters most: `GPU2::GPU2` is where every stage is `new`ed,
so an undersized view is a heap overrun, not just a wrong offset.

Six views still differ in **size** and are meant to: they are opaque
handles whose TU neither allocates the class nor reads past the members
it declares, and every member it *does* declare is at the canonical
offset.  These are `AddrConv` in `addrconv.c` (no data members at all,
and `address_convert` touches none), `GPU2` in `dbg.c` (just the leading
`PP *`), `Memory` in `bitblt.c`/`clut.c` (just the 4 MB `vram[]`, which
is at offset 0 in the real class too), `MemIF` in `clut.h` and `PCalc` in
`pre3.h` (virtual-call target only), and `TexFunc` in `texfunc.c`.

### Types

The ILP32 assumption turned out to be almost absent from the sources.
A sweep for bare `long` (not `long long`) finds exactly two places:

* `src/dda.c:79` `IsMinusDCDX(long v)` -> `(unsigned long)v >> 19 & 1`.
  Width-independent: bit 19 of a value is bit 19 whether the conversion
  to `long` truncates at 32 bits or sign-extends to 64.
* `src/xif.c` / `include/xif.h` -- `unsigned long rmask/gmask/bmask`,
  `SetColormap`, `highbit`.  These are Xlib's own type for visual masks
  and pixel values and *must* stay `unsigned long`; they are right on
  both.  `XWindow` grows on LP64 as a result, consistently (it has one
  definition).

`slong` -- "the signed-long helpers" -- is a red herring for this
purpose: it is `unsigned long long` throughout, so it is width-stable.
There are no pointer-to-integer casts anywhere in the model; every `(int)`
cast in `src/` narrows a `long long` register datum or a `float`.

## Floating point

`doc/RECONSTRUCTION.md` warns that the objects were compiled for a 387
and that 80-bit excess precision is observable.  The two runtime-built
tables are where it would show, since both truncate a `double`
expression to an integer:

* `div.c` `Reciproc::Reciproc` -- `tbl0`/`tbl1`, 256 entries each,
  `(long long)(y * 268435456.0)`.
* `txm_div.c` `NormTexCoord::mktable` -- `OFFSET_TBL`/`SLOPE_TBL`,
  128 entries each, `(int)rint(...)`.

Measured rather than assumed: all four tables were dumped out of a live
`gsreplay` (break on the builder, `finish`, `dump binary memory`) from
the era i386 build and from three modern builds -- m32/x87, m64/x87,
m64/SSE.  **All four tables are byte-identical in all four builds.**

    Reciproc tbl0   e4fe3fa9d7f1a9b3584448eeb0701d28   (era == m32 == m64x87 == m64sse)
    Reciproc tbl1   a32e0b780db158b01f77980209efa1a3
    OFFSET_TBL      e1d2bdb91d862b47fb49ebd787762221
    SLOPE_TBL       01a8a99a3b5a6871f295847927ace18a

A full `-m64` SSE build also replays all three corpus dumps to the exact
era md5s and passes probe with 0 failures.  So SSE is not *observed* to
differ -- but `-mfpmath=387` is still what `build-modern.sh` asks gcc for,
because it is the semantics the model was written against and costs
nothing; the script probes for it and drops it for clang, which does not
accept `-mfpmath=` on x86 at all (and defaults to x87 at `-m32` anyway).

`-fno-strict-aliasing` is not optional in the same way: `clut.c`,
`drawprim.c` and `gpu2reg.c` punt floats through `*(int *)&f`
(`gpu2reg.h`'s `FtoI`), which `-fstrict-aliasing` is entitled to
miscompile.  `-fwrapv` is there for the same reason -- the model shifts
and negates signed values freely, the way 1998 C++ did.

## Results

Behaviour gate, identical for both widths:

| build | probe | r614 | o519 | RRV 142940 |
| --- | --- | --- | --- | --- |
| era i386 (baseline) | 0 failures | `9c7c73b1…` | `9fbc3187…` | `82657dfd…` |
| modern `-m32` | 0 failures | equal | equal | equal |
| modern `-m64` | 0 failures | equal | equal | equal |
| modern `-m64` clang | 0 failures | equal | equal | - |

Full md5s: `9c7c73b156f8664633055e0300990a82`,
`9fbc3187d1f98e0dff84e5d9aa5689df`,
`82657dfd651625d235983775b7bef849`.

### Speed

Replaying the 28 MB Ridge Racer V dump
(`out/Ridge Racer V_SLUS-20002_20260902143700`), same machine, same
end-state md5:

    era i386,  g++ 2.7.2.3 -O      1446.6 s   (24m 07s)
    modern amd64, g++ 14.2.1 -O2    324.1 s   ( 5m 24s)
                                    -------------------
                                    4.5x faster

Both are our 23 objects and no 1998 ones (the era side is build.sh's
`OWN=1` archive), both single-threaded on an otherwise idle 8-core box,
and both end on the same VRAM: md5 `a59380f08dd221c060d81a85958c53f5`.
That is a fourth dump, four times the size of the corpus's RRV frame,
agreeing bit for bit -- worth having as its own datapoint.

## A README quickstart snippet

> ### Building it natively
>
> `tools/build.sh` uses the bundled 1998 compiler and produces 32-bit
> binaries, because the original archive is 32-bit.  For everyday use
> there is a native build of the same sources:
>
> ```sh
> tools/build-modern.sh          # -> tools/gsreplay64, tools/probe64
> tools/gsreplay64 mydump -w
> BITS=32 tools/build-modern.sh  # the i386 variant, if you want to compare
> ```
>
> It needs only a C++ compiler (g++ or clang) and libX11 - no 32-bit
> userland, no era compiler.  It replays the corpus to the same VRAM,
> byte for byte, as the 1998 build, and is about 4.5x faster.
> How the same sources satisfy both compilers is
> [doc/notes/modern-port.md](doc/notes/modern-port.md).

## Files touched

    include/param.h       guarded the ambiguous operator<< declaration
    include/pcalc.h       vptr hole in the Pre3 view
    include/clut.h        vptr hole in the MemIF view
    include/dda.h         LP64 growth in the PCalc view's hole
    include/modern.h      new; force-included by the modern build only
    src/txm.c             vptr hole + LP64 growth in the DDA view
    src/gpu2.c            LP64 growth in the PCalc/Pre3/Pre1/DDA views
    src/dbg.c             LP64 growth in the Pre3/PCalc/DDA views
    tools/shims-modern.c  new; operator new/delete + the i386 divide helpers
    tools/build-modern.sh new
    .gitignore            the suffixed modern build products
