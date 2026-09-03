# libgpu2.o — the public API layer

Reconstruction notes for `orig/lib/libgpu2.o` → `src/libgpu2.c`.
Read 2026-09-02.  Nine functions, 3350 bytes of `.text`, 0x24 of `.bss`,
0x15 of `.rodata`, no `.data`, **no `.ctors`**.

## Build recipe (this object is the odd one out)

    GCC272_ALT=rh42-2721 tools/gcc272/g++272 -O2 -m386 -Iinclude -c src/libgpu2.c

Three things differ from the rest of the archive and all three were found
by byte-diffing, not assumed:

| | libgpu2.o | the other 22 objects |
|---|---|---|
| compiler | **gcc 2.7.2.1** (`.comment`) | gcc 2.7.2.3 |
| tuning | **-m386**: `leave` epilogues, `.align 4` | -m486: `mov %ebp,%esp`, `.align 16` |
| optimisation | **-O2** | -O (addrconv, per doc/MATCHING.md) |

`-m386` is forced by the epilogue and the inter-function padding: the very
first function, `GS_InitSim`, matches byte-for-byte with it and cannot
without it.  `-O2` is forced by `GS_PutCtlPort`, which is a 325-byte
mismatch at `-O` and an exact match at `-O2` (at `-O` the `reg` local keeps
a register; the object spills it, which is what `-O2` does here).  Flag
toggles that were tried and rejected: `-fno-caller-saves`,
`-fno-expensive-optimizations`, `-fno-strength-reduce`,
`-fno-cse-follow-jumps`, `-fno-rerun-cse-after-loop`, `-fschedule-insns`,
`-fno-force-mem`, `-fno-thread-jumps`, `-fno-defer-pop`, `-O3`,
`-O -fexpensive-optimizations`.  `-O2` alone is the best of all of them.

The 2.7.2.1 C++ compiler is `tools/gcc272/alt/rh42-2721` (Red Hat 4.2,
i386-linux, libc5).  Its `.comment` string matches the 1998 object exactly,
so the comparison is against the actual compiler, not a near relative.

### The 2.7.2.1-vs-2.7.2.3 caveat is empirically nil

Worth stating plainly, because it was the stated risk going in: at
`-O2 -m386` the two compilers emit **byte-identical `.text`, `.rodata` and
`.bss`** for this source.  Only the `.comment` string differs.
`test/run_libgpu2.sh` (2.7.2.3) and `test/run_libgpu2_721.sh` (2.7.2.1)
report the same residual, function for function, byte for byte.

So **none of the residual below is minor-version noise** — the version
difference contributes exactly zero bytes, and the remaining gap is source
shape or the modified-gcc factor of `doc/compiler.md`.  A practical
consequence for hybrid builds: `tools/build.sh` can use the ordinary
`g++272`, but it must be told `-O2 -m386` (its default is `-O`), and the
resulting object will differ from the 1998 one in `.comment` only.

## Status

`test/run_libgpu2_721.sh` (add `-d`, or `-f <func>`, for disassembly).

| | size | result |
|---|---|---|
| `.comment` | 0x14 | **identical** (`GCC: (GNU) 2.7.2.1`) |
| `.note` | 0x14 | identical |
| `.rodata` | 0x15 | **identical** (`"w"`, `"Can not open %s!!\n"`) |
| `.bss` | 0x24 | **identical**, same three local symbols at the same offsets |
| `.data` / `.ctors` | 0 / absent | identical |
| relocations | 195 | **identical**, same types and targets in the same order |
| symbol table | 9 functions | identical bindings and names; two sizes differ |

| function | bytes | result |
|---|---|---|
| `GS_InitSim` | 85 | **exact** |
| `GS_OpenSim` | 1024 | **exact** apart from 1 byte: the `call initPCRTC` displacement, which moves only because GS_SaveImage (between it and initPCRTC) is 16 B smaller |
| `GS_CloseSim` | 16 | **exact** |
| `GS_PutPort` | 68 | **exact** |
| `GS_PutCtlPort` | 354 | **exact** |
| `GS_SetSaveImageArea` | 63 | **exact** |
| `GS_GetSaveImageArea` | 63 | **exact** |
| `GS_SaveImage` | 1209 → 1193 | register allocation only (below) |
| `initPCRTC__Fv` | 458 | **exact** since the gpu2 drop — see "closed" below |

`test/run_libgpu2_diff.sh` links the 1998 object and ours into one binary
with the entry points renamed old_*/new_* — their bss globals are *local*
symbols, so the two implementations keep independent state — stubs GPU2 and
compares the `GPU2::Put` trace, ctor arguments, new/delete counts, return
values, save-area state and the `GS_SaveImage` output bytes:

    39710 checks, 0 failures

