# gpu2.o — GPU2, the model's top-level class

Reconstruction notes for `orig/lib/gpu2.o` → `src/gpu2.c`.
Read 2026-09-03.  Six own functions (0xca0 bytes incl. padding), plus
0xe4a bytes of weak inline copies instantiated from pcrtc.h/xif.h, 0x430
of `.rodata` (strings + 14 local vtables), 0xc of `.bss`.

## Build recipe

    env GCC272_1998=1 tools/gcc272/g++272 -O -idirafter /usr/include \
        -Iinclude -c src/gpu2.c

`GCC272_1998=1` is forced by the virtual calls (`GPU2::Put`,
`ResizeWindow`, `PPOut::Put` all use the 1998 vfn-ref codegen:
`mov 0x4(%ecx),%edx; call *%edx` and the reload of the object pointer
for the this-adjust).  `-idirafter /usr/include` is for X11 via xif.h,
exactly as for pcrtc.c/xif.c.  tools/build.sh needs `gpu2` added to the
`xif|pcrtc` era-case.

## What the object is

The task briefing conflated two objects: the `GS_*` API functions live
in **libgpu2.o** (already reconstructed).  gpu2.o is the GPU2 class:

| fn | bytes | what it does |
|---|---|---|
| `dumpCRT__FiiPCUi` (file static) | 135 | XWindowDump's frame callback: (re)allocate `r_buf`, copy the frame, reset `r_count` |
| `GetCRT__4GPU2` | 56 | `r_count < r_size ? r_buf[r_count++] : 0` |
| `__4GPU2Pciii` | 2845 | allocate + wire the whole pipeline (below) |
| `Get__4GPU2` | 26 | `m->bitblt.ReadPixel(m)` — one 64-bit host-ward read |
| `Put__4GPU2ix` | 92 | route: `(char)addr < 0 \|\| addr > 0xff` → virtual `pcrtc->SetRegister`, else direct `pp->pre1->Put`; returns 1 |
| `ResizeWindow__4GPU2ii` | 46 | virtual `pcrtc->Resize(w, h)` |

The weak tail (PCRTC/PCRTCdmy virtuals, MemRead16/24/32::ReadPixel,
PixelBlend1a/Alp::blend, all of XWindowDump, ~Xifbase, PPOut::Put) is
emitted from the included pcrtc.h/xif.h plus gpu2.c's own PPOut; see
"header shape" below.

## The constructor, and what it pins

`GPU2::GPU2(char *title, int width, int height, int disp_on)` — the
allocation map in doc/STRUCTS.md came from this function and held up.
Findings beyond it:

* **The 0x4001c8 block is just `new Memory`** — include/memory.h's
  Memory (4 MB vram + FB/ZB configs + the trailing `BitBLT bitblt`)
  already has exactly that sizeof; there is no ctor call.  `GPU2::Get`
  is `m->bitblt.ReadPixel(m)` through a `Memory *m = mem;` local (the
  local is byte-visible: it lets the last use clobber the pseudo,
  `push %edx; add $0x400144,%edx; push %edx`; spelling it `mem->...`
  keeps `mem` in a second register and costs 2 bytes).
* **The front-end block is a ctor-less `class PP`** { pre1, pre3,
  pcalc, ppout } — same shape dbg.c already used.  It is built into a
  local (`PP *p`, live in %edi across all five allocations) and only
  assigned to `this->pp` at the end.
* **PPOut/PPDDA** are declared in gpu2.c itself (as in dbg.c), PPDDA
  *before* txm.h and PPOut *after* it but *before* pcrtc.h — the local
  vtable emission order (reverse declaration order) pins exactly this
  interleaving: `.rodata` ends `_vt.5PPOut, _vt.6DDATXM, _vt.5PPDDA`.
* **Every stage ctor the header modules reconstructed inlines here
  byte-for-byte**: PCalc(PPDDA*), Pre3(PCalc*), DDA(DDATXM*) (bodies
  copied verbatim from pcalc.h/pre3.h/dda.h into local views), TXM
  (from txm.h, countdown loop included), MemIF (body repeated in gpu2.c
  as `inline MemIF::MemIF` since include/memif.h declares it
  out-of-line; the 1998 memif.h evidently had it inline — gpu2.o
  inlines it and has no call), PCRTC/PCRTCdmy/PCRTCxif (from pcrtc.h),
  XWindow/XWindowDump/Frame2d (from xif.h).  `Pre1::Pre1` and
  `param::param`/`Reciproc::Reciproc` stay out-of-line calls.
