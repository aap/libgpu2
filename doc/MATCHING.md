# Byte-matching status per object

Compiler: `tools/gcc272/g++272 -O` (Debian hamm g++272 2.7.2.3-4.8, era
GAS 2.9.1).  See doc/compiler.md — the 1998 objects were built by a
**modified** gcc 2.7.2.3 (systematic vfn-ref codegen difference vs stock,
0/186 folded), so residual diffs at the RTL-expand/regalloc level are
expected even for perfect source.

| object | .text | .data | relocs | verification |
|---|---|---|---|---|
| addrconv | **100% byte-identical** (+.rodata) | identical | identical | differential 80.5M calls 0 mismatch; oracle replay bit-identical; probe suite 0 failures |
| libgpu2 | 7/9 functions exact (GS_OpenSim off only by a callee-size displacement); GS_SaveImage + initPCRTC differ in DImode register/spill allocation only (doc/notes/libgpu2.md) | identical (+.rodata/.bss/.comment/.note) | identical (195 records) | differential 39710 checks 0 failures; oracle replay bit-identical (REPLACE="addrconv libgpu2") |
| pre1 | 2825/2889 bytes; 3/6 fn byte-identical, 2 more identical bar displacement/jump-table addresses; Put 25 insns short | n/a | identical | joint pre1+pre3 differential: 2M register writes, 711661 PCalc-boundary state snapshots, 0 mismatches; oracle bit-identical incl. a full RRV game dump (301k vertex kicks) |
| pre3 | 2528/2688 bytes; 5/10 fn byte-identical, 3 more instruction-identical; Triangle 39 insns short | n/a (+.rodata/_vt.4Pre3 identical) | identical | same joint differential + oracle |

pre1/pre3 residuals look like the SAME compiler mod as the vfn-ref
anomaly (a "force address through a value" change in expand): the
originals show `lea 0(,idx,4)` + `disp(tmp,base,1)` forms (7 sites in
pre3.o, 0 reproducible) and matching extra register pressure.  Also
learned: era `<stdlib.h>` abs() must NOT be used (attribute changes arg
popping — declare it locally), and the originals' loops are
systematically unrotated (`for(;;)` + block-scoped decl + `if break`
reproduces it; expect in pcalc/memif/txm).

libgpu2.o build settings differ from the rest of the archive and are
forced by bytes: RH 4.2 gcc **2.7.2.1** (`GCC272_ALT=rh42-2721`),
**`-O2 -m386`** — vs 2.7.2.3 `-O` (i486-configured) for the other 22.
Empirically 2.7.2.1 vs 2.7.2.3 emit identical code at those flags, so
the split only matters for `.comment` fidelity.

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
