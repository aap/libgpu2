# gpu2vec.o — Sony's pipeline-tap / RTL-test-vector layer

`orig/lib/gpu2vec.o` (0x8afa `.text`, 26 own functions + 21 header
inlines) is a **parallel top level to GPU2**.  It builds the same
pipeline out of instrumented subclasses and, on demand, writes one
line of test vectors per stage event to a `FILE *` before forwarding to
the real stage code.  Nothing in the archive references it: the
`gsreplay` link pulls `gpu2.o`, never this.  It is Sony's own bridge
between the C model and the Verilog testbench, and it is the closest
thing in the archive to a specification of what each pipeline stage
receives.

Reconstruction: `src/gpu2vec.c` + `include/gpu2vec.h`.  Build:

    env GCC272_1998=1 tools/gcc272/g++272 -O -idirafter /usr/include \
        -Iinclude -c src/gpu2vec.c

Byte accounting and the residual classes: `doc/notes/gpu2vec-matching.md`.
Differential: `test/run_gpu2vec.sh` (17.28M checks, 0 failures, 13.9 MB
of vector-file bytes compared exactly).

---

## 1. The object graph

`GPU2VEC` has the same six pipeline slots as `GPU2` plus two words of
vector state:

| off | member | |
|---|---|---|
| 0x00 | `MyPP *pp` | Pre1/Pre3/PCalc/PPOut block + the register tap |
| 0x04 | `MyDDA *dda` | |
| 0x08 | `MyTXM *txm` | |
| 0x0c | `MyMemIF *memif` | |
| 0x10 | `MyMemory *mem` | |
| 0x14 | `PCRTC *pcrtc` | `PCRTCdmy` or `PCRTCxif`, exactly as GPU2 |
| 0x18 | `int vec` | which tap `SetVector` last selected |
| 0x1c | `FILE *fp` | the raw-register tap of mode 6 |

The constructor `GPU2VEC(char *title, int w, int h, int disp_on)` news,
in order (sizes read off the 1998 object):

    MyMemory 0x4001d0   MyMemIF 0x100   MyTXM 0x730   MyDDA 0x258
    MyPP 0x14  ->  PPOut 0x8, PCalc 0xc00, Pre3 0x14c, Pre1 0xac
    then the display arm:  PCRTCdmy 0x40      (disp_on 0)
                           PCRTCxif 0x1d4     (disp_on 1)
                           PCRTCxif 0x1d4 + XWindowDump 0x24 (disp_on 2)
                           fprintf(stderr, "invalid argument xdisp -- [%d]\n")
                           + exit(1)          (anything else)

`MyPP`'s constructor takes the `MyDDA *` and builds the front end
itself (`new PPOut(d)`, `new PCalc(out)`, `new Pre3(pcalc)`,
`new Pre1(pre3)`), where `GPU2::GPU2` inlines the same four `new`s
straight into its own body.  That is the only structural difference
between the two top levels.

Each instrumented stage adds exactly one word behind the base object:

| class | base | tap slot |
|---|---|---|
| `MyDDA : DDA` | 0x250 | `FILE *fp` at 0x254 (vptr 0x250) |
| `MyTXM : TXM` | 0x72c | `fp` at 0x72c |
| `MyMemIF : MemIF` | 0xf8 | `fp` at 0xfc (vptr 0xf8) |
| `MyMemory : Memory` | 0x4001c8 | `fp` at 0x4001c8, **vptr at 0x4001cc** |
| `MyPP` | — | `fp` at 0x10 |

`Memory` has no virtuals of its own, so `MyMemory`'s (inline, virtual)
destructor puts the vptr *after* the tap word — the g++ 2.7 "vptr last
when the derived class introduces it" layout.  It is also why
`_vt.8MyMemory` is local and `_._8MyMemory` weak, while `MyDDA`,
`MyTXM` and `MyMemIF` — whose `Put`/`Stamp` overrides are defined out
of line in the .c — are key-method classes whose vtables *and* all
their inline members (`SetVector`, ctor, dtor) come out global.

`~My*()` closes the tap: `if (fp) fclose(fp);`.

### Original bugs

* **`vec` and `fp` are never initialised by the constructor.**  Until
  the first `SetVector` call, `GPU2VEC::Put`'s `if (vec == 6)` reads
  uninitialised heap.  (Reproduced faithfully; the differential feeds
  both sides the same garbage.)
* `MyMemory::Dump` always walks the full 0x100000 words of vram,
  ignoring the frame buffer's actual size.