* **disp_on is a 4-case switch** (`case 0` PCRTCdmy, `1` PCRTCxif with
  a real XWindow, `2` PCRTCxif with an XWindowDump + `dumpCRT`,
  `default` `fprintf(stderr, "invalid argument xdisp -- [%d]\n")` +
  `exit(1)`), cases in that source order.
* **`GPU2::Put` returns int** (always 1), and its else-arm goes through
  a block-scoped `PP *q = pp;` — the 1998 object loads `pp` *before*
  the three argument pushes and `pre1` after them, which no plain
  spelling of `pp->pre1->Put(...)` produces (stock g++ 2.7 emits both
  loads after the pushes).  The same q-before-pushes shape appears at
  the same spot in gpu2vec.o's `GPU2VEC::Put`.
* `GetCRT` returns `unsigned int`; the bss is
  `static unsigned int r_count, r_size; static unsigned int *r_buf;`
  in exactly that declaration order (bss offsets 0/4/8) while the
  *symbol table* order (r_buf, r_size, r_count) is first-use order in
  dumpCRT — both fall out of the one spelling.
* dumpCRT's realloc/malloc size is spelled `w*4*h` (gcc 2.7 swaps the
  constant-times-variable grouping, so `w*4*h` compiles to the object's
  `lea 0(,h,4); imul w` — `h*4*w` comes out mirrored) and the memcpy
  length `r_size*4` rides the CSE of the just-stored product.

## Header facts this object proved (fixes applied to shared headers)

pcrtc.o and gpu2.o are the only objects that inline Frame2d's and
XWindowDump's constructors; doc/notes/pcrtc.md had already predicted
all three fixes and this object forced them in:

1. **xif.h's assert `__FILE__` is parameterised** (`XIF_FILE`, default
   `"xif.h"`); pcrtc.h defines it to `"../gpu2u/xif.h"` before
   including xif.h, which is the string both pcrtc.o and gpu2.o carry.
   Line anchors 139/146/158 preserved.  xif.o is bit-identical before
   and after (verified).
2. **`XWindowDump::XWindowDump(void (*f)(int, int, const unsigned int *) = 0)`**
   with body `{ func = f; bg = 0; }` (stores in that order), and
   pcrtc.h's dump-ctor does `new XWindowDump(func)`.  The `const` in
   the member type matches `__8PCRTCxifP6MemoryPciiPFiiPCUi_v`.
