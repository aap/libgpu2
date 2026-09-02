# memif.o - the interface stage

`orig/lib/memif.o`, 0x1cbd B .text, 0x70 B .rodata.  src/memif.c +
include/memif.h (+ include/memory.h).

TXM hands MemIF one 8x2 `PixelStamp` at a time through MemIF's single
virtual (`Stamp`), and MemIF runs the per-pixel back end on it - alpha
test, destination alpha test, depth test, alpha blend, dither, colour
clamp - then writes it back through `FBConfig::WriteStamp` and
`ZBConfig::WriteStamp`.  The same `Stamp()` carries register writes down
the pipe (`PixelStamp::type != 0`).

| function | orig | ours | state |
|---|---|---|---|
| `AlphaTest::Pass(int)` | 272 | 272 | **byte-identical** |
| `AlphaTest::ATest(PixelStamp&)` | 170 | 154 | shape reproduced; the else arm's address form |
| `DAlphaTest::DATest(Memory*,PixelStamp&)` | 272 | 272 | **same size, same 68 instructions**; register allocation only |
| `DepthTest::ZTest(Memory*,PixelStamp&)` | 416 | 400 | shape reproduced; address form |
| `AlphaBlend::Blend(Memory*,PixelStamp&)` | 621 | 765 | shape reproduced; eight address-form sites |
| `Dither::Dithering(PixelStamp&)` | 666 | 685 | shape reproduced; both loops rotate (see below) |
| `ColorClamp::Clamp(PixelStamp&)` | 266 | 250 | shape reproduced; address form |
| `MemIF::Stamp(PixelStamp&)` | 2730 | 2653 | shape reproduced; jump table identical (0x4a entries, same targets) |
| `PixelStamp::AAMask() const` | 13 | 13 | **byte-identical** (weak) |
| `MemIF::ReadWord(int)` | 25 | 18 | 7 B, address form |
| `MemIF::MemIF(Memory*)` | 27 | 27 | **byte-identical** |
| `MemIF::SetContext(Gpu2RegCtxt)` | 186 | 186 | **byte-identical** |
| `MemIF::SetPABE(long long)` | 202 | 202 | **byte-identical** |
| `MemIF::SetCOLCLAMP(long long)` | 22 | 22 | **byte-identical** |
| `MemIF::SetDTHE(long long)` | 22 | 22 | **byte-identical** |
| `MemIF::SetDIMX(long long)` | 455 | 467 | shape reproduced; the base-address form |
| `MemIF::SetALPHA(int,long long)` | 307 | 307 | **byte-identical** |
| `MemIF::SetTEST(int,long long)` | 387 | 387 | **same size, same 125 instructions**; one stack-slot number |
| `MemIF::Context()` | 186 | 186 | **byte-identical** |

**9 of 19 functions byte-identical (1237 of 7357 .text bytes)**; two more
(`DATest`, `SetTEST`) are the same size with the same instruction stream and
differ only in which hard registers the allocator picked.
7405 vs 7357 .text bytes.  All 87 relocations identical in set and order;
`.comment`/`.note` identical; `.rodata` identical but for the two-byte
alignment pad before `_vt.5MemIF` (the original's assembler filled it with
`89 f6`, era GAS 2.9.1 with zeroes - the same one-byte-class residual as
xif.o).  The symbol table matches name for name, binding for binding, in
order, except that `_vt.5MemIF` lands two positions later in ours: the
original's assembly mentions the vtable between `Stamp` and `ReadWord`,
ours only at the constructor that references it.

Differential test `test/run_memif.sh`: **5,302,000 calls, 0 mismatches**
(34968 error-path prints, 8154 fatal traps compared).  memif.o cannot be
tested alone - `Stamp` reaches VRAM through memory.o - so *both* objects
are renamed apart per side and linked together, each side's memif
resolving to its own memory; the two halves share only the original
`addrconv.o` and `bitblt.o`.  Each side gets a 16 MB arena whose first
0x4001c8 bytes are a `Memory`, its own 0xfc `MemIF` and its own 0x3cc
`PixelStamp`; MemIF (minus the vptr), stamp and Memory config block are
compared after every call, VRAM every 512 calls and in full per phase.

## class MemIF (0xfc, GPU2+0x0c, vptr at 0xf8)

