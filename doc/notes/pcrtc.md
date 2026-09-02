# pcrtc.o - the display / CRTC circuit

`orig/lib/pcrtc.o`: 19309 B `.text`, 1028 B `.rodata`, no `.data`, no
`.bss`, 25 global functions plus 17 weak out-of-line copies of header
inlines and 12 vtables.  Reconstructed as `include/pcrtc.h` +
`src/pcrtc.c`; differential test `test/diff_pcrtc.c` / `test/run_pcrtc.sh`.

Everything `GPU2::Put` routes to when `(signed char)addr < 0 || addr > 0xff`
lands here.  There is deliberately **no CSR, IMR or BUSDIR code in the
object** - see "What is *not* here" below.

## Class graph

```
Xifbase (xif.h)                 the 9-virtual display sink
  XWindow                       real X11 window          (xif.o)
  XWindowDump                   headless, two Frame2d's  (xif.h, weak here)

PixelBlend      {int alp; vptr}                 sizeof 8, vptr at +4
  PixelBlendAlp   blend with PMODE.ALP
  PixelBlend1a    blend with 2 x circuit 1's own alpha, saturated

MemRead : AddrConv  {AddrConv 0x20; PSM, FBP, FBW; vptr}   sizeof 0x30
  MemRead32  PSMCT32   MemRead24  PSMCT24   MemRead16  PSMCT16/16S

Circuit             a POD, sizeof 0x34
DispCirc            the two circuits + three readers + merge rect, 0x114
DispInfo : AddrConv PMODE/SMODE/SYNC/BGCOLOR state + DispCirc + blenders,
                    0x190; derives from AddrConv because oldDispPixelMag
                    addresses local memory through *itself*

PCRTC               0x40, EXTBUF/EXTDATA/EXTWRITE + 2 virtuals
  PCRTCxif : PCRTC  0x1d4 = PCRTC + DispInfo di + Xifbase *xif
  PCRTCdmy : PCRTC  0x40, adds one virtual of its own
```

`PCRTCxif` is the only class in the hierarchy with a non-inline virtual
(`SetRegister`, `Resize` is inline), so g++ 2.7 emits `_vt.8PCRTCxif`
*globally* here and, with it, an out-of-line copy of every one of
PCRTCxif's inline members - as **global** symbols.  gpu2.o, which
constructs a PCRTCxif, carries none of them and an undefined
`_vt.8PCRTCxif`.  PCRTC / PCRTCdmy / MemRead* / PixelBlend* / XWindowDump
have no key method at all, so their vtables are local (`r`) in every
including object and their inlines come out **weak**, emitted only where a
vtable references them.  That is the rule that decides `T` vs `W` here.

### Layout, PCRTCxif-relative

| off | field | notes |
|---|---|---|
| 0x00 | `Memory *mem` | |
| 0x04 | `EXBP` | EXTBUF.EXBP*64, a word address |
| 0x08 | `EXBW` | EXTBUF.EXBW*64, pixels; init 0x20 |
| 0x0c | `WDX` | |
| 0x10 | `WDY` | |
| 0x14 | `WFFMD` | |
| 0x18 | `EMODA` | |
| 0x1c | `EMODC` | |
| 0x20 | `SX` | EXTDATA; init 0x100 |
| 0x24 | `SY` | init 0x80 |
| 0x28 | `WW` | EXTDATA.WW+1; init 0x100 |
| 0x2c | `WH` | EXTDATA.WH+1; init 0x80 |
| 0x30 | `SMPH` | |
| 0x34 | `SMPV` | |
| 0x38 | `Frame2d *ext` | the incoming video; **never set by anything** |
| 0x3c | vptr | `_vt.5PCRTC` then `_vt.8PCRTCxif` |
| 0x40 | `DispInfo di` | |
| 0x1d0 | `Xifbase *xif` | XWindow (disp_on 1) or XWindowDump (2) |

`DispInfo`, PCRTCxif-relative (DispInfo-relative in brackets):

