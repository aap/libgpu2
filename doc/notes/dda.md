# dda.o — the scanline rasterizer

`src/dda.c` + `include/dda.h`, verified against `orig/lib/dda.o`
(7540 bytes .text, 11 exported functions + 12 file-static helpers,
`_vt.3DDA` in .rodata).

The DDA is the stage the whole front end exists to feed.  PCalc hands
it a set-up primitive; the DDA walks it as a grid of **stamps** — a
2-scanline by 4- or 8-pixel block — decides which of the stamp's pixels
are inside the primitive, interpolates every attribute for both
scanlines, and pushes the block to TXM through `DDATXM`'s vtable.

## Class layout (sizeof 0x254, vptr at 0x250, one virtual: `Put`)

Four blocks of the same 19-word shape (`struct DDAvalue`, 0x4c bytes)
carry the interpolator: the live values, the copy saved at the start of
the scanline, the per-stamp x step, and the per-stamp y step.

    off     member                  meaning
    0x000   PCalc *pcalc            the primitive being drawn
    0x004   DDATXM *txm             the next stage (TXM derives from it)
    0x008   int y                   the stamp's base scanline (mask row 0)
    0x00c   int bt                  rows past the bbox start edge, +2 a row
    0x010   int bb                  rows left to the bbox end edge, -2 a row
    0x014   int yn                  scanlines left to the last vertex
    0x018   DDAvalue v              live values
    0x064   DDAvalue sv             saved at the scanline start
    0x0b0   int dyy, dybt, dybb, dyyn     y step of y/bt/bb/yn
    0x0c0   DDAvalue dx             per-stamp x step
    0x10c   DDAvalue dy             per-stamp y step
    0x158   int isreg               1 = register write, not a stamp
    0x15c   int first               1 = first stamp of the primitive
    0x160   int reg_addr            \ verbatim copy of the register write
    0x164   long long reg_data      /
    0x16c   int px                  x, bit 0 = xdir (or the register addr)
    0x170   int py                  y/2 — the stamp row
    0x174   int mask                2x8 pixel mask (see below)
    0x178   long long z0            row 0: Z
    0x180   int a0, b0, g0, r0, f0  row 0: colour and fog
    0x194   long long z1            row 1: Z
    0x19c   int a1, b1, g1, r1, f1
    0x1b0   int s0, t0, q0          row 0: texture coordinates
    0x1bc   int s1, t1, q1          row 1
    0x1c8   int cova0               row 0 AA coverage, edge 0
    0x1cc   int covb0               row 0 AA coverage, edge 1 or 2
    0x1d0   int cova1               row 1, edge 0
    0x1d4   int covb1               row 1, edge 1 or 2
    0x1d8   int covdxa              coverage d/dx, edge 0
    0x1dc   int covdxb0             coverage d/dx for row 0's second edge
    0x1e0   int covdxb1             ... row 1's
    0x1e4   int m_1e4, m_1e8, m_1ec zeroed by the ctor, never touched here
    0x1f0   int esel0               row 0 uses edge 2, not edge 1
    0x1f4   int esel1               row 1 uses edge 2, not edge 1
    0x1f8   int amask               AA edge mask (or the register addr)
    0x1fc   long long dzdx          per-pixel slopes, sign-extended
    0x204   int dadx, dbdx, dgdx, drdx, dfdx
    0x218   int dsdx, dtdx, dqdx
    0x224   int zc                  Z carries out of the stamp
    0x228   int ydir, TME, FGE, ABE, FST, AA1
    0x240   int m_240               zeroed by the ctor, never touched here
    0x244   int CTXT
    0x248   int maxexp
    0x24c   int type                0 point, 1 line, 2 triangle, 3 sprite
    0x250   vptr                    _vt.3DDA

