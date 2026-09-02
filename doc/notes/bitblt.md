# bitblt.o - the local memory transfer engine

`orig/lib/bitblt.o`, 0xdfc B .text, 0x8c B .rodata.  src/bitblt.c +
include/bitblt.h (+ include/addrcalc.h).

| function | orig | ours | state |
|---|---|---|---|
| `BitBLT::read(Memory*,int,int)` | 318 | 312 | instruction shape reproduced; 6 B, address-form residual |
| `BitBLT::write(Memory*,unsigned,int,int)` | 402 | 395 | same, 7 B |
| `BitBLT::DoBitBLT(Memory*)` | 458 | 411 | same shape; `this` spilled in the original, kept in a register by us |
| `BitBLT::WritePixel(Memory*,long long)` | 1057 | 1057 | **instruction-identical** (jump-table contents shift with DoBitBLT's size) |
| `BitBLT::ReadPixel(Memory*)` | 1308 | 1308 | **instruction-identical** (same) |

.rodata identical (0x8c), symbol table and relocation set identical.
2365 of the 3580 .text bytes are instruction-for-instruction exact.

Differential test `test/run_bitblt.sh`: **1,820,000 calls, 0 mismatches**
(400k `read`, 400k `write`, 60k `DoBitBLT`, 60k x 8 `WritePixel`,
60k x 8 `ReadPixel`, 3254 error-path hits).  Each side gets its own 16 MB
fake VRAM and its own object; the object is compared after every call and
VRAM every 256 calls and at the end of every phase.

## class BitBLT (0x84, at Memory+0x400144)

Derived from AddrConv, so 0x00..0x1c is the shared address block
(see doc/notes/clut.md).

| off | field | notes |
|---|---|---|
| 0x20 | SBP | BITBLTBUF source base, a **word** address (SBP field * 64) |
| 0x24 | SBW | source width in **pixels** (SBW field * 64) |
| 0x28 | DBP | destination base |
| 0x2c | DBW | destination width |
| 0x30 | SPSM | |
| 0x34 | DPSM | |
| 0x38 | SSAX | TRXPOS |
| 0x3c | SSAY | |
| 0x40 | DSAX | |
| 0x44 | DSAY | |
| 0x48..0x54 | ? | written by `Memory::SetRegister`, unread here |
| 0x58 | DIR | TRXPOS.DIR, 0..3 |
| 0x5c | RRW | TRXREG width |
| 0x60 | RRH | TRXREG height, **counted down destructively** |
| 0x64, 0x68 | ? | |
| 0x6c | TRXDIR | 0 host->local, 1 local->host, 2 local->local, 3 off |
| 0x70 | count | pixels left in the current row |
| 0x74 | x | current column |
| 0x78 | phase | 24-bit packing phase; **only Memory::SetRegister advances it** |
| 0x7c | save | leftover bits of a pixel straddling a 64-bit word |
| 0x80 | ? | a flag `Memory::SetRegister` tests ten times |

Offsets 0x6c..0x80 are confirmed from memory.o, which addresses them as
`0x4001b0`..`0x4001c4` off the `Memory*` (= BitBLT+0x6c..+0x80) and is the
only writer of `phase` (`movl $0x0,0x4001bc` and `incl 0x4001bc`).

## What the five functions do

`read`/`write` are the pixel accessors: clamp x to 11 or 10 bits depending
on whether the buffer is wider than 1024, mask y to 11 bits, run
`AddrConv::Address`, then take/insert the pixel at `bitpos` with a
`(1 << Depth(psm)) - 1` mask.  32-bit formats bypass the mask entirely.
`write` also drops the pixel when `DBW <= x`.

`DoBitBLT` is TRXDIR 2: it sets a start corner and an (xstep, ystep) from
`DIR` and copies the RRW x RRH rectangle one pixel at a time through
read/write.

`WritePixel`/`ReadPixel` are the 64-bit host port.  PSMCT24/PSMZ24 get a
dedicated three-phase path because 8 pixels of 24 bits span 3 words; the
leftover 16 or 8 bits live in `save` between calls.  Every other format
falls into a table-driven loop: 2 pixels/word at 32 bpp, 4 at 16, 8 at 8,
16 at 4 (also the default for unknown PSMs).

## Original bugs and oddities

1. **`WritePixel` never advances `phase`, `ReadPixel` does.**  ReadPixel
   reads `phase % 3` and then increments it; WritePixel only reads it.  The
   host->local side depends on `Memory::SetRegister` bumping the counter
   (which it does), so the two halves of the same mechanism are driven
   from different places - easy to get out of step, and a real asymmetry.
2. **`DoBitBLT` hangs on a negative RRH.**  The loop is
   `for (; RRH != 0; RRH--)`, so RRH < 0 counts down forever.  The
   equivalent loops in Write/ReadPixel are guarded by `if (RRH <= 0) break`
   - the check exists everywhere *except* DoBitBLT.
3. **`DoBitBLT`'s "Unknown direction" is not fatal**: it prints and returns,
   unlike almost every other error in the archive, which calls `exit(1)`.
   DIR is a 2-bit field, so the arm is unreachable from real register
   writes anyway.
4. `read` masks x to 10 bits when `SBW <= 1024`, i.e. the comparison is on
   the *pixel* width, and `SBW == 1024` exactly takes the 10-bit path -
   an off-by-one against a 1024-wide buffer whose x can legitimately be
   0..1023, so harmless, but it means an 1024-wide buffer and a 64-wide
   one are clamped identically.

## Source shape lessons (forced by bytes)

* **`AddrConv::Address(x, y, psm, bw, tbp)` divides `bw` and `tbp` by 64
  itself.**  The originals compute `bw/64` and `tbp/64` *at the push sites*,
  interleaved with the six `lea` argument pushes; passing pre-divided
  arguments makes g++ 2.7 pre-evaluate them into pseudos before the pushes.
  clut.o agrees once its calls are written `Address(x, y, PSMCT32, 64,
  a.CBP)` - the 64 folds to the constant 1 the original pushes.
* **Every loop in bitblt.c is a `for` with a comma-separated increment
  clause** - `for (n = 0; n != 2; n++, data >>= 24, x++, count--)`.  That is
  what puts the deferred `add $0x14,%esp` argument pop *before* the
  increments (g++ 2.7 flushes the pending stack adjustment at the loop's
  continue label): a `for(;;)` with the increments at the end of the body
  puts the pop after them, and the two big functions do not match until the
  loops are written this way.  This is the counterpart of the "unrotated
  loop" lesson in doc/MATCHING.md - clut.o wants `for(;;)`+`if break`, and
  bitblt.o wants comma increments; both are the *unrotated* shape, they just
  differ in where the increment lives.
* **The masked coordinate goes into the call, not back into the parameter.**
  `Address(x, y & 0x7ff, ...)` gives the original's "y masked into a fresh
  temp, x masked in the parameter's register"; `y &= 0x7ff;` before the call
  makes g++ modify the parameter's incoming stack slot in place.
* **`WritePixel` case 1 has no temporary.**
  `write(mem, (((int)data & 0xff) << 16) | save, x, DSAY)` written inline is
  exactly 1057 bytes; hanging it on an `int` variable first adds an eight
  byte stack slot (the `long long & 0xff` needs a DImode temp) and 16 bytes.

## The residual

`read`, `write` and `DoBitBLT` differ only in the
`lea 0(,idx,4)` + `disp(base,tmp,1)` address form doc/MATCHING.md attributes
to the 1998 compiler modification (0/7 reproducible in pre3.o), and in the
register allocation that follows from the extra pressure it creates: in
`DoBitBLT` the original spills `this` and keeps the loop's `x` in a
register, we do the opposite, which costs 47 bytes of reloads.  Confirmed
not a flag or compiler-build question: the era Debian cc1plus, the patched
1998 one, RH 5.0's and gcc 2.7.2.1 all emit the identical shorter form.

## Oracle (hybrid replay)

A 13-object hybrid archive (addrconv libgpu2 pre1 pre3 slong div txm_div
texfunc param clut bitblt dbg xif, built in an isolated farm outside
tools/) replays bit-identically to the pure-Sony build:

    out/r614   9c7c73b156f8664633055e0300990a82
    out/o519   9fbc3187d1f98e0dff84e5d9aa5689df
    tools/probe                0 failures

and every out/Ridge Racer V dump swept (22 of them, including the 13 MB
143000/143006 streams) ends with the same 4 MB VRAM md5 as the render the
pure-Sony build stored beside it.
