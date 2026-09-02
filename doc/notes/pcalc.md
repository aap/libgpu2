# pcalc.o — PCalc, primitive setup

`src/pcalc.c` + `include/pcalc.h`, built with
`GCC272_1998=1 tools/gcc272/g++272 -O -Iinclude`.

40 383 bytes of `.text` in 36 functions — the largest object in the
archive, and the mathematical heart of the model: everything the DDA
needs to walk a primitive is computed here.

## Status

| function | orig bytes | new bytes | orig/new insns | status |
|---|---|---|---|---|
| `SwapLine__5PCalcRiN31` | 59 | 59 | 30 / 30 | **byte-identical** |
| `SwapLine__5PCalcRUiN31` | 59 | 59 | 30 / 30 | **byte-identical** |
| `SortVertex__5PCalcP4Pre3P5param` | 2708 | 2708 | 691 / 691 | **byte-identical** |
| `GetSPoint__5PCalc` | 369 | 369 | 74 / 74 | **byte-identical** |
| `CorrectSPoint__5PCalc` | 793 | 824 | 206 / 210 | 4 insns long |
| `CorrectEPoint__5PCalc` | 806 | 790 | 198 / 188 | 10 insns short |
| `Slope__5PCalcP4Pre3P5param` | 8685 | 10492 | 2212 / 2429 | 217 insns long |
| `CheckOverFlow__5PCalc` | 770 | 770 | 214 / 214 | **byte-identical** |
| `StartVal__5PCalcP4Pre3P5paramG5param` | 3238 | 3334 | 772 / 761 | 11 insns short |
| `GetDDAStart__5PCalcP4Pre3` | 1591 | 1588 | 389 / 382 | 7 insns short |
| `AASlope__5PCalcxii` | 305 | 303 | 116 / 115 | 1 insns short |
| `C_Hosei__5PCalcxi` | 189 | 189 | 76 / 78 | 2 insns long |
| `SortCoverage__5PCalcP4Pre3` | 786 | 786 | 163 / 163 | **byte-identical** |
| `AAStartVal__5PCalciiii` | 338 | 340 | 118 / 119 | 1 insns long |
| `AACoverage__5PCalcP4Pre3` | 758 | 662 | 239 / 222 | 17 insns short |
| `DrawTriangle__5PCalcP4Pre3` | 2602 | 2430 | 532 / 508 | 24 insns short |
| `SortLine__5PCalcP5param` | 324 | 324 | 78 / 78 | same count, regalloc |
| `CorrectLineStart__5PCalc` | 857 | 841 | 238 / 230 | 8 insns short |
| `CorrectLineEnd__5PCalc` | 679 | 640 | 195 / 178 | 17 insns short |
| `LineSlope__5PCalcP5paramiR5paramRxT4RiT6` | 4572 | 4925 | 1116 / 1186 | 70 insns long |
| `LineDDAEdgeStart__5PCalc` | 1287 | 1289 | 282 / 285 | 3 insns long |
| `LineAACov__5PCalcP4Pre3` | 417 | 417 | 122 / 121 | 1 insns short |
| `DrawLine__5PCalcP4Pre3` | 1777 | 1776 | 377 / 374 | 3 insns short |
| `SpriteSlope__5PCalcxiRx` | 480 | 522 | 178 / 194 | 16 insns long |
| `SpriteStartVal__5PCalcRxxxi` | 240 | 240 | 89 / 89 | **byte-identical** |
| `DrawSprite__5PCalcP4Pre3` | 1777 | 1840 | 436 / 447 | 11 insns long |
| `DrawPoint__5PCalcP4Pre3` | 1160 | 1223 | 232 / 240 | 8 insns long |
| `BBox__5PCalc` | 504 | 488 | 116 / 113 | 3 insns short |
| `ReverseDir__5PCalc` | 203 | 203 | 58 / 58 | **byte-identical** |
| `Primitive__5PCalcP4Pre3` | 1068 | 1100 | 239 / 237 | 2 insns short |
| `Register__5PCalcP4Pre3` | 420 | 420 | 97 / 97 | **byte-identical** |
| `Put__5PCalcP4Pre3` | 174 | 174 | 41 / 41 | instruction-identical (displacements only) |
| `__5PCalcP5PPDDA` | 296 | 290 | 58 / 57 | 1 insns short |
| `Floor__5PCalcRCi` | 20 | 20 | 12 / 12 | **byte-identical** |
| `Ceil__5PCalcRCi` | 46 | 47 | 23 / 22 | 1 insns short |
| `Subpixel__5PCalcRCi` | 26 | 26 | 12 / 12 | **byte-identical** |