| off | field | |
|---|---|---|
| 0x40 [0x00] | AddrConv | addr/page/blk/bnk/pos/wd/np/bitpos |
| 0x60 [0x20] | `EN1` | PMODE bit 0 |
| 0x64 [0x24] | `EN2` | bit 1 |
| 0x68 [0x28] | `CRTMD` | bits 2-4 |
| 0x6c [0x2c] | `AMOD` | bit 6 |
| 0x70 [0x30] | `MMOD` | bit 5 |
| 0x74 [0x34] | `SLBG` | bit 7 |
| 0x78 [0x38] | `ALP` | bits 8-15 |
| 0x7c [0x3c] | `hstart` | SYNCH1: `(d>>11 & 0x7ff) + (d>>43 & 0x3ff)`; init 652 |
| 0x80 [0x40] | `vstart` | SYNCV: `(d>>20 & 0x3ff) + (d>>32 & 0x3ff) + (d>>53 & 0x3ff)`; init 38 |
| 0x84 [0x44] | `interlace` | 0 iff INT==0 && FFMD==0; init 1 |
| 0x88 [0x48] | `INT` | SMODE1 bits 13-14; init 2 |
| 0x8c [0x4c] | `FFMD` | SMODE2 bit 0; init 1 |
| 0x90 [0x50] | `aout1` | `AMOD == 0`; init 1 |
| 0x94 [0x54] | `aout2` | `AMOD != 0`; init 1 |
| 0x98 [0x58] | `PixColor bg` | BGCOLOR, A always 0 |
| 0xa8 [0x68] | `DispCirc dc` | |
| 0x1bc[0x17c] | `PixelBlendAlp balp` | alp only ever set by SetPMODE |
| 0x1c4[0x184] | `PixelBlend1a b1a` | alp zeroed by DispInfo's ctor |
| 0x1cc[0x18c] | `PixelBlend *bl` | MMOD picks one; init `&b1a` |

`DispCirc`, PCRTCxif-relative (DispCirc-relative in brackets):

| off | field | |
|---|---|---|
| 0xa8 [0x00] | `over` | the two DISPLAY rects overlap |
| 0xac [0x04] | `mx`, `my`, `mw`, `mh` | .. 0xb8: their bounding box |
| 0xbc [0x14] | `MemRead *rd[2]` | DISPFB.PSM picks one per circuit |
| 0xc4 [0x1c] | `MemRead32 r32` | |
| 0xf4 [0x4c] | `MemRead24 r24` | |
| 0x124[0x7c] | `MemRead16 r16` | |
| 0x154[0xac] | `Circuit c[2]` | 0x34 each, c[1] at 0x188 |

`Circuit`: `FBP FBW PSM DBX DBY w h DX DY DW DH MAGV MAGH` at +0x00..+0x30.
`w`/`h` shadow `DW`/`DH`: DISPLAY writes both pairs, the read-back loops use
`w`/`h` and the merge arithmetic `DX/DY/DW/DH`.  Power-on: FBP 0, FBW 640,
PSM 0, DBX=DBY=DX=DY=0, w=DW=640, h=DH=480, MAGV=MAGH=0.

## The registers PCRTCxif::SetRegister decodes

Jump table over `0x80 .. 0x101` (130 entries):

| addr | | |
|---|---|---|
| 0x80 | PMODE | EN1 EN2 CRTMD AMOD MMOD SLBG ALP; picks the blender and sets aout1/aout2 |
| 0x81 | SMODE1 | INT = bits 13-14 |
| 0x82 | SMODE2 | FFMD = bit 0 |
| 0x83 | SRFSH | **falls into `default`** - "Unknown register" + `exit(0)` |
| 0x84 | SYNCH1 | hstart |
| 0x85 | SYNCH2 | **empty body**, jumps straight to the epilogue |
| 0x86 | SYNCV | vstart |
| 0x87 | DISPFB1 | `dc.SetDISPFB(0, data)` |
| 0x88 | DISPLAY1 | `dc.SetDISPLAY(0, data)` |
| 0x89 | DISPFB2 | `dc.SetDISPFB(1, data)` |
| 0x8a | DISPLAY2 | `dc.SetDISPLAY(1, data)` |
| 0x8b-0x8d | EXTBUF/EXTDATA/EXTWRITE | `PCRTC::SetRegister(addr, data)`, inlined |
| 0x8e | BGCOLOR | |
| 0x100 | *Display* | the pre-merge path, `oldDispPixel`/`oldDispPixelMag` |
| 0x101 | *DisplayPcrtc* | the real two-circuit merge, `DispInfo::DisplayPixel` |
| else | | `fprintf(stderr, "Unknown register( 0x%x )\n", addr); exit(0);` |