## The two residuals (initPCRTC's since closed)

Both are register/spill-slot allocation, with the same signature: the
original keeps the running DImode accumulator of an `|`-chain in a register
pair and spills the individual terms; ours does the opposite.

**`GS_SaveImage`** — 379 insns vs 376, and the *mnemonic sequence* differs
on 9 lines: one `lea 0x0(,%esi,8)` became `shl $0x3` (both are `x*8`), four
`mov` fewer (the original spills all four 16-bit pixel CSE copies, ours
keeps two in `%ebx`/`%ecx`), one extra alignment `nop`.  Ignoring stack-slot
numbering, 109 of 379 lines differ, all of them register names.

**`initPCRTC`** — 155 insns vs 193.  Ours emits 11 `cltd` where the original
emits 11 `sar $0x1f` (a different `extendsidi2` alternative) and 37 extra
`mov`.  Mechanism: on i386 a DImode pseudo needs a *consecutive* hard-reg
pair — (ax,dx), (dx,cx), (cx,bx), (bx,si), (si,di).  `extendsidi2`'s
cheapest alternative is `"=A"` (ax:dx, i.e. `cltd`), so every
`(long long)fb.field` term claims ax:dx; with the shifted term in bx:si that
leaves no free pair for the accumulator and it goes to memory.  The 1998
object allocates the accumulator dx:cx *first*, which pushes the extensions
onto the `?*r,r` alternative.  Confirmed to be an allocation outcome, not
arithmetic: writing the chain with `+` instead of `|` flips the allocation
and lands within 10 bytes (468 vs 458) — but the object plainly uses `or`,
so that is not the source.

Rejected as the cause, by experiment: `|=` accumulation (573), right
associativity (525), splitting the first term out (537), dropping the `data`
variable (537), `register` on any local (537), declaration order (537),
`unsigned long long` (537), extra `long long` temp (537), inlining `magh-1`
(533).  Every `-O`-level and flag combination listed above.