| off | field | size |
|---|---|---|
| 0x00 | `Memory *mem` | 4 |
| 0x04 | `int ctxt` | 4 |
| 0x08 | `AlphaTest atest` | 0x10 |
| 0x18 | `AlphaTest atestc[2]` | 0x20 |
| 0x38 | `DAlphaTest datest` | 8 |
| 0x40 | `DAlphaTest datestc[2]` | 0x10 |
| 0x50 | `DepthTest ztest` | 8 |
| 0x58 | `DepthTest ztestc[2]` | 0x10 |
| 0x68 | `AlphaBlend blend` | 0x18 |
| 0x80 | `AlphaBlend blendc[2]` | 0x30 |
| 0xb0 | `Dither dither` | 0x44 |
| 0xf4 | `ColorClamp clamp` | 4 |
| 0xf8 | vptr -> `_vt.5MemIF` | 4 |

`atestc`/`datestc`/`ztestc`/`blendc` really are arrays, not two named
members: `SetALPHA` and `SetTEST` index them with `ctx` (`lea
0x80(%edi,%eax,8)` for `blend[ctx]`, `lea 0x18(%ebx,%eax,1)` for
`atest[ctx]`).

The six units:

* `AlphaTest`  = ATE, ATST, AFAIL, AREF  (0x10)
* `DAlphaTest` = DATE, DATM              (8)
* `DepthTest`  = ZTE, ZTST               (8)
* `AlphaBlend` = PABE, A, B, **D, C**, FIX (0x18 - D comes before C, the
  order the ALPHA register decodes them in: A bits 0-1, B 2-3, C 4-5,
  D 6-7)
* `Dither`     = DTHE, `int mat[4][4]`   (0x44)
* `ColorClamp` = CLAMP                   (4)

`_vt.5MemIF` is 0x10 bytes: the 8-byte zero prefix plus one entry pointing
at `MemIF::Stamp`.  So the pipeline's virtual "Put" for this stage is
`Stamp(PixelStamp&)`.

## What the stage does

`MemIF::Stamp` splits on `s.type`:

**Pixels** (`type == 0`):

```
	s.mask = s.mask | s.AAMask();
	atest.ATest(s);
	datest.DATest(mem, s);
	ztest.ZTest(mem, s);
	blend.Blend(mem, s);
	dither.Dithering(s);
	clamp.Clamp(s);
	mem->fb.WriteStamp(mem, s);
	if (ztest.ZTE == 1)
		mem->zb.WriteStamp(mem, s);
```

The antialias coverage mask is OR'd into the live mask *before* the tests,
and `ZBConfig::WriteStamp` then excludes those pixels from the Z write
again - that is how AA edge pixels get colour but not depth.

**A register write** (`type != 0`): a 0x4a-entry jump table.  MemIF
consumes ALPHA_1/2, DIMX, DTHE, COLCLAMP and PABE and stops; it consumes
*and forwards* TEST_1/2 and PRIM/PRMODE (Memory needs ZTE and the context
flag); everything else falls through the `default` straight into
`mem->SetRegister(reg, data)`.  PRIM/PRMODE take the context from
`s.ctxt`, not from the register data.

The per-context units are reloaded by the same four-statement block after
every register that changes them (`Context()` out of line, and a macro
expansion at each of the ten sites inside `Stamp`, `SetContext`,
`SetPABE`, `SetALPHA` and `SetTEST`).

The blend is `Cout = ((A - B) * C >> 7) + D` per component, with the four
selectors picking source (0), destination (1) or 0/FIX (else), applied to
R, G and B only.

## Original bugs and oddities

1. **`DepthTest::ZTest` is full of self-assignments.**  The ZTST == ALWAYS
   arm is a complete no-op loop - `s.pix[i].z = s.pix[i].z;`, sixteen
   times - and the "test passed" arm of both GEQUAL and GREATER re-stores
   the Z it has just read.  The compiler emits all of it.
2. **`AlphaTest::Pass`'s illegal-function arm calls `exit(0)`**, not
   `exit(1)` (the same oddity as `Memory::SetRegister`'s unknown-register
   arm).  ATST is a 3-bit field, so it is unreachable from a real register
   write.
3. **The alpha test is written the other way round**: `return AREF > a;`
   for ATST == LESS, `AREF <= a` for GEQUAL, and so on.  Equivalent to the
   manual's "A op AREF", but the source reads inverted.
4. **`Dither::Dithering` can index its matrix out of bounds.**  The index
   is C's signed `%`: `mat[y%4][x%4]`, and `x` walks *down* from the base
   column when that column is odd, so a stamp at x = 1 reaches x = -6 by
   the eighth pixel and reads `mat[dy][-1]`..`mat[dy][-3]` - i.e. the
   `DTHE` field and the tail of `blendc[1]`.  Latent: it needs an
   odd-based stamp near column 0.