DISPFB's PSM switch is a 20-entry jump table (`psm > 0x13` -> default):
0 -> r32, 1 -> r24, 2 -> r16, 10 -> r16, 19 (PSMT8) -> "PS_GPU2 display mode
is not supported" + `exit(0)`, everything else -> "DISPFB: PSM[%d] is
invalid." then PSM 0 / r32.  Case 0's body is cross-jumped into the tail of
`default`, which is why the standalone SetDISPFB1/2 have only five case
bodies.

## The display pipeline

`DispInfo::DisplayPixel(dn, mem, xif)` is the whole vsync:

```
mode = dn == 0 ? (0 <= CRTMD <= 3 ? 0 : 2)
                : switch (CRTMD) { 1,5 -> 0;  2,6 -> 1;  else 2 }
xif->SetBackground(bg.R, bg.G, bg.B);  xif->ClearDisplay();
mode 0: the merge (below)
mode 1: circuit 2 alone with AOutAlpha, or just Flush() if EN2 == 0
mode 2: SetBackground(0,0,0); ClearDisplay(); Flush()
```

Mode 0, when the two DISPLAY rectangles do **not** overlap: circuit 1 and
then circuit 2 are painted independently, each with `displayNoBlend` or
`displayNoBlendMag` (MAGH/MAGV non-zero), except that circuit 1 goes through
`displayBlendBG` unless `MMOD == 1 && ALP == 0xff` (i.e. unless it is
completely opaque).

Mode 0 when they overlap:

```
EN1 && (!EN2 || (SLBG == 1 && aout1 != 0)) -> circuit 1 alone, as above
EN1 &&  EN2 &&  SLBG == 1 && aout1 == 0    -> displayBlendBGAmod2
EN1 &&  EN2 &&  SLBG != 1                  -> displayBlend
!EN1 && EN2                                -> circuit 2 alone
```

`displayBlend` walks the merged bounding box and per pixel:

| in circuit 1 | in circuit 2 | result |
|---|---|---|
| yes | yes | read both, `bl->blend(c1pix, c2pix)`, alpha from c2 if `aout1 == 0` |
| yes | no  | read c1, `bl->blend(c1pix, bg)`, alpha 0 if `aout1 == 0` |
| no  | yes | read c2 straight out, alpha 0 if `aout2 == 0` |
| no  | no  | the background colour |

`displayBlendBGAmod2` is the same skeleton with circuit 1 always blended
against the *background* rather than against circuit 2, the alpha taken from
circuit 2, and the circuit-2-only case emitting the background colour with
circuit 2's alpha.

Source coordinates are `(x - DX)/(MAGH+1) + DBX` and
`(y - DY)/(MAGV+1) + DBY`; the finished rectangle is handed over as
`xif->DisplayPixel(DX - hstart, DY - VStart(), DW, DH)` where `VStart()` is
`vstart`, halved when `interlace == 1`.

`PixelBlendAlp::blend(d, s)` is `d = (d*alp + (255-alp)*s) >> 8` per
component with `alp` = PMODE.ALP; `PixelBlend1a::blend` uses
`a = min(2*d.A, 255)` instead.  Neither touches alpha.

## What is *not* here

