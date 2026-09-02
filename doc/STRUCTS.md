# Object graph and struct layout evidence

Primary source: `GPU2::GPU2` (`gpu2.o:__4GPU2Pciii`, 0xb1d bytes), which
allocates the whole pipeline with `__builtin_new` (sizes = sizeof) and
inlines most member construction (stores = field offsets + initial
values).  Cross-checked against `Pre1::Pre1`, `Pre1::SendData/SendRegister`,
`GPU2::Put/Get`, `PPOut::Put`, `GS_OpenSim`.  Disassembly offsets cited as
`+0xNNN` inside the ctor unless said otherwise.  Read 2026-09-02.

aap's IDA session owns the evolving full layouts (`othersrc/libgpu2/
from_ida/`); this file records what the ctor pass pins down, so the two can
be cross-checked.

## Allocation map (all from GPU2::GPU2, in program order)

| # | size | class | stored at | ctor style |
|---|---|---|---|---|
| 1 | 0x4001c8 | Memory+BitBLT block (see below) | GPU2+0x10 | none seen (open) |
| 2 | 0xfc | MemIF | GPU2+0x0c | inline, vptr +0xf8 |
| 3 | 0x72c | TXM | GPU2+0x08 | inline, vptr +0x00 |
| 4 | 0x254 | DDA | GPU2+0x04 | inline, vptr +0x250 |
| 5 | 0x10 | front-end block, name unknown | GPU2+0x00 | inline, no vptr |
| 6 | 0x8 | PPOut | FE+0x0c | inline, vptr +0x00 |
| 7 | 0xc00 | PCalc | FE+0x08 | inline + 6× `param::param`, 1× `Reciproc::Reciproc`; vptr +0xbfc |
| 8 | 0x14c | Pre3 | FE+0x04 | inline, vptr +0x148 |
| 9 | 0xac | Pre1 | FE+0x00 | out-of-line `__4Pre1P4Pre3` |
| 10a | 0x40 | PCRTCdmy (disp_on=0) | GPU2+0x14 | inline; vptr +0x3c set to `_vt.5PCRTC`, then `_vt.8PCRTCdmy` |
| 10b | 0x1d4 | PCRTCxif (disp_on=1,2) | GPU2+0x14 | inline; PCRTC base then derived vptr at +0x3c |
| 11a | 0x40 | XWindow (disp_on=1) | PCRTCxif+0x1d0 | vptr +0x00; `OpenWindow(title,w,h)` |
| 11b | 0x24 | XWindowDump (disp_on=2) | (tail, unread) | vptr +0x00; 2× `malloc(0x40000)` image buffers |

`GPU2` itself is `new(0x18)` in `GS_OpenSim` (libgpu2.o), stored in a bss
singleton that `GS_PutPort`/`GS_PutCtlPort`/`GS_SaveImage` use.

### The 0x4001c8 block: Memory + BitBLT

`GPU2::Get` (gpu2.o) calls
`BitBLT::ReadPixel(Memory*)` as `ReadPixel(block+0x400144, block)`:

- **Memory at +0x0, sizeof 0x400144** (4 MB VRAM first — the
  `lg2_bigalloc` hook in tools/shims.c depends on exactly this — then
  0x144 bytes of FB/ZB config state).
- **BitBLT at +0x400144, sizeof 0x84.**

Whether the outer allocation is one class with two members or `Memory`
with a trailing `BitBLT` member is open; no constructor call for either
was seen in the portion of the ctor read so far.

### Corrected: sizeof(param) = 0x50, not 0x10

PCalc's embedded `param` subobjects get `__5param` ctor calls at PCalc
+0x14, +0x64, +0xb4, +0x10c, +0x15c, +0x1ac — uniform 0x50 spacing (with
`__8Reciproc` at +0x104, spacing to +0x10c confirming sizeof(Reciproc)=8).
The `re/` claim "param 0x10" mis-attributed allocation #5 (the front-end
block) to `param`.

## GPU2 (0x18)

| off | value | evidence |
|---|---|---|
| 0x00 | front-end block* | ctor +0x33e; `GPU2::Put` reads it |
| 0x04 | DDA* | +0x187 |
| 0x08 | TXM* | +0x13a |
| 0x0c | MemIF* | +0x107 |
| 0x10 | Memory* (the 0x4001c8 block) | +0xe3; `GPU2::Get` |
| 0x14 | PCRTC* (dmy or xif) | +0x3f1 / +0x791 |

`GPU2::Put(int addr, long long)`: `(signed char)addr < 0 || addr > 0xff`
→ virtual `pcrtc->vt[0]` (= `PCRTC::SetRegister(int,long long)`); else
**direct non-virtual** `Pre1::Put(addr, data)` on FE+0x00's Pre1.
`GS_PutCtlPort` maps 0x120000n0 → 0x80|n (PMODE 0x80 … BGCOLOR 0x8e),
plus 0x1200100x → higher codes; `GS_OpenSim` pours the power-on defaults
through `GPU2::Put` for regs 0x01–0x53 and 0x80/0x87–0x8a/0x8e, then
calls `initPCRTC__Fv`.  (Curiosity: the init sequence writes register
0x17 twice and 0x16 never — looks like an original TEX2_1 init bug;
verify before relying.)

## Front-end block (0x10, class name unknown)

| off | value | evidence |
|---|---|---|
| 0x00 | Pre1* | +0x336 |
| 0x04 | Pre3* | +0x31f |
| 0x08 | PCalc* | +0x2d9 |
| 0x0c | PPOut* | +0x1af |

No vptr.  Candidate identity: the "PP" in dbg.o/gpu2vec.o naming
(`MyPP::Put(int,long long)` exists in gpu2vec.o) — unresolved.

## Pipeline wiring and dispatch

```
GS_PutPort(addr,data)
  └─ GPU2::Put ── direct ──► Pre1::Put(int,ll)        [pre1.o, no vtable]
       switch(addr 0x00-0x1b), >0x1b → default
       SendData/SendRegister:
  Pre1 ── virtual Pre3::vt[0](Pre1*) ──► Pre3         [vptr +0x148]
  Pre3 ── (Pre3+0 = PCalc*) ─────────► PCalc          [vptr +0xbfc]
  PCalc ── virtual PPOut::vt[0](PCalc*) ► PPOut       [tap, always installed]
  PPOut ── virtual DDA::vt[0](PCalc*) ─► DDA          [vptr +0x250; PPOut::Put confirmed forwarding, dbg.o]
  DDA  ── (DDA+4 = TXM*) ────────────► TXM::Put(DDA*) [vptr +0x0, 13 KB]
  TXM  ── (TXM+0x24, +0x278 = MemIF*) ► MemIF         [vptr +0xf8]
  MemIF ── (MemIF+0 = Memory*) ──────► pixels
