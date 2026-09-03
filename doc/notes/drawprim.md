# drawprim.o - grfw immediate-mode vertex helpers

Five tiny functions that let Sony's host "grfw" graphics framework feed
its vertices straight into the GS model: `grfwSwitchVertex(Vertex0,
Vertex1, Vertex2)` and `grfwSwitchTriangleRasterlizer(DrawTriangle)`
(sic - the misspelling is grfw's) are installed by `GPU2Reg::GPU2Reg()`
(gpu2reg.o), and everything funnels through `pGPU2Reg->Put()`.

Reconstruction: src/drawprim.c, include/drawprim.h.  Not on any replay
path; tested together with gpu2reg by test/run_gpu2reg.sh.

## _grfwVertex

Host-defined vertex record, layout pinned by the field accesses
(names guessed; the double z is certain - `fldl 0x8(%edx)`):

    0x00 float x, y;      /* window coords, converted to 12.4 */
    0x08 double z;        /* depth, truncated to 24 bits */
    0x10 float r, g, b, a;/* 0..255; a rescaled to GS 0..128 */
    0x20 float s, t, q;   /* passed as raw IEEE bits */
    0x2c float f;         /* fog, truncated to 8 bits */

## The kick protocol

`PutVertex(tme, v)` (static) emits per vertex:

    RGBAQ (0x01) = (int)r&0xff | g<<8 | b<<16 | (((int)a<<7)/255)<<24
                   | FtoI(q)<<32
    ST    (0x02) = FtoI(s) | FtoI(t)<<32          only when tme
    XYZF2 (0x04) = (int)(16*x)&0xffff | (int)(16*y)<<16
                   | ((int)z&0xffffff)<<32 | ((int)f&0xff)<<56

Note the alpha rescale `a*128/255` and that XYZF2 carries *raw window
coordinates* - grfw is expected to have applied its own transform; the
model's XYOFFSET then applies as usual.

`Vertex0(type, flag, v)` opens a primitive: PRIM (0x00) = trifan (5),
or tristrip (4) when `type == 1`, with IIP/TME/FGE/AA1 from flag bits
0-3 - **ABE (PRIM bit 6) is never driven**, and FST cannot be set, so
grfw geometry is always STQ-mapped and never alpha-blended unless PRMODE
is switched in by console.  Then the first vertex is sent.
`Vertex1`/`Vertex2` send subsequent vertices (`flag>>1 & 1` = TME
again).  The type/flag bits are extracted with *unsigned* shifts.

`DrawTriangle` is empty and `DrawLine` prints
"You must write DrawLine routine!" once (a function-local
`static int already`, .bss) - the rasterizer plug-in slots are parked:
triangles rasterise through the model's own vertex-kick pipeline, and
lines were simply never implemented on this path.

## already.826

The one symbol-table difference of the reconstruction: the original's
local static is `already.826`, ours is `already.4`.  The suffix is g++
2.7's private-name counter, which advances by 2 for every function body
compiled in the TU *including header inlines expanded at parse time*
(measured: base 2, +2 per body).  826 therefore says DrawLine was
preceded by ~412 inline function bodies from drawprim.c's include graph
- Sony's grfw/jtcl headers dragged in a substantial C++ class library
that is lost.  Our include graph has exactly one prior inline (FtoI), so
the honest number is 4.  Reproducing 826 would mean padding a header
with 411 dummy inlines; not done.

## Codegen notes

The three Put calls carry the same 1998 argument-presaturation shapes as
gpu2reg.o (see doc/notes/gpu2reg-matching.md).  Vertex1/Vertex2 and
DrawTriangle are byte-identical; DrawLine differs only in .bss-vs-.data
placement history (now .bss, identical bar the suffix); PutVertex and
Vertex0 are mnemonic-shape-equal with register/slot renaming from the
unreproduced compiler mod.  drawprim.o's .rodata differs from the
original only in the two alignment pad bytes before the 16.0f constant
(1998 GAS `89 f6` code-nop fill vs era GAS zero fill - the known xif.o
residual class).