* **No CSR, no IMR, no BUSDIR, no SIGLBLID.**  `GS_PutCtlPort` maps
  `0x120010n0` to `0xc0|n`, and 0xc0..0xcf all land in PCRTCxif::
  SetRegister's `default`, i.e. `"Unknown register( 0xc0 )"` and `exit(0)`.
  There is no `GS_GetCtlPort` in `libgpu2.h` at all, so there is no CSR read
  path and **no revision id anywhere in the model** - the 1998 simulator
  simply does not implement the CSR register, and a program that writes it
  through the documented API kills the process.  (`GPU2::GetCRT` is not a
  CSR read: it walks gpu2.o's `.bss` ring of captured CRT frames.)
* No field/interlace *rendering*: `interlace` only halves `vstart` in the
  final `DisplayPixel` call.  SMODE2.FFMD reaches the object but nothing
  else reads it.
* `PCRTC::ext` is never assigned by anything in the archive, so the whole
  EXTWRITE write-back path (a fifth of PCRTC::SetRegister) is dead code in
  practice.  It is still fully reconstructed and differential-tested.

## How the display is reached

`GS_PutPort(0x7f, data)` (libgpu2.o) rewrites addr 0x7f to **0x100** and,
when the bss `Field` is set, ORs bit 48 into the data - so a vsync from the
documented API drives the *old* unmerged path (`oldDispPixel` /
`oldDispPixelMag`), never the merge.  Register **0x101** (`DisplayPcrtc`,
the real two-circuit merge) is only written by `gpu2reg.o`, the
register-level front end.  `gsreplay -w` therefore exercises
`oldDispPixel*`, not `displayBlend*`.

## Original bugs

1. **`oldDispPixelMag` unpacks 16-bit pixels with six-bit masks.**
   `r = (d & 0x3f) << 3`, `g = ((d>>5) & 0x3f) << 3`, `b = ((d>>10) & 0x3f)
   << 3` - each field takes one bit too many, so red bleeds green's LSB,
   green bleeds blue's, and blue picks up the alpha bit.  Components come
   out up to 504.  (`MemRead16::ReadPixel`, the path everything else uses,
   is correct: 5 bits widened as `(v<<3)|(v>>2)`.)
2. **`oldDispPixelMag`'s replication loops are off by one**: `if (h < oy+i)
   break;` and `if (ox+j > w) break;` allow `oy+i == h` and `ox+j == w`,
   one past the buffer the same function just asked for.
3. **DISPFB's invalid-PSM path poisons the reader.**  The `default` case
   sets `c[n].PSM = 0` and `rd[n] = &r32` but the tail then does
   `rd[n]->Set(psm, ...)` with the *raw* PSM, so the MemRead is left with
   an illegal PSM and the next display aborts inside
   `AddrConv::address_convert` ("illegal <psm> parameter", `exit(1)`).
4. **SRFSH (0x83) is not decoded** and takes the "Unknown register" exit
   with everything else in 0x8f..0xff.
5. `SetSYNCV` adds three fields that do not correspond to any single GS
   field boundary: `VBP + bits 32..41 + bits 53..62`.  Whether that is a
   bug or a deliberate approximation is unknowable from the object; it is
   reproduced exactly.
6. `PCRTCdmy` declares a *third* virtual, `Resize()` with no arguments,
   which hides `PCRTC::Resize(int,int)` rather than overriding it - that is
   why `_vt.8PCRTCdmy` is 0x20 bytes where `_vt.5PCRTC` and
   `_vt.8PCRTCxif` are 0x18.

## Byte-match status

Built with `GCC272_1998=1 tools/gcc272/g++272 -O -idirafter /usr/include
-Iinclude` (pcrtc.c pulls in xif.h, so it needs the same `-idirafter`
treatment `xif` does; `tools/build.sh` needs `xif|pcrtc)` in its `case`).

* `.rodata`: **identical bar two padding bytes** once xif.h's `__FILE__`
  string is fixed (below).  Every string, in order, and all 12 vtables with
  identical contents and relocations.  The two bytes are the 1998
  assembler's `8d 76` code-nop fill before the first vtable; era GAS 2.9.1
  fills with zeroes - the same residual xif.o has.
* **Relocation set identical** (65 `.rodata`, 10 `__assert_fail`, 3
  `realloc`, ...), and the `.text` relocation *order* differs only where a
  function's internal order does.
* **Symbol table identical** - names, bindings, types and order.
* `.note` and `.comment` identical.
* 9 functions byte-identical, 43 total; `.text` 19004 vs 19309 B.

Per-function (with the repo's current xif.h):

| function | orig | ours | |
|---|---|---|---|
| DispInfo::DisplayPixel | 896 | 880 | -16 |
| displayNoBlend | 437 | 413 | -24 |
| displayNoBlendMag | 485 | 461 | -24 |
| displayBlend | 1137 | 984 | -153 |
| displayBlendBGAmod2 | 1073 | 968 | -105 |
| displayBlendBG | 491 | 481 | -10 |
| oldDispPixel | 391 | 359 | -32 |
| oldDispPixelMag | 1418 | 1418 | same size, 236 B differ |
| PCRTCxif::SetRegister | 4682 | 4666 | -16 |
| PCRTCdmy::Resize | 7 | 7 | **exact** |
| PCRTCxif::Resize | 51 | 51 | **exact** |
| PCRTCxif::PCRTCxif(...,func) | 1044 | 1111 | +67 |
| PCRTCxif::PCRTCxif | 922 | 970 | +48 |
| DisplayPcrtc | 72 | 72 | same size, 5 B differ |
| Display | 169 | 166 | -3 |
| SetSYNCV | 93 | 86 | -7 |
| SetSYNCH2 | 7 | 7 | **exact** |
| SetSYNCH1 | 66 | 66 | **exact** |
| SetSMODE2 | 59 | 62 | +3 |
| SetSMODE1 | 78 | 78 | same size, 58 B differ |
| SetBGCOLOR | 54 | 54 | **exact** |
| SetPMODE | 232 | 238 | +6 |
| SetDISPLAY1/2 | 444 | 444 | same size, 52 B differ |
| SetDISPFB1/2 | 377 | 371 | -6 |
| PCRTC::Resize | 7 | 7 | **exact** |
| PCRTC::SetRegister | 1482 | 1466 | -16 |
| MemRead16/24/32::ReadPixel | 266/213/191 | 265/216/193 | -1/+3/+2 |
| PixelBlend1a::blend | 109 | 105 | -4 |
| PixelBlendAlp::blend | 95 | 95 | **exact** |
| XWindowDump::Flush | 33 | 33 | **exact** |
| XWindowDump::SetBackground | 32 | 32 | **exact** |

The nine XWindowDump/Xifbase entries come straight out of the committed
`include/xif.h` and their deltas here are **byte for byte the same as
xif.o's** - they are xif's residual, not pcrtc's.

### Two xif.h fixes this object proves

pcrtc.o and gpu2.o are the only objects that inline `Frame2d`'s and
`XWindowDump`'s constructors, so they are the only evidence for them:

1. `Frame2d::Frame2d(int width, int height)` allocates
   `malloc(height*4*width)` from the **parameters**, not
   `malloc(h*4*w)` from the members it has just assigned: the 1998 object
   folds it to `push $0x40000`, ours reloads `w`.
2. The `__FILE__` in xif.h's assert macro is `"../gpu2u/xif.h"` in pcrtc.o
   (xif.o has `"xif.h"` - the two TUs spelled the `#include` differently).
   Parameterising the string is what makes pcrtc.o's `.rodata` identical.
