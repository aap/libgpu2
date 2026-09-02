# xif.o - the X11 interface

`orig/lib/xif.o`, 0xf5d B .text, 0x80 B .data, 0x240 B .rodata, 28 functions,
three vtables.  src/xif.c + include/xif.h.

    GCC272_1998=1 tools/gcc272/g++272 -O -Iinclude -idirafter /usr/include \
        -c src/xif.c

`-idirafter` (not `GCC272_HOSTINC=1`) is what gets the host's X11 headers
*after* the era ones, so `<stdio.h>` still comes from libc5 and `stderr`
still expands to `&_IO_stderr_` the way the 1998 object references it.
`XLIB_ILLEGAL_ACCESS` is defined in xif.h because the 1998 `Xlib.h` had a
public `struct _XDisplay`; with it, today's X11 headers give byte-identical
field offsets (`dpy->screens` at 0x8c, `Screen` stride 80, `root` at +8,
`sizeof(XVisualInfo)` 40, `XColor` 12, `XImage` with `data` at 0x10,
`bytes_per_line` 0x28, `f.destroy_image` 0x44, `f.put_pixel` 0x4c) -
verified against the original's addressing.

## Result

* **.data byte-identical** (both dither matrices).
* **.rodata identical except one padding byte** at 0x14f - the original fills
  the gap before the vtables with 0x90, era GAS 2.9.1 fills it with 0x00
  (see "the padding byte" below).  All 12 strings, all three vtables and
  their 24 relocations are exact.
* **Symbol table set and order identical** (28 functions, the three vtables,
  the two file-static tables, every undefined).
* **13/28 functions byte-identical, 783/3739 .text bytes.**  Everything else
  is instruction-shape correct; the residuals are listed below.

Byte-identical: `SetColormap`, `highbit`, `DisplayPixel`, `ClearDisplay`,
`dither8to2`, `dither8to3`, `XWindowDump::Flush`, `XWindowDump::SetBackground`,
`~XWindowDump`, `XWindow::Flush`, `XWindow::Resize`, `XWindow::DrawPixel`
(6 arg), `~XWindow`.

No differential harness: every entry point either talks to an X server or
mutates X11 structures, so there is nothing to compare against without a
display.  Verified instead by (a) byte-matching, (b) linking a 13-object
hybrid archive that still replays every GS dump bit-identically and passes
the probe suite - which exercises xif.o's ABI (pcrtc.o constructs XWindow
and XWindowDump from the original object against our class layout).

## Class hierarchy

    class Xifbase {                     /* sizeof 4, vptr at 0 */
            virtual ~Xifbase();                 /* vt entry 0 */
            virtual void PrepareImgBuffer(int, int) = 0;        /* 1 */
            virtual void DrawPixel(int,int,int,int,int) = 0;    /* 2 */
            virtual void DrawPixel(int,int,int,int,int,int) = 0;/* 3 */
            virtual void DisplayPixel(int,int,int,int) = 0;     /* 4 */
            virtual void Resize(int, int) = 0;                  /* 5 */
            virtual void ClearDisplay() = 0;                    /* 6 */
            virtual void SetBackground(int,int,int) = 0;        /* 7 */
            virtual void Flush() = 0;                           /* 8 */
    };

`_vt.7Xifbase` (local, 9 entries: the dtor then eight `__pure_virtual`)
confirms both the count and the order; `XWindow::DrawPixel(6 args)` calling
vtable offset 0x18 (entry 2) and `XWindowDump::DisplayPixel` calling 0x48
(entry 8) pin which is which.

### XWindow (0x40)

| off | field | | off | field |
|---|---|---|---|---|
| 0x00 | vptr | | 0x24 | `char *data` |
| 0x04 | `Display *dpy` | | 0x28 | `unsigned long rmask` |
| 0x08 | `Window win` | | 0x2c | `gmask` |
| 0x0c | `XVisualInfo *vis` | | 0x30 | `bmask` |
| 0x10 | `Colormap cmap` | | 0x34 | `int rshift` |
| 0x14 | `GC gc` | | 0x38 | `gshift` |
| 0x18 | `int width` | | 0x3c | `bshift` |
| 0x1c | `int height` | | | |
| 0x20 | `XImage *img` | | | |

`rshift` is `7 - highbit(mask)`, so it is *negative* for a mask whose top bit
is above bit 7 and `DrawPixel` shifts left in that case.

### XWindowDump (0x24)

