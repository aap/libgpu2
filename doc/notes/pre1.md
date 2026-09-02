# pre1.o — Pre1, the GS register/vertex input stage

`src/pre1.c` + `include/pre1.h`, built with
`GCC272_1998=1 tools/gcc272/g++272 -O -Iinclude`.

| function | status |
|---|---|
| `Reverse__Fi` | **byte-identical** (0x29) |
| `__4Pre1P4Pre3` (ctor) | **byte-identical** (0xd1) |
| `Put__4Pre1ix` | 0x736 vs 0x776 — 25 instructions short, see below |
| `SendData__4Pre1` | identical except 3 `call Reverse` displacements |
| `SendRegister__4Pre1ix` | **byte-identical** (0x3a) |
| `MaxExp__4Pre1` | identical except the 7 jump-table words (addresses) |
| `.rodata` | **identical** |

`SendData` and `MaxExp` differ only in values that are functions of
`Put`'s size; they become byte-identical the moment `Put` does.

Behaviour is verified: `test/run_pre13.sh` drives the 1998 pre1.o+pre3.o
and the reconstruction with the same 2,000,000 register writes and
compares the complete Pre3 object at every `PCalc::Put` (711,661
snapshots) plus the full persistent Pre1/Pre3 state after every write —
**0 mismatches**.

## What Pre1 does

`GPU2::Put` calls `Pre1::Put(addr, data)` **non-virtually** for register
addresses below 0x80.  Pre1 holds the current vertex attributes, decodes
PRIM/PRMODE into individual flag words, and pushes work down to Pre3
through Pre3's vtable (`pre3->Put(this)` — Pre3 then reads Pre1's
`send_*` fields).

### The register map is *not* the retail one

`Put`'s jump table covers 0x00–0x1b; everything else falls into
`default:` and is passed down verbatim as a register write
(`send_type = 1`).  Handled cases, in source order (= code order):

| addr | action | kick | `nodraw` |
|---|---|---|---|
| 0x00 | PRIM: `PRIM = data & 7`, `newprim = 1`; if PRMODECONT (`AC==1`) also decode the attribute bits | – | – |
| 0x0a | X, Y, Z, F — **no kick at all** | no | – |
| 0x01 | RGBA, Q | no | – |
| 0x02 | S, T | no | – |
| 0x03 | U, V | no | – |
| 0x18 | XYOFFSET_1 (OFX bits 0-15, OFY bits **32-47**) | – | – |
| 0x19 | XYOFFSET_2 | – | – |
| 0x04 | X, Y, Z, F (XYZF2) | yes | 0 |
| 0x11 | RGBA, Q | **yes** | 0 |
| 0x12 | S, T | **yes** | 0 |
| 0x13 | U, V | **yes** | 0 |
| 0x0c | X, Y, Z, F (XYZF3) | yes | 1 |
| 0x05 | X, Y, Z (F preserved, XYZ2) | yes | 0 |
| 0x0d | X, Y, Z (F preserved, XYZ3) | yes | 1 |
| 0x1a | PRMODECONT: `AC = data & 1` | – | – |
| 0x1b | PRMODE: decode the attribute bits when `AC == 0` | – | – |

0x00–0x05, 0x0a, 0x0c, 0x0d, 0x18–0x1b match the retail GS.  **0x11,
0x12 and 0x13 do not exist in the retail GS**: they are RGBAQ / ST / UV
writes that *also* perform a drawing kick.  0x0a is an XYZF write that
performs *no* kick — a plain "XYZF" with neither the 2 nor the 3
variant's behaviour.  This looks like a 1998-era map in which the
"kick" variants of the attribute registers lived at +0x10 and were
dropped before the retail GS.  Both are reachable from `GS_PutPort`, so
if the user ever wants to know what they do, the model will say.

`nodraw` (Pre1+0x7c) is cleared at the top of every `Put` and set by the
XYZ3/XYZF3 cases; it travels to Pre3, which uses it to fill the vertex
queue without drawing.

Attribute bit assignment (identical in the PRIM and PRMODE decoders, in
this source order): FST=bit 8, AA1=7, ABE=6, FGE=5, TME=4, IIP=3,
FIX=10, CTXT=9.  CTXT is special: it is compared against the stored
value and a *register write is synthesised* (`SendRegister(addr, data)`)
only when it changes.  The constructor sets `CTXT = 2` precisely so that
the very first PRIM/PRMODE write always propagates it.

### SendData

```
send_type = 0
ofx/ofy   = XYOFFSET_1 or _2 by CTXT&1
X -= ofx; Y -= ofy                    (only for the duration of the call)
FST==0 ? send_U/V/Q = S/T/Q[2] >> 8   (24-bit float = IEEE single >> 8)
       : send_U/V   = U/V, send_Q = Q[2] >> 8
maxexp = TME==1 ? MaxExp() : 0
if (send_Q & 0x800000)                (Q negative -> flip the whole triple)
        FST==0 && (send_U = Reverse(send_U), send_V = Reverse(send_V))
        send_Q = Reverse(send_Q)
pre3->Put(this)
X += ofx; Y += ofy
rotate the S/T/Q ring                 (fan: slot 0 pinned to the first vertex)
newprim = 0; pre3->restart = 0
```