---

## 2. The public surface

```c
GPU2VEC(char *title, int width, int height, int disp_on);
unsigned int GetCRT();               /* one word of the captured frame */
long long    Get();                  /* mem->bitblt.ReadPixel(mem)     */
int          Put(int addr, long long data);
void         ResizeWindow(int w, int h);
void         SetVector(int sel, FILE *f);
```

`Put` is `GPU2::Put` plus two taps:

```c
if (vec == 6)
        fprintf(fp, "%02x %08x %08x\n", addr, (int)(data >> 32), (int)data);
if ((signed char)addr < 0 || addr > 0xff)
        pcrtc->SetRegister(addr, data);
else {
        if (addr == 0x7f)              /* GS_FIELD: dump vram */
                mem->Dump();
        pp->Put(addr, data);
}
return 1;
```

So writing register **0x7f** (`GS_FIELD`) is the trigger that makes the
memory tap emit a frame; the write itself still goes down the pipe.

`GetCRT`/`dumpCRT`/`r_buf`/`r_count`/`r_size` are the same frame-capture
pair as `gpu2.o`'s, byte for byte.

### `SetVector(sel, FILE *f)` — the tap selector

```
sel 1 -> pp->SetVector(f)      MyPP::fp     register writes into Pre1
sel 2 -> dda->SetVector(f)     MyDDA::fp    PCalc -> DDA vectors
sel 3 -> txm->SetVector(f)     MyTXM::fp    DDA -> TXM vectors
sel 4 -> memif->SetVector(f)   MyMemIF::fp  TXM -> MemIF vectors
sel 5 -> mem->SetVector(f)     MyMemory::fp vram dumps
sel 6 -> this->fp = f          the raw GPU2VEC::Put trace
other -> nothing but `vec = sel`
```

`vec = sel` is assigned for **every** value of `sel`, including the ones
that install no file (a `jmp` to the common tail; the switch is a jump
table over `sel-1` in `0..5`).  Only mode 6 is gated on `vec`; modes
1..5 are gated on the stage's own `fp` being non-null, so several taps
can be live at once — `SetVector` never clears the previous one.

---

## 3. The file format

Every record is one line.  A line is a sequence of **columns**; every
column is terminated by `'_'`, and the line ends with `'\n'` (written
by a separate `fprintf(fp, "\n")`, so a record always ends `_` then
newline).  A column is one of

* a **value column**: the field, masked, in zero-padded lower-case hex;
* a **don't-care column**: a run of Verilog-style `'x'` digits;
* a **forced-zero column** (TXM tap only): a value column wired to 0.

Column *widths* are given in bits and a `w`-bit column is `(w+3)/4`
hex digits — i.e. the Verilog vector width rounded up to whole nibbles.
The six writers in `src/gpu2vec.c` are the whole machine:

| writer | argument | emits |
|---|---|---|
| `Field(fp, data, mask, width)` | bits | `sprintf("%0<n>x_")` of `data & mask`, `n = (width+3)/4` |
| `FieldLL(fp, data, mask, width)` | bits | `"%0<n-8>x%08x_"` of `(int)(data>>32) & mask` then `(int)data` |
| `ZField(fp, width)` | bits | the same value column, wired to 0 |
| `PutX(fp, n)` | **digits** | `n` `'x'` then `'_'` |
| `XField(fp, width)` | bits | `(width+3)>>2` `'x'` then `'_'` |
| `XLine(fp, "23 23 30 …")` | bits | one don't-care column per width in the list |

Note the asymmetry, which is load-bearing when reading the object:
`PutX` takes a **digit count**, everything else takes a **bit width**.
`XLine`'s argument is the same whitespace-separated width list a
testbench-side reader would use to split the record; it is parsed with
`sscanf("%d")` and a manual scan to the next space, and stops at the
first token that does not parse.

The mask is always `(1 << width) - 1` for `Field` and
`(1 << (width-32)) - 1` for `FieldLL` — the masks are literal arguments
in the source, but every one of the 250-odd call sites obeys that rule,
so the mask can be read as "the field's own width".

---

## 4. The five records

Each stage's register-write record and primitive record have **the same
column layout and the same column count**: a register write is the
primitive line with the address/data overlaid on a couple of columns
and every other column x-ed out.  A reader can therefore split any line
from a tap with one fixed width list.

### 4.1 `sel 6` — the raw register trace (`GPU2VEC::Put`)