`DDAvalue` (0x4c):

    0x00 int x            0x18 int a       0x38 int e[3]   the edge functions
    0x04 long long z      0x1c int b       0x44 int el     distance to bbox left
    0x0c int cov[3]       0x20 int g       0x48 int er     distance to bbox right
                          0x24 int r
                          0x28 int f
                          0x2c int s, t, q

Note the colour order: **a, b, g, r, f**, and the code always assigns
`f` first and then a, b, g, r.  The pairing with PCalc's output block
(`f, a, r, g, b`) is what pins it: `v.b` takes `obdy`, `v.g` takes
`ogdy`, `v.r` takes `ordy`.

`DDATXM` is a bare interface (`virtual void Put(DDA*)`, vptr at offset
0) — the mangled constructor name `__3DDAP6DDATXM` is what names it.
It is TXM's base class (txm.o carries a local `_vt.6DDATXM`).

## Fixed-point formats

Everything arrives from PCalc **doubled**: `InitWalk` multiplies every
start value and slope by 2 (`v.f = p->ofv*2`, `v.z = p->ozv << 1`, …).
That extra bit is what lets a stamp address half-steps; the `Ext*`
helpers below shift it back out.

* `ddax`/`dday` are the DDA start in **2×2-stamp units** (PCalc halves
  them with an unsigned shift, doc/notes/pcalc.md).  `InitWalk` doubles
  them again: `y = dday*2` is the pixel row of the stamp's top line and
  `v.x = ddax*2 | xdir` is the pixel column with the scan direction
  folded into bit 0 (`ddax*2` is always even, so the bit is free).
  `px` hands that composite to TXM unchanged; `py = y >> 1`.
* The three **edge functions** `v.e[i]` are twice the natural edge
  value: one pixel of x costs `2*ddx[i]`, one scanline `2*ddy[i]`.  A
  whole stamp therefore steps `dx.e[i] = sw*ddx[i]*2` across and
  `dy.e[i] = ddy[i] << 2` (two rows) down.
* Colours and Z are carried in a 6-bit sub-pixel fraction.  `ZaddSpan`
  and `CaddSpan` add one scanline's worth of slope with that fraction
  dropped, rounding **up** when the slope is negative and the fraction
  is non-zero — `IsMinusDCDX` reads the sign at bit 19 of a colour
  slope, `IsMinusDZDX` at bit 43 of a Z slope.
* The `Ext*` helpers pull the field TXM wants out of the doubled value
  and sign-extend it: `ExtZslope` = 44 bits >> 4, extended to 40;
  `ExtZvalue` = 44 bits >> 6, extended to 38; `ExtCvalue` = 20 bits >>
  6, extended to 14; `ExtCovvalue` = 18 bits >> 6, extended to 12;
  `ExtTvalue` = 28 bits, no shift.
* `sign_extent(v, n)` builds its high mask as
  `((1 << (32-n)) - 1) << n`.  For the 64-bit overload with n > 32 that
  count is *negative*; it works only because the i386 shift is taken
  mod 64, so `32-n` becomes `96-n` and the mask still runs from bit n
  to bit 63.  Sloppy, but correct for the two values it is called with
  (38 and 40).

## The walk

`Put(PCalc*)` stores the PCalc pointer, then either `Register()` (a
pass-through register write) or `Primitive()`.

`Primitive()` is the whole rasterizer:

    n = 0;
    InitStamp();                     /* per-primitive constants */
    InitWalk();                      /* start values and the two steps */
    if (Stamping(n)) { txm->Put(this); n++; }
    do {
        if (IsVerticalWalk()) VerticalWalk(); else HorizontalWalk();
        if (Stamping(n)) { txm->Put(this); n++; }
    } while (IsWalk());

* `HorizontalWalk` adds `dx` to `v` — one stamp (4 or 8 pixels) along
  the scanline, left or right by `xdir`.  Before stepping it notes
  whether the stamp was still outside edge 0 or left of the bounding
  box; if it was, the *post-step* state is saved into `sv`, so `sv`
  always holds the leftmost still-interesting position of the row.