`Reverse(a)` just toggles bit 23 — the sign bit of the 24-bit float
format — so the "reverse" is a negation of S, T and Q together, which
leaves S/Q and T/Q unchanged but makes Q positive for the divider.

`S/T/Q[3]` are a ring: `[0]` and `[1]` are the two previous vertices,
`[2]` the current one.  `MaxExp` walks it from a per-PRIM start index
(point 2, line/linestrip/sprite 1, triangle/strip/fan 0) to 2 and
returns the largest IEEE exponent `(x >> 23) & 0xff`; for FST==1 only Q
participates.  PRIM 7 prints `PRE1:Illegal primitive type` and exits.

## Struct facts to feed back into IDA

`from_ida/pre1_3.h`'s Pre1 is correct except:

* `S_xx/S_yy/S` (0x14/0x18/0x1c), `T_xx/T_yy/T` (0x20/0x24/0x28) and
  `Q_xx/Q_yy/Q` (0x2c/0x30/0x34) are three `int[3]` **arrays** —
  `MaxExp` indexes them (`0x14(%ebx,%edx,4)`), so the array form is
  pinned by the code, not a guess.
* `Z` (0x4c) and `F` (0x50) are **one `long long`**: Z in bits 0-23, F
  in bits 32-39.  `Put` builds and stores it as a 64-bit value, and
  Pre3 reads `(unsigned)Z` and `(Z >> 32) & 0xff` out of it.
* `m_7c` is the XYZ3/XYZF3 "no drawing kick" flag; `m_a0` is
  "PRIM was just written" (the fan pivot flag).
* `gap_a4[8]` (0xa4, 0xa8): never read or written by pre1.o.

## Residual: `Put`, 25 instructions

Every instruction in `Put` matches except in the four cases that assign
a plain (unmasked) 64-bit half to a member — 0x01, 0x02, 0x11, 0x12.
The 1998 object routes each of those through **two extra spilled
pseudos** (a DImode one at `-0x10(%ebp)` and an SImode one at
`-0x4(%ebp)`), giving a 0x10-byte frame where ours needs 0x8:

```
orig  case 0x02:  mov %esi,-0x10(%ebp)   ; DImode temp = data
                  mov %esi,-0x4(%ebp)    ; SImode temp = (int)temp
                  mov 0x8(%ebp),%eax
                  mov -0x4(%ebp),%edx
                  mov %edx,0x1c(%eax)    ; S[2] = temp
ours              mov 0x8(%ebp),%eax
                  mov %esi,0x1c(%eax)
```

Tried and rejected (all coalesced away by stock 2.7.2.3, byte-for-byte
identical output): an `int` temp, a `long long` temp, both at function
and block scope; `(int)` casts; `& 0xffffffff` / `& 0xffffffffLL`
masks; an inline `SetRGBAQ`/`SetST` member function whose parameter
would be the temp.  The residual is a register-allocation outcome, not
a source shape — see the note in doc/notes/pre3.md about the 1998
compiler forcing values through registers.

## Two source shapes that *were* pinned by bytes

1. **`z` and `f` in the XYZ cases must be block-scoped.**  At function
   scope they become multi-block allocnos, `data` wins the first
   call-saved register pair and the whole function comes out with
   `addr`, `data.lo`, `data.hi` in `edi/ebx/esi` instead of the
   original's `ebx/esi/edi`.  Block-scoping them makes them local
   allocnos and the register assignment matches exactly.

2. **`MaxExp`'s two loops must not be rotated.**  g++ 2.7's
   `expand_end_loop()` (stmt.c) unconditionally rolls a top-tested
   loop's exit test to the bottom unless the loop's last insn is
   already a conditional jump, or the scan from the loop top hits a
   `CODE_LABEL`, a `CALL_INSN` or a `NOTE_INSN_BLOCK_BEG` before it
   finds the test.  The 1998 object has the test at the top with a
   `.align 4` loop label, i.e. it was *not* rotated.  `for(;;)` with a
   block-scoped `int e` and an `if (i == 3) break;` first hits the
   block note, suppresses the rotation, keeps the loop note (hence the
   alignment) and reproduces the original exactly.

   This is worth generalising: **the 1998 objects are full of
   unrotated loops** (`inc; jmp` back to a top test) where stock
   2.7.2.3 emits `inc; cmp; jne` — pcalc.o 21 vs 1, memif.o 10 vs 0,
   txm.o 9 vs 4.  Several of those (e.g. `memif.o:ATest+0x1a`, a plain
   `for (i = 0; i < 16; i++)` over a stamp array) cannot plausibly be
   `goto` loops in the source, so either the 1998 cc1plus did not run
   this transformation or something systematically inhibited it.
   Whoever attacks pcalc/memif/txm should expect this and reach for
   the same `for(;;)` + block-scoped-declaration idiom.
