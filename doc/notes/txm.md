# txm.o - the texture machine

`orig/lib/txm.o`, 0x638d B .text, 0x100 B .data, 0x164 B .rodata.
src/txm.c + include/txm.h (+ include/memif.h, include/clut.h,
include/txm_div.h, include/bitblt.h).

TXM is the fragment stage between the rasterizer and the pixel back end.
The DDA pushes one 8x2 stamp at a time into `TXM::Put` through the
`DDATXM` interface; `Put` expands the DDA's interpolator state into a
`PixelStamp` (sixteen `Pixel`s with colour, Z, S/T/Q and fog),
`TXM::Stamp` textures, antialiases and fogs it, and `MemIF::Stamp` takes
it from there.  Register writes ride the same path.

| function | orig | ours | state |
|---|---|---|---|
| `TXM::GetOneTexel(int,int,int,PixColor&)` | 1322 | 1162 | shape reproduced; address forms + `this` reloads |
| `TXM::NFilter(NormTexCoord&,NormTexCoord&,int,PixColor&)` | 659 | 672 | **same instruction count** (217); register allocation |
| `TXM::LFilter` | 1345 | 1281 | shape reproduced; 14 insns of address form |
| `TXM::NMNFilter` | 792 | 798 | shape reproduced |
| `TXM::NMLFilter` | 1659 | 1642 | shape reproduced |
| `TXM::LMNFilter` | 59 | 59 | **instruction-identical**, 2 bytes (one register pair) |
| `TXM::LMLFilter` | 194 | 178 | shape reproduced; the `l0` spill |
| `TXM::Texturing(Pixel&,int,Gpu2RegFST)` | 448 | 448 | **same size**, jump table identical; 13 B of regalloc |
| `adjtbl(int)` | 111 | 111 | **byte-identical** |
| `log2_main(unsigned)` | 13 | 13 | **byte-identical** |
| `log2_adj(unsigned)` | 19 | 19 | **byte-identical** |
| `TXM::ComputeLod(PixelStamp&)` | 318 | 316 | instruction shape reproduced; one spill slot fewer |
| `TXM::Stamp(PixelStamp&)` | 240 | 243 | shape reproduced; `mask` kept in a register |
| `TXM::Put(DDA*)` | 13349 | 12821 | shape reproduced; the address forms in the two pixel loops and the spills they force |
| `TXM::ExtCov(PixelStamp&)` | 426 | 426 | **same size**, register allocation only |
| `TXM::AA1(PixelStamp&)` | 137 | 137 | **same size**, one register pair swapped |
| `TexAttr::MipTbpAuto()` | 620 | 572 | shape reproduced; 8 `lea 0(,i,4)` sites |
| `AA::Set(const DDA*)` | 15 | 15 | **byte-identical** |
| `Fog::Fogging(Pixel&)` | 100 | 98 | instruction shape reproduced |
| `_GLOBAL_.I._3TXM.valid8` | 148 | 148 | **byte-identical** |
| `TXM::MFilter1(int,int,int)` | 30 | 30 | **byte-identical** |
| `TXM::LFilter1(int,int,int,int,int,int)` | 79 | 79 | **byte-identical** |
| `TXM::TXM(MemIF*)` | 39 | 39 | **byte-identical** |
| `TXM::SearchQlevel(PixelStamp&)` | 85 | 92 | same instruction count; the stamp index address form |
| `TXM::ClampQ(int)` | 54 | 54 | **same size**, register allocation |
| `TXM::ClampT(int)` | 53 | 53 | **byte-identical** |
| `TXM::ClampLod(int)` | 38 | 38 | **byte-identical** |
| `TXM::SetFOGCOL(long long)` | 76 | 76 | **byte-identical** |
| `TXM::SetTEXA(long long)` | 71 | 71 | **byte-identical** |
| `TXM::SetTEXCLUT(long long)` | 196 | 196 | **same size**, two DImode spills |
| `TXM::SetTEX2(int,long long)` | 244 | 244 | **same size**, register allocation |
| `TXM::SetContext(Gpu2RegCtxt)` | 174 | 174 | **byte-identical** |
| `TXM::SetFRAME(int,long long)` | 139 | 139 | **byte-identical** |
| `TXM::SetSCISSOR(int,long long)` | 187 | 187 | **byte-identical** |
| `TXM::SetCLAMP(int,long long)` | 337 | 337 | **same size**, register allocation |
| `TXM::SetMIPTBP2(int,long long)` | 203 | 225 | shape reproduced; `this` not held across `Context()` |
| `TXM::SetMIPTBP1(int,long long)` | 203 | 225 | ditto |
| `TXM::SetTEX1(int,long long)` | 225 | 225 | **byte-identical** |
| `TXM::SetTEX0(int,long long)` | 709 | 549 | shape reproduced; the DImode spills the original makes |
| `TXM::Context()` | 58 | 58 | **byte-identical** |
| `PixelStamp::AAMask() const` | 13 | 13 | **byte-identical** (weak) |

