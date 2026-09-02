# slong.o

**100% byte-identical — the whole object file, not just .text.**
`cmp orig/lib/slong.o <rebuilt>` is silent: 1227/1227 .text bytes, empty
.data/.bss, identical symbol table, .note and .comment.

Differential test: `test/run_slong.sh`, 6 369 119 calls, 0 mismatches.

## What slong is

A 96-bit signed-magnitude fixed-point accumulator, 28 bytes:

| off | type | meaning |
|---|---|---|
| 0x00 | `unsigned long long hi` | high half of the magnitude |
| 0x08 | `unsigned long long lo` | low half, kept in [0, 2^32) |
| 0x10 | `int sign` | +1 / -1 |
| 0x14 | `unsigned long long mask` | 0xffffffff, set by the (inline, header) ctor |

The value is `sign * (hi * 2^32 + lo)`, and `Combine()` folds it back to a
single truncating `long long`.  `Multiply(a, b)` forms the *exact* 64x64
product: it takes the sign out, `abs()`es both operands, swaps so
`a <= b`, then `hi = a*(b>>32); lo = a*(b&mask); hi += lo>>32; lo &= mask`.
The swap is the only reason it is exact for the operand ranges PCalc uses.

`operator<<`/`operator>>` shift the 96-bit value as a unit, carrying bits
across the hi/lo boundary; both take and return `slong` **by value**
(hidden return-slot pointer as argument 0), and both start by
default-constructing a local, which is where the ctor's `mask` store
comes from — slong has no out-of-line ctor in this object, so it was
defined inline in the header.

## Source shapes that were pinned by the bytes

1. `Multiply` needs two *named* sign temporaries (`sa`, `sb`) so the sign
   is `sa * sb` (an `imul`); folding the expression into one statement
   makes gcc emit a conditional `negl` on the member instead.
2. `Multiply` needs named `bh = b >> 32; bl = b & mask;` **before** the
   two products — that is what keeps `b>>32` in a register pair across
   the first multiply and puts the accumulator in memory.
3. `Combine` is `((hi << 32) + lo) * sign`, not `sign * (...)`: gcc's
   DImode multiply expander puts the *second* source operand in `%eax`
   here, so the operand order is observable.
4. `operator>>`/`operator<<` need three locals in this order —
   `unsigned long long t, u;` then `long long mask` — and the shift
   results must go through `t` (`t = s.lo >> i; s.lo = t;`) and the
   masked field through `u`.  Writing the shift straight into `s.lo`
   loses the spill-and-copy the original has; the declaration order
   fixes which spill slot each pseudo gets (-0x40 = u, -0x48 = mask,
   -0x50 = t).
5. `t`/`u` must be **unsigned** long long: `(s.lo & mask) >> (32-i)` is
   `shrd/shr` in the original, `shrd/sar` if the temp is signed.

## Open questions

- Field names are invented (`hi`, `lo`, `sign`, `mask`); nothing in the
  object names them.
- The ctor lives in the header and is only visible through its inlined
  `mask = 0xffffffff` store, so `slong()` could equally be
  `slong(void) { mask = 0xffffffffLL; }` — the constant is 0x00000000ffffffff
  either way.

## Load-bearing for PCalc

`Multiply` is the exact-product primitive and `Combine` the only way the
result leaves the class, so PCalc's slope arithmetic is
"multiply exactly, shift, then truncate" — the truncation happens in
`Combine`, not in the multiply.
