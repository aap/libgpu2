# pre3.o — Pre3, primitive assembly

`src/pre3.c` + `include/pre3.h`, built with
`GCC272_1998=1 tools/gcc272/g++272 -O -Iinclude`.

| function | orig / new insns | status |
|---|---|---|
| `Register__4Pre3P4Pre1` | 21 / 21 | **byte-identical** (0x4d) |
| `Float2Fix__4Pre3iUi` | 41 / 41 | **byte-identical** (0x71) |
| `SetAttr__4Pre3P4Pre1` | 33 / 33 | **byte-identical** (0x8f) |
| `Triangle__4Pre3P4Pre1` | 244 / 205 | 39 insns short (`this` reloads) |
| `Point__4Pre3P4Pre1` | 63 / 63 | identical bar 5 displacement bytes |
| `Line__4Pre3P4Pre1` | 125 / 121 | 4 insns short |
| `Sprite__4Pre3P4Pre1` | 6 / 6 | identical bar 1 displacement byte |
| `Primitive__4Pre3P4Pre1` | 220 / 219 | 1 insn short (3 `lea` merges) |
| `Put__4Pre3P4Pre1` | 27 / 27 | identical bar 5 displacement bytes |
| `NumVertex__4Pre3` | 5 / 5 | **byte-identical** (0xd) |
| `__4Pre3P5PCalc` (ctor) | 12 / 12 | **byte-identical** (0x40) |

`.rodata` (`_vt.4Pre3`, 0x10 bytes) and its relocation to
`Put__4Pre3P4Pre1` are **identical**, and so is the symbol table:
same names, same bindings, same order, with `NumVertex` and the
constructor emitted *after* `Put` — the g++ 2.7 signature of inline
functions defined in the header.

Behaviour is verified together with pre1.o: `test/run_pre13.sh`,
2,000,000 register writes, 711,661 whole-Pre3 snapshots at the
`PCalc::Put` call site, 0 mismatches.

## Header / TU shape

* **No `#pragma interface` / `#pragma implementation`.**  Without them
  g++ 2.7 emits `_vt.4Pre3` in the TU that defines the first non-inline
  virtual function (`Pre3::Put`, i.e. pre3.c) as a *global*, which is
  exactly what the 1998 object has, and gpu2.o refers to it as
  undefined.  Adding the pragmas also drags in synthesised
  `operator=`/copy-ctor symbols for `Pre3`, `Vertex` and `PCalc` that
  the original does not have.
* `Pre3::Pre3(PCalc*)` and `NumVertex()` are **defined inline in the
  header** — that is why gpu2.o can inline the constructor while
  pre3.o still carries an out-of-line global copy, emitted last.
* `abs()` is declared locally as `extern "C" int abs(int);` rather than
  included from `<stdlib.h>`.  The era header marks it
  `__attribute__((__const__))`, which makes g++ pop each argument
  immediately; the 1998 object defers both pops into one
  `add $0x8,%esp`.  This is worth remembering for every other object
  that calls a libc function the era headers annotate.

## Member map (sizeof 0x14c, vptr at 0x148)

This replaces the `m_*` names in `from_ida/pre1_3.h`.