* `VerticalWalk` restores `v` from `sv`, adds `dy` (two scanlines) and
  re-saves.  `y += ±2` (sign from `ydir`), `bt += 2`, `bb -= 2`,
  `yn -= 2`.
* `IsVerticalWalk` says "the row is finished": edge 1 or edge 2 has
  gone negative, or `er` or `bt` has.  (`bt`/`bb` are distances from
  the DDA start, not screen coordinates — PCalc has already folded the
  scan direction into them, so the walk always does `bt += 2` and
  `bb -= 2` whichever way `ydir` points.)
* `IsWalk` says "the primitive is not finished": `bb >= 0`, or the
  current position is still inside the useful region.  Written with a
  **bitwise** `|` — the first operand compiles to the branchless
  `not; shr $31` sign trick, the second to the usual `&&` chain.
* `n` counts the stamps already sent; `Stamping` uses it only to set
  `first` (0x15c) on the first stamp that actually has pixels.

## `Stamping(n)` — one stamp

1. `px`/`py` are published; `isreg` cleared.
2. Both scanlines' Z/RGBA/F/STQ are extracted.  Row 0 is the current
   value and row 1 is one scanline further when `ydir == 0`; the two
   are swapped when the scan runs bottom-to-top.  STQ uses a plain
   `v.s + p->osdy` — no rounding — while Z and the colours go through
   `ZaddSpan`/`CaddSpan`.
3. `zc` (0x224) records whether the sub-pixel Z fraction carries out of
   the stamp: for a positive dz/dx it is `(z & 0x3f) + (dzdx*4 & 0x3c)
   > 0x3f`; for a negative one it is 0 when the low nibble of dz/dx is
   zero and otherwise the same test inverted and widened to -1.
4. With AA1, the coverage values for both rows and both active edges
   are extracted.  `esel0`/`esel1` pick edge 2 instead of edge 1 once
   the scan has passed the middle vertex — they are just the sign bits
   of `yn+1` and `yn`.  `covdy` is masked with `~0x3f` before being
   added (gcc renders that as `andb $0xc0`, which is the tell).
5. `sw` = 8 for a flat primitive, else 4.  With AA1 the three edge
   functions are pulled in by `abs(2 * (steep[i] ? ddx[i] : ddy[i]))`,
   i.e. by one pixel measured along the edge's major axis, so that the
   partially-covered pixels one step outside the edge can be found.
6. The column loop, `i = 0 .. sw-1`, evaluates both scanlines at
   column `sw-1-i` (row A) and `sw-i` (row B) and ORs the results into
   six bit masks: `isvld` (fully inside), `isaps` (AA "start" pixel:
   outside edge 0 but within one pixel of it), `isape_1` (AA "end"
   pixel on edge 1 or edge 2, whichever `yn` selects).  Each column
   also runs `isbbox` against the clipped bounding box and `Scnmsk`
   against SCANMSK for that scanline's parity.
7. The masks are packed into `mask` (0x174) and `amask` (0x1f8).
   Without AA it is simply `rowB << 8 | rowA` (rows swapped by `ydir`).
   With AA the nibbles interleave: row 0's interior mask in bits 0-3,
   its AA-start mask in 4-7, row 1's in 8-11 and 12-15, and the AA-end
   masks go to `amask` as `apea | apeb << 4`.  Note this means an
   AA1 primitive is limited to a **4**-pixel-wide stamp — consistent
   with `stampw = 8` only for `flat` primitives, which by PCalc's
   definition have AA1 == 0.

## `Register()` — a register write inside the interpolator

