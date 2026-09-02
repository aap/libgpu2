# memory.o - the VRAM back end

`orig/lib/memory.o`, 0x15ba B .text, 0x5b B .rodata.  src/memory.c +
include/memory.h (+ include/bitblt.h, include/addrcalc.h, include/dbg.h).

| function | orig | ours | state |
|---|---|---|---|
| `FBConfig::ReadPixel(Memory*,int,int)` | 424 | 423 | shape reproduced; 6 insns differ (one rematerialised `addr`, one scheduling swap) |
| `FBConfig::ReadStamp(Memory*,PixelStamp&)` | 314 | 314 | **same size**; regalloc + address form |
| `FBConfig::WritePixel(Memory*,int,int,PixColor,int,int,int)` | 496 | 466 | shape reproduced; `x` spilled in the original, kept in a register by us |
| `FBConfig::WriteStamp(Memory*,PixelStamp&)` | 602 | 762 | shape reproduced; six address-form sites |
| `ZBConfig::ReadZ(Memory*,int,int)` | 307 | 267 | shape reproduced; the original spills `this` and six `&member` pushes |
| `ZBConfig::ReadStamp(Memory*,PixelStamp&)` | 202 | 202 | **same size**; 8 of 75 insns differ (regalloc, deferred-pop position) |
| `ZBConfig::WriteZ(Memory*,int,int,unsigned)` | 334 | 345 | shape reproduced; two address-form sites |
| `ZBConfig::WriteStamp(Memory*,PixelStamp&)` | 314 | 277 | shape reproduced |
| `Memory::SetRegister(int,long long)` | 2522 | 2570 | shape reproduced; five address-form sites; jump table identical (0x80 entries, same targets) |

`.rodata` **identical** (0x5b), `.comment`/`.note` identical, **symbol table
identical** (names, bindings, types and order) and **all 144 relocations
identical in set and order**.  5690 vs 5562 .text bytes.

Differential test `test/run_memory.sh`: **4,802,000 calls, 0 mismatches**
(78416 error-path prints, 2618 fatal traps compared).  Both objects are
renamed apart with objcopy and linked into one i386 binary with the
original `addrconv.o` and `bitblt.o` as shared support; each side gets its
own 16 MB arena whose first 0x4001c8 bytes are the `Memory`, and the
0x1c8-byte config block is compared after every call, VRAM every 512 and
in full at the end of each phase.  `exit()` is overridden with a
`longjmp` so the fatal arms (illegal PSM in `address_convert`, unknown
register) are compared instead of killing the run.

## class Memory (0x4001c8, GPU2+0x10)

| off | field | notes |
|---|---|---|
| 0x000000 | `int vram[0x100000]` | the 4 MB of local memory |
| 0x400000 | `FBConfig fb` | the *active* frame buffer |
| 0x400034 | `FBConfig fb1` | FRAME_1 + FBA_1 |
| 0x400068 | `FBConfig fb2` | FRAME_2 + FBA_2 |
| 0x40009c | `ZBConfig zb` | the active Z buffer |
| 0x4000d4 | `ZBConfig zb1` | ZBUF_1 + TEST_1.ZTE |
| 0x40010c | `ZBConfig zb2` | ZBUF_2 + TEST_2.ZTE |
| 0x400144 | `BitBLT bitblt` | doc/notes/bitblt.md |
| 0x4001c4 | the active-context flag | see below |