```
%02x %08x %08x\n     addr, data[63:32], data[31:0]
```
Space separated, not `_` separated: this one is a debug log, not a
vector file.  Written for every `Put`, before routing, with the address
unmasked (so privileged writes ≥ 0x100 and negative addresses appear
as-is, `%02x` widening past two digits).

### 4.2 `sel 1` — the Pre1 input (`MyPP::Put`)

```
%02x %08x %08x\n     addr & 0x7f, data[63:32], data[31:0]
```
Same shape, but only the drawing-register address space reaches it and
the address is masked to 7 bits.  Fires before `pre1->Put`.

### 4.3 `sel 2` — the DDA input (`MyDDA::Put`, 69 columns)

Fires on every `PCalc -> DDA` handoff, before `DDA::Put`.
`p->send_type` selects the arm:

* `send_type != 0` → `RegisterVec(p)`
* `send_type == 0` → `TriangleVec(p)` (all primitive types, not just
  triangles; `p->type` 0..3 = point/line/triangle/sprite selects the
  coverage arms inside).

The 69 columns, in order, with the widths `TriangleVec` uses:

| # | width | field | register record |
|---|---|---|---|
| 1-3 | 23, 23, 31 | `m_abc`, `m_ac0`, `m_ac4` | x (`23 23 30`) |
| 4-9 | 16,17,17,17,16,17 | `ddx[0..2]`, `ddy[0..2]` | x (`17`×6) |
| 10-14 | 14 ×5 | `bbl`, `bbt`, `bbr`, `bbb`, `m_af0` | x (`13`×5) |
| 15-16 | 10, 10 | `ddax`, `dday` | `ddax` at width **11**, then x (`11`) |
| 17-25 | 17,17,17,18,18,18,18,18,18 | `covs[0..2]`, `covdx[0..2]`, `covdy[0..2]` — only when `AA1`, else `XLine "17 17 17 18 18 18 18 18 18"`; the third of each triple is `PutX(5)` for `type == 1` (lines) | x (`19`×9) |
| 26 | 43 | `ozv` (`FieldLL`, mask 0x7ff) | `ozv` at width **44**, mask 0xfff |
| 27 | 19 | `ofv` if `FGE`, else `PutX(5)` | `ofv` at width **20** |
| 28-31 | 19 ×4 | `oav`, `orv`, `ogv`, `obv` | same at width **20** |
| 32-34 | 28 ×3 | `osv`, `otv`, `oqv` when `m_bf4 == 0 && (TME & 1)`, else x | x (`28 28 28`) |
| 35-43 | 44,20×5,28×3 | the d/dx block: `ozdx`, `ofdx` (gated on `FGE`), `oadx`, `ordx`, `ogdx`, `obdx`, then `osdx`/`otdx`/`oqdx` when `TME && ((m_bf4 ^ 1) & 1)` | x (`44 20 20 20 20 20 28 28 28`) |
| 44-52 | 44,20×5,28×3 | the d/dy block, same shape; the STQ triple is gated on `TME && !(m_bf4 & 2)` | x (same list) |
| 53-55 | 1 ×3 | `TME`, `FGE`, `ABE` | value |
| 56 | 1 | `AA1` | x (`1`) |
| 57-59 | 1 ×3 | `FST`, `CTXT`, `send_type` | value |
| 60-61 | 1, 1 | `xdir`, `ydir` | x (`1 1 1 1 1 1 1`) |
| 62 | 2 | `SCANMSK` | ″ |
| 63-65 | 1 ×3 | `steep[0..2]` when `AA1` (third is `PutX(1)` for lines), else `XLine "2 2 2"` | ″ |
| 66 | 1 | `flat` | ″ |
| 67 | 7 | x (`XLine "7 64"`) | `send_addr`, mask 0x7f |
| 68 | 64 | x (″) | `send_reg` (`FieldLL`, mask 0xffffffff) |
| 69 | 8 | `maxexp`, or `PutX(2)` when `type == 0 \|\| TME == 0` | x (`8`) |

Two width inconsistencies in Sony's own source, reproduced exactly:
the register record's x-runs say 30 where the value column is 31 bits
(same 8 digits, harmless) and **17 where `ddx[0]`/`ddy[1]` are 16 bits**
— 5 `x` against 4 hex digits, so a register line and a triangle line
from the DDA tap are *not* the same length.  `ddax` also changes width
(11 in the register record, 10 in the triangle record) though both are
3 digits.