PCalc has already re-encoded the 64-bit register value into its output
block; the DDA re-encodes it *again*, into the very fields TXM would
otherwise read colour and texture coordinates out of:

    mask   = ((v >> 60) & 0xf) << 8 | ((v >> 56) & 0xf)
    s0     = (v        & 0x3fff) << 10      t0 = ((v >> 28) & 0x3fff) << 10
    s1     = ((v >> 14) & 0x3fff) << 10     t1 = ((v >> 42) & 0x3fff) << 10
    z0     = ((v >> 40) & 0xffffff) << 4
    r0/g0/b0/a0/f0 = byte 0/1/2/3/4 of v, << 4
    px = amask = reg_addr = send_addr,  reg_data = v,  isreg = 1

then `txm->Put(this)`.  The whole 64-bit datum plus its address ride
down the pipe twice over: once verbatim in `reg_addr`/`reg_data`, once
smeared across the interpolator channels.  Which one TXM uses is a
question for the txm attack.

## Original bugs / oddities

1. **`sign_extent(long long, int)` uses `32-n`, not `64-n`** (above).
   Correct only by accident of the i386 shift-count masking.
2. `InitWalk`'s `v.x = pcalc->ddax*2 | pcalc->xdir;` leaves a dead
   store at `-0x10(%ebp)` — reload spills the `ddax*2` pseudo and then
   uses the register anyway.  Not a source bug, but it is a useful
   marker: our build reproduces it, so the expression is one statement,
   not two.
3. **The Z-carry test is asymmetric.**  The positive-slope arm yields
   0/1 (`setg`) and the negative-slope arm 0/-1, and the negative arm
   short-circuits to 0 when `dz/dx & 0xf` is zero.  Whether TXM only
   ever tests it against zero is unverified.
4. `Stamping` reads `pcalc->AA1` through the object five times while
   using a cached `PCalc *p` for everything else in the same function
   — a source-level inconsistency that is visible in the bytes (the
   `mov (%edi),%edx` reloads).  Ditto `InitStamp`, which caches `p`
   only for its trailing flag-copy block.

## Compiler lessons (new)

* **`abs()` is declared `__attribute__((__const__))` here.**  dda.o
  pops each `abs` argument immediately after the call instead of
  deferring it, which is exactly the era-libc5 `<stdlib.h>` spelling.
  This is the opposite of pre3.o/pcalc.o, where a plain
  `extern "C" int abs(int);` is what matches.  The attribute is a
  per-object decision, not an archive-wide one.
* **A store to one member kills gcc's CSE entry for its neighbour.**
  `esel0 = ...; esel1 = ...; ... esel0 == 0 ? ...` compiles the test as
  a *memory* compare, because the `esel1` store invalidated the CSE
  value for `esel0`.  The 1998 object tests a register there, so the
  source kept a local: `esel0 = sel = ...; ... sel == 0 ? ...`, with
  `sel` block-scoped inside the `ydir` arm.
* **A pending argument pop is flushed by a COND_EXPR, and that decides
  what CSE can still see.**  `esel0 = yn + 1 < 0 ? 1 : 0;` and
  `esel0 = yn + 1 < 0;` produce *identical* arithmetic (`shr $31`), but
  the ternary makes `expand_expr` call `do_pending_stack_adjust()`
  before evaluating, so the `add $8,%esp` lands *before* the statement
  instead of after it.  In dda.o that ordering is load-bearing: with
  the pop in the wrong place gcc reallocates registers around it and
  80 bytes of cross-jumping in the AA block changes shape.  The 1998
  source wrote `? 1 : 0`.
* **`x & ~0x3f` compiles to `andb $0xc0,%dl`.**  Seeing a byte AND that
  leaves the top 24 bits alone means the source constant was
  `0xffffffc0`, not `0xc0`.
* **`if (c) a = 8; else a = 4;` and `a = 4; if (c) a = 8;` differ in
  where the condition's operand is loaded**, even though both emit two
  stores: the if/else form loads the pointer *before* the first store.
  `InitWalk` wants the first spelling, `Stamping` the second — both in
  the same file.
* **`sw - (i+1)` is folded to `(sw-1) - i` by `fold`** and yields
  `dec`; the 1998 object's `lea 0x1(%esi),%edx; sub %edx,%ecx` needs
  the `i+1` to survive as its own tree, which no spelling we tried
  reproduces (4 bytes × 3 sites).