**11 of 36 functions are byte-identical**, one more is instruction-identical
(`Put`, off only by two call displacements), and 21 of the remaining 24 are
within 20 instructions of the original.  `.rodata` (`_vt.5PCalc`, 0x10 bytes),
`.comment`, `.note` and the relocation multiset are identical, and so is the
**whole symbol table** — same names, same bindings, same order, including the
four inline-in-header functions (constructor, `Floor`, `Ceil`, `Subpixel`)
emitted last, after `Put`, which is the g++ 2.7 signature of a header inline.

Behaviour is verified two ways:

* `test/run_pcalc.sh` — 50 000 000 register writes driven through the
  *original* pre1.o + pre3.o, producing **20 357 319 `PCalc::Put` calls**
  and **17 794 162 downstream (PPOut) Puts**; the complete 0xc00-byte PCalc
  object, the mutated Pre3 and every downstream Put image are compared
  after each call: **0 mismatches**.  Each side runs against its own
  param/div/slong cluster, so the whole call graph below PCalc is under
  test, not just PCalc.
* the hybrid oracle: a 10-object replacement archive (`addrconv libgpu2
  pre1 pre3 slong div txm_div texfunc param pcalc`), built in an isolated
  farm, replays to **bit-identical final VRAM**:
  `out/r614` → `9c7c73b156f8664633055e0300990a82`,
  `out/o519` → `9fbc3187d1f98e0dff84e5d9aa5689df` (both the known-good
  9-object baselines) and 11 Ridge Racer V game dumps, each matching the
  pure-Sony build of the same dump exactly (the first is
  `82657dfd651625d235983775b7bef849`).  An earlier build of the same
  source cleared 11 more RRV dumps.  `tools/probe` reports 0 failures.

## Header / TU shape

* **No `#pragma interface` / `#pragma implementation`** (same as pre3.o):
  `_vt.5PCalc` comes out global in the TU that defines `PCalc::Put`.
* The constructor, `Floor()`, `Ceil()` and `Subpixel()` are **defined
  inline in the header** — `gpu2.o` inlines the constructor, and pcalc.o
  emits out-of-line copies of all four after `Put`, in that order.
  `Floor`/`Ceil`/`Subpixel` are also inlined at their ~40 call sites.
  **g++ 2.7 emits these deferred inlines in reverse declaration order**, so
  the class has to declare them `Subpixel`, `Ceil`, `Floor`, constructor to
  come out constructor, `Floor`, `Ceil`, `Subpixel` in `.text`.  Getting
  that right is what makes the symbol table identical, and it turns
  `Subpixel` byte-identical as a side effect (whichever function lands last
  in `.text` otherwise absorbs the section padding).
* `abs()` is declared locally as `extern "C" int abs(int);` — *without*
  the era header's `__attribute__((const))`.  Pinned by `AAStartVal`,
  which pops two `abs` arguments with one combined `add $0x8,%esp`;
  adding the attribute costs 6 instructions there (see Residuals for the
  `AACoverage` case that pulls the other way).
* `Pre3` is declared in `include/pcalc.h` rather than pulled from
  `include/pre3.h`, because that header carries a stand-in `PCalc` of its
  own and the two cannot be included together.  In PCalc's view of Pre3,
  `send_addr` is `unsigned int` (`Register` does `shr $1` on it) and the
  vertex `r/g/b/a/f` are `unsigned int` (they are zero-extended to 64 bits,
  while `s/t/q` are sign-extended).