**18 of 41 byte-identical**; 25485 vs 24586 .text bytes (96.5%).  `.data`
(TXM::valid8, 0x100 zero bytes), `.ctors`, `.note` and `.comment` are
identical; `.rodata` is identical but for the two-byte alignment pad
before the 128.0 constant (the 1998 assembler filled it with `89 f6`,
era GAS 2.9.1 with zeroes - the same one-byte-class residual as xif.o and
memif.o).  **All 205 relocations match in set and order, and the symbol
table matches name for name, binding for binding, in order** (only the
addresses and four sizes move).

## Layout

    class TXM : public DDATXM, public AddrConv {      /* 0x72c */
            MemIF *memif;           /* 0x024 */
            int m_28;               /* 0x028  never read or written */
            Gpu2RegCtxt ctxt;       /* 0x02c */
            TexAttr attr;           /* 0x030  the live context */
            TexAttr attrc[2];       /* 0x0e8, 0x1a0 */
            TexClutCtx clut;        /* 0x258  TexClut + its ctxt at 0x6f0 */
            TexA texa;              /* 0x6f4  AEM, TA0, TA1 */
            TexFunc texfunc;        /* 0x700  func, funcc[2], tcc, tccc[2], ctxt */
            Fog fog;                /* 0x71c  FOGCOL R, G, B */
            AA aa;                  /* 0x728  const DDA * */
            static Valid8 valid8;   /* .data, built by .ctors */
    };

* The **vptr is at 0x00** because TXM's first base, `DDATXM`, is the
  abstract one-virtual interface the DDA calls through; `DDATXM::Put` is
  **pure**, which is why txm.o carries a *local* `_vt.6DDATXM` whose one
  entry is `__pure_virtual` (and an undefined `__pure_virtual`).
* **`AddrConv` is TXM's second base**, at 0x04: `GetOneTexel` calls
  `address_convert` with `this+4` and the six out-parameters at
  `this+8 .. this+0x1c`, then recomposes `addr` and `bitpos` with
  addrcalc.h's six-term sum.  g++ 2.7 has no empty-base optimisation, so
  this is what puts `memif` at 0x24 and everything else where it is.
* `TexClut` (include/clut.h) is 0x498 and stops at `attr2`; the
  per-context selector the CLUT switch reads lives one word behind it, at
  TXM+0x6f0.  txm.h derives `TexClutCtx` from `TexClut` to add it rather
  than touching clut.o's own view.
* `TexAttr` is 0xb8 = the 0x2e words `Context()` copies:

      0x00 TBP[7]   0x1c TBW[7]   0x38 PSM   0x3c W  0x40 H
      0x44 TW  0x48 TH  0x4c TCC  0x50 LCM  0x54 L  0x58 K  0x5c MXL
      0x60 MMAG  0x64 MMIN  0x68 MTBA  0x6c WMS  0x70 WMT
      0x74 MINU*16  0x78 MAXU*16  0x7c MINV*16  0x80 MAXV*16
      0x84 SCAX0  0x88 SCAX1  0x8c SCAY0  0x90 SCAY1
      0x94 FBP*2048  0x98 FBW*64  0x9c FPSM
      0xa0 MAXU  0xa4 MINU  0xa8 MAXV  0xac MINV     (raw, REGION_REPEAT)
      0xb0 bits(MINU)  0xb4 bits(MINV)

  FRAME and SCISSOR are decoded here even though TXM never uses them -
  the same TexAttr is presumably what pcrtc/gpu2 debug code reads.