3. **`Frame2d::Frame2d` mallocs from the parameters**
   (`malloc(height*4*width)`), so the 256×256 defaults fold to
   `push $0x40000` as in both 1998 objects.  (`Resize` keeps using the
   members — xif.md's lesson still holds there.)

Net effect on pcrtc.o: dump ctor +67 → +51, `.rodata` now identical to
the 1998 object except GAS's two `8d 76` pad bytes.  Also *rejected* by
experiment: turning UPDATEMERGE's mw/mh if/else into ternary-minus
(matches the ctor's shared-subtraction shape but breaks SetDISPLAY1/2's
444-byte size match and costs SetRegister 200+ bytes — the ctor sharing
is cross-jumping over identical arm tails, not source shape).

## The `int Put(int, long long)` bonus: libgpu2.o's initPCRTC falls

Declaring `GPU2::Put` with its true `int` return type in include/gpu2.h
(the object always sets %eax to 1) changes the RTL of every call site in
libgpu2.o — the call insn now clobbers a value register — and that flips
`initPCRTC`'s DImode register allocation into exactly the 1998 layout:
**initPCRTC 458 bytes, byte-identical** (was the documented +79
register-allocation residual).  libgpu2.o is now 8/9 functions exact;
`GS_SaveImage` keeps its -16 spill residual, and GS_OpenSim's single
differing byte is the `call initPCRTC` displacement that moves with
SaveImage's size.  `test/run_libgpu2_diff.sh`: 39710 checks, 0 failures
after the change.

## The GS_SaveImage patch (task item)

`orig/lib/libgpu2-patched.a` differs from the pristine archive in its
**libgpu2.o** member only (GS_SaveImage `fwrite` size 3 → 4); its
`gpu2.o` member is byte-identical to `orig/lib/gpu2.o`.  So the patch
does not touch this object.  The pristine 3-byte bug lives in
src/libgpu2.c (documented there and in doc/notes/libgpu2.md).  No
harness binary calls GS_SaveImage (grep over tools/*.c: probe, swz,
fmt, regprobe, gsreplay use GS_InitSim/OpenSim/PutPort/CloseSim only),
so hybrid builds that link the source-built libgpu2.o get pristine
behaviour and nothing regresses.

## Header shape (what pins the include order)

`.rodata` reads, in order: bitblt.h's two unreferenced strings, txm.h's
SearchQlevel assert triple, xif.h's Frame2d assert set (with
`"../gpu2u/xif.h"`), pcrtc.h's message+assert set, gpu2.c's own
`"invalid argument xdisp"`, then the 14 local vtables in reverse
declaration order (PCRTCdmy, PCRTC, MemRead16/24/32, MemRead,
PixelBlend1a, PixelBlendAlp, PixelBlend, XWindowDump, Xifbase, PPOut,
DDATXM, PPDDA).  That fixes the TU as:

    param.h, div.h,
    PPDDA (local),
    txm.h  (→ memif.h → memory.h → bitblt.h → addrcalc.h; clut.h; txm_div.h),
    PCalc/Pre3/Pre1/DDA local views, PPOut (local),
    pcrtc.h (→ Xlib.h, memory.h(dup), xif.h),
    PP, gpu2.h, inline MemIF::MemIF, the six definitions.

The symbol table (names, bindings, types *and* order — locals,
including the vtables' first-appearance order, and the U set) comes out
identical with this arrangement.

## Verification

* `test/run_gpu2.sh` (exit 0): 3,407,958 checks, 0 failures.  Both
  objects renamed o_*/n_* and linked with the 1998 addrconv.o; a
  recording bump allocator plays __builtin_new/malloc/realloc, stubs
  record param/Reciproc/Pre1 ctor calls, OpenWindow, Pre1::Put,
  BitBLT::ReadPixel, fprintf/exit/__assert_fail (caught by longjmp),
  and fake g++ 2.7 vtables stand in for _vt.3DDA/_vt.8PCRTCxif etc.
  Exercised: all four ctor arms plus the assert arm (including the
  `"../gpu2u/xif.h"`/line-139 assert argument bytes), Put over
  addr −0x80..0x10f against both a real PCRTCdmy (EXTBUF/EXTDATA
  decode compared as object words) and a recording PCRTCxif vtable,
  Get, ResizeWindow, PPOut::Put, MemRead16/24/32 over a patterned 4 MB
  arena, both blends over 256×256/35 alpha combinations, and two full
  SetBackground/ClearDisplay/DrawPixel/DisplayPixel→dumpCRT→GetCRT
  frame round-trips (malloc and realloc paths, exhaustion tail).
  Object-graph comparison translates pool pointers to offsets and
  ignores only image-internal pointers (each side's own local vtables).
  Canaries: a routing flip at addr 0 and a one-pixel r_size error are
  both caught.
* Hybrid oracle (isolated farm, REPLACE=all 20): probe 0 failures;
  gsreplay end-state md5 equal to the pure-Sony baselines for r614,
  o519 and "Ridge Racer V_SLUS-20002_20260902142940".
* Pull-set: the gsreplay link pulls exactly the 20 source-built
  members; no symbol from gpu2reg.o/drawprim.o/gpu2vec.o appears in
  the binary.

## Residuals (all inherited/known classes; see gpu2-matching.md)

`__4GPU2Pciii` +96: the two PCRTCxif arms inline pcrtc.h's ctor bodies
and inherit their documented +48/+51 (the DispCirc `over` cse fold and
the mw/mh cross-jump — compiler-mod address-form cascade), plus ~12
bytes of the pop-scheduling quirk around `__8Reciproc` and the PPOut
`new` (identical signature in pcalc.o's own out-of-line ctor and in
gpu2vec.o: the 1998 compiler flushes pending arg-pops and pops
immediately at those two call sites where stock 2.7.2.3 defers; no
source spelling reproduces it — presumed part of the unreproduced
argument-presaturation mod).  The 12 differing weak inlines are
byte-identical to what the same headers emit into pcrtc.o (verified
reloc-masked, both directions), i.e. exactly pcrtc/xif's accepted
residuals.
