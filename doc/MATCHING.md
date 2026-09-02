# Byte-matching status per object

Compiler: `tools/gcc272/g++272 -O` (Debian hamm g++272 2.7.2.3-4.8, era
GAS 2.9.1).  See doc/compiler.md — the 1998 objects were built by a
**modified** gcc 2.7.2.3 (systematic vfn-ref codegen difference vs stock,
0/186 folded), so residual diffs at the RTL-expand/regalloc level are
expected even for perfect source.

| object | .text | .data | relocs | verification |
|---|---|---|---|---|
| addrconv | **100% byte-identical** (+.rodata) | identical | identical | differential 80.5M calls 0 mismatch; oracle replay bit-identical; probe suite 0 failures |

## addrconv: how the last 51 bytes fell (a case study)

The compiler-archaeology agent proved every stock gcc 2.7.2.x emits
identical code for given source (doc/compiler.md §7), so the residuals
had to be source shape.  Both were:

1. **Z-arm of blk**: the original ran through a *narrow-typed* temp —
   `t = (unsigned)addr>>6 ^ 1; blk = t & 1;` with `t` any of
   char/uchar/short/ushort (mode difference blocks copy-propagation,
   producing `mov %eax,%edi; and $1,%edi`; an int/unsigned temp gets
   coalesced away).  We chose `unsigned char`; the narrow type is pinned,
   which of the four is not.
2. **wd lookup**: not a 2D subscript at all — a byte-offset index
   variable and a cast deref:
   `i = ((x << 1) & 0xc) + ((y << 3) & 0x10); wd = *(int *)((char *)table + i);`
   The `<<` spelling is pinned (`x + x` compiles differently); the
   x-term-first order and the explicit index variable are pinned.
3. The esi/edi ripples were pure cascade — they vanished when 1-2 were
   fixed.

Also pinned by bytes: the per-case page temp (`p = (...) & 0x1ff;
addr = p << 7`), term order in every case, `-O` (not -O2), the Z
range-check shape, jump tables at identical offsets.  Lesson for the
other objects: when a residual survives all flag hunting, suspect a
source temp with a different type or a differently-spelled equivalent
expression — evaluation order and pseudo-register structure follow the
source exactly at -O.
