# dbg.o - the pixel watchpoint

`orig/lib/dbg.o`, 506 B .text, 144 B .rodata, 0x14 B .bss, 4 B .ctors.
**src/dbg.c compiles to a byte-identical `.o` (cmp of the whole file).**

    GCC272_1998=1 tools/gcc272/g++272 -O -Iinclude -c src/dbg.c

## What it is

A debugging tap the model calls on every pixel write.  The host program
points it at a GPU2 and a screen position; when a write lands on that
pixel the current Pre3 vertex queue and the PCalc z/rgba slopes are
printed to stdout.  Nothing else in the library reads it: the only
in-archive user is `memory.o` (undefined `DbgWatch__Fiii`), which calls
`DbgWatch(0, x, y)` right after computing a write address in
`FBConfig::WritePixel` and friends.  `DbgInit`/`DbgMode` are exported for
the application and are called from nowhere in libgpu2.a.

## Interface (include/dbg.h)

    DbgInit__FiPv     void DbgInit(int mode, void *p)
    DbgMode__Fi       void DbgMode(int mode)
    DbgWatch__Fiii    void DbgWatch(int type, int x, int y)
    Dump__3Dbg        void Dbg::Dump()
    _GLOBAL_.I.DbgInit__FiPv        .ctors entry, runs the Dbg ctor

`DbgInit`'s first argument and `DbgWatch`'s first argument are **both
unused** by the 1998 code - see "original oddities" below.

## class Dbg (0x14, file-static object `debug` in .bss)

| off | field | evidence |
|---|---|---|
| 0x00 | `int mode` | `DbgMode` stores its arg; `DbgWatch` tests it |
| 0x04 | `int x` | `DbgWatch` compares arg 2 |
| 0x08 | `int y` | `DbgWatch` compares arg 3 |
| 0x0c | `Pre3 *pre3` | `Dump` reads `+0x70` (vertex array) and `+0x10c` |
| 0x10 | `PCalc *pcalc` | `Dump` reads `+0xb48..+0xb88` |

The constructor is inline (no `__3Dbg` symbol) and zeroes only **four** of
the five members, in **reverse declaration order**:

    Dbg() { pre3 = 0; y = 0; x = 0; mode = 0; }

Both facts are forced by the bytes of `_GLOBAL_.I.DbgInit__FiPv`
(stores to +0xc, +0x8, +0x4, +0x0 in that order, nothing to +0x10); a
member-initialiser list `: mode(0), x(0), y(0), pre3(0)` compiles to the
opposite order and does not match.  Reverse-order zeroing turns out to be
a Sony house style - `GPU2::GPU2` zeroes the inlined DDA ctor's fields
+0x240, +0x1ec, +0x1e8, +0x1e4 the same way.

## `DbgInit`'s argument

    void DbgInit(int mode, void *p)
    {
            GPU2 *gpu;
            debug.x = ((DbgPos*)p)->x;
            debug.y = ((DbgPos*)p)->y;
            gpu = ((DbgPos*)p)->gpu;
            debug.pre3  = gpu->pp->pre3;
            debug.pcalc = gpu->pp->pcalc;
    }

`p` is a `{int x; int y; GPU2 *gpu;}`.  The double dereference
`(*(p+8))[1]` / `(*(p+8))[2]` pins it: the pointer at `p+8` is chased to
its first word (GPU2+0x00 = the 0x10-byte front-end block) and then to
that block's +0x04 and +0x08 - exactly the Pre3* and PCalc* slots
doc/STRUCTS.md derived from `GPU2::GPU2`.  **This is independent
confirmation of the front-end block layout** (pre1, pre3, pcalc, ppout).

## What `Dump` prints, and the PCalc slope block it discovers

    %d\n                                                    pre3->type (+0x10c)
    p%d |%05x %05x %08x |%02x %02x %02x %02x| %04x %04x %04x\n
            i, v[i].x, v[i].y, v[i].z, v[i].r, v[i].g, v[i].b, v[i].a,
            v[i].s, v[i].t, v[i].q                          for i = 0..2
    d[zrgba]x %09x %05x %05x %05x %05x\n
    d[zrgba]y %09x %05x %05x %05x %05x\n

The vertex loop reads `pre3 + 0x70 + i*0x30` at field offsets
0x00 0x04 0x08 0x10 0x14 0x18 0x1c 0x24 0x28 0x2c - the `Vertex` layout
include/pre3.h already documents, and the `d[zrgba]` label names the
colour fields r,g,b,a in that order.  (Note the printed Z is only the low
word of pre3.h's `long long z`, so dbg.c's local view declares `int z;
int zh;`.)

**New PCalc knowledge** (not in doc/STRUCTS.md before): PCalc carries two
identical 0x28-byte slope blocks, the x-derivatives at **+0xb48** and the
y-derivatives at **+0xb70**, with

    +0x00 z    +0x0c a    +0x10 r    +0x14 g    +0x18 b

i.e. **a before r,g,b** - the opposite of `Vertex`'s r,g,b,a.  Offsets
0x04/0x08/0x1c/0x20/0x24 of the block are not printed and stay unknown.
Handy for the pcalc attack.