## The sampling algorithm, as implemented

**Per stamp** (`Put`, `Stamp`):

1. `Put` fills a `PixelStamp` from the DDA block: TME/FGE/ABE/FST/AA1/
   CTXT/maxexp, the live mask, the AA masks
   (`m_20 = (mask>>4)&0xf0f`, `m_24 = (amask&0xf)|((amask&0xf0)<<4)`,
   `aamask = m_20|m_24` when AA1 else 0, and with AA1 the live mask is
   narrowed to `mask & 0xf0f`).
2. With AA1 it points `aa.dda` at the DDA and runs `ExtCov`, which
   re-walks the two coverage edges across the stamp and leaves a 0..0x80
   coverage in each pixel's `m_2c`.
3. Sixteen pixels, two rows of eight, are interpolated from the row's
   start value and the per-pixel d/dx with `k = i-2` (row 0) and
   `k = i-10` (row 1); pixels 4..7 and 12..15 add the DDA's `zc` carry to
   the Z base.  Colour and fog go through the 3-bit overflow code
   `e = (v>>11)&7`: `e` 2/3/4 saturates to 0xff, 5/6/7 (i.e. negative)
   to 0, otherwise `(v>>4)&0xff`.  Z uses the same code at bit 35 with
   0xffffffff for the saturation.  S and T go through `ClampT`
   (sign-magnitude, 17-bit magnitude, saturating), Q through `ClampQ`.
4. `Stamp` ORs `AAMask()` into the mask, textures every live pixel if
   TME, applies AA1's coverage-as-alpha, fogs if FGE, forces ABE when
   AA1, and hands the stamp to `MemIF::Stamp` through MemIF's vtable.

**Per fragment** (`Texturing`): two `NormTexCoord`s do the perspective
divide (txm_div.o) on S/Q and T/Q; then `MMAG` picks NFilter/LFilter when
the LOD is 0, and `MMIN` (0..5) picks
NFilter/LFilter/NMNFilter/NMLFilter/LMNFilter/LMLFilter otherwise; then
`TexFunc::Func` applies TFX/TCC.

**LOD** (`ComputeLod`): with `LCM == 1` the LOD is just `K`.  Otherwise
the Q of one live pixel is picked out of the stamp's low four columns -
`vld8 = (mask & 0xf) | ((mask >> 4) & 0xf0)` indexes `valid8`, whose
priority order 1, 9, 2, 10, 0, 8, 3, 11 tries column 1 of both rows,
then column 2, then 0, then 3 - normalised to an exponent `b` and a
15-bit mantissa `m`, and

    lod = (K*2 - ( (((b-1)*128 + log2_main(m)) << L >> 2)
                 + (((maxexp-141)*128 + log2_adj(m)) << L >> 2) )) >> 1

`log2_main(m)` is `m >> 8` (the linear part) and `log2_adj(m)` is
`adjtbl(m>>8)`, the correction that makes `i + adjtbl(i)` equal
`round(128*log2(1 + i/128))`.  `adjtbl` computes that with `log()` in
double, narrowed through a `float`, so it is the one place in the object
where x87 rounding is observable.  The result is clamped to
`0 .. MXL*16` by `ClampLod`.

**Coordinates** (`TexCoordN`/`TexCoordL` + `TexAttr::WrapU/WrapV`): the
divider's sign/exponent/mantissa become a 12.4 texel coordinate

    e = ue + re + (qzero ? 0 : log2size) - lod - 1     (capped at 16)
    m = ((rm | 0x8000) * um >> 15) & 0xffff
    e == 16, or e == 15 with m's bit 15 set  ->  saturate to 0 / 0x7fff
    e < 0                                    ->  0
    otherwise                                ->  m >> (15-e), negated by sign