| off | name | meaning |
|---|---|---|
| 0x000 | `pcalc` | next stage; `pcalc->Put(this)` through vt entry 0 at PCalc+0xbfc |
| 0x004 | `nvtx` | vertices in the queue = index of the next slot; `NumVertex()` returns it |
| 0x008 | `S[3]` | raw 24-bit floats copied from `Pre1::send_U` |
| 0x014 | `T[3]` | from `Pre1::send_V` |
| 0x020 | `Q[3]` | from `Pre1::send_Q` |
| 0x02c | `dx[3]` | `dx[i] = v[i].x - v[i+1].x` (indices mod 3) |
| 0x038 | `dy[3]` | `dy[i] = v[i+1].y - v[i].y` — note the opposite sign, this is the edge-function form |
| 0x044 | `dxzero[3]` | `dx[i] == 0` |
| 0x050 | `dyzero[3]` | `dy[i] == 0` |
| 0x05c | `steep[3]` | `abs(v[i].x - v[i+1].x) < abs(v[i].y - v[i+1].y)` — Y is the major axis for that edge |
| 0x068 | `area` | `long long`, `(long long)dx[2]*dy[0] - (long long)dx[0]*dy[2]` = twice the signed triangle area (sign = winding) |
| 0x070 | `v[3]` | vertex queue, 0x30 bytes each (see below) |
| 0x100 | `send_reg` | `long long`, pass-through register value |
| 0x108 | `send_addr` | pass-through register address |
| 0x10c | `type` | primitive class: 0 point, 1 line, 2 triangle, 3 sprite |
| 0x110 | `send_type` | 0 = primitive, 1 = register write |
| 0x114 | `CTXT` | `p->CTXT & 1` |
| 0x118 | `FST` | |
| 0x11c | `AA1` | **forced to 0 for PRIM 0 (point) and 6 (sprite)** |
| 0x120 | `m_120` | zeroed by the ctor, never written again by pre3.o |
| 0x124 | `ABE` | |
| 0x128 | `FGE` | |
| 0x12c | `TME` | |
| 0x130 | `IIP` | |
| 0x134 | `FIX` | |
| 0x138 | `maxexp` | **not PRIM** — it is `Pre1::maxexp` (Pre1+0x70), the common exponent for this primitive |
| 0x13c | `m_13c` | never touched by pre3.o |
| 0x140 | `m_140` | never touched by pre3.o |
| 0x144 | `restart` | "reload the queue"; ctor sets 1, `Primitive` sets it from `Pre1::newprim`, `Pre1::SendData` clears it after every vertex |
| 0x148 | vptr | `_vt.4Pre3` |

`Vertex` (0x30):

| off | name |
|---|---|
| 0x00 | `x` (XYOFFSET already subtracted by Pre1) |
| 0x04 | `y` |
| 0x08 | `z`, `long long`, `(unsigned)Pre1::Z` (high word forced to 0) |
| 0x10/0x14/0x18/0x1c | `r`, `g`, `b`, `a` — the four bytes of `Pre1::RGBA` |
| 0x20 | `f` — `(Pre1::Z >> 32) & 0xff` |
| 0x24/0x28/0x2c | `s`, `t`, `q` — fixed point at the common exponent |

## Semantics

`Put(Pre1 *p)`: `SetAttr(p)` unconditionally, then `Register(p)` if
`p->send_type` else `Primitive(p)`.

`Primitive(Pre1 *p)`:

```
send_type = 0
if (p->newprim) restart = 1
maxexp = p->maxexp
if (restart)                            /* reload the queue */
        PRIM 0     : type = 0, nvtx = 2   /* point: slot 2 only */
        PRIM 1 or 2: type = 1, nvtx = 1   /* line/linestrip: slots 1-2 */
        PRIM 6     : type = 3, nvtx = 1   /* sprite: slots 1-2 */
        else       : type = 2, nvtx = 0   /* triangle/strip/fan: 0-2 */
v[nvtx] <- X, Y, Z, RGBA bytes, F;  S/T/Q[nvtx] <- send_U/V/Q
nvtx++
PRIM 0: Point(p);            nvtx = 2
PRIM 1: if (Line(p))         nvtx = 1
PRIM 2: if (Line(p))     { v[1]=v[2]; STQ[1]=STQ[2];               nvtx = 2 }
PRIM 3: if (Triangle(p))     nvtx = 0
PRIM 4: if (Triangle(p)) { v[0]=v[1]; v[1]=v[2];
                           STQ[0]=STQ[1]; STQ[1]=STQ[2];           nvtx = 2 }
PRIM 5: if (Triangle(p)) { v[1]=v[2]; STQ[1]=STQ[2];               nvtx = 2 }
PRIM 6: if (Sprite(p))   { v[1]=v[2]; STQ[1]=STQ[2];               nvtx = 1 }
```

So the queue is always loaded such that the *last* vertex of a
primitive lands in slot 2, and after a draw the surviving vertices are
shifted down: a triangle strip keeps both (0←1←2), a fan keeps the
pivot and shifts only 1←2, a sprite/line keeps 1←2 and reloads.  The
`v[i] = v[j]` struct assignments compile to `rep movsl` of 12 dwords.