| off | field |
|---|---|
| 0x00 | vptr |
| 0x04 | `Frame2d out` (b, w, h) - what the callback gets |
| 0x10 | `Frame2d draw` (b, w, h) - what DrawPixel writes |
| 0x1c | `unsigned bg` |
| 0x20 | `void (*func)(int w, int h, unsigned *b)` |

doc/STRUCTS.md's reading of gpu2.o (`+0x4`/`+0x10` = `malloc(0x40000)`,
`+0x8/+0xc/+0x14/+0x18` = 0x100, `+0x1c` = 0, `+0x20` = a text pointer) is
exactly two `Frame2d(256, 256)` members plus the background and the callback.

### Frame2d (0x0c)

`{unsigned *b; int w, h;}` with `Frame2d(int,int)`, `~Frame2d`, `isValid`,
`Resize`, `Set`, `Copy`, `Get` - all inline, all in xif.h.

## The load-bearing line numbers

Frame2d's asserts bake `__LINE__` into the object:

| assert | line |
|---|---|
| `Frame2d::Resize`: `width > 0 && height > 0` | 139 |
| `Frame2d::Set`: `isValid(x, y)` | 146 |
| `Frame2d::Copy`: `width <= src.w && height <= src.h` | 158 |

and `__FILE__` is the bare string `"xif.h"`.  include/xif.h is laid out to
put those three asserts on those three lines; the comment blocks around
Frame2d are padding, not decoration.  The `assert` macro is written out by
hand (`__assert_fail(#e, "xif.h", __LINE__, __PRETTY_FUNCTION__)` with
`__assert_fail` declared `__attribute__((__noreturn__))`) so the file string
does not depend on which `-I` found the header; `__PRETTY_FUNCTION__` under
g++ 2.7 reproduces every one of the five function-name strings exactly
("void Frame2d::Copy(int, int, int, int, const class Frame2d &)" and all).

The `noreturn` attribute is required: without it g++ keeps the arguments
live across the `__assert_fail` call and pushes three callee-saved registers
in `Frame2d::Resize`'s callers.

## What the definition order is forced to be

g++ 2.7 emits pending inline functions LIFO at `finish_file`, so the .text
order after the last out-of-line function reads the declaration order
backwards.  From xif.o's layout the header must declare, in this order:

1. `Xifbase` (its inline dtor is emitted last of all)
2. `XWindow` - ctors `XWindow()`, `XWindow(char*)`, `XWindow(char*,int,int)`,
   then `~XWindow`, `DrawPixel(6)`, `Resize`, `Flush` inline; everything else
   out-of-line
3. `Frame2d` - ctor, `~Frame2d`, `isValid`, `Resize`, `Set`, `Copy`, `Get`
   (that order is fixed by the order the assert strings appear in .rodata)
4. `XWindowDump` - `~XWindowDump`, `PrepareImgBuffer`, `DrawPixel(5)`,
   `DrawPixel(6)`, `DisplayPixel`, `Resize`, `ClearDisplay`, `SetBackground`,
   `Flush`

and xif.c must define, in this order: `SetColormap`, `ChooseVisual`,
`OpenWindow`, `highbit`, `PrepareImgBuffer`, `DrawPixel`, `DisplayPixel`,
`ClearDisplay`, `SetBackground`, then the two `inline` dither helpers
(`dither8to3` before `dither8to2`, so they come out in the observed order).

**XWindow's inline members are emitted as ordinary globals, XWindowDump's as
weak.**  That is g++ 2.7's pre-standard interface model: the TU that owns a
class's vtable (`_vt.7XWindow` is global here, `_vt.11XWindowDump` and
`_vt.7Xifbase` are local) also owns its inlines.

## Source-shape lessons

* **The dither helpers are `inline` members taking `int *c`.**  Taking `&r`
  is what makes `DrawPixel`'s and `SetBackground`'s `r`, `g`, `b` parameters
  *addressable*, so g++ leaves them in their incoming stack slots and does
  read-modify-write there instead of promoting them to registers.  Writing
  the same code as a macro gets the arithmetic right and the storage wrong,
  and costs 50 bytes in `SetBackground` alone.
* **The dither table is indexed by byte offset**, the way addrconv's `wd`
  table is (doc/MATCHING.md): `i = (y%4)*4 + (x%4)*16;` then
  `*(int *)((char *)dm8_3 + i)`.  A plain `dm8_3[x%4][y%4]` computes the two
  terms in the other order and folds the scale into the addressing mode.