`qzero` is set when FST said UV, in which case the texture size does not
scale the coordinate.  The bilinear variant subtracts half a texel (8 in
12.4) unless the coordinate saturated.  Then CLAMP is applied: REPEAT
masks to `(1 << (w+4)) - 1`, CLAMP saturates to `0 .. (16<<w)-16`,
REGION_CLAMP to `MINU..MAXU` shifted down by the LOD, REGION_REPEAT does
`(c & ((MINU>>lod)<<4 | 0xf)) | ((MAXU>>lod)<<4)`.

**Bilinear** (`LFilter`): the second sample is the first with its low
`n` bits incremented and wrapped, `n = max(3, (WMS==3 ? bits(MINU) :
TW) - lod)`; the four texels are blended with `LFilter1`, i.e.
`((16-a)*p0 + a*p1) >> 4` twice horizontally and once vertically.
Trilinear (`NMLFilter`, `LMLFilter`) blends two levels with `MFilter1`
using `lod & 0xf`.

**Texel fetch** (`GetOneTexel`): `Address(u, v, PSM, TBW[lod],
TBP[lod])`, then a 0x3b-entry jump table on PSM:

* PSMCT32/PSMZ32 - the word, R/G/B/A from its four bytes.
* PSMCT24/PSMZ24 - the same, then A comes from TEXA:
  `AEM && R==G==B==0 ? 0 : TA0`.
* PSMCT16/16S/PSMZ16/16S - the half word selected by `bitpos`, expanded
  1-5-5-5 with each component replicated (`(t<<3)|(t>>2)`), then
  A = `A ? TA1 : (AEM && rgb==0 ? 0 : TA0)`.
* PSMT4/8/4HL/4HH/8H - the word shifted by `bitpos` and masked to
  `Depth(PSM)` bits, through `TexClut::Lookup`, then unpacked as 32 bit
  or (CPSM != 0) as 16 bit with the same TEXA rule.
* anything else - `"TXM:Illegal Texture pixel format\n"` and `exit(0)`.

## Original bugs and oddities

1. **`GetOneTexel`'s illegal-format arm calls `exit(0)`**, not `exit(1)`
   - the same oddity as `AlphaTest::Pass` and `Memory::SetRegister`.
   Everything else in the object exits 1.
2. **`NMNFilter` floors the LOD at 0 and the other three mip filters do
   not.**  NMNFilter reads `if (lod <= 7) l = 0; else l = min((lod+8)>>4,
   MXL)`, but LMNFilter has no such test and NMLFilter/LMLFilter use a
   bare `lod >> 4`; `ComputeLod` already clamps to 0..MXL*16, so the
   guard is dead - but it is only in one of the four.
3. **The colour saturation only recognises three negative codes.**  With
   `e = (v >> 11) & 7`, `e` of 2, 3 or 4 gives 0xff and 5, 6 or 7 gives
   0 - so a negative value clamps to 0 only down to -0x1800; at -0x1801
   `e` wraps back to 4 and it saturates to 0xff instead.  The Z path has
   exactly the same three-way test at bit 35.
4. **`ExtCov` only ever writes eight of the sixteen pixels' coverage.**
   The masks it walks are `(mask>>4) & 0xf0f` and
   `(amask&0xf) | ((amask&0xf0)<<4)`, so bits 4..7 and 12..15 are always
   zero and pixels 4..7 and 12..15 always take the `0x800` (full)
   default; the same is true of `AA1`, which only loops 0..3 and 8..11.
5. **`MipTbpAuto` refuses a non-square or smaller-than-32 texture with
   `exit(1)`**, and prints `"Width and Height must be greter 32 when MTBA
   mode\n"` - the typo is in the 1998 object.  It knows four rules:
   PSMCT32/PSMCT24/PSMT8H/PSMT4HL/PSMT4HH (32 bits a texel), PSMCT16,
   PSMT8 and PSMT4; **PSMCT16S and every Z format fall through to
   `"MTBA::PSM [%d] is invalid.\n"` - which prints but does *not* exit**,
   so the mip pointers are left as they were.