`Memory+0x4001c4` is written by PRIM/PRMODE and read ten times by
`SetRegister` to pick which context copy to load; nothing outside memory.o
touches it.  Semantically it is `Memory`'s own `int ctxt` and `sizeof
(BitBLT)` is 0x80, but include/bitblt.h (owned by the bitblt drop) carries
it as `BitBLT::m_80`, so src/memory.c writes `bitblt.m_80`.  Both spellings
assemble to the same address and to the same `sizeof (Memory)` = 0x4001c8.

### FBConfig (0x34) and ZBConfig (0x38)

Both derive from `AddrConv` (include/addrcalc.h), so 0x00..0x1c is the
shared address block and the members below start at 0x20.

| off | FBConfig | ZBConfig |
|---|---|---|
| 0x20 | `FBP`  FBP*2048, a word address | `ZBP`  ZBP*2048 |
| 0x24 | `FBW`  FBW*64, pixels | `ZBW`  copied from FRAME's FBW |
| 0x28 | `PSM` | `PSM`  0x30/0x31/0x32/0x3a |
| 0x2c | `FBMSK`  1 bits are not written | `ZMSK`  1 = do not write |
| 0x30 | `FBA`  the alpha-correction bit | `mask`  usable Z bits for the PSM |
| 0x34 | - | `ZTE`  from TEST.ZTE |

### PixelStamp (0x3cc) and Pixel (0x38)

The 8x2 quantum the whole back end works on, and the carrier for register
writes travelling down the pipe.  Fields memory.o/memif.o pin:

| off | field |
|---|---|
| 0x000 | `type`  0 = pixels, else a register write |
| 0x004 | `reg` |
| 0x008 | `long long data` |
| 0x014 | `StampPos pos` - an **8-byte record** holding x and y (below) |
| 0x01c | `mask`  which of the 16 pixels are live |
| 0x028 | `aamask`  antialias-only pixels (`PixelStamp::AAMask()`) |
| 0x038 | `ABE` |
| 0x048 | `ctxt` |
| 0x04c | `Pixel pix[16]`, stride 0x38 |

`Pixel`: `PixColor c` at 0x00 (four ints R,G,B,A), `z` at 0x18, `pass` at
0x30 (alpha test passed), `afail` at 0x34 (AFAIL when it did not).

`pos` is a nested record and not two ints: memif.o's three `ReadStamp`
callers copy it with two loads followed by two stores, which is what g++
2.7 does for an 8-byte record (`compute_record_mode` gives it DImode) and
never what it does for two separate `int` assignments.

## The 8x2 stamp geometry

Every `*Stamp` function walks the stamp the same way:

```
	x0 = s.pos.x;  x = x0;  y = s.pos.y*2;
	xstep = 1;  if (x & 1) xstep = -1;
	i = 0..7:   pixel i at (x, y),   x += xstep
	x = x0;  y++;
	i = 8..15:  pixel i at (x, y),   x += xstep
```

so a stamp with an odd base column runs right to left, and `pos.y` is a
*row pair* index - the real rows are `y*2` and `y*2+1`.

## Memory::SetRegister

A 0x80-entry jump table.  The handled registers, in the order their bodies
appear in `.text` (which is the order of the `case` labels in the source):

FBA_1, FBA_2, FRAME_1, FRAME_2, ZBUF_1, ZBUF_2, BITBLTBUF, TRXPOS, TRXREG,
TRXDIR, HWREG, TEST_1, TEST_2, PRIM/PRMODE, `default`, and finally
TEXFLUSH/0x7f - an empty case that *is* the `break` target, so it comes
last in the source.

Everything with a per-context copy ends with the same four-line context
load (`fb = fb1/fb2`, `zb = zb1/zb2`); g++ 2.7 cross-jumps the identical
tails, which is why the object carries only two copies of each block.

FRAME also converts FBMSK for a 16-bit PSM:

```
	FBMSK = (FBMSK>>3 & 0x1f) | (FBMSK>>6 & 0x3e0) |
	        (FBMSK>>9 & 0x7c00) | (FBMSK>>16 & 0x8000);