**Closed (2026-09-03, with the gpu2 drop): the cause was outside this TU.**
The `Put(reg, data)` calls are `GPU2::Put`, and the 1998 gpu2.o sets
`%eax = 1` on the normal path — the method's true type is `int
GPU2::Put(int, long long)`.  Once include/gpu2.h declares that return
type, every call insn in initPCRTC clobbers a value register, the DImode
accumulator claims dx:cx first, and **initPCRTC comes out byte-identical
at its 1998 size (458 B)** — none of the intra-TU spellings above could
ever have produced that.  GS_SaveImage's allocation residual remains,
now the file's only one (8/9 functions exact).

## Resolved: where `field` goes

`doc/STRUCTS.md` asked how `GS_OpenSim(title,w,h,disp_on,field)` reaches a
constructor mangled `__4GPU2Pciii` (four arguments).  It does not:

    GPU2::GPU2(char *title, int width, int height, int disp_on)

is called with exactly those four, and `field` is stashed in the bss
`Field` immediately before `initPCRTC()`.  `Field` is read by `initPCRTC`
(MAGV / the MAGH arm), by `GS_PutPort` (the 0x7f data bit) and by
`GS_SaveImage` (height halving).  It never leaves the file.
`api.txt` independently confirms `GPU2::GPU2(char*, int, int, int)`.

## bss map (names are the object's own local symbols)

| offset | size | name | meaning |
|---|---|---|---|
| 0x00 | 4 | `gpu2` | the `GPU2*` singleton, `new(0x18)` |
| 0x04 | 4 | `fb.fbp` | save area base pointer |
| 0x08 | 4 | `fb.fbw` | buffer width |
| 0x0c | 4 | `fb.psm` | pixel format — the one `GS_SaveImage` switches on |
| 0x10 | 4 | `fb.posx` | |
| 0x14 | 4 | `fb.posy` | |
| 0x18 | 4 | `fb.width` | |
| 0x1c | 4 | `fb.height` | |
| 0x20 | 4 | `Field` | interlace flag from `GS_OpenSim`'s 5th argument |

0x04..0x1c is one `FRAME_BUFFER`; `GS_Set/GetSaveImageArea` copy it field by
field (not `memcpy`) in declaration order, which is how the field order in
the shipped header is confirmed against the object.

## GS_InitSim defaults

`Field = 0`, then `fbp = 0, fbw = 10, psm = 0, width = 640, height = 480,
posx = 0, posy = 0` — in that store order, which is the source order.

## GS_OpenSim power-on table

44 registers written with 0, then six PCRTC registers, then `Field = field`
and `initPCRTC()`.  In program order:

    0x1a 0x1b 0x0a 0x01 0x02 0x03 0x18 0x19 0x22 0x06 0x07
    0x14 0x15 0x17 0x17 0x1c 0x34 0x35 0x36 0x37 0x3b 0x08
    0x09 0x3d 0x40 0x41 0x47 0x48 0x42 0x43 0x49 0x44 0x45
    0x46 0x4a 0x4b 0x4c 0x4d 0x4e 0x4f 0x50 0x51 0x52 0x53   all = 0

    0x80  PMODE     0x0003fd00_00010000
    0x87  DISPFB1   0x00000000_00004000     FBP 0, FBW 32, PSM 0
    0x88  DISPLAY1  0x00100180_00040080     DX 128, DY 64, DW 384, DH 256
    0x89  DISPFB2   0x00000000_00014080     FBP 128, FBW 32, PSM 2
    0x8a  DISPLAY2  0x00100180_00080100     DX 256, DY 128, DW 384, DH 256
    0x8e  BGCOLOR   0

### The 0x16/0x17 anomaly: CONFIRMED

`doc/STRUCTS.md` flagged this as needing verification.  It is real.  The
sequence is `0x14, 0x15, 0x17, 0x17` — **register 0x17 (TEX2_2) is written
twice and 0x16 (TEX2_1) never**.  Given the `_1`/`_2` context-pair pattern
of every neighbour in the table (0x14/0x15 TEX1, 0x18/0x19 XYOFFSET,
0x06/0x07 TEX0, 0x08/0x09 CLAMP …), the intent was clearly `0x16, 0x17`.
An original typo.  Reproduced verbatim; the two writes are byte-identical
call sites in the object, so there is no reading under which they differ.

### Why the three 64-bit constants go through `int` halves

`GS_OpenSim` materialises the three defaults whose *high* half is non-zero
(0x80, 0x88, 0x8a) into `%edx`/`%ecx` and pushes the registers, while the
ones that fit in 32 bits (0x87, 0x89, and all 44 zeros) push immediates.
Any constant-folded expression — a `long long` literal, a shift/or of
literals, a `static const`, a comma or ternary, `(long long)(double)`, an
`inline` helper's result — pushes immediates in *both* cases; verified
against the 2.7.2.1 binary.  The register form appears only when the two
halves are non-constant trees at expand time, because gcc 2.7's CSE re-folds
them into the pseudo but will not substitute a DImode `CONST_DOUBLE` back
into a push (it *does* substitute a `CONST_INT`, which is exactly why the
small ones still push immediates).

An `inline` helper reproduces the instruction shape but not the code around
it: `expand_call` does a `do_pending_stack_adjust()` before
`expand_inline_function`, which injects an `add $N,%esp` the 1998 object
does not have — its stack-pop bunching is perfectly regular at every second
call throughout the function.  Block-scoped `int hi, lo` reproduces both.
The *shape* is pinned by bytes; the spelling (variable names, whether the
braces were a macro in Sony's source) is a guess.

## GS_PutCtlPort

Recognised window: `0x12000000..0x120000c0`, plus exactly `0x12001000`,
`0x12001010`, `0x12001040`, `0x12001080`, `0x12001090`, `0x120010f0`.
The range test is unsigned (`lea -0x12000000(%ecx),%edx; cmp $0xc0,%edx;
jbe`).  Mapping: `reg = ((addr>>4) & 0x7f) | (addr & 0x1000 ? 0xc0 : 0x80)`,
so `0x120000n0 → 0x80|n` and `0x120010n0 → 0xc0|n`.  Returns 1 if
recognised, 0 otherwise — the object settles the header discrepancy noted in
FINDINGS: **`int`, not `void`**.

Two writes are snooped into the save area (two sequential `if`s, not
`else if`):

    reg 0x87 (DISPFB1): fbp = data & 0xff          ← 8 bits, field is 9
                        fbw = (data>>9)  & 0x3f
                        psm = (data>>15) & 0xf     ← 4 bits, field is 5
                        posx = (data>>32) & 0x7ff
                        posy = (data>>43) & 0x7ff
    reg 0x88 (DISPLAY1): width  = ((data>>32) & 0xfff) / (((data>>23) & 0xf) + 1)
                         height = (data>>44) & 0xfff

The two narrow masks are the object's, not a transcription slip (`movzbl`
and `and $0xf`).  `width` is `DW/(MAGH+1)`, and it is a genuine 64-bit
signed divide — `__divdi3` is the only libgcc reference in the file.

## GS_PutPort

    if (addr == 0x7f) { addr = 0x100; if (Field) data |= 1LL<<48; }
    gpu2->Put(addr, data);

0x100 is > 0xff, so `GPU2::Put` routes it to `PCRTC::SetRegister` rather
than down the drawing pipe; bit 48 is presumably the field select.

## initPCRTC

    magh = 0xa00 / fb.width;            /* 2560 = full-screen VCK width */

    Put(0x87, fbp | fbw<<9 | psm<<15 | posx<<32 | posy<<43)

which pins the DISPFB field layout: FBP 0-8, FBW 9-14, PSM 15-19,
DBX 32-42, DBY 43-53.

    Field != 0:  Put(0x88, (magh*width)<<32 | height<<44 | Field<<27 | (magh-1)<<23)
    Field == 0:  Put(0x88, (magh*width)<<32 | height<<44 | 0x2000000)

i.e. DW = magh*width, DH = height, MAGV = Field, MAGH = magh-1 — except in
the non-field arm, where MAGH is the **literal 4** (`0x2000000 == 4<<23`)
instead of `magh-1`, and MAGV is left 0.  For the default 640-wide buffer
magh is 4 and magh-1 is 3, so the two arms genuinely disagree.  DX and DY
are never written.  Reproduced as-is; whether the literal is a third bug or
deliberate is not decidable from this object alone.

`magh-1` is computed before the `Put(0x87)` call, not inside the `if`, so
it is a second source variable — gcc 2.7 has no code hoisting.

## GS_SaveImage — both paths

Programs a local→host BitBLT and dumps raw bytes.  `h` is `fb.height/2`
when `Field` is set (signed halving: `shr $31; add; sar $1`), else
`fb.height`.

    Put(0x50 BITBLTBUF, (fbp<<5) | fbw<<16 | (psm<<24 unless psm==1))
    Put(0x51 TRXPOS,    posx<<16 | posy)
    Put(0x52 TRXREG,    h<<32 | width)
    Put(0x53 TRXDIR,    1)                 /* local -> host */

**psm 0 and 1** — the loop guard is `(unsigned)psm <= 1`, which pins the
source spelling as `psm == 0 || psm == 1` (an `int` `<= 1` would emit
`jle`; the object has `ja`).  `h*width/2` iterations, one `GPU2::Get` each,
two 32-bit pixels per 64-bit read:

    p = full 32-bit byteswap of the pixel        /* 4-term, incl. >>24 */
    fwrite(&p, 1, 3, fp)                          /* only 3 bytes! */

An ABGR word byteswaps to the bytes A,B,G,R and only A,B,G are written.
Red is dropped and every pixel is shifted one byte — the documented
"3 of 4 bytes" bug.  `orig/lib/libgpu2-patched.a` is this object with the
`3` changed to a `4`.

Note `psm == 1` (PSMCT24) deliberately leaves SPSM at 0 in BITBLTBUF, so
24-bit data is read back as 32-bit words; the 3-byte `fwrite` is then the
right *length*, though still the wrong three bytes.

**psm 2** — `h*width/4` iterations, four 16-bit pixels per 64-bit read.
Each 5-5-5 pixel is expanded and then byteswapped with a **three**-term
expression (no `>>24`; the top byte is known zero):

    v = (x & 0x7c00)<<9 | (x & 0x3e0)<<6 | (x & 0x1f)<<3     /* 0x00RRGGBB */
    v = v<<24 | (v & 0xff00)<<8 | (v & 0xff0000)>>8          /* 0xBBGGRR00 */
    fwrite(&v, 1, 3, fp)                                     /* 00, RR, GG */

So the 16-bit path writes a leading zero byte, then red and green, per
pixel.  Same one-byte shift, different flavour.

Any other `psm` runs neither loop: the file is opened, the four registers
are programmed, and an empty file is closed.  Return is 1 on success, 0 if
`fopen` fails (after `fprintf(stderr, "Can not open %s!!\n", filename)` —
the object references glibc 2.0's `_IO_stderr_`).

**TRXPOS is swapped.**  TRXPOS is SSAX 0-10, SSAY 16-26, so
`posx<<16 | posy` puts *posy* in SSAX and *posx* in SSAY.  Invisible while
the save area starts at 0,0, which is presumably why it survived.  Both
orderings were checked against the disassembly; the object evaluates
`posx<<16` first and ORs `posy` into the low half.

## Loose ends

- Whether the `{ int hi, lo; }` blocks in `GS_OpenSim` were a macro in
  Sony's source is unknowable from the object; only the tree shape is.
- The DISPLAY1 literal `4<<23` in the non-field arm of `initPCRTC`.
- The two remaining functions' allocation residual — see above; it is the
  only thing standing between this object and a full byte match, and it
  costs `GS_OpenSim` its last byte too.