### 4.4 `sel 3` — the TXM input (`MyTXM::Put`, 50 columns)

Fires on every `DDA -> TXM` handoff, before `TXM::Put`; `d->isreg`
selects `RegisterVec` / `PrimitiveVec`.  Column layout:

| # | width | field | register record |
|---|---|---|---|
| 1 | 1 | `en` — the literal 1 | 1 |
| 2 | 1 | `isreg` | `isreg` |
| 3 | 1 | `first` | `PutX(1)` |
| 4 | 11 | `px` | `px` (the register address rides in `px`) |
| 5 | 11 | `py` | `PutX(3)` |
| 6 | 1 | `ydir` | `PutX(1)` |
| 7 | 16 | `mask` | `mask` (the register data's low half rides in `mask`) |
| 8-13 | 1 ×6 | `TME`, `FGE`, `ABE`, `FST`, `AA1`, `CTXT` | value |
| 14-17 | 14 ×4 | `a0`, `b0`, `g0`, `r0` | value |
| 18-25 | 14 ×8 | `a1`, `b1`, `g1`, `r1`, `dadx`, `dbdx`, `dgdx`, `drdx` | `PutX(4)` ×8 |
| 26 | 38 | `z0` (`FieldLL`, mask 0x3f) | `z0` |
| 27-28 | 38, 40 | `z1`, `dzdx` | `PutX(10)` ×2 |
| 29 | 2 | `zc` | `PutX(1)` |
| 30-38 | 28 ×9 | `s0 t0 q0 s1 t1 q1 dsdx dtdx dqdx` when `TME`, else **nine `ZField(28)`** (forced zeros, not x) | `s0`, `t0`, `PutX(7)`, `s1`, `t1`, `PutX(7)` ×4 |
| 39-41 | 14 ×3 | `f0`, `f1`, `dfdx` when `FGE`, else three `ZField(14)` | `f0`, `PutX(4)` ×2 |
| 42-48 | 12 ×7 | `cova0 covb0 cova1 covb1 covdxa covdxb0 covdxb1` when `AA1`, else `PutX(3)` ×7 | `PutX(3)` ×7 |
| 49 | 8 | `amask` | `amask` |
| 50 | 8 | `maxexp` | `PutX(2)` |

The forced-zero columns are the tell that the RTL block *wires* those
inputs to 0 when TME/FGE are off, where the AA coverage inputs are
merely don't-care.

### 4.5 `sel 4` — the MemIF input (`MyMemIF::Stamp`, 40 columns)

Fires on every `TXM -> MemIF` stamp, before `MemIF::Stamp`; `s.type`
selects the arm.

`PrimitiveVec`:

| # | width | field |
|---|---|---|
| 1 | 1 | `en` (literal 1) |
| 2 | 1 | `isreg` (literal 0) |
| 3 | 1 | `s.ctxt` |
| 4 | 11 | `s.pos.x` |
| 5 | 11 | `s.pos.y` (the row *pair* index) |
| 6-21 | 32 ×16 | pixel colour, `A<<24 \| B<<16 \| G<<8 \| R`, for every `i` with `livemask` bit `i` set; `XField(32)` otherwise |
| 22-37 | 32 ×16 | `s.pix[i].z`, same gating |
| 38 | 16 | `combmask` = `s.mask \| (s.aamask << 4)` |
| 39 | 1 | `s.ABE` |
| 40 | 1 | `ok` |

with

    ok       = (s.m_34 | s.m_30 | s.m_40) == 0;
    combmask = s.mask | (s.aamask << 4);
    livemask = ok ? s.mask : (s.mask | s.aamask);

`ok` is the "plain stamp" predicate (no per-pixel special case); when
it is false the antialias-only pixels are printed as well.  Note
`combmask` packs `aamask` four bits up, so the 16 colour columns and
the 16-bit `combmask` do **not** index the same way — that shift is a
property of the RTL port, not a bug.

`RegisterVec` overlays the register write on the same 40 columns and
**writes nothing at all for `reg == 0` and `reg == 0x1b`** (an early
`return` before the first column) — those two register addresses are
not part of the MemIF interface:

    1: en=1  2: isreg=1  3: PutX(1)  4: reg (11 bits)  5: PutX(3)
    6: (int)data                     7-13:  XField(32) ×7
    14: (int)(data>>32)              15-21: XField(32) ×7
    22-37: XField(32) ×16            38-40: XLine "16 1 1"

so the 64-bit register value occupies colour columns 0 and 8 — the two
row-0/row-1 lanes of the stamp.

### 4.6 `sel 5` — the memory dump (`MyMemory::Dump`)

```c
if (fp == 0) return;
for (i = 0; ; i++) { if (i > 0xfffff) return; fprintf(fp, "%08x\n", vram[i]); }
```

The whole 4 MB of local memory, one 32-bit word per line, 1048576
lines (9,437,184 bytes).  Fired by `GPU2VEC::Put(0x7f, …)`.

---

## 5. Reading the taps back (blueprint notes for the debugger)

* One tap = one file = one stage's input port.  There is no framing, no
  header and no timestamp: the *n*-th line of the `sel 3` file is the
  *n*-th thing the DDA handed the texture unit.  Cross-stage correlation
  has to be done by counting, which is exactly what a testbench does.
* Every line splits on `'_'`; the last field before `'\n'` is empty.
  The column count is fixed per stage (69 / 50 / 40), so a reader only
  needs the width list once — and the value columns are already
  nibble-aligned hex, no re-alignment needed.
* `x` runs and forced zeros are distinguishable, and that distinction
  carries information: `x` means "the RTL does not care", `0` means
  "the RTL sees zero".  Only the TXM tap uses forced zeros (the STQ and
  fog inputs when TME/FGE are off).
* Turning a tap on is *not* idempotent with turning it off:
  `SetVector` never clears a previously installed file, and the
  destructor is the only thing that closes one.
* The natural driver is `GPU2VEC::Put`: mode 6 gives a ground-truth
  register log that can be replayed, and register 0x7f punctuates it
  with full vram snapshots.

---

## 6. Reconstruction notes

`include/gpu2vec.h` reproduces the 1998 include graph: bitblt's two
messages, `TXM::SearchQlevel`'s assert strings, `Frame2d`'s, the display
messages and then gpu2vec.c's own strings land in `.rodata` in parse
order, and the local vtables and deferred inlines come out in reverse
declaration order.  `.rodata` is byte-identical to the 1998 object bar
two GAS pad bytes, the relocation set and order are identical, and the
symbol table (names, bindings, types and order) is identical.

The middle of the header stack is *cloned* rather than included,
because the per-object headers (`pcalc.h`, `pre3.h`, `dda.h`,
`memif.h`) each carry a narrow stand-in view of their neighbours and
those views conflict:

* `PPDDA::Put` is pure here (gpu2vec.o's local `_vt.5PPDDA` entry is
  `__pure_virtual`);
* `PCalc` sits next to the real `Pre3`, `DDA` next to the real `PCalc`
  and `txm.h`'s pure `DDATXM`;
* `MemIF`'s constructor is inline (gpu2vec.o and gpu2.o both expand it;
  neither has an `UND __5MemIFP6Memory`).