```

(the last term is emitted as a 16-bit load of FBMSK's high half).

### The host<->local transfer mechanism

`Memory::SetRegister` is the whole of the transfer state machine outside
BitBLT's two pixel loops:

* **BITBLTBUF** decodes SBP/SBW/SPSM/DBP/DBW/DPSM straight into the live
  BitBLT fields (0x20..0x34).
* **TRXPOS** decodes SSAX/SSAY/DSAX/DSAY into *staging* fields at
  BitBLT+0x48..+0x54 - not into the live SSAX..DSAY at +0x38..+0x44 - and
  DIR straight into +0x58.
* **TRXREG** decodes RRW/RRH into staging fields at +0x64/+0x68, each
  clamped to 0x800.
* **TRXDIR** *commits*: staged positions -> live SSAX/SSAY/DSAX/DSAY,
  staged RRW/RRH -> live RRW/RRH, `TRXDIR = data & 3`, and then primes the
  packing state: `count = RRW`, `x = DSAX`, `phase = 0`.  If TRXDIR == 2 it
  runs the local-to-local blit there and then.
* **HWREG**, with TRXDIR == 0, calls `BitBLT::WritePixel(this, data)` and
  then does `bitblt.phase++`.

That increment is the missing half of the mechanism doc/notes/bitblt.md
records: `BitBLT::WritePixel` *reads* `phase` to pick the three-word
PSMCT24 packing slot but never advances it, while `BitBLT::ReadPixel`
(local->host) advances its own copy.  Host->local only works because
`Memory::SetRegister` bumps the counter after every HWREG word.  The two
directions of the same state machine are therefore driven from two
different objects.

## Original bugs and oddities

1. **The unknown-register arm exits with status 0.**
   `fprintf(stderr, "Unknown register( 0x%x )\n", addr); exit(0);` - every
   other fatal error in the archive uses `exit(1)`, so an application that
   writes a register the model does not know terminates *successfully*.
2. **A depth-mismatched local-to-local blit still arms the transfer.**
   The TRXDIR case commits the staged positions, RRW/RRH, `count`, `x` and
   `phase` *before* it checks `(SPSM & 7) == (DPSM & 7)`; the mismatch arm
   only prints "BITBLTBUF: Depth is different" (no `exit`), so the engine
   is left primed with a transfer that never ran.
3. **No PSM validation anywhere.**  FRAME's PSM is `(data>>24) & 0x3f`
   and goes straight into `AddrConv::address_convert`, whose default arm
   prints "illegal <psm> parameter" and calls `exit(1)`.  A single bad
   FRAME write therefore kills the host program at the next pixel.
4. **Neither ReadPixel/WritePixel nor ReadZ/WriteZ clamps x or y.**
   `BitBLT::read`/`write` mask x to 10 or 11 bits and y to 11; the frame
   and Z buffer accessors do not, so an out-of-range coordinate walks
   straight out of the 4 MB array.  (Legal GS coordinates keep it in
   range, so nothing in a real dump trips it.)
5. **`FBConfig::WritePixel` takes FBP and FBW as arguments** rather than
   reading its own members - and `WriteStamp`, its only caller, passes
   `this->FBP` and `this->FBW`.  Dead flexibility that costs two pushes
   per pixel.
6. TEXFLUSH (0x3f) and register 0x7f (the `Field` pseudo-register
   `GS_PutPort` writes) are explicit no-op cases rather than falling into
   the fatal default.

## Source shape lessons (forced by bytes)

* **`switch` on a `long long` calls `__cmpdi2`.**  `switch ((data>>24) &
  0xf)` in the ZBUF cases cost a kilobyte of libcall sequences until the
  value went through an `int` local; the original spills that local to
  `-0x24(%ebp)` and compares it four times.
* **A COND_EXPR assigned straight into a struct member stores into the
  member in each arm.**  `c.A = (short)data < 0 ? 0x80 : 0;` gives two
  `movl $imm,mem`; the original's `xor %eax,%eax / test / mov $0x80,%eax /
  mov %eax,-0x4(%ebp)` needs the value to go through a scalar local first
  (`a = ... ? 0x80 : 0; c.A = a;`).  gcc's `safe_from_p` decides whether
  the assignment target may be used as the COND_EXPR's own target.
* **`s.pix[i].c = c = ReadPixel(...)`** - the chained form.  Because the
  outer left-hand side is a COMPONENT_REF, `preexpand_calls` expands the
  call into its own `keep`-flagged stack temp first, and the value is then
  copied temp -> `c` -> destination: two 4-word copies and *two* 16-byte
  return slots (one per call site), exactly what the object has.  Plain
  `c = ReadPixel(...)` puts the struct return straight into `c`, and
  `s.pix[i].c = ReadPixel(...)` gives one copy out of a shared temp.
* **A block-scoped declaration inside the loop body is what keeps the
  loops unrotated** - see doc/MATCHING.md; `PixColor c` in the FBConfig
  stamps and `Pixel *p` in the ZBConfig ones.
* **`Depth(fb1.PSM)` re-reads the member; `Depth((data>>24) & 0x3f)`
  re-uses the register.**  The store to `fb1.FBMSK` between the two kills
  CSE's memory equivalence for `fb1.PSM`, so writing the member back into
  `Depth()` costs a reload the original does not have; repeating the
  expression instead makes CSE hand back the same pseudo.
* The `if` in the TRXDIR case is written `!=` first
  (`if ((SPSM&7) != (DPSM&7)) fprintf(...); else DoBitBLT(...);`); the
  other way round swaps two relocations.

## The residual

Everything left is the address form doc/MATCHING.md attributes to the
second, unfixed 1998 compiler modification, plus the register pressure it
creates.  The mod forces every address through a value, so each
`&s.pix[i]`-style computation has exactly one use and `combine` folds the
scale into it (`lea 0x4c(%ecx,%eax,8),%edx` + `0x30(%edx)`); stock 2.7.2.3
instead CSEs the *index*, emits `shl $0x3` and addresses every field with
a two-register `disp(%eax,%edx,1)`.  That is 25-ish bytes per site and it
also frees a register, which is why our functions systematically keep
`this`/`mem`/`s` in registers where the original reloads them from its
incoming stack slots.  The pattern is visible in every function here and
is not reproducible by any source spelling or flag combination we found -
where the original clearly holds *one* address across several accesses a
pointer local does reproduce it (see doc/notes/memif.md), but in memory.o
the original recomputes the address per basic block, which a pointer
local cannot imitate.