5. **`ColorClamp` wraps rather than saturating when COLCLAMP != 1**, and
   it does so with a byte zero-extension, so a negative component comes
   out as a large positive one.
6. **`MemIF::SetPABE` writes both context copies** (PABE is not a
   per-context register), unlike `SetALPHA`/`SetTEST` which take a `ctx`.
7. **A 24-bit frame buffer makes the destination alpha test all-or-
   nothing**: `DATest` returns immediately, having zeroed the whole
   stamp's mask when DATM == 0 and left it untouched when DATM == 1.
8. `MemIF::ReadWord` exists and is exported but nothing in the archive
   calls it.

## Source shape lessons (forced by bytes)

* **An 8-byte struct assignment wants a *ternary*.**  `compute_record_mode`
  gives a two-int record DImode, so `a = b` is load/store/load/store, but
  `a = c ? b0 : b1` loads both words in each arm and shares one store pair
  - which is what `Context()` does for `DAlphaTest` and `DepthTest`, and
  what `if/else` does not.  The 0x10-byte `AlphaTest` and the 0x18-byte
  `AlphaBlend` copies are `if/else` (a class-typed ternary would need a
  BLKmode temporary, which the object does not have).  Getting this one
  line right turned `SetContext`, `SetPABE` and `Context` byte-identical.
* **A pointer local reproduces the 1998 address form where the original
  holds one address across several accesses.**  `SetALPHA` and `SetTEST`
  compute `&blendc[ctx]` / `&atestc[ctx]` once and use 4-bit
  displacements; writing that pointer explicitly (and assigning it
  immediately before each group, not all three up front) makes `SetALPHA`
  byte-identical and `SetTEST` instruction-identical.
* **A block-scoped declaration inside the loop body keeps the loop
  unrotated** (doc/MATCHING.md).  `Pixel *p` in `ATest`, `ZTest` and
  `Clamp`, `int a, pass` in `DATest`, `int alpha` in `Blend`.
  `Dithering` is the one function where the object shows no such
  declaration and no register-allocated candidate, so its two loops rotate
  where the original's do not - about 12 bytes.
* **`if (m & 1<<i)` gives `bt`; `if (!(m & 1<<i)) continue;` gives
  `sar %cl / xor $1 / test $1`.**  Same for `if (a & 0x80)` (`testb
  $0x80`) versus `if ((a & 0x80) == 0)` (`cmpb $0x0 / jge`, a sign test).
  Both matter inside `Blend`.
* **`x = cond ? 0 : (x > 255 ? 255 : x)` into a struct member stores in
  each arm; through a scalar local it goes through a register.**  The
  original's `ColorClamp` needs `v = s.pix[i].c.R; if (v < 0) v = 0; else
  if (v > 255) v = 255; s.pix[i].c.R = v;`.
* **`(unsigned)a >> 7` versus `a >> 7`.**  `DATest`'s DATM == 0 arm is
  `((unsigned)a >> 7 ^ 1) & 1` (`shr`), the DATM != 0 arm `(a >> 7) & 1`
  (`sar`); with `a` signed in both arms the object gains a byte and loses
  the pair.
* **`PixelStamp::AAMask()` is declared `inline` in the class and defined
  at the *end* of memif.c** (and of txm.c).  That is why every use is a
  call and the body comes out last in `.text` as the file's only weak
  symbol.  Declaring it `inline` in the class body as well silences g++'s
  "used before it was declared inline" warning without changing a byte.
* The register decoders are macros: the same text is expanded inside
  `MemIF::Stamp`'s switch *and* in the standalone `Set*` members, because
  g++ 2.7 does not inline a non-inline function within its own translation
  unit (verified) and the `Set*` symbols are global, not weak - so they
  cannot have been inline either.

## The residual

Every remaining byte in `ATest`, `ZTest`, `Blend`, `Clamp`, `ReadWord`,
`SetDIMX` and `Stamp` is the address form described at the end of
doc/notes/memory.md - the original forces addresses through values, so
`combine` folds the scale into a single `lea` and the field references get
4-bit displacements, while stock 2.7.2.3 CSEs the index, emits `shl` and
addresses each field with a two-register form.  `DATest` and `SetTEST`
differ only in which hard registers the allocator picked (and one stack
slot number), and `Dithering` additionally in the loop rotation above.
