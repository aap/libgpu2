# Byte-matching status per object

Compiler: `tools/gcc272/g++272 -O` (Debian hamm g++272 2.7.2.3-4.8, era
GAS 2.9.1).  See doc/compiler.md — the 1998 objects were built by a
**modified** gcc 2.7.2.3 (systematic vfn-ref codegen difference vs stock,
0/186 folded), so residual diffs at the RTL-expand/regalloc level are
expected even for perfect source.

| object | .text | .data | relocs | verification |
|---|---|---|---|---|
| addrconv | **100% byte-identical** (+.rodata) | identical | identical | differential 80.5M calls 0 mismatch; oracle replay bit-identical; probe suite 0 failures |
| libgpu2 | 7/9 functions exact (GS_OpenSim off only by a callee-size displacement); GS_SaveImage + initPCRTC differ in DImode register/spill allocation only (doc/notes/libgpu2.md) | identical (+.rodata/.bss/.comment/.note) | identical (195 records) | differential 39710 checks 0 failures; oracle replay bit-identical (REPLACE="addrconv libgpu2") |
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

Fourteen of 23 objects replaced (addrconv libgpu2 pre1 pre3 slong div
txm_div texfunc param pcalc dbg clut bitblt xif); the hybrid archive
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