`Triangle`/`Line` return 0 while `nvtx != 3` (queue still filling) and
1 once the primitive has been consumed; when `p->nodraw` is set
(XYZ3/XYZF3) they consume the vertex and return 1 without computing or
drawing anything.  `Sprite` is literally `return Line(p);` — the sprite
setup is entirely PCalc's job; Pre3 only supplies the two corners plus
`type == 3`.  `Point` returns void and only does the S/T/Q conversion
for slot 2.

`Float2Fix(int val, unsigned maxexp)` — the 24-bit float format is an
IEEE-754 single shifted right by 8, i.e. bit 23 sign, bits 22:15
exponent, bits 14:0 the top 15 mantissa bits:

```
sign = (val >> 23) & 1
exp  = (val >> 15) & 0xff
man  = val & 0x7fff
if (exp) man |= 0x8000              /* hidden bit */
if (sign) man = -man
if (exp) man >>= 1                  /* room for the hidden bit */
if (maxexp - exp > 14)              /* unsigned: exp > maxexp wraps here too */
        man = man >= 0 ? 0 : -1     /* saturate, the value is negligible */
else
        exp = maxexp - exp, man >>= exp
return man
```

The `exp = maxexp - exp; man >>= exp;` spelling is **pinned by bytes**:
`man >>= maxexp - exp` gets CSEd with the test and produces a
one-register-different, one-byte-longer function.  (`sh = maxexp - exp;
man >>= sh;` with a separate variable is equally exact.)

In FST==1 (integer UV) mode `Triangle`/`Line`/`Point` skip Float2Fix
for S and T and just double them (`v[i].s = S[i] * 2`, i.e. UV in
1/16 units becomes 1/32), converting only Q.

## Residuals

Every remaining difference is one of two RTL-level effects, and both
have the same flavour as the documented 1998-cc1plus modification:

1. **`this` is a memory-equivalent pseudo in `Triangle`** — the 1998
   object reloads `mov 0x8(%ebp),%reg` at 35 call sites and has a
   0x44-byte frame; ours keeps `this` in `%edi` with a 0x20-byte frame.
   That accounts for all 39 missing instructions.  Everything else in
   the function — statement order, operand order of the six `==`
   comparisons, the `imull` widening multiplies, the two conversion
   loops — matches instruction for instruction.
2. **`lea 0x0(,idx,4)` + `disp(tmp,base,1)`** where we emit
   `disp(base,idx,4)`, for `S[i]`/`T[i]`/`Q[i]` in `Triangle`, `Line`
   and `Primitive` (7 sites in the 1998 object, 0 in ours).  Tried and
   rejected: `int STQ[3][3]` instead of three arrays,
   `*(int *)((char *)S + i*4)`, `*(S + i)`, `-m386`, `-O2`, the stock
   (unpatched) 1998 cc1plus.

   Note that pre1.o's `MaxExp` *does* use the folded
   `0x14(%ebx,%edx,4)` form and matches byte-for-byte, so the 1998
   compiler folds when the base is a plain allocated pseudo.  The
   unfolded form appears exactly where an operand of the address is
   memory-equivalent — which is the same situation the documented
   `build_vfn_ref` modification creates deliberately
   (`add $0x8,%edx; movswl (%edx)` instead of `movswl 0x8(%edx)`).

   **Hypothesis worth testing on the cc1plus patch**: the 1998
   modification was not confined to `cp/class.c:build_vfn_ref` but was
   a general "force the address through a real value" change.  That
   would produce extra pseudos, hence extra register pressure, hence
   both this residual *and* the two spilled temporaries missing from
   `Pre1::Put` (doc/notes/pre1.md).  If `tools/gcc272/patches` grows a
   second hunk, pre1.o and pre3.o are the cheapest places to test it:
   both are otherwise instruction-for-instruction complete.

`Point`, `Sprite` and `Put` are already instruction-identical; their
only differing bytes are call/branch displacements that follow from
`Triangle`/`Line`/`Primitive` being shorter.