```

- Every stage's `_vt` has exactly one entry: `Put`.  The `gpu2vec.o`
  `My*` classes override it to write binary trace vectors, which is the
  point of the virtual dispatch.
- `Pre1` passes **itself** to `Pre3::Put`; the receiving stage reads the
  sender's `send_*` fields (send_addr/send_reg/send_U/send_V/send_Q/
  send_type at Pre1+0x58…0x74, per `SendData`/`SendRegister`).
  send_type: 0 = vertex data, 1 = register pass-through.  Inference (to
  verify): registers each stage cares about are picked off in its `Put`
  via its `Set*` methods as the write travels down the pipe.
- `Pre1::SendData` also: applies XYOFFSET context-sensitively by
  *temporarily* subtracting OFX/OFY from X/Y around the call (+0x8c6/
  +0x985), picks STQ vs UV by FST (+0x84), calls `MaxExp` only when
  TME=1, runs `Reverse()` on the send values when bit7 of +0x6e
  (Q sign? verify) is set, and rotates the tristrip vertex ring
  (PRIM==5 path).  After the virtual call it clears `Pre3+0x144`.

## Per-class pinned fields (beyond the above)

**Pre1 (0xac)** — matches aap's `from_ida/pre1_3.h`; ctor zeroes vertex
state, `+0x80 = 2` (from_ida calls it CTXT — initial value 2 is odd,
verify semantics), `+0x78 PRIM`, `+0x7c` cleared on every `Put`,
send block 0x58–0x74, flags 0x84–0x9c (FST/AA1/ABE/FGE/TME/IIP/FIX),
`+0xa0` tristrip-phase flag.

**Pre3 (0x14c)**: +0x0 PCalc*; +0x4 = 0; +0x120 = 0; +0x138 = 0;
+0x144 = 1 (cleared by Pre1 after each data send); vptr +0x148.

**DDA (0x254)**: +0x4 TXM*; +0x1e4, +0x1e8, +0x1ec, +0x240 = 0;
vptr +0x250.

**TXM (0x72c)**: vptr +0x0 (inherited — likely from DDATXM, whose local
vtable txm.o carries); +0x24 MemIF*; +0x278 MemIF*; static
`TXM::valid8[256]` built by `.ctors` initializer.

**MemIF (0xfc)**: +0x0 Memory*; vptr +0xf8.

**PCalc (0xc00)**: +0x0 PPOut*; +0x4 = 4; +0x8 = 0x16; +0xc = 0x10;
+0x10 = 1; param subobjects at +0x14/+0x64/+0xb4/+0x10c/+0x15c/+0x1ac;
Reciproc at +0x104; +0xaa4 = 0xffff; +0xaac = 0xffff0000; +0xab4 = 0x10;
+0x264…+0x280, +0xbb0, +0xbe0 = 0; vptr +0xbfc.

**PPOut (0x8)**: +0x0 vptr; +0x4 DDA*.  `Put(PCalc*)` forwards
virtually to DDA.

**PCRTC (0x40 as PCRTCdmy)**: +0x0 Memory*; +0x8 = 0x20; +0x20/+0x28 =
0x100; +0x24/+0x2c = 0x80; rest of 0x4–0x38 zeroed; vptr +0x3c
(vt 0x18 = 2 entries: SetRegister, Resize — both weak in gpu2.o).

**PCRTCxif (0x1d4)**: PCRTC base 0x0–0x3f; embedded objects with their
own vptrs: three MemRead-family readers (fields at +0xe4/+0x114/+0x144,
vptrs `_vt.9MemRead32/24/16` at +0xf0/+0x120/+0x150, 0x30 apart), two
PixelBlend objects (vptrs `_vt.13PixelBlendAlp` +0x180,
`_vt.12PixelBlend1a` +0x188), display-rect state around +0x154–+0x1b8
(defaults 640/480), XWindow*/XWindowDump* at +0x1d0.  Field semantics
deferred to the pcrtc attack.

**XWindowDump (0x24)**: +0x0 vptr; +0x4/+0x10 = malloc(0x40000)
(256×256×4 image buffers); +0x8/+0xc/+0x14/+0x18 = 0x100; +0x1c = 0;
+0x20 = a .text function pointer (unidentified).

## Prior-claim corrections (DeepSeek-era material in othersrc)

1. `FINDINGS.md` §9 "flat (TME=0) primitives do not rasterize" — false
   (measured 2026-09-01, gsreplay README §1).
2. `FINDINGS.md` §9 "model PSMCT32 is (A,B,G,R) byte-reversed" — only the
   `GS_SaveImage` path; local memory matches retail (same source).
3. `re/README.md` "No C++ virtual dispatch calls exist anywhere" — false;
   the entire stage pipeline dispatches `Put` through vtables
   (pre1.o:SendData+0xde and everywhere downstream).
4. `FINDINGS.md` §9 ".ctors sections are empty / no static constructors" —
   false; txm.o and dbg.o `.ctors` carry relocations to `_GLOBAL_.I.*`
   (TXM::valid8 table, DbgInit).
5. (minor) `re/` "param sizeof 0x10" — actually 0x50; see above.

## Open questions

- Where are `Memory`/`BitBLT` constructed?  (GPU2 ctor tail 0xb90–0xbf0
  unread; or they may be constructed lazily/not at all.)
- Name of the 0x10 front-end block; relation to `MyPP`/`PP`.
- `DDATXM`: base class of TXM?  Who instantiates `PPDDA`?
- The double 0x17 / missing 0x16 in `GS_OpenSim`'s init table.
- `GPU2::GPU2` mangles as `Pciii` (4 args) but `GS_OpenSim` takes 5
  (title,w,h,disp_on,field) — where does `field` go?
- PCRTCxif internals (the +0xa8 rect-intersection block, the three
  readers' 0x30 stride vs their 0x10 tail).
