# Byte-matching status per object

Compiler: `tools/gcc272/g++272 -O` (Debian hamm g++272 2.7.2.3-4.8, era
GAS 2.9.1).  See doc/compiler.md — the 1998 objects were built by a
**modified** gcc 2.7.2.3 (systematic vfn-ref codegen difference vs stock,
0/186 folded), so residual diffs at the RTL-expand/regalloc level are
expected even for perfect source.

| object | .text | .data | relocs | verification |
|---|---|---|---|---|
| addrconv | **100% byte-identical** (+.rodata) | identical | identical | differential 80.5M calls 0 mismatch; oracle replay bit-identical; probe suite 0 failures |
| libgpu2 | **8/9 functions exact** (GS_OpenSim off only by a callee-size displacement, now caused by GS_SaveImage's -16); GS_SaveImage differs in DImode register/spill allocation only (doc/notes/libgpu2.md).  initPCRTC became byte-identical when include/gpu2.h declared `int GPU2::Put(int, long long)` — a callee's declared return type is byte-visible in the caller's allocation (gpu2 lessons below) | identical (+.rodata/.bss/.comment/.note) | identical (195 records) | differential 39710 checks 0 failures; oracle replay bit-identical (REPLACE="addrconv libgpu2") |
| pre1 | 2825/2889 bytes; 3/6 fn byte-identical, 2 more identical bar displacement/jump-table addresses; Put 25 insns short | n/a | identical | joint pre1+pre3 differential: 2M register writes, 711661 PCalc-boundary state snapshots, 0 mismatches; oracle bit-identical incl. a full RRV game dump (301k vertex kicks) |
| pre3 | 2528/2688 bytes; 5/10 fn byte-identical, 3 more instruction-identical; Triangle 39 insns short | n/a (+.rodata/_vt.4Pre3 identical) | identical | same joint differential + oracle |
| slong | **whole .o cmp-identical** (1227/1227) | n/a | identical | 6.4M calls 0 mismatch |
| div | 2879/2889; reciproc 100%, 10 B in ctor (stack-temp reuse) | .rodata identical | identical | tables 512/512 identical; 13.0M calls 0 mismatch |
| txm_div | 747/951; 4/5 fn 100%, mktable 204 B (x87 eval order) | .data/.rodata identical | same set | generated tables 256/256 bit-identical; 4.3M calls 0 mismatch |
| texfunc | instruction shape reproduced, regalloc differs (762 vs 826) | .rodata identical | same set | 3.7M calls 0 mismatch |
| param | 4029/5571; **12/14 fn exact** | n/a | same set | 4.2M calls 0 mismatch (caught ShiftARGBSlope's unconditional extra >>2) |
| pcalc | 40383 B in 36 fn; **11 fn byte-identical** (incl. the 2708-byte SortVertex), 1 more instruction-identical, 21 of the rest within 20 insns | n/a | `_vt.5PCalc` + reloc identical; **symbol table identical** | differential 20.4M PCalc::Put / 17.8M downstream Puts, 0 mismatches; 10-object hybrid replays r614, o519 and 11 RRV game dumps bit-identically; probe 0 failures |
| dbg | **whole .o cmp-identical** (506/506 + .rodata/.ctors/.bss) | identical | identical | no harness needed (5 fn, all trivial); hybrid oracle bit-identical |
| clut | **Lookup byte-identical**; load2 and LoadData same size and instruction-identical bar one register-pair swap each; load1 0x40 short (compiler-mod address form + the spill it forces) | n/a (+.rodata identical) | set+order identical, **symbol table identical** | differential 3.11M calls 0 mismatch (load1/load2/LoadData/Lookup, whole 0x498 object + overrun slack compared); hybrid oracle bit-identical |
| bitblt | 2365/3580 B; **WritePixel (1057 B) and ReadPixel (1308 B) instruction-identical**; read/write/DoBitBLT shape-exact, 6/7/47 B of compiler-mod address forms | n/a (+.rodata identical) | set+order identical, **symbol table identical** | differential 1.82M calls 0 mismatch (own 16 MB VRAM each side, object compared every call); hybrid oracle bit-identical |
| xif | **13/28 fn byte-identical** (783 B), 16/28 instruction-identical (926 B); the rest shape-exact (regalloc + address forms) | **identical** (both dither matrices) | set+order identical, **symbol table identical**; .rodata identical but for one 0x90-vs-0x00 pad byte | no harness possible (X11); 13-object hybrid links against the original pcrtc.o and replays r614, o519 and the RRV dumps bit-identically, probe 0 failures |
| memory | 5690/5562 B; shape reproduced everywhere, no function byte-exact; `ReadStamp` (both classes) same size | n/a (+.rodata identical) | **set + order identical (144)**, **symbol table identical** | differential 4.80M calls 0 mismatch (own 16 MB Memory each side, config block compared every call, VRAM every 512 + per phase); hybrid oracle bit-identical |
| memif | **9/19 fn byte-identical** (1237 of 7357 B); 2 more (DATest, SetTEST) same size and same instruction stream bar register allocation; 7405/7357 B | n/a (+.rodata identical but for one `89 f6`-vs-`00 00` pad) | **set + order identical (87)**; symbol table identical bar `_vt.5MemIF`'s position | differential 5.30M calls 0 mismatch (memory+memif linked per side); hybrid oracle bit-identical |
| dda | **21/28 fn byte-identical** (2478 of 7320 fn bytes), 22/28 instruction-identical; of the 7 that differ, `Primitive` differs only in two call displacements and the other 6 are shape-exact (Stamping 2928/2944, InitWalk 1198/1189, ZaddSpan and sign_extent(long long) same size, ExtCslope/CaddSpan -2 each) | n/a (+.rodata `_vt.3DDA`, .note, .comment identical) | set+order identical (offsets shift with the size deltas); **symbol table identical bar 4 sizes** | differential 100000 primitives / 1.62M TXM pushes, 0 mismatches (whole 0x254 object compared on every downstream TXM call and after every Put; a 1-bit change to one sign-extend width is caught in 12 iterations); hybrid oracle bit-identical: probe 0 failures, r614, o519 and three RRV game dumps |
| pcrtc | **10/43 fn byte-identical**, 18988/19309 B; every function shape-exact; `oldDispPixelMag` (1418 B), `SetDISPLAY1/2` (444 B), `SetSMODE1`, `DisplayPcrtc` same size; the plain ctor +48 (one cse fold) and the dump ctor +51 (was +67 before the xif.h fixes landed with gpu2), the display loops -24..-153 (compiler-mod address forms + the loop-invariant hoisting they enable) | n/a | **set identical**, **symbol table identical** (names, bindings, types and order), `.rodata` reloc order identical, `.note`/`.comment` identical; `.rodata` identical bar two `8d 76` pad bytes (xif.h's `__FILE__` is parameterised as `XIF_FILE`: pcrtc.h defines `"../gpu2u/xif.h"`, xif.c's spelling `"xif.h"` is the default) | differential 2.00M register writes / 140625 displays / 42.1M Xifbase callbacks / 2.18M object+VRAM checks, 0 mismatches (fake 9-entry Xifbase, own 16 MB Memory each side; five 1-bit canaries caught in 9-895 iterations); 18-object hybrid replays r614, o519 and 7+ RRV dumps bit-identically, probe 0 failures, `gsreplay -w` runs clean |
| gpu2 | **12/25 fn byte-identical** (all six own functions but the ctor — dumpCRT, GetCRT, Get, Put, ResizeWindow — plus 7 weak inlines); `__4GPU2Pciii` +96 = the two inlined PCRTCxif-ctor arms' documented +48/+51 plus ~12 B of the Reciproc/PPOut pop quirk (lessons below); the 12 differing weak inlines are byte-identical (reloc-masked, both directions) to what the same headers emit into pcrtc.o — pcrtc/xif's accepted residuals, inherited, no residual class of gpu2's own | n/a (+`.data`/`.bss`/`.note`/`.comment` identical) | set + order identical (133 `.rel.text` + 33 `.rel.rodata`); `.rodata` identical bar GAS's two `8d 76` pad bytes; **symbol table identical** (names, bindings, types, order; 13 sizes track the fn sizes) | differential 3.41M checks 0 failures (all four ctor disp arms + the assert arm, Put sweep vs real PCRTCdmy and fake PCRTCxif vtables, MemRead/blend sweeps, two DrawPixel→dumpCRT→GetCRT frame round-trips; routing and r_size canaries caught); 20-object hybrid: probe 0 failures, r614/o519/RRV end-state md5s equal pure-Sony; the gsreplay link pulls only decompiled members (`--print-map` + nm: no gpu2reg/drawprim/gpu2vec symbols) |
| txm | **18/41 fn byte-identical**, 24570/25485 B (96.5%); every function shape-exact; `Texturing` (448 B), `ExtCov` (426), `AA1`, `SetCLAMP`, `SetTEX2`, `SetTEXCLUT`, `ClampQ` and `LMNFilter` same size with the same instruction stream (register allocation only), `NFilter` same instruction count; the rest is `Put` -528 and `GetOneTexel` -160, i.e. the compiler-mod address forms and the register pressure they cause | identical (the 0x100 `TXM::valid8`), and `.ctors`/`.note`/`.comment` with it; `.rodata` identical but for one `89 f6`-vs-`00 00` pad pair | **set + order identical (205)**, **symbol table identical** | differential 130.1M calls / 129.8M comparisons / 365650 fatal arms, 0 mismatches (own 96 MB mapped VRAM and fake MemIF each side; the whole 0x72c object *and* the clut[] overrun slack behind it compared after every call, plus every PixelStamp field TXM writes; six 1-token mutations all caught); 18-object hybrid: probe 0 failures, r614, o519 and four RRV dumps (1.4-18 MB) bit-identical to a pure-Sony build |

Twenty of 23 objects replaced (addrconv libgpu2 pre1 pre3 slong div
txm_div texfunc param pcalc dbg clut bitblt xif memory memif dda
pcrtc txm gpu2 — every member the gsreplay link pulls); the hybrid archive
replays r614, o519 and the RRV game dumps bit-identically (probe 0
failures).  New reusable lesson from txm_div: the era libc5 <math.h>
declared math functions __attribute__((__const__)) and gcc 2.7 pops a
const call's args immediately — declare rint/fabs (and abs!) locally with
era attributes, never include modern headers.

New lessons from dbg/clut/bitblt/xif (details in their doc/notes files):

* **g++ 2.7 builds RTL for an inline member as soon as it parses it.**  Its
  string constants land in `.rodata` and its library-call externals get a
  `.globl` in *every* including TU, even when the function is never called.
  That is where clut.o/bitblt.o/memif.o/memory.o/txm.o/pcrtc.o/gpu2.o's
  unreferenced "BITBLTBUF: Depth is different" / "HWREG:Now not Host to
  Local mode" pair and their unreferenced `memcpy` come from, and where
  xif.o's whole set of Frame2d assert strings comes from.  Reproducing an
  object's `.rodata` can therefore require writing *header* code that emits
  no instructions at all.
* **A merely declared class emits no vtable; one with an inline constructor
  emits a local one in every TU** (plus weak copies of its inline virtuals).
  That is why dbg.o carries `_vt.5PPOut`/`_vt.5PPDDA`.
* **AddrConv has state.**  `addr, page, blk, bnk, pos, wd, np, bitpos` at
  0x00..0x1c are AddrConv's own members, inherited by TexClut, BitBLT,
  FBConfig and ZBConfig, together with an inline `Address()` that calls
  `address_convert` and recomposes the word address — and divides `bw` and
  `tbp` by 64 *itself*.  include/addrcalc.h carries that declaration;
  include/addrconv.h's stateless one is the older, narrower view.  g++ 2.7
  has no empty-base optimisation, so getting this wrong shifts every derived
  member by 4.
* **Two unrotated loop spellings, not one.**  clut.o wants
  `for(;;) { if (i == N) break; ... i++; }`; bitblt.o wants
  `for (i = 0; i != N; i++, data >>= sh, x++, count--)` with a comma
  increment clause — that is what puts the deferred argument pop *before*
  the increments, and WritePixel/ReadPixel do not match until it is written
  that way.
* **Taking a parameter's address keeps it in its incoming stack slot.**  The
  1998 code reaches read-modify-write on `0x14(%ebp)` by passing `&r` to an
  inline helper; a macro doing the same arithmetic promotes the parameter to
  a register and loses ~50 bytes per function.
* **`__assert_fail` must be declared `__attribute__((__noreturn__))`**, and
  `__LINE__`/`__FILE__` in a header are byte-visible: xif.h's line numbers
  are load-bearing (139/146/158).
* **The 1998 assembler filled `.rodata` alignment with code nops** (0x90,
  `89 f6`); era GAS 2.9.1 fills with zeroes.  One byte of xif.o.

pre1/pre3 residuals look like the SAME compiler mod as the vfn-ref
anomaly (a "force address through a value" change in expand): the
originals show `lea 0(,idx,4)` + `disp(tmp,base,1)` forms (7 sites in
pre3.o, 0 reproducible) and matching extra register pressure.  Also
learned: era `<stdlib.h>` abs() must NOT be used (attribute changes arg
popping — declare it locally), and the originals' loops are
systematically unrotated (`for(;;)` + block-scoped decl + `if break`
reproduces it; expect in pcalc/memif/txm).

libgpu2.o build settings differ from the rest of the archive and are
forced by bytes: RH 4.2 gcc **2.7.2.1** (`GCC272_ALT=rh42-2721`),
**`-O2 -m386`** — vs 2.7.2.3 `-O` (i486-configured) for the other 22.
Empirically 2.7.2.1 vs 2.7.2.3 emit identical code at those flags, so
the split only matters for `.comment` fidelity.

## addrconv: how the last 51 bytes fell (a case study)

The compiler-archaeology agent proved every stock gcc 2.7.2.x emits
identical code for given source (doc/compiler.md §7), so the residuals
had to be source shape.  Both were:

1. **Z-arm of blk**: the original ran through a *narrow-typed* temp —
   `t = (unsigned)addr>>6 ^ 1; blk = t & 1;` with `t` any of
   char/uchar/short/ushort (mode difference blocks copy-propagation,
   producing `mov %eax,%edi; and $1,%edi`; an int/unsigned temp gets
   coalesced away).  We chose `unsigned char`; the narrow type is pinned,
   which of the four is not.
2. **wd lookup**: not a 2D subscript at all — a byte-offset index
   variable and a cast deref:
   `i = ((x << 1) & 0xc) + ((y << 3) & 0x10); wd = *(int *)((char *)table + i);`
   The `<<` spelling is pinned (`x + x` compiles differently); the
   x-term-first order and the explicit index variable are pinned.
3. The esi/edi ripples were pure cascade — they vanished when 1-2 were
   fixed.

Also pinned by bytes: the per-case page temp (`p = (...) & 0x1ff;
addr = p << 7`), term order in every case, `-O` (not -O2), the Z
range-check shape, jump tables at identical offsets.  Lesson for the
other objects: when a residual survives all flag hunting, suspect a
source temp with a different type or a differently-spelled equivalent
expression — evaluation order and pseudo-register structure follow the
source exactly at -O.

## pcalc: three more reusable lessons

1. **gcc 2.7 loads the *first* operand of a memory-to-memory comparison
   into a register**: `mov A,%eax; cmp %eax,B` came from `A ? B`, never
   from `B ? A`.  Reading the operand order off the object recovers which
   way round every `if` was written, which is what made `SortLine`,
   `GetSPoint`, `DrawSprite` and pcalc's four `Correct*` functions fall
   into place.  (Corollary already known from addrconv: at `-O` the
   pseudo-register structure follows the source exactly.)
2. **A class-typed ternary shares one return temporary between its arms.**
   `s = cond ? s >> a : s << b;` (slong) produces one `operator=` after
   the join; the `if`/`else` spelling gives each arm its own temporary and
   an extra 28-byte `rep movsl`.  Twelve of those in pcalc's `Slope`.
3. **Deferred header inlines are emitted in reverse declaration order.**
   pcalc.o ends with the constructor, `Floor`, `Ceil`, `Subpixel`; the
   class declares them `Subpixel`, `Ceil`, `Floor`, constructor.  Getting
   this right made the whole symbol table — names, bindings *and* order —
   identical, and turned the last function in `.text` byte-identical as a
   side effect (it stops being the one that absorbs the section padding).

And one more compiler-mod candidate, from pcalc's `reciproc` call sites:
the 1998 object **precomputes every argument of a call into a pseudo
before pushing any of them** (spilling when it runs out of registers),
where stock 2.7.2.x computes each argument immediately before its own
push.  No flag combination reproduces it.  `AASlope` (129 B) and
`C_Hosei` (76 B) are the cheapest places to test a patch for it; if it
lands, most of `Slope`'s and `LineSlope`'s residual should go with it.

## dda: five more reusable lessons

The rasterizer fell to source *shape* almost everywhere; the residuals
are register allocation, not structure.  What pinned it:

1. **`abs()` is `__attribute__((__const__))` in dda.c** — the object
   pops each `abs` argument immediately instead of deferring it.  That
   is the era libc5 `<stdlib.h>` spelling, and it is the *opposite* of
   pre3.o/pcalc.o, which need a plain `extern "C" int abs(int);`.  The
   attribute is a per-object decision; read it off the `add $0x4,%esp`
   placement, not off the archive.
2. **A COND_EXPR flushes the pending argument pop, and that decides
   what CSE can still see.**  `x = a < 0 ? 1 : 0;` and `x = a < 0;`
   emit the same arithmetic (`shr $31`), but the ternary makes
   `expand_expr` call `do_pending_stack_adjust()` first, so the
   `add $N,%esp` lands *before* the statement.  In dda.o that ordering
   is load-bearing — with the pop in the wrong place the register
   allocation changes and 80 bytes of cross-jumping reshape.  When a
   deferred pop sits somewhere you cannot explain, look for a `?:`.
3. **`x & ~0x3f` compiles to `andb $0xc0,%dl`** — a byte AND that
   leaves the top 24 bits alone means the constant was `0xffffffc0`,
   never `0xc0`.
4. **`a = 4; if (c) a = 8;` and `if (c) a = 8; else a = 4;` are not the
   same object code**: both emit two stores, but the if/else form loads
   the condition's operand *before* the first store.  dda.c uses one
   spelling in `InitWalk` and the other in `Stamping`.
5. **For `long long`, `x << 1` and `x * 2` differ**: `<<1` gives
   `shld/shl`, `*2` and `x + x` give `add/adc`.  `InitWalk` uses both
   in adjacent statements and the bytes say which is which.

One more, from `sign_extent(long long, int)`: **a block-scoped
`long long m` for the mask in *one* arm of the `if` is the difference
between 324 bytes and the original's 308** — the then-arm reads
`{ long long m = ((long long)1 << n) - 1; r = v & m; }` while the
else-arm keeps its mask inline.  Making both arms symmetric (either
way) misses by 16 or by 50.  Getting that one right also turned the
five `Ext*` wrappers byte-identical, because their only remaining
difference was the call displacement into it.

Unsolved in dda.o (4 B x 3 sites): the original's
`lea 0x1(%esi),%edx; sub %edx,%ecx` for `sw - (i+1)`.  gcc's `fold`
reassociates that to `(sw-1) - i` (a `dec`) for every spelling we
tried, and an explicit `k = i + 1` local is computed once, not three
times.  Also unexplained: `ExtCslope`/`CaddSpan`'s `mov %ebx,%edx`
before the conditional `inc` (2 B each), and 16 B of DImode temp
allocation in `sign_extent(long long, int)`.


## memory/memif: the loop-rotation half-lesson, and four more

`memory.o` (the 4 MB of local memory, the FB/ZB configs and the whole
BITBLTBUF/TRXPOS/TRXREG/TRXDIR/HWREG transfer machine) and `memif.o` (the
per-pixel back end: alpha test, destination alpha test, depth test, alpha
blend, dither, colour clamp) are doc/notes/memory.md and
doc/notes/memif.md.  With the two of them in the REPLACE list the hybrid
still replays r614, o519 and the Ridge Racer V dump bit-identically with
probe reporting 0 failures, and each of the two new objects does so alone
as well.

* **A block-scoped declaration inside a loop body is what stops g++ 2.7
  rotating the loop.**  `expand_end_loop` (stmt.c) scans forward from the
  loop's start label for the last conditional branch to the loop's *end*
  label and rolls everything up to it to the bottom - unless the scan
  first hits a `CALL_INSN`, a `CODE_LABEL`, or a `NOTE_INSN_BLOCK_BEG`,
  which g++ emits for any compound statement that declares a variable.  So

  ```c
  for (;;) { Pixel *p; if (i == 8) break; ...; i++; }
  ```

  keeps the test at the top with a `jmp` back at the bottom, while the
  identical loop without the declaration comes out bottom-tested (the
  entry test folds away because `i` is provably 0).  This is the missing
  half of the "unrotated loop" lesson above: clut.o's loops have no calls
  and no declarations and did not need it, but every loop in memory.o and
  memif.o does.  (`goto` out of the loop instead of `break` has the same
  effect for the other reason - the branch does not target `end_label`.)
* **An 8-byte struct is copied with a *ternary*, not `if`/`else`.**
  `compute_record_mode` gives a two-int record DImode, so `a = b` expands
  to load/store/load/store, but `a = c ? b0 : b1` loads both words in each
  arm and shares one store pair.  memif.o's `Context()` does exactly that
  for its `DAlphaTest` and `DepthTest` copies while using `if`/`else` for
  the 0x10- and 0x18-byte ones; the single line turned three functions
  byte-identical.  The same tell identifies `PixelStamp`'s x/y pair as a
  nested 8-byte record rather than two `int`s - the callers copy it with
  two loads and then two stores.
* **`switch` on a `long long` silently calls `__cmpdi2`** once per case
  label.  `switch ((data >> 24) & 0xf)` cost a kilobyte in
  `Memory::SetRegister` before the value went through an `int`.
* **A COND_EXPR assigned straight into a struct member stores into the
  member in each arm; assigned to a scalar local it goes through a
  register.**  `c.A = cond ? 0x80 : 0;` gives two `movl $imm,mem`;
  `a = cond ? 0x80 : 0; c.A = a;` gives `xor %eax,%eax / test / mov
  $0x80,%eax / mov %eax,mem`, which is what the 1998 objects have (gcc's
  `safe_from_p` decides whether the target may double as the intermediate).
  Same class: `x = call()` puts the struct return straight into `x`, while
  `s.pix[i].c = c = ReadPixel(...)` makes `preexpand_calls` allocate a
  `keep`-flagged temp per call site and copy temp -> c -> destination.
* **A pointer local reproduces the compiler-mod address form - sometimes.**
  The unfixed mod forces addresses through values, so each reference has
  one use and `combine` folds the scale into a single
  `lea disp(base,idx,N)` with 4-bit displacements on the members; stock
  2.7.2.3 CSEs the *index*, emits `shl` and addresses every field with a
  two-register `disp(idx,base,1)`.  That costs ~25 bytes a site and frees
  a register, which is why the originals reload `this`/`mem`/`s` from
  their incoming stack slots where ours keep them live.  Where the
  original holds one address across several accesses (`MemIF::SetALPHA`,
  `MemIF::SetTEST`) an explicit pointer local reproduces it exactly; where
  it recomputes per basic block (all of memory.o's stamp loops) nothing
  does.
* g++ 2.7 does **not** inline a non-inline function inside its own
  translation unit (verified), and emits an out-of-line copy of an inline
  as *weak* when it is referenced but *global* when it is merely emitted -
  so memif.o's global `Set*`/`Context` members prove the 1998 source
  repeated their bodies (macros) inside `MemIF::Stamp`, and the weak
  `PixelStamp::AAMask` proves it was declared in the class and defined
  `inline` at the end of the .c file, after every use.

## pcrtc: five more reusable lessons

`pcrtc.o` (PMODE/SMODE/SYNC/DISPFB/DISPLAY/EXTBUF/EXTDATA/EXTWRITE/BGCOLOR,
the two-circuit display merge and the X11 hand-off) is
doc/notes/pcrtc.md.  With it in the REPLACE list the hybrid still replays
r614, o519 and the Ridge Racer V dumps bit-identically with probe reporting
0 failures, and `gsreplay -w` drives the model's real X window clean.

* **An inlined call evaluates all its arguments into pseudos before the
  body runs - and that is visible.**  Three assignments `c.R = r; c.G = g;
  c.B = b;` emit load/store three times; an inline
  `SetRGB(PixColor &c, int r, int g, int b)` emits *three loads and then
  three stores*, with the loads coming out in the order g, b, r.  Two
  functions in pcrtc.o have exactly that shape and are byte-exact only with
  the helper.  The same mechanism is what lets `combine` turn
  `(d >> 8) & 0xff` into a `movzbl 1(mem)` byte load: keeping the three
  extractions in separate argument pseudos stops cse merging the loads
  first.  (`MemRead32::ReadPixel` and `SetBGCOLOR`.)
* **Passing a bitfield through an inline function stops `fold` folding the
  shift back into the mask.**  `((d >> 5) & 0x1f) << 3` compiles to
  `(d >> 2) & 0xf8`; `ext5((d >> 5) & 0x1f)` with
  `inline int ext5(unsigned v) { return (v << 3) | (v >> 2); }` keeps
  `and $0x1f` and `lea 0(,%eax,8)` apart, which is what the object has.
* **A predicate that is materialised into a register and *then* tested came
  from a function call, not from an `if`.**  `if (a || b)` branches
  directly; `if (F())` with `F` inline goes through `preexpand_calls`,
  which expands the call into a pseudo first, so the object shows
  `xor %eax,%eax / ... / mov $1,%eax / test %eax,%eax / jne`.  The same
  `preexpand_calls` makes an inline call inside an argument expression
  evaluate *before* the other operand - and it is the only way to get one
  subtraction out of `dy - VStart()`, because `fold` distributes the
  subtraction into both arms of a COND_EXPR operand
  (`dy - (c ? v/2 : v)` gives two subtractions, and the objects have one).
* **`INTEGRATE_THRESHOLD` (`8 * (8 + nargs)`) decides function vs macro.**
  A ~70-insn member with only `this` is *not* inlined and leaves a symbol
  the original does not have, so it has to be a macro; a ~110-insn member
  with two arguments *is* inlined and stays a function.  Both cases occur
  in pcrtc.h and the object tells them apart.
* **A store through a pointer kills cse's constant propagation through
  memory** even between provably distinct offsets off one base: an
  `rd[0] = &r32` between the constant initialisation of an array and the
  comparison chain that reads it is what leaves 48 bytes of dead
  comparison in both constructors.

And one more piece of the vtable-linkage rule from memif: **the class with
a non-inline virtual (the key method) gets its vtable *and every one of its
inline members* emitted globally in that one TU** - which is why pcrtc.o
carries `_vt.8PCRTCxif` plus fifteen global `PCRTCxif::Set*` copies while
gpu2.o, which constructs a PCRTCxif, carries none of them; classes with no
key method (PCRTC, PCRTCdmy, MemRead*, PixelBlend*, XWindowDump) get local
vtables and weak inlines in every TU instead.  Deferred inline emission is
in reverse declaration order *of the classes as well as of their members*,
and the vtables in `.rodata` follow the same reverse order - between them
they pin the whole header's shape.

## txm: six more reusable lessons

`txm.o` (the texture machine: the DDA's stamp expanded into a
PixelStamp, LOD, the six filter modes, every PSM's texel fetch, the
CLAMP modes, MTBA, antialias coverage and fog) is doc/notes/txm.md.
With it in the REPLACE list the hybrid still replays r614, o519 and four
Ridge Racer V dumps bit-identically with probe reporting 0 failures.

* **A declaration that runs a constructor flushes the pending argument
  pop.**  `TXM::Texturing`'s two `NormTexCoord::InitTable()` calls each
  have their argument popped immediately (`add $0x4,%esp`) rather than
  deferred, and no spelling of two adjacent statements reproduces that -
  but writing them as `NormTexCoord u, v;` with an inline
  `NormTexCoord() { InitTable(); }` does, exactly.  When a call's pop is
  flushed for no visible reason, look for a constructor.
* **`return` out of a loop is not `break`.**  g++ 2.7 rotates
  `for (i = 0; ; i++) { if (i > N) break; ... }` (test at the bottom);
  the same loop with `return` instead of `break` is *not* rotated,
  because the branch does not target the loop's end label.  That single
  keyword is the difference between 148 bytes and 151 for
  `_GLOBAL_.I._3TXM.valid8`, and it is the third member of the
  unrotated-loop family after "block-scoped declaration" and "goto".
* **`(x >> n) & m` on a `long long` narrows to SImode, `* 64` does
  not.**  `((data >> 14) & 0x3f) * 64` keeps the value in DImode - the
  high word's `xor %ecx,%ecx` survives before every `shl` - while
  `((data >> 14) & 0x3f) << 6` narrows and matches.  `convert_to_integer`
  will pass a truncation down through `LSHIFT_EXPR` but not through
  `MULT_EXPR`.  Thirty-odd sites in txm.o turn on that one character.
* **`(A && B) & C` materialises the conditions; `A && B && C`
  branches.**  `fold` only rewrites `TRUTH_ANDIF` into `TRUTH_AND` when
  the right operand satisfies `simple_operand_p` - a bare DECL, never a
  comparison - so three `&&`s always come out as short-circuit jumps.
  The 1998 objects' `seta`/`setne`/`test` triples (28 pairs in txm.o's
  colour and Z saturation alone) need a *bitwise* `&` between the last
  two terms.
* **`if (c) return X; ... return Y;` and `if (!c) { ...; return Y; }
  return X;` lay the arms out differently.**  The first puts the early
  return inline and inverts the branch; the second leaves the main path
  falling through and jumps forward to the early return, which is what
  `TXM::ClampQ` and `TXM::ClampLod` have (and, in the same family,
  `if (e != 2) { ... } else r = 0x80;` for `ExtCov`'s coverage decode).
* **A helper with many arguments must be a member, not a free inline or
  a macro.**  txm.o's CLAMP wrap reads WMS/MINU/MAXU *inside the arm
  that needs them* - so the arguments cannot be evaluated up front, which
  rules out a free inline - *and* pins the coordinate to a stack slot -
  which rules out a macro.  `TexAttr::WrapU(int &c, int w, int lod)` does
  both, and the same shape (members reached through `this`, one `int &`)
  is worth trying whenever those two tells appear together.

And two smaller ones: a pseudo that is **set exactly once** has known
`nonzero_bits`, so `t >> 2` on it comes out as `shr` while the same
expression through a reused variable is `sar` (three separate locals in
txm.o's 16-bit texel unpack, not one reused `t`); and a `switch` whose
index is written out as a member reference compares the memory operand
directly (`cmpl $0x3a,0x68(%ecx)`), while `int psm = attr.PSM; switch
(psm)` puts it in a register for the compare and lets reload
rematerialise it for the table jump - which is what both of txm.o's
switches do.

txm.o also confirms the vtable-linkage rule from memif/pcrtc from the
other side: **all twenty** of `TXM`'s inline members are emitted
out-of-line and *global* at the end of `.text`, after
`_GLOBAL_.I.*`, in reverse declaration order - while an inline member of
a class with no vtable in the TU (`Bits`, `Unpack16`, `TexCoordN`,
`TexClutCtx::Context`) is not emitted at all.  Their order in the class
body is therefore byte-visible, and getting it right is what makes the
symbol table identical.

New lessons from gpu2 (details in doc/notes/gpu2.md):

* **A declared return type is byte-visible at every call site.**
  `GPU2::Put` returns int (the object sets %eax = 1); declaring it so in
  include/gpu2.h makes each call insn clobber a value register, and that
  alone flips libgpu2.o's `initPCRTC` DImode allocation into the 1998
  layout — **initPCRTC is now byte-identical** (was +79, the documented
  allocation residual).  When a caller's register allocation won't
  settle, check the *callees' declared return types* against `%eax`
  liveness in the caller.
* **A local pointer whose last use may clobber it.**  `Get` needs
  `Memory *m = mem; return m->bitblt.ReadPixel(m);` — one pseudo, so
  the address arithmetic destroys it (`push; add $0x400144; push`);
  `mem->bitblt.ReadPixel(mem)` keeps the member load alive in a second
  register.  Same family as &param pinning, opposite direction.
* **`w*4*h`, not `h*4*w`.**  gcc 2.7 regroups a constant-in-the-middle
  product: the spelling `w*4*h` emits `lea 0(,h,4); imul w`.  Read the
  lea's index register to recover which operand was written first —
  it is the *other* one.
* **`if (c) return X; return 0;` vs the moved-block shape** (third data
  point after txm's): `if (r_count < r_size) return r_buf[r_count++];
  return 0;` keeps the fall-through `return 0` inline with `ja` to the
  tail block — and the branch target label kills cse, which is why the
  original loads r_count twice.  The `>=`-first spelling gets rotated by
  the jump optimizer into a shape with one shared load.
* **The three xif.h fixes predicted by pcrtc.md are applied and proven**:
  the `XWindowDump` ctor takes the callback (`XWindowDump(void (*f)(int,
  int, const unsigned int *) = 0) { func = f; bg = 0; }`), xif.h's assert
  `__FILE__` is the *spelling of the 1998 #include* (parameterised as
  `XIF_FILE`; pcrtc.o and gpu2.o say `"../gpu2u/xif.h"`), and
  `Frame2d::Frame2d` mallocs from its parameters.  xif.o is bit-identical
  before/after; pcrtc.o's dump ctor +67 → +51 and its `.rodata` is
  identical-bar-pad.  UPDATEMERGE stays if/else: the ternary-minus
  spelling matches the ctors' shared subtraction but breaks
  SetDISPLAY1/2's 444-byte size match (the ctor sharing is cross-jumping,
  not source shape).
* **The Reciproc pop signature is context-independent and still
  unexplained**: around `__8Reciproc` (and only there among the seven
  member-ctor calls), and around PPOut's `new(8)`, the 1998 compiler
  flushes pending arg-pops before the call and pops immediately after
  (`add $0xc; push; call; add $0x4`), in pcalc.o's out-of-line ctor,
  in gpu2.o's inlined copy, and in gpu2vec.o alike.  Member dtors,
  explicit mem-init lists, and dtors on the newed class were all tried
  and change nothing; stock 2.7.2.3 defers.  Presumed a facet of the
  unreproduced argument-presaturation mod (the same sites also
  pre-evaluate the inline ctor's pointer argument before the
  allocation call, which `{ DDA *d = dda; ... }` reproduces but the
  pops do not follow).
* **gpu2.c declares its own pipeline views** (dbg.c pattern) because
  the per-object headers carry conflicting stand-ins; the ctor bodies
  are copied verbatim from pcalc.h/pre3.h/dda.h and inline to the same
  bytes.  MemIF's ctor had to be a header inline in 1998 (gpu2.o
  inlines it); we repeat the one-line body in gpu2.c as
  `inline MemIF::MemIF` rather than disturb memif.h/memif.c.