## Member map (sizeof 0xc00, vptr at 0xbfc)

| off | name | meaning |
|---|---|---|
| 0x000 | `out` | `PPDDA*`, the next stage — note the ctor's mangled name is `__5PCalcP5PPDDA`, so the *declared* type of the tap is `PPDDA`, not `PPOut` |
| 0x004 | `sft` | subpixel bits, 4 — `Floor`/`Ceil`/`Subpixel` shift by this |
| 0x008 | `msft` | 0x16 — the AA coverage fixed point |
| 0x00c | `pix` | 0x10, one pixel in subpixel units |
| 0x010 | `one` | 1 |
| 0x014 | `dx` | `param`, d/dx of every attribute |
| 0x064 | `dy` | `param`, d/dy |
| 0x0b4 | `sv` | `param`, the start value at the DDA start position |
| 0x104 | `rcp` | `Reciproc` (div.o) |
| 0x10c | `A` | `param`, the three vertices after sorting |
| 0x15c | `B` | |
| 0x1ac | `C` | |
| 0x1fc | `spoint` | `char`, 'A' or 'C': which vertex the scan starts at |
| 0x1fd | `epointy` | `char`, which vertex supplies the end **Y** |
| 0x1fe | `epointx` | `char`, which vertex supplies the end **X** |
| 0x200 | `sx`, `sy` | start point, subpixel |
| 0x208 | `sxi`, `syi` | start point, pixels |
| 0x210 | `ex`, `ey` | end point, subpixel |
| 0x218 | `exi`, `eyi` | end point, pixels |
| 0x220 | — | never referenced |
| 0x224 | `stampw` | 8 for a flat primitive, else 4 |
| 0x228 | `m_228` | AA sample offset (1<<sft, or half that) |
| 0x22c | `m_22c` | 1<<sft when AA1/0x120 |
| 0x230 | `m_230` | AA coverage rounding constant (1<<msft, or half) |
| 0x234 | `m_234` | scanline-count seed, `B.y - ((dday<<sft)+pix)` |
| 0x238 | `sortcode` | 1..18, the triangle's orientation class |
| 0x23c | `m_23c` … 0x248 `m_248` | signs of the four split-Z slope halves |
| 0x24c | `cov[6]` | AA coverage gradients, per edge (dy,dx pairs) |
| 0x264 | `scissor[2]` | `{scax0, scax1, scay0, scay1}` per context |
| 0x284 | — | **0x800 bytes never referenced by pcalc.o** |
| 0xa84 | `m_a84` … 0xa9c `m_a9c` | `long long`, split-Z slopes: dz/dx hi, dz/dx lo, dz/dy hi, dz/dy lo |
| 0xaa4 | `m_aa4` | `long long` 0xffff — low-half mask |
| 0xaac | `m_aac` | `long long` 0xffff0000 — high-half mask |
| 0xab4 | `m_ab4` | 0x10 — the split shift |
| 0xab8 | `FIX` | from Pre3+0x134 |
| 0xabc | `m_abc`, 0xac0 `m_ac0`, 0xac4 `m_ac4` | the three edge functions at the DDA start |
| 0xac8 | `ddx[3]` | dE/dx per edge (= the edge's Δy, from `Pre3::dy`) |
| 0xad4 | `ddy[3]` | dE/dy per edge (= the edge's −Δx, from `Pre3::dx`) |
| 0xae0 | `bbl`, 0xae4 `bbt`, 0xae8 `bbr`, 0xaec `bbb` | clipped bounding box, as distances from the DDA start |
| 0xaf0 | `m_af0` | scanlines from the DDA start to the last vertex |
| 0xaf4 | `ddax`, 0xaf8 `dday` | DDA start position (2×2-stamp aligned) |
| 0xafc | `covs[3]` | `unsigned`, AA coverage start values |
| 0xb08 | `covdx[3]` | `unsigned`, AA coverage x gradients |
| 0xb14 | `covdy[3]` | `unsigned`, AA coverage y gradients |
| 0xb20 | the **output block** | `long long z; int f,a,r,g,b,s,t,q;` start values, then the same shape for d/dx at 0xb48 and d/dy at 0xb70 |
| 0xb98 | `xdir` | 1 = the scan runs right-to-left |
| 0xb9c | `ydir` | 1 = bottom-to-top |
| 0xba0 | `steep[3]` | per edge (sorted), Y is the major axis |
| 0xbac | `flat` | AA1==0 && 0x120==0 && FGE==0 && TME==0 |
| 0xbb0 | `SCANMSK` | from register 0x22 |
| 0xbb4 | `send_type`, 0xbb8 `send_addr`, 0xbbc `send_reg` | pass-through register write |
| 0xbc4 | `TME` 0xbc8 `FGE` 0xbcc `ABE` 0xbd0 `AA1` 0xbd4 (Pre3+0x120) 0xbd8 `CTXT` 0xbdc `FST` | |
| 0xbe0 | `maxexp` | |
| 0xbf0 | `rem` | scratch: the `reciproc()` remainder output nobody reads |
| 0xbf4 | `m_bf4` | sprite degeneracy bits: 1 = zero width, 2 = zero height |
| 0xbf8 | `type` | 0 point, 1 line, 2 triangle, 3 sprite |
| 0xbfc | vptr | `_vt.5PCalc` |

The 0x800-byte hole at 0x284 is genuinely dead: no instruction in the
object addresses anything between 0x284 and 0xa83, and nothing constructs
an object there.  It is declared as `char m_284[0x800]` so the rest of the
offsets line up.

## What PCalc does

`Put(Pre3 *p)` copies the attribute flags out of Pre3 and dispatches to
`Register` (pass-through register write) or `Primitive`.

### Register writes travel through the interpolator

`Register()` re-encodes the 64-bit register value into the *start-value*
fields of the output block, 8 bits per field, each shifted left by 9:

```
r = (v      ) & 0xff  <<9      f = (v >> 32) & 0xff  <<9
g = (v >>  8) & 0xff  <<9      z = (v >> 40) & 0xffffff <<9
b = (v >> 16) & 0xff  <<9
a = (v >> 24) & 0xff  <<9
```

and puts the register *address* into `ddax` (`addr >> 1`) and `xdir`
(`addr & 1`).  So the DDA reconstructs a register write from the same
fields it would otherwise read interpolated colour out of — the whole
64-bit value plus its address ride down the pipeline inside the
interpolator's start values.  (`send_addr`/`send_reg` are also copied
verbatim at 0xbb8/0xbbc, so the encoding is redundant; the DDA presumably
uses one or the other.)

Three registers are consumed here rather than passed on: SCISSOR_1 (0x40)
and SCISSOR_2 (0x41) into `scissor[0]`/`scissor[1]`, and SCANMSK (0x22)
into `SCANMSK`.

### Triangles

`DrawTriangle` is the spine:

```
        build param v[3] from Pre3's vertex queue      (RGBAF zero-extended,
                                                        STQ sign-extended)
        if (!IIP) v[0..1] colour = v[2] colour          flat shading
        SortVertex(p, v)      -> sortcode, A/B/C, ddx/ddy, xdir
        GetSPoint()           -> spoint, ydir, epointx, epointy
        if (spoint == 'C') SwapLine(ddx[1],ddy[1],ddx[2],ddy[2])
        if (FIX) broadcast the start vertex's attributes to the other two
        CorrectSPoint(); CorrectEPoint()               -> sxi/syi/exi/eyi, dda*
        clear dx, dy, and the split-Z accumulators
        if (!FIX) Slope(p, v)
        if (AA1)  CheckOverFlow()
        StartVal(p, v, spoint=='A' ? A : C)
        if (AA1 || 0x120) AACoverage(p)
        if (p->area < 0) negate ddx/ddy and p->area     (mutates Pre3!)
        GetDDAStart(p); BBox()
```

`SortVertex` classifies the triangle by the *signs* of the three edge Y
deltas into 18 `sortcode`s (1..18) and relabels the vertices so that the
DDA always walks the same way: A is the start vertex, edge 0 is C→A,
edge 1 A→B, edge 2 B→C.  `SortCoverage` and `GetSPoint` key off the same
code.  The six permutations are:

| sortcode | A B C | ddx/ddy edges |
|---|---|---|
| 4, 11, 14 | v0 v1 v2 | 2, 0, 1 |
| 3, 12, 15 | v0 v2 v1 | 0, 2, 1 |
| 2, 10, 13 | v1 v0 v2 | 1, 0, 2 |
| 6, 9, 18 | v1 v2 v0 | 0, 1, 2 |
| 5, 7, 16 | v2 v0 v1 | 1, 2, 0 |
| 1, 8, 17 | v2 v1 v0 | 2, 1, 0 |

`xdir` is taken from the sign of the doubled area, but only for the first
code of each group (4, 3, 2, 6, 5, 1 — the "general position" codes);
the degenerate horizontal-edge codes force `xdir = 0`.

### The division: three levels of precision

Everything downstream is `numerator / (2 * area)`, and PCalc computes it
three different ways depending on how much precision the attribute needs:

1. **R, G, B, A, F** — the cheap path.  `Slope()` multiplies the numerator
   by `v0 >> 24` (the *unrefined* reciprocal from `Reciproc::reciproc`)
   in plain 64-bit arithmetic, then `param::ShiftARGBSlope(-sft)` — which
   carries a hidden extra `>> 2` (doc/notes/param.md).
   F uses the *refined* reciprocal `v1` instead of `v0` unless the
   triangle is flat-shaded, untextured *and* nearly Z-constant.
2. **Z, S, T, Q** — the exact path.  `slong::Multiply` forms the full
   96-bit product against `v0`, shifts by the reciprocal's exponent and
   then by 26, and `Combine()`s back to 64 bits.
3. **Z again, when it varies by more than 16 bits across the triangle** —
   the split path.  The Z delta of each of the two edges is split into
   its high and low 16-bit halves (`& m_aac` then `>> m_ab4`, and
   `& m_aa4`), each half is carried through its own exact `slong`
   division against the *refined* reciprocal `v1` with shifts of
   `26 - m_ab4` and `26`, and the two are added back together.  The four
   halves and their signs live in `m_a84`..`m_a9c` / `m_23c`..`m_248`
   because `StartVal` needs them again to compute the Z start value at
   the same precision.

   Note that when the Z range is small the *first* Z path runs and the
   split path does not; when it is large the split path runs and
   **overwrites** `dx.z`/`dy.z`.  The two are never both authoritative.

`CheckOverFlow()` (AA1 only) clamps: if |dr/dx| … |db/dy| exceed 0x3ffff
all six are zeroed; the same for A and for F as separate groups; and if
either Z slope leaves ±2^42 both Z slopes and all four split-Z halves are
zeroed.

### Lines, sprites and points

`DrawLine` uses only vertices 1 and 2, zeroes vertex 0, picks the major
axis from `Pre3::steep[1]`, and runs `LineSlope` on whichever of dx/dy is
the major one (the other is zeroed).  `LineSlope` is `Slope` with the
area replaced by the major-axis extent and only one direction to
interpolate; it has the same three-level precision structure, including
the split-Z path (which is where its four `long long &` / `int &`
out-parameters go).

`DrawSprite` has no edge functions and no colour or Z gradients at all:
only S and T step, one per axis, through `SpriteSlope`/`SpriteStartVal`.
`DrawPoint` has no gradients whatsoever — it only positions the DDA and
converts the single vertex's attributes to the start values.

### Antialiasing

`AASlope(area, n, d)` is `n/d` in the coverage fixed point (multiply by
the reciprocal of `d<<4`, `>> 18`, sign restored by hand); `AAStartVal(a,
b, c, d)` is `(a*c + b*d + m_230) >> 5` — computed in signed-magnitude
form (four `abs`, four sign multiplies) although the result is
mathematically the same as the plain expression.  `C_Hosei` ("hosei" =
補正, correction) supplies the one-off correction added to the third
coverage start value: `|2*area| / |major edge delta|`, `>> 23`.

## Things worth knowing about the original

1. **`DrawSprite`'s FIX block copies S and T the wrong way.**  With FIX
   set, R/G/B/A/Z/F/Q are copied from vertex 2 onto vertex 1, but S and T
   are copied from vertex 1 onto vertex 2.  It is in the object, it is
   reproduced, and it is almost certainly a copy-paste slip in the 1998
   source.
2. **`PCalc::Primitive` mutates its input.**  `DrawTriangle` negates
   `Pre3::area` in place (`p->area = p->area * -1`) when the winding is
   negative.  The differential test has to hand each side its own copy of
   the Pre3 snapshot because of this.
3. **`Register()` clobbers `ddax` and `xdir`** with the register address.
   Both are recomputed by the next primitive, so it is harmless, but it
   means the two fields carry completely different things depending on
   `send_type`.
4. **`Ceil`, `Floor` and `Subpixel` take `const int &`.**  Every call site
   that passes an expression therefore materialises a stack temporary and
   the inlined body dereferences it — visible all over
   `CorrectSPoint`/`CorrectEPoint`/`CorrectLine*` as
   `mov %reg,-0xN(%ebp); … sar %cl,…`.
5. **`Reciproc::reciproc`'s `rem` output is written to a member
   (`PCalc+0xbf0`) at all six call sites and never read.**  It is a
   throwaway argument that happens to need an lvalue.
6. **The 18 `sortcode`s, not a switch.**  `SortVertex` and `SortCoverage`
   both test the code against three constants per group with `||`, which
   is why gcc emits compare chains and not a jump table.

## Source shapes pinned by the bytes

* **gcc 2.7 loads the *first* operand of a memory-to-memory comparison
  into a register.**  `mov A,%eax; cmp %eax,B` is `A ? B`, not `B ? A`.
  This one rule fixed `SortLine`, `GetSPoint`, `DrawSprite` and the
  `Correct*` functions; it is the cheapest way to recover which way round
  a comparison was written.
* Sony's style is **repetitive**: where a small conditional selects one
  field of a four-field assignment, the *whole* block is written out
  twice.  `GetSPoint` only became byte-identical once all four stores were
  duplicated into both arms of each inner `if`.
* `if (a < b) … else if (a > b) … else if (a == b) …` — the trailing
  redundant `==` test is real (`SortLine`, `GetSPoint`), and so are the
  `if (x == 0) … else if (x == 1) …` chains that re-test the same variable
  (`SortVertex`, `LineDDAEdgeStart`).
* An explicit flag variable, not a compound `if`: `CheckOverFlow` needs
  `char over; over = 0; if (…||…) over = 1; if (over) { … }` — writing the
  condition straight into the `if` costs 14 instructions.  It is a `char`,
  not an `int` (the object uses `%bl`).
* `stampw = flat ? 8 : 4;` (one store) and not `if/else` (two stores).
* The block-scoped `slong` in `Slope`/`LineSlope`: the mask store
  `movl $0xffffffff,-0x47c(%ebp)` reappears before *every* 96-bit divide,
  always into the same slot, i.e. each divide is its own `{ slong s; … }`
  block.  Reproduced here as the `SLOPEDIV` macro; a single function-scope
  `slong` constructs once and shifts every local's stack slot.
* Inside that macro the direction-dependent shift is a **ternary**,
  `s = sft >= 0 ? s >> sft : s << -sft;`, not an `if`/`else` with an
  assignment in each arm.  A class-typed ternary makes both arms return
  into one temporary, so a single `slong::operator=` follows the join;
  the `if` form gives each arm its own temporary and costs one extra
  28-byte `rep movsl` per divide — 12 of them in `Slope`.  With the
  ternary the `rep movsl` count matches the original exactly (61 in
  `Slope`, 31 in `LineSlope`).
* `param v[3];` as a local produces g++ 2.7's array-construction loop
  (`i = 2; do { param(&v[i]); } while (--i != -1)`), which is what
  `DrawTriangle`, `DrawLine` and `DrawSprite` open with.
* `ddax`/`dday` are `int` but `Primitive` halves them with an **unsigned**
  shift — `ddax = (unsigned int)ddax >> 1;`.  `covs`, `covdx` and `covdy`
  really are `unsigned int` members (that is why `SwapLine` has an
  `unsigned int &` overload at all, used only by `AACoverage`).
* `x0 = ddax - 1; x0 = x0 + stampw;` as two statements, not
  `x0 = ddax - 1 + stampw` — gcc reassociates the single expression to
  `(stampw - 1) + ddax`.
* `AAStartVal` returns `(int)(e + f + m_230) >> 5`; giving the sum a named
  `int` costs a stack slot.
* `SpriteStartVal`'s product is `b * c`, not `c * b`: the two cross terms
  of the DImode multiply come out in the opposite order.
* The `steep ? dy : dx` ternaries in `AACoverage`/`LineAACov` are written
  `steep == 0 ? dx : dy` — the fall-through arm is the `dx` one.

## Residuals

Three distinct effects account for essentially all of the remaining
difference, and all three are compiler-level rather than source-level.

1. **Call arguments are precomputed.**  Every `Reciproc::reciproc` call
   site in the 1998 object evaluates *all seven* argument values into
   pseudos first and only then pushes them (spilling one address to the
   stack when it runs out of registers); stock 2.7.2.x — patched or not,
   at `-O` or `-O2`, with or without `-fforce-mem`/`-fforce-addr`/
   `-fno-defer-pop` — computes each argument immediately before its own
   push.  This is what `AASlope` (1 insn), `C_Hosei` (2), `SpriteSlope`
   (16) and a large share of `Slope`/`LineSlope` are made of.  It looks
   like a **second 1998 cc1plus modification**, in the same family as the
   documented `build_vfn_ref` change and as the `lea 0(,idx,4)` anomaly in
   pre3.o: both amount to "force this address/value through a real
   pseudo".
2. **Nested calls do not defer their pop.**  Where the original has
   `call abs; add $0x4,%esp; push %eax; …; call AASlope; add $0x14,%esp`,
   ours accumulates and pops `$0x18` once.  Six sites in `AACoverage`,
   two in `LineAACov`.  This is *not* the `__attribute__((const))`
   question — `AAStartVal` and `CorrectLineStart` prove abs is not const
   in this TU, because they pop two abs arguments with a single
   `add $0x8,%esp`.
3. **Register allocation / stack-slot assignment.**  Everything else:
   `this` and `p` reloaded from the frame vs kept in a register, a
   constant hoisted into `%ebx` vs used as an immediate (`Primitive`'s
   `0x3ffff`), a value kept in `%eax` across a join so the store can be
   shared vs stored in both arms (`BBox`, `CorrectEPoint`, `StartVal`),
   one extra 4-byte slot.  `Slope` and `LineSlope` are the worst cases
   simply because they are the biggest: `Slope` is 2464 instructions
   against 2212, and the excess is the argument-precompute effect
   multiplied by 14 `reciproc`/`slong` call sequences plus the cascade of
   different slot numbers that follows from it.

Note that `SortVertex` — 2708 bytes, 691 instructions, 40 calls, the
function with the *highest* register pressure in the object — is
byte-identical, so the reloading-`this` behaviour that pre3.md flagged as
a compiler difference is reproduced correctly when the pressure is
genuinely high.  The residual is specifically about the *call sequence*,
not about `this`.

## Open questions

* Whether the "precompute all call arguments" behaviour can be added to
  `tools/gcc272/patches` as a second hunk.  `AASlope` and `C_Hosei` are
  the cheapest test cases in the tree (129 and 76 bytes, one call each);
  `SpriteSlope` is the next.  If it lands, `Slope` and `LineSlope` should
  collapse with it.
* What the 0x800-byte hole at PCalc+0x284 was for.
* Field names at 0x220, 0x22c, 0x234 and 0xbf4 are inferred from use only.