6. **`TXM::valid8` is indexed by a code that only covers the stamp's low
   four columns** (`(mask & 0xf) | ((mask>>4) & 0xf0)`), and
   `SearchQlevel` asserts it is non-zero - a stamp whose four leftmost
   columns are all dead aborts the model.  `ExtCov`, three functions
   away, folds the *other* four columns (`(mask>>4) & 0xf0f`) instead.
7. **TXM+0x28 is never read or written** by anything in the archive.
8. The register decode consumes SCANMSK (0x22) silently: TXM neither uses
   it nor forwards it to MemIF, so `Memory` never sees it.
9. `TEXA` and `FOGCOL` are the only registers TXM decodes that are *not*
   per-context - correctly, since neither is on the hardware, but it
   means `SetTEXA`/`SetFOGCOL` are the only two `Set*` members with no
   `Context()` call and no `ctx` argument.

## Source-shape lessons (all forced by bytes)

* **A class's inline members are emitted out-of-line, globally, wherever
  its vtable is emitted.**  An unused inline member of a plain class is
  not emitted at all (measured); but TXM has a vtable, so *all twenty* of
  its inline members come out at the end of `.text`, after
  `_GLOBAL_.I.*`, in reverse declaration order - which pins the order the
  class declares `Context`, `SetTEX0`, `SetTEX1`, `SetMIPTBP1`,
  `SetMIPTBP2`, `SetCLAMP`, `SetSCISSOR`, `SetFRAME`, `SetContext`,
  `SetTEX2`, `SetTEXCLUT`, `SetTEXA`, `SetFOGCOL`, `ClampLod`, `ClampT`,
  `ClampQ`, `SearchQlevel`, the constructor, `LFilter1` and `MFilter1`.
  This also explains pcalc.o's `Floor`/`Ceil`/`Subpixel` and memif.o's
  `Set*`/`Context`, which are inline members too, not repeated macros.
* **A declaration that runs a constructor flushes the pending argument
  pop.**  `Texturing`'s two `InitTable()` calls each have their argument
  popped immediately (`add $0x4,%esp`) instead of being deferred, which
  no statement spelling reproduces - but declaring the two coordinates
  with a constructor that calls `InitTable()` does, exactly.  So
  `NormTexCoord` had `NormTexCoord() { InitTable(); }`; txm.h derives a
  `TexCoord` from txm_div.o's view rather than change it.
* **`return` out of a loop is not `break`.**  `valid8`'s constructor is
  `for (i = 0; ; i++) { if (i > 255) return; ... }`: with `break` g++ 2.7
  rotates the loop and puts the test at the bottom, with `return` the
  branch does not target the loop's end label, the rotation is skipped
  and the function is byte-identical.  (A block-scoped declaration in the
  body has the same effect - that is what keeps `Put`'s two pixel loops
  and `ExtCov`'s two loops unrotated - and `AA1`'s loops need a `Pixel
  *p` for both that reason and its addressing.)
* **`(x >> n) & m` on a `long long` assigned to an `int` narrows, `* 64`
  does not.**  `((data >> 14) & 0x3f) * 64` keeps the value in DImode and
  leaves a dead `xor %ecx,%ecx` before every `shl`; `((data >> 14) &
  0x3f) << 6` narrows to SImode and matches.  Six functions and twenty
  sites in txm.o turn on that one character.