* **The compared value is evaluated before the table lookup**:
  `v = *c & 0x1f;` first, then the index, then `if (v > table)`.  Writing
  `if (table < (*c & 0x1f))` puts the load after the index and loses
  `DrawPixel` and both helpers.
* **`Frame2d::Resize` sizes the buffer from the members, not the parameters**
  (`realloc(b, h*4*w)` after `w = width; h = height;`).
* **`highbit` needs the mask in a variable** (`unsigned long mask =
  0x80000000;`): as a literal, combine folds the loop test into the sign flag
  of the shift and the function comes out six bytes short.
* Loops use `!=` (`for (r = 0; r != 8; r++)` in `SetColormap`), like the rest
  of the archive.
* `XWindowDump::ClearDisplay` is a Duff's device: a 7-case fallthrough switch
  on `n % 8` followed by an 8-at-a-time loop over `n / 8`.

## Remaining residuals

| function | ours | orig | what |
|---|---|---|---|
| `ChooseVisual` | 252 | 222 | we materialise `best`/the loop differently; the original's `XCreateColormap` relocation is one place later, so its two arms are laid out the other way round |
| `OpenWindow` | 387 | 396 | |
| `PrepareImgBuffer` | 277 | 277 | same size, register allocation |
| `DrawPixel` (5) | 540 | 540 | same size; two instructions: `and $0xe0,%dl` vs `%edx`, and `img` from a register vs memory |
| `SetBackground` | 394 | 391 | the same `%dl` narrowing, 3 bytes |
| `XWindowDump::ClearDisplay` | 202 | 193 | |
| `Frame2d::Resize` callers | 101 | 95 | instruction-identical, `width`/`height` in swapped registers |
| `XWindowDump::DisplayPixel` | 260 | 285 | |
| `XWindowDump::DrawPixel` x2 | 141 | 145 | instruction-identical, ebx/esi swapped |
| the three `XWindow` ctors | 47/49/47 | 47/49/47 | identical but for the `call OpenWindow` displacement, which only closes when every earlier function does |
| `~Xifbase` | 28 | 29 | the `__in_chrg` flag tested in memory vs in a register |

Several of these are the `lea 0(,idx,4)` + `disp(base,tmp,1)` address form
doc/MATCHING.md attributes to the 1998 compiler modification, plus the
register pressure it drags along.

## The padding byte

`.rodata` byte 0x14f - the single byte between the last string and
`_vt.11XWindowDump` - is 0x90 in the 1998 object and 0x00 in ours.  This is
not a source question: gcc emits a bare `.align 4` (verified from `-S` with
both the era Debian cc1plus and the patched one, and with the RH 4.2 and RH
5.0 alternates), and era GAS 2.9.1 fills a `.rodata` alignment with zeroes -
measured directly on a hand-written `.s`.  The 1998 objects fill it with
*code* nops: 0x90 here, `89 f6` (`mov %esi,%esi`) in memif.o and txm.o.  So
whatever assembled the 1998 archive treated `.rodata` alignment as text
alignment - one more toolchain difference to add to doc/compiler.md's list,
and the only byte of xif.o's non-text sections we do not reproduce.

## Building the hybrid

`tools/build.sh`'s era-compiler line needs the same flag to take xif:

    *) era="env GCC272_1998=1 gcc272/g++272 -O -idirafter /usr/include";;

(the `GCC272_1998=1` is harmless for the other objects and required for
xif.o's one virtual call, `XWindow::DrawPixel(6)`).  Linking against
libX11 is already in the script.

## Original oddities

* `XWindow::DrawPixel(x, y, r, g, b, a)` **drops the alpha** and forwards to
  the five-argument overload - and does it through the vtable, not directly,
  so a subclass overriding only `DrawPixel(5)` still gets called.
* `XWindow::dither8to2`/`dither8to3` are emitted but **never called** by
  anything in the archive: the two call sites are inlined, and no other
  object references them.
* `SetBackground` dithers with the matrix cell for (0,0) - i.e. it always
  uses `dm8_3[0][0] == 0`, which makes the test `(c & 0x1f) > 0`.  So a
  background colour whose low five bits are non-zero always rounds *up*.
* `Frame2d::Copy` clips the destination but only *asserts* on the source, so
  a too-small source aborts rather than being clipped.
* `Frame2d::Resize` reallocates unconditionally and never rechecks
  `isValid`, so `XWindowDump::PrepareImgBuffer(0, n)` aborts on the assert
  rather than returning.