`memory.h`, `clut.h`, `txm_div.h`, `pre1.h` and `pcrtc.h` are included
as-is.  Everything under `PCRTC` — `xif.h`'s `XWindowDump`/`Xifbase`,
the `MemRead*`/`PixelBlend*` readers, `PCRTC::SetRegister` — is
inherited unchanged from those headers, and every one of those inlines
is byte-identical (reloc-masked) to what the same headers emit into
`gpu2.o`, in both the 1998 pair and ours (see the matching note).

Source shapes taken over from `src/gpu2.c` because the bytes demand it:

* `Get` needs the local-pointer spelling `Memory *m = mem; return
  m->bitblt.ReadPixel(m);` — the single pseudo is destroyed by the
  `add $0x400144` and the object reloads nothing.
* `dumpCRT` needs `width*4*height`, not `height*4*width`: gcc 2.7
  regroups the constant-in-the-middle product and emits
  `lea 0(,%ebx,4); imul %esi` — the `lea`'s index register is the
  *second* operand written.  Getting this backwards was the only real
  source bug found while finishing the object, and it costs 0 bytes
  (pure register rename), so only the byte compare catches it.
* the `for (i = 0; i <= 15; i++)` pixel loops stay unrotated by
  themselves: their bodies contain calls, and `expand_end_loop`'s
  forward scan stops at a `CALL_INSN`.

## 7. Verifying

    test/run_gpu2vec.sh            # differential, exits 0
    # oracle (isolated farm, gpu2vec is never pulled by the link):
    REPLACE="... gpu2vec" ./build.sh && ./probe && ./gsreplay <dump> -o …
