# Byte-matching status per object

Compiler: `tools/gcc272/g++272 -O` (Debian hamm g++272 2.7.2.3-4.8, era
GAS 2.9.1).  See doc/compiler.md — the 1998 objects were built by a
**modified** gcc 2.7.2.3 (systematic vfn-ref codegen difference vs stock,
0/186 folded), so residual diffs at the RTL-expand/regalloc level are
expected even for perfect source.

| object | .text | .data | relocs | verification |
|---|---|---|---|---|
| addrconv | 1412/1463 bytes (51 residual) | identical | identical | differential 80.5M calls 0 mismatch; oracle replay bit-identical (both dumps); probe suite 0 failures |

## addrconv residuals (51 bytes, no semantic content)

All three patterns are expand/register-allocation shape, not behaviour:

1. **Z-arm of the blk computation**: original emits
   `mov %eax,%edi; and $1,%edi` (result into a fresh call-saved reg);
   ours folds `and $1,%eax` in place.  Source-level temps (int/unsigned,
   scoped or not) get copy-propagated by stock 2.7.2.3 — could not
   reproduce; suspected compiler-mod artifact.  The `shr` (unsigned
   shift) is reproduced with an `(unsigned)addr` cast.
2. **wd table lookup order**: original computes the x term
   (`(2x)&0xc`) before the y term (`(8y)&0x10`); stock gcc evaluates a
   2D `table[y..][x..]` row-first.  Flat-table and `(y&2)>>1`-style
   reformulations produce structurally different (worse) code.
3. **esi/edi assignment ripples** downstream of 1-2, incl. three
   2-byte swaps in the second switch's out stores.

Every other choice is pinned by bytes: the per-case page temp
(`p = (...) & 0x1ff; addr = p << 7`), term order in each case, `-O`
(not -O2), the Z range-check shape, jump tables at identical offsets.
