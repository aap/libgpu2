# txm_div.o — `NormTexCoord`

**747/951 .text bytes.**  `TexDiv`, `InitTable`, `evalute` and `cal_y`
are **byte-identical**; all 204 residual bytes are inside `mktable`.
.data is identical **and the three static members land at the original
offsets** (table_init +0x00, SLOPE_TBL +0x04, OFFSET_TBL +0x204);
.rodata is identical (9 doubles, in the original per-function pool
order); relocations identical.

Differential test: `test/run_txm_div.sh` — `InitTable()` is run on both
sides and **all 256 table entries compare identical**, then 4 299 822
calls covering `cal_y` over the whole 16-bit x range, `evalute` over
every bucket and `TexDiv` exhaustively over 16-bit q plus random
32-bit inputs: 0 mismatches.

## What NormTexCoord is

`class NormTexCoord` (fields inferred from use):

| off | name | meaning |
|---|---|---|
| 0x00 | `sign` | sign of u |
| 0x04 | `qzero` | set when the `f` argument was nonzero |
| 0x08 | `um` | 15-bit mantissa of \|u\| |
| 0x0c | `rm` | 15-bit mantissa of 1/q |
| 0x10 | `ue` | exponent of \|u\| |
| 0x14 | `re` | exponent of 1/q |

plus `static int table_init, SLOPE_TBL[128], OFFSET_TBL[128]`.

`TexDiv(u, q, f)` normalises `q` (bit-scan down from bit 15 of `q>>1`,
clamping the exponent at 0), builds a 15-bit mantissa `m`, and looks the
reciprocal up piecewise-linearly:

```
i  = ((unsigned)m >> 6) & 0x1fc;                  /* byte offset */
rm = (OFFSET_TBL[i/4] - ((m & 0xff)*SLOPE_TBL[i/4] >> 6)) >> 1 & 0x7fff;
re = -n + (m == 0 ? 6 : 5);
```
then does the same normalisation for `u` (with `>>2` instead of `>>1`).
`f != 0` means "q is degenerate": `re = rm = 0, qzero = 1`.

`mktable()` is the table generator, run once from `InitTable()`.  For
each of the 128 mantissa buckets `x = 0x8000, 0x8100, ... 0xff00` it
sweeps 100 candidate offsets (`d` from 0 down to -1e-4 in steps of
-1e-6), forms

```
offset = rint((1/(x/32768) + d) * 131072);
slope  = rint((offset/131072 - (1/(x/32768 + 1/128) + d)) / (1/128) * 256);
offset = (offset+1) & 0xffff;   slope = slope & 0xff;
```
and keeps the pair minimising `evalute()`, which is the worst-case
`|cal_y(i,offset,slope)/65536 - 32768/i|` over the 256 values in the
bucket.  `cal_y` is the fixed-point evaluation of the line, with the two
top bits (`0x10000` when the low 15 bits of x are zero, else `0x8000`)
that the hardware's format carries.

## Source shapes pinned by the bytes

1. **`__attribute__((__const__))` on `rint` and `fabs`.**  The 1998
   `<math.h>` (libc5 style) marked the pure math functions const; the
   era header set in `tools/gcc272/root` does not.  gcc 2.7 pops a
   const call's arguments *immediately* (`calls.c`: "When calling a const
   function, we must pop the stack args right away") instead of deferring
   the adjustment, and that is exactly the difference between the
   original's `add $0x8,%esp` after each `rint`/`fabs` and our deferred
   `add $0x20,%esp`.  Declaring them const in the source reproduces it.
   **This is a header property, not a source property** — worth
   remembering for every other object that calls libm.
2. `a` and `b` (the two reciprocals) are computed **inside** the offset
   sweep loop, not before it.  gcc hoists them into the loop preheader,
   which is what stops CSE from replacing the literal `1.0` with the
   `min = 1.0` already sitting in a stack slot — the original really does
   load 1.0 from .rodata twice.  With `a`/`b` above the loop, our build
   CSEs and the 1.0 constant disappears from .rodata entirely.
   `b` must be written **before** `a` for the constant-pool order to match.
3. `evalute` calls `cal_y` into a **named `int y`**, not inline inside
   the `fabs(...)` argument: inside the argument list `NO_DEFER_POP` is
   in force and cal_y's 16 argument bytes get popped immediately, which
   the original does not do.
4. The two table lookups in `TexDiv` use the byte-offset idiom
   (`i = (m >> 6) & 0x1fc; *(int *)((char *)TBL + i)`), the same trick
   MATCHING.md found in addrconv — a plain `TBL[(m>>8) & 0x7f]` gives
   scaled-index addressing and a different instruction sequence.
5. `m` is `unsigned int` (the `shr`/`imul` chain is unsigned), `n < 0`
   clamps are written as ternaries (`n = n < 0 ? 0 : n;`) so the value
   goes through a separate register, and the `ue` clamp does the same on
   the member.
6. Static member definition order in the file is irrelevant; what fixes
   the .data layout is the **declaration order inside the class**
   (OFFSET_TBL, SLOPE_TBL, table_init — gcc emits them reversed).

## Residual

`mktable`, 204 bytes, same total size (0x17e).  Two differences:

* the original computes `x/32768 + 1/128` **before** `1.0/(x/32768)`
  (`fld %st(0); faddl; fxch; fdivrl; fstpl a; fdivrl; fstpl b`), ours
  computes the two divisions in source order and needs no `fxch`.  No
  spelling of the two statements produced the add-first order: putting
  the sum in a dead temp first spills `x/32768`, swapping the statements
  swaps which reciprocal is stored first.
* a 4-byte shift in the middle of the loop body that re-syncs before the
  end (the `a+d`/`b+d` pair is emitted in the other order).

Both are x87 scheduling, not behaviour: the generated tables are
bit-identical.

## Load-bearing for TXM

`TexDiv` is the only interface TXM uses; the object's *table contents*
are what the rasterised texture coordinates depend on, and those are
verified identical.  `InitTable()` must be called before the first
`TexDiv` (the table is zero otherwise) — there is no static constructor
in this object, so TXM is responsible for calling it.
