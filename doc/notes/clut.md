# clut.o - the texture CLUT (palette) cache

`orig/lib/clut.o`, 0x793 B .text, 0x8f B .rodata.  src/clut.c + include/clut.h
+ include/addrcalc.h.

| function | orig | ours | state |
|---|---|---|---|
| `TexClut::load1(ClutAttr&)` | 0x4da | 0x49a | instruction shape reproduced; 0x40 short, see "residual" |
| `TexClut::load2(ClutAttr&)` | 0x17a | 0x17a | instruction-identical, one register pair swapped (eax/edx) |
| `TexClut::LoadData(ClutAttr&)` | 0xee | 0xee | instruction-identical, one register pair swapped (ecx/edx) |
| `TexClut::Lookup(int)` | 0x43 | 0x43 | **byte-identical** |

(Writing the calls `Address(x, y, PSMCT32, 1, a.CBP/64)` against an
`AddrConv::Address` that does *not* divide makes load2 byte-identical
instead, but then load1's argument pushes come out in the wrong order and
bitblt.o's `read`/`write` lose their whole prologue: the division belongs
inside the helper.  See doc/notes/bitblt.md.)

.rodata, .data, .comment, .note identical; relocation set, order and symbol
table identical (only offsets shift with load1's size).  `Lookup` is
byte-identical; `load2` and `LoadData` are the right size and differ by one
register-pair swap each (eax/edx and ecx/edx respectively).

Differential test `test/run_clut.sh`: **3,112,000 calls, 0 mismatches** -
300k randomized `load1`, 300k `load2`, 400k `LoadData` with the cbp0/cbp1
history carried across calls, and 4000 CLUT states x 528 `Lookup` indices.
The whole 0x498-byte object plus its overrun slack is compared after every
call, not just the return value.

## Where TexClut lives

`TexClut` is **embedded in TXM at +0x258** and is **0x498 bytes**, which
lands exactly on TXM+0x6f0 (TXM's CTXT field) - the fit is what pins the
size.  Cross-checks from txm.o:

* `TXM::SetTEXCLUT` takes `&this[0x258]` into a local and then addresses
  `+0x42c` and `+0x474` off it - TexClut's `attr` and `attr2`.
* `Lookup` reads `+0x438` and `+0x448`, i.e. `attr.CPSM` and `attr.CSA`.
* `TexClut+0x20` is TXM+0x278, which doc/STRUCTS.md already identified as
  a `MemIF*`; VRAM is reached as `memif->mem->vram[addr]` (two loads).
  This independently confirms MemIF+0x00 = Memory* and Memory+0x00 = the
  4 MB word array.

```
class TexClut : public AddrConv {   /* 0x498 */
        MemIF *memif;           /* 0x020 */
        int clut[256];          /* 0x024  256 words = 256 or 512 entries */
        int cbp0;               /* 0x424  CLD 2/4 remembered CBP */
        int cbp1;               /* 0x428  CLD 3/5 remembered CBP */
        ClutAttr attr;          /* 0x42c  active */
        ClutAttr attr1;         /* 0x450  context 1 */
        ClutAttr attr2;         /* 0x474  context 2 */
};
```

## AddrConv really has state

`TexClut`, `BitBLT` (bitblt.o) and `FBConfig`/`ZBConfig` (memory.o) all start
with the same eight ints and all pass `&this->page` ... `&this->np` straight
into `AddrConv::address_convert`, with `this` as the AddrConv `this`.  That
is only possible if those fields are **in AddrConv** and everybody derives
from it: g++ 2.7 has no empty-base optimisation, so an empty AddrConv base
would push `addr` to offset 4 - measured, and it breaks every offset by 4.

    class AddrConv {
            int addr, page, blk, bnk, pos, wd, np, bitpos;   /* 0x00..0x1c */
            void address_convert(...);
            void Address(int x, int y, int psm, int bw, int tbp) {   /* inline */
                    address_convert(x, y, psm, bw, tbp, page,blk,bnk,pos,wd,np);
                    addr = (page<<11) + (blk<<10) + (bnk<<9) + (pos<<4)
                            + wd*4 + (np>>3);
                    bitpos = (np & 7)*4;
            }
    };

The six-term recomposition + `bitpos` appears identically in clut.o (5 sites)
and memory.o, so it was an inline helper, not copy-paste.  include/addrconv.h
is read-only (owned by the addrconv work) and declares AddrConv stateless,
which is true of `address_convert` itself; **include/addrcalc.h carries the
fuller declaration and supersedes it - include one or the other, never both.**

## ClutAttr (0x24, 9 ints) - decoded by TXM, consumed here

| off | field | how TXM fills it (txm.o `TXM::Put`, `SetTEXCLUT`) |
|---|---|---|
| 0x00 | CBP | `((TEX0>>37)&0x3fff)*64` - a **word** address, so clut.c divides by 64 to get the address_convert `tbp` |
| 0x04 | CBW | `(TEXCLUT&0x3f)*64` - pixels; `/64` gives `bw` |
| 0x08 | PSM | the **texture** PSM, not the CLUT's; only `&7` is used, to pick 256 vs 16 entries |
| 0x0c | CPSM | `(TEX0>>51)&0xf`; 0 = PSMCT32, anything else = 16 bit |
| 0x10 | CSM | `(TEX0>>55)&1` |
| 0x14 | COU | `((TEXCLUT>>6)&0x3f)*16` |
| 0x18 | COV | `(TEXCLUT>>12)&0x3ff` |
| 0x1c | CSA | `((TEX0>>56)&0x1f)*16` - already an **entry index** |
| 0x20 | CLD | `(TEX0>>61)&7` |

`TXM::SetTEXCLUT` copies whole ClutAttr's with a 9-word `rep movsl`, which is
what pins the size at 0x24.

## Behaviour

`LoadData` implements the GS CLD table exactly: 0 = no load, 1 = load,
2 = load and latch CBP into CBP0, 3 = latch into CBP1, 4 = load only if CBP
differs from CBP0, 5 = same against CBP1.  Then CSM picks `load1` (CSM1,
the CLUT is a 16x16 or 8x2 texel block) or `load2` (CSM2, a run of 16-bit
texels from (COU,COV)).

`load1` has four arms - {32-bit CLUT, 16-bit CLUT} x {8-bit texture (256
entries), 4-bit texture (16 entries)}.  The 256-entry arms walk a 16x16 block
and un-swizzle with

    n = y/2*32 + y%2*8 + x + x/8*8;

the classic CSM1 palette order.  16-bit CLUTs pack two entries per word,
entry k<256 in the low half and k>=256 in the high half; `Lookup` undoes it.

## Original bugs and oddities

1. **PSMCT16S CLUTs are read as PSMCT16.**  `CPSM` reaches clut.c as the raw
   4-bit TEX0 field, so PSMCT16S (0x0a) is possible, but both `load1` and
   `load2` pass the literal `PSMCT16` (2) to `address_convert`.  PSMCT16S has
   a different in-page block arrangement (see src/addrconv.c), so a 16S CLUT
   is fetched from the wrong words.  The test that selects the arm is only
   `CPSM != 0`.
2. **clut[] overrun.**  In the 32-bit/4-bit arm `load1` writes
   `clut[CSA .. CSA+15]` with CSA already multiplied by 16, so CSA=16..31
   (legal in the 5-bit field, only meaningful for 16-bit CLUTs) writes up to
   `clut[511]` - 256 words past the end of the buffer, straight through
   `cbp0`, `cbp1` and all three ClutAttr's and out of TexClut entirely.
   `Lookup` reads the same way.  Reproduced identically by our source (the
   differential test compares the overrun region on purpose).
3. **CSA is applied asymmetrically.**  The 16-bit arms add CSA to the
   un-swizzled index; the 32-bit 256-entry arm ignores CSA completely.
4. The `bit[]` table is indexed with `psm & 7` but only has five entries, so
   `psm&7 == 5,6,7` reads two words past it (no such PSM exists, so it never
   fires).

## Source-shape lessons (all forced by bytes)

* **`{32,24,16,8,4}` lives in an inline function.**  The table's five stores
  appear *after* the `a.PSM` load and are duplicated at the *same* stack slot
  in both arms of `load1` - the signature of g++ 2.7 expanding an inline
  twice (argument evaluated at the call site first, callee locals sharing a
  stack temp).  A plain local array puts the init first and only once.
* **The table must be an array of aggregates.**  `int bit[]` folds the access
  into `cmpl $8,-0x14(%ebp,%eax,4)`; the original computes the base
  separately (`and $7,%edx; lea -0x14(%ebp),%eax; cmpl $8,(%eax,%edx,4)`).
  `struct {int bit;} bit[] = {{32},{24},{16},{8},{4}}` reproduces it exactly,
  as does `int bit[5][1]` and an `int *p = bit;` pair with the index computed
  first; we use the struct.  What is pinned is "the array element is not a
  bare int", not which spelling.
* **`n = 16; if (Depth(...) == 8) n = 256;` must go through a temp**
  (`int b = Depth(a.PSM); n = 16; if (b == 8) n = 256;`) - that is what puts
  the `n = 16` store between the `lea` and the `cmpl`.
* Loops are unrotated, as everywhere else in this archive: `x = 0; for (;;) {
  if (x == 16) break; ... x++; }` gives `cmpl $0x10,slot / je` at the top.
* The inner loop counter is `x` and the outer `y`, declared `int x, y;` in
  that order: reload hands out spill slots in pseudo order, so the source
  order decides that x is at -0x18 and y at -0x1c.
* `Lookup`'s second arm needs `k = attr.CSA; k += i;` - `k = attr.CSA + i;`
  compiles to `mov %eax,%edx; add mem,%edx`, the original has the load first.
* `LoadData` needs a `cld` temp (`int load = 0; int cld = a.CLD;`) and the
  CLD 4/5 tests written `if (a.CBP != cbp0)`, not the other way round.

## The residual in load1 (0x30 bytes)

Two things, and they are one thing:

    orig:  mov (%edi),%ecx / lea 0x0(,%ecx,4),%eax / mov (%eax,%edx,1),%eax
    ours:  mov (%esi),%eax /                         mov (%edx,%eax,4),%eax

This is exactly the `lea 0(,idx,4)` + `disp(tmp,base,1)` form doc/MATCHING.md
records as unreproducible in pre3.o (7 sites, 0 reproduced) and attributes to
the 1998 compiler modification.  It costs 8 bytes at each of the two sites
where the word address has to be reloaded from `this->addr`, **and** it raises
register pressure enough that the original spills the inner loop counter `x`
to the stack in all four arms (frame 0x24 vs our 0x20, and a fifth spill slot
for the `bitpos` temp).  Our build gives `x` a register (edi) instead, which
accounts for the rest.  Everything else - the six `lea` argument pushes, the
address recomposition, the un-swizzle arithmetic, the 16-bit packing, all
branch structure - is instruction-for-instruction identical.

Tried and rejected: `((int*)memif->mem)[addr]`, `*(int*)((char*)memif->mem +
addr*4)` and `memif->mem->vram[addr]` all produce the same (folded) code;
so do `-O2`, `-m386`, gcc 2.7.2.1 (rh42) and the RH 5.0 cc1plus.

## The two strings clut.o does not use

`.rodata` opens with `BITBLTBUF: Depth is different\n` and
`HWREG:Now not Host to Local mode\n`, neither of which has a relocation -
and the same pair opens bitblt.o, memif.o, memory.o, txm.o, pcrtc.o and
gpu2.o.  There is also an undefined `memcpy` with no relocation.  Cause:
**g++ 2.7 generates RTL for an inline member function as soon as it parses
it**, so the string constants it mentions are emitted into `.rodata` and the
library-call externals it needs get a `.globl`, in every translation unit
that includes the header - even when the function is never called.  So those
two messages sit in inline register-setters of `class BitBLT` in the header
every VRAM client includes, and one of its inlines calls `memcpy` with a
non-constant length.  include/bitblt.h reproduces that; see doc/notes/bitblt.md.

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