3. The 1998 `XWindowDump` constructor takes the callback:
   `XWindowDump(void (*f)(int, int, const unsigned int *) = 0)` with a body
   of `{ func = f; bg = 0; }`.  The inlined ctor stores `func =
   <parameter>` *before* `bg = 0`, which a `{ bg = 0; func = 0; }` body
   followed by `dump->func = func` cannot produce; making the change (and
   `new XWindowDump(func)` here) takes the dump constructor from +67 to
   +51.  Note also that the member's type is
   `void (*)(int, int, const unsigned int *)` - the `const` is in
   PCRTCxif's mangled name, `...PFiiPCUi_v`.

Also proven here: `PCRTCxif`'s dump constructor calls `dump->out.Resize(w,
h)` - it sizes the **output** frame, not `draw`; `PrepareImgBuffer` (which
resizes `draw`) would be a virtual call, and the object has none.

### Residual classes

* **Compiler-mod address forms.**  The 1998 object systematically holds
  `&di` (`this+0x40`) and `&di.dc` (`this+0x68`) in registers and addresses
  through them, while stock 2.7.2.3 folds every one into a displacement off
  `this`.  Where the mod materialises the base, loop-invariant motion then
  hoists `&c`, `4*dn` and `this+0x68` into the loop preheaders
  (`displayNoBlend` -24, `oldDispPixel` -32, and most of `displayBlend`'s
  -153 and `displayBlendBGAmod2`'s -105) and the `rd[dn]` load comes out as
  `0x14(4*dn, dcptr)` where ours is `0x7c(this, dn, 4)`.  The same mod
  spills each indirect-call target to its own stack slot before
  `call *%reg` - eight sites in `displayBlend` alone.  All of this is the
  documented `lea 0(,idx,4)` / "force addresses through a value" class.
* **Both constructors, +48 each**: the 1998 object's `over` test in the
  inlined `DispCirc` ctor keeps only the two *Y* comparisons - cse folded
  the two X ones because they read through `this` (the same base the
  initialising stores used) while the Y ones read through the `&dc`
  pseudo, which cse cannot match.  Ours addresses all four through `this`
  and, because the intervening `rd[0]=rd[1]=&r32` pointer stores invalidate
  cse's memory table, folds none.  (Moving `UPDATEMERGE()` in front of the
  `rd[]` stores recovers 23 of the 48 bytes, but the object's store order
  says the 1998 source had it after, so we keep the faithful order.)
* `SetSMODE1`/`SetSMODE2`/`SetPMODE`/`SetDISPLAY*`: same size or +3/+6, the
  whole difference being `0x44(%dip)` vs `0x84(%this)` for the same member
  in one arm of an if/else.
* `oldDispPixelMag`: same size, 236 differing bytes = stack-slot numbering
  plus one `mov (%edi),%eax` reload of `this->addr` (the 1998 object keeps
  the address in `%eax` across the `bitpos` computation, ours does not).
* `PixelBlend1a::blend` -4: the 1998 object leaves
  `if (t <= 0xfe) a = t; else a = 0xff;` as a real if/else with a `jmp`;
  our jump optimiser hoists the else-arm's constant store above the branch.

## Compiler lessons (new)

1. **An inlined call evaluates its arguments into pseudos before the body
   runs, and the argument order that comes out is g, b, r for a three-int
   call.**  Three plain assignments `c.R = ...; c.G = ...; c.B = ...` give
   load/store, load/store, load/store; an inline `SetRGB(PixColor &c, int
   r, int g, int b)` gives *three loads and then three stores*, with the
   loads in the order G, B, R.  Both `SetBGCOLOR` and
   `displayBlendBGAmod2` need it, and it is also what turns
   `MemRead32::ReadPixel`'s `(d>>8)&0xff` into a `movzbl 1(...)` byte load:
   the argument pseudos keep the three extractions separate long enough for
   combine to fold each into the memory operand.  `SetBGCOLOR` and
   `MemRead32::ReadPixel` are byte-exact only with it.
2. **Passing a bitfield through an inline function stops `fold` folding
   the shift back into the mask.**  `((d>>5) & 0x1f) << 3` becomes
   `(d>>2) & 0xf8`; `ext5((d>>5) & 0x1f)` with
   `inline int ext5(unsigned v) { return (v<<3)|(v>>2); }` keeps the mask
   and the shift apart, which is what MemRead16 has.  (The `unsigned`
   parameter is what makes `v>>2` a `shr`.)
3. **A predicate that is *materialised then tested* came from a function,
   not from an `if`.**  `if (a || b)` branches directly; `if (F())` with
   `F` inline goes through `preexpand_calls`, which expands the call into a
   pseudo first, so the object shows `xor %eax,%eax / ... / mov $1,%eax /
   test %eax,%eax / jne`.  Three sites in `DisplayPixel` need it
   (`dc.Magnified(n)`, `NeedBlend()`), and `PCRTC::SetRegister`'s size
   check needs the same shape with a `char` variable (`xor %al,%al`).
4. **The same `preexpand_calls` makes an inline call in an argument
   expression evaluate *first*.**  `dy - VStart()` loads `vstart` before
   `dy`; the spelling `dy - (interlace == 1 ? vstart/2 : vstart)` is *not*
   equivalent, because `fold` distributes the subtraction into both arms of
   a COND_EXPR operand and the object has one subtraction, not two.
   `VStart()` has to be a function; a macro does not work.
5. **`INTEGRATE_THRESHOLD` decides function vs macro.**  `UpdateMerge` is
   ~70 insns and g++ 2.7 refuses to inline it, leaving a symbol the 1998
   object does not have; as a macro it expands in all six places the
   original has it.  `SetDISPFB`/`SetDISPLAY` (~110 insns each) *are*
   inlined, so they stay functions - the threshold is
   `8 * (8 + number of arguments)` and two arguments buy enough room.
6. **A store through a pointer invalidates cse's whole memory table for the
   purposes of constant propagation** even when the two addresses are
   provably distinct offsets off the same base: `rd[0] = &r32` between the
   constant initialisation of `c[]` and the comparison chain that reads it
   is what stops the 48 bytes of dead comparison from folding.
7. `PCRTCxif : public PCRTC` with `DispInfo di;` as a **member**, not a
   second base: with multiple inheritance g++ 2.7 stores the derived vptr
   after *all* base constructors, and the 1998 ctor stores it between the
   PCRTC part and the DispInfo part.  The layout is identical either way;
   the vptr store's position is the only evidence, and it is decisive.