* **`if (c) return X; ... return Y;` and `if (!c) { ...; return Y; }
  return X;` lay out differently.**  The 1998 `ClampQ` and `ClampLod` put
  the early-out *after* the main path (`jne`/`jl` to a block at the end),
  which is the `if (!c) { ... }  return X;` spelling; the natural
  `if (c) return X;` puts it inline and inverts the branch.  Same class:
  `ExtCov`'s coverage decode is `if (e != 2) { r = 0; if (e != 3) r =
  (unsigned char)w; } else r = 0x80;`.
* **The CLAMP wrap is a member, not a free inline and not a macro.**  It
  reads WMS/MINU/MAXU inside the arm that needs them - so its inputs
  cannot be evaluated up front, which rules out a free inline with those
  eight parameters - *and* it pins the coordinate to a stack slot, which
  rules out a macro.  `TexAttr::WrapU(int &c, int w, int lod)` and
  `WrapV` do both.
* **The mantissa arithmetic is unsigned.**  `m` in the coordinate helper
  has to be `unsigned` - `m >> (15-e)` is `shr`, and
  `(unsigned)((rm|0x8000)*um) >> 15` is `shr` too, while an `int` gives
  `sar` in both places.  Likewise `a` in `ClampT`/`ClampQ` is `unsigned`,
  which is what stops g++ folding `(v>>8)>>19` into `v>>27`.
* **`e = e > 16 ? 16 : e;` is not `if (e > 16) e = 16;`.**  The ternary
  loads 16 unconditionally and overwrites it in the other arm, which is
  what the object does (the memif.o COND_EXPR lesson again).
* **The `switch` index has to be a local.**  `switch (attr.PSM)` compares
  the memory operand directly (`cmpl $0x3a,0x68(%ecx)`); `int psm =
  attr.PSM; ... switch (psm)` puts it in a register for the compare and
  lets reload rematerialise it for the table jump, which is what both of
  the object's switches do.
* **`x = A | f();` loads A first only if it is a separate statement.**
  `Stamp`'s mask is `mask = s.mask; mask |= s.AAMask();`.
* `int m = attr.MXL; if (l > m) l = m;` - with `attr.MXL` written out
  twice the compare uses the memory operand and the assignment reloads
  it; with the local it is one load, a register compare and a register
  move, in all four mip filters.
* **`(A && B) & C` materialises the conditions, `A && B && C` branches.**
  The colour/Z saturation test is written
  `if ((e != 2 && e != 3) & (e != 4))`: `fold` turns the `&&` pair into
  the range test `(unsigned)(e-2) > 1`, and the *bitwise* `&` forces both
  halves into `seta`/`setne` values that are then `test`ed - which is
  exactly the 28 `seta`/`setne` pairs the 1998 object has and that no
  spelling with three `&&`s produces (gcc's `simple_operand_p` only lets
  `fold` turn `TRUTH_ANDIF` into `TRUTH_AND` when the right operand is a
  bare DECL, and a comparison is not).
* **Every field store in `Put`'s stamp loops goes through its own
  `Pixel *p`.**  The object recomputes `&s.pix[i]` into a *fresh* stack
  slot for each of the nine fields it writes; one shared pointer local
  gives one computation, and no pointer at all lets gcc fold the frame
  pointer into the addressing.  A store macro whose body opens a block
  and declares `Pixel *p = &s.pix[i]` reproduces it - and, as a bonus,
  is the block-scoped declaration that keeps both loops unrotated.
* **`w = v >> 4` is hoisted out of the arm that uses it** in the colour
  and Z saturation, and `(d & 0x1f)`, `((d>>5) & 0x1f)` and
  `((d>>10) & 0x1f)` are three separate locals in the 16-bit unpack: a
  pseudo that is set once has known `nonzero_bits`, so `t >> 2` comes out
  as `shr`; reuse one variable for all three and it is `sar`.

## The residual

Everything left is the two known compiler-mod classes (doc/MATCHING.md):

* **address forms.**  The 1998 object forces addresses through values -
  `lea 0x0(,%esi,4)` plus `disp(tmp,base,1)` where stock 2.7.2.3 folds
  the scale into one `disp(base,idx,4)`.  `MipTbpAuto` has exactly eight
  such sites (48 bytes, the whole of its difference), `SearchQlevel` one,
  `GetOneTexel` two, and `Put`'s two pixel loops one per field per pixel
  (`&s` hoisted into a pseudo rather than folded into the frame-pointer
  displacement) - which is most of `Put`'s 528 bytes.
* **the register pressure that follows from it.**  Because each address
  has one use, the original runs out of registers earlier and reloads
  `this` from its incoming stack slot (`mov 0x8(%ebp),%eax; add $N,%eax`
  six times in `GetOneTexel`, twice per `Context()` in `SetMIPTBP1/2`)
  and spills DImode shift temporaries (`SetTEX0`, `SetTEXCLUT`) where
  ours keeps them live.  `NFilter`, `ExtCov`, `AA1`, `SetCLAMP`,
  `SetTEX2`, `ClampQ` and `Texturing` are all the *same size with the
  same instruction stream* and differ only in which hard registers the
  allocator picked.

## Verification

**Differential** (`test/run_txm.sh`, `test/diff_txm.c`): both texture
machines are renamed apart and linked into one binary with the original
`addrconv.o`, `clut.o`, `texfunc.o` and `txm_div.o` shared between them
(TXM calls those with its own subobjects as `this`, so sharing them keeps
the comparison about txm.o alone).  Each side gets its own 96 MB mapped
VRAM arena - filled identically for the first 16 MB - its own fake MemIF
whose single virtual snapshots every `PixelStamp` handed downstream, and
its own 0x72c-byte TXM with 0x800 bytes of slack behind it, because
`TexClut::load1` writes `clut[CSA..CSA+15]` with CSA already multiplied
by 16 and runs off the end of the object.  `fprintf`, `exit` and
`__assert_fail` are replaced so the six fatal arms - five `exit()`s and
`SearchQlevel`'s assert - are compared (message *and* status) instead of
killing the run.

Seven phases: the leaf clamps and filters called directly; `GetOneTexel`
over every PSM and both CLUT depths; register writes through `Put` for
every register TXM decodes plus five it only forwards; MTBA; the
`clut[]` overrun with CSA >= 16 (and the repair it needs, because
otherwise every later `Lookup` indexes with a CSA that is a VRAM word);
stamps through `Put` with all six filter modes, both contexts and
TME/FGE/AA1/ABE; and a deterministic sweep of the fatal arms.  The whole
TXM object (minus the vptr and the two MemIF pointers) and its slack are
compared after every call, together with every downstream `PixelStamp`
field TXM actually writes.

**130,125,064 calls, 129,750,064 comparisons, 365,650 fatal arms, 0
mismatches.**  Five deliberate one-token mutations (the bilinear
half-texel offset, the 16-bit alpha bit, the bilinear wrap width, a
REGION_CLAMP shift, `Bits`'s argument) are all caught within a few
thousand iterations; a sixth (a `valid8` priority entry) within 100k.

**Hybrid oracle** (18 objects: addrconv libgpu2 pre1 pre3 slong div
txm_div texfunc param pcalc dbg clut bitblt xif memory memif dda txm):
`probe` reports **0 failures**, and the replays are bit-identical to the
pure-Sony build of the same farm:

    out/r614                              9c7c73b156f8664633055e0300990a82
    out/o519                              9fbc3187d1f98e0dff84e5d9aa5689df
    out/Ridge Racer V_..._20260902142940  82657dfd651625d235983775b7bef849
    out/Ridge Racer V_..._20260902143217  5499cf3b9738b71b59382b2fc79ef958
    out/Ridge Racer V_..._20260902143451  593c5e1918dee4554016a99bab2ba6fa
    out/Ridge Racer V_..._20260902143527  582f13f6df5c4b5515ee87f70ccf81a3
    tools/probe                           0 failures

r614 and o519 are the values doc/MATCHING.md already records for the
17-object hybrid; all four Ridge Racer V streams were also replayed
through a pure-Sony build of the same farm and came out identical word
for word (143527 is the 18 MB late in-game stream - the one that hammers
the texture unit hardest - 143451 is 5.8 MB, 143217 is 3.1 MB and
142940 is 1.4 MB).