## Why PPOut/PPDDA turn up in dbg.o

`.rodata` ends with two file-local vtables, `_vt.5PPOut` (-> the weak
`Put__5PPOutP5PCalc`) and `_vt.5PPDDA` (-> `__pure_virtual`), and dbg.o
has no code that touches either class.  Reason: g++ 2.7 marks a class's
vtable used while it *parses an inline member body that stores the vptr*,
i.e. as soon as a header defines an inline constructor for a class whose
virtuals are all inline - emission then follows in every TU that includes
the header, as a **local** symbol, dragging the inline virtual in as a
weak function.  So dbg.c includes the header that defines

    class PPDDA { virtual void Put(PCalc *p) = 0; };
    class PPOut : public PPDDA {
            DDA *dda;
            PPOut(DDA *d) { dda = d; }
            void Put(PCalc *p) { dda->Put(p); }
    };

and that inline ctor is exactly what `GPU2::GPU2` expands at gpu2.o+0x19c
(`new(8)`, store `_vt.5PPOut`, store the DDA*).  gpu2.o and gpu2vec.o
carry the same pair of local vtables for the same reason.  Verified
experimentally: merely declaring the classes emits nothing; adding the
inline ctor emits both vtables plus the weak `Put`, in the observed
`.rodata` order (PPOut first).

`PPOut::Put` also re-confirms the ABI: it loads DDA's vptr at **+0x250**,
skips the 8-byte prefix, sign-extends the 16-bit this-delta and calls
through entry 0.

## Source-shape residuals that were forced by bytes

1. **`Vertex *v = pre3->v;` as the first statement of `Dump`.**  Without
   it the loop reloads `this->pre3` every iteration and indexes
   `0x70+f(%edx,%ecx,1)`; with it the pointer is materialised once at
   function entry (before the first `printf`, so it is *not* loop-invariant
   motion - it is a real local) and the loop indexes `f(%ebx,%edx,1)`.
   This is the addrconv "explicit index variable" lesson again.
2. **`PCalc *volatile pcalc;`** - the original reloads `this->pcalc` for
   every one of the ten printf arguments; a plain member is CSE'd into one
   load and the function comes out 24 bytes (8 loads x 3 B) short.  The
   volatile qualifier is the only construct found that reproduces it; it
   is behaviour-neutral here.  Ruled out: `-fvolatile` (changes the
   `add $0x70` to a `lea` and still CSEs), `-O2`, `-m386`, gcc 2.7.2.1
   (rh42) and the RH 5.0 cc1plus.  The *true* source construct may have
   been something else with the same effect; treat the qualifier as a
   marker, not as established history.
3. **`DbgWatch` materialises the comparison into a variable.**  The
   original does `xor %eax,%eax / cmp / cmp / inc %eax / test %eax,%eax`,
   which a plain `if (a && b && c)` never produces.  Any of
   `hit = debug.x == x && debug.y == y; if (hit) ...`, the same inside an
   `if (debug.mode) { }` block, or an inline `debug.Hit(x, y)` predicate
   reproduces it byte for byte; src/dbg.c uses the early-return form
   because it also explains the otherwise pointless `type` parameter
   (a filter that was cut).

## Original oddities

* `DbgInit(int mode, void *p)` **never uses its first argument** - the
  mode has to be set separately through `DbgMode`.  Dead parameter.
* `DbgWatch(int type, int x, int y)` **never uses `type`** either, and
  memory.o always passes `0`.  Whatever distinguished FB from ZB writes
  was dropped before shipping.
* The `Dbg` constructor leaves `pcalc` uninitialised (it is in .bss so it
  is zero anyway, but the asymmetry is real: four of five members).
* `Dbg::Dump` dereferences `pre3`/`pcalc` unconditionally, so calling
  `DbgMode(1)` without a preceding `DbgInit` crashes the model.

## Local class views

src/dbg.c defines its own `Pre3`, `PCalc`, `DDA`, `PP`, `GPU2`, `Vertex`
and `Slope` because include/pre3.h (read-only, owned by the pre1/pre3
work) declares a *stand-in* `PCalc` with no fields, and include/gpu2.h a
different minimal `GPU2`; including either here would clash.  The public
include/dbg.h therefore carries only the three free-function prototypes,
which is all memory.c needs.  When pcalc.h/gpu2.h grow real declarations,
dbg.c's private block can be replaced by them - the offsets above are the
contract.

## Oracle (hybrid replay)

A 13-object hybrid archive (addrconv libgpu2 pre1 pre3 slong div txm_div
texfunc param clut bitblt dbg xif, built in an isolated farm outside
tools/) replays bit-identically to the pure-Sony build:

    out/r614   9c7c73b156f8664633055e0300990a82
    out/o519   9fbc3187d1f98e0dff84e5d9aa5689df
    tools/probe                0 failures

and every out/Ridge Racer V dump swept (22 of them, including the 13 MB
143000/143006 streams) ends with the same 4 MB VRAM md5 as the render the
pure-Sony build stored beside it.