* **`x << 1` and `x * 2` are different for `long long`**: `<<1` gives
  `shld/shl`, `*2` (and `x + x`) gives `add/adc`.  `InitWalk` uses both
  — `v.z = p->ozv << 1` but `dy.z = p->ozdy*2` — and the bytes say so.
* The mask packing `(apsa << 4 | mska) | (apsb << 12 | mskb << 8)` is
  pinned by association: gcc evaluates the parenthesised halves and
  ORs them, so a flat `a | b | c | d` chain comes out with a different
  instruction order.
* A local `PCalc *p = pcalc;` survives a call; a CSE'd `this->pcalc`
  does not.  Where the 1998 object reloads `(%edi)` after every call
  the source used `pcalc->`; where it keeps a register or a stack slot
  across calls there is a real local.  `InitWalk` has *no* calls at
  all, so its two spilled `pcalc` copies are pure CSE artefacts split
  by the `rep movsl` (which invalidates all of CSE's memory) and by the
  two `if` joins that follow it.
* Assignment order inside a function is visible: `Register()` needs
  `isreg = 1;` *before* `p = pcalc;` — one instruction, and it is the
  difference between byte-identical and not.
* **Call arguments computed into locals stay in registers across the
  next statement.**  `bok = isbbox(xl, xr, bt - 1, bb + 2);` pushes
  the two computed arguments inline; the 1998 object has them in
  registers *before* the `nn = yn + 1;` statement that precedes the
  call, which is what `int bt1, bb1;` reproduces.

## Byte-match status

21 of 28 functions byte-identical (2478 of 7320 function bytes); 22 of
28 instruction-identical; `.rodata`, `.note`, `.comment`, the
relocation set and the symbol table (names, bindings and order) all
identical, four symbol *sizes* apart.  What is left:

    sign_extent(long long,int)  308 = 308, register choice + one
                                narrowed compare (gcc folds the
                                known-zero high half in the original)
    ExtCslope, CaddSpan         -2 each: the 1998 object copies the
                                shifted value into a second register
                                before the conditional `inc'
    ZaddSpan                    125 = 125, one basic block's operand
                                registers swapped
    Stamping                    2928 vs 2944: 3 x 4 B of `sw - (i+1)'
                                (see the lessons above) + alignment; the rest of
                                the function is instruction-identical
                                bar an %ebx/%ecx swap in the loop
    InitWalk                    +9: three reload spill stores that the
                                original's allocator avoids
    Primitive                   two call displacements into the above

## Verification

* `test/run_dda.sh [iterations]` — differential test.  Both DDAs are
  driven from the same randomised PCalc output block through
  `DDA::Put`; a fake `DDATXM` with a hand-built g++ 2.7 vtable
  snapshots the whole 0x254-byte DDA object on every `txm->Put`, and
  the object is compared again after the call returns.  The generator
  keeps the bounding box small so the walk terminates, and mixes
  register writes in at 1 in 8.  Final run: **100000 primitives,
  1619139 TXM pushes, 0 mismatches**.  Negative control: changing one
  `sign_extent` width from 14 to 13 is caught after 12 iterations.
* Hybrid oracle: `REPLACE="… dda" ./build.sh` in an isolated symlink
  farm, `./probe` → 0 failures, and `gsreplay` on `out/r614`
  (9c7c73b156f8664633055e0300990a82), `out/o519`
  (9fbc3187d1f98e0dff84e5d9aa5689df) and three RRV game dumps
  (82657dfd651625d235983775b7bef849, 22b490f29f3fa653b1f22847de0a2859,
  4813f1bbee4957411db6a5d2e07074f7) → bit-identical VRAM.  The last
  two md5s were taken from a pure-1998 build of the same farm, so they
  are references, not just self-consistency.
