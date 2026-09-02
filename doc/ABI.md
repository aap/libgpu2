# g++ 2.7.2.3 i386 ABI facts, as observed in the 1998 objects

Everything here was verified by disassembly of `orig/lib/*.o` (2026-09-02).
Citations are `object:function` or `object:offset`.  This is the contract
our reconstructed source must hit when compiled with the era compiler, and
the reason modern compilers cannot produce drop-in replacements for the
virtual-dispatch classes.

## Name mangling (GNU v2, pre-Itanium)

    __4Pre1P4Pre3            Pre1::Pre1(Pre3*)
    Put__4Pre1ix             Pre1::Put(int, long long)
    Put__3TXMP3DDA           TXM::Put(DDA*)
    _._11XWindowDump         XWindowDump::~XWindowDump()
    _vt.4Pre3                vtable for Pre3
    _3TXM.valid8             static member TXM::valid8
    _GLOBAL_.I.<sym>         static-constructor function

Modern `c++filt` cannot demangle these; use
`othersrc/libgpu2/notes/demangle-gcc2.py`.

## Calling convention

- cdecl, args right-to-left on the stack; `this` is the first argument
  (leftmost).  No register args.
- `long long` is passed as two 32-bit stack words (lo, hi), 4-byte aligned.
- **Constructors return `this` in %eax and callers use it**
  (`gpu2.o:__4GPU2Pciii+0x330`: the `Pre1::Pre1` return value is stored).
  A modern ctor returns void — calling one from 1998 code, or vice versa,
  silently propagates garbage.

## Vtable layout (non-thunk, `-fvtable-thunks` OFF)

`_vt.<Name>` is an array of 8-byte entries after an 8-byte zero prefix:

    struct vt_entry { short delta; short pad; void (*fn)(); };
    offset 0x00: 0, 0            (prefix, both words zero)
    offset 0x08: entry 0
    offset 0x10: entry 1 ...

Sizes observed: 0x10 (one virtual: Pre3, DDA, TXM, PCalc, MemIF, DDATXM,
PPOut, PPDDA, MemRead*, PixelBlend*), 0x18 (PCRTC: SetRegister + Resize),
0x20 (PCRTCdmy, GPU2Reg), 0x50 (Xifbase/XWindow/XWindowDump: 9 entries).

## Virtual call sequence

The caller loads the vptr, indexes the entry, sign-extends the 16-bit
`delta`, adds it to the object pointer, and calls (`pre1.o:SendData+0xde`):

    mov  (%ebx),%eax        ; next-stage object
    mov  0x148(%eax),%edx   ; its vptr (here: Pre3+0x148)
    add  $0x8,%edx          ; skip prefix -> entry 0
    push %ebx               ; argument (the sending stage)
    movswl (%edx),%eax      ; this-delta
    add  (%ebx),%eax
    push %eax               ; adjusted this
    call *0x4(%edx)

## Vptr placement

- A class that itself introduces its virtuals gets the vptr **after all
  its data members**: Pre3 +0x148/0x14c, DDA +0x250/0x254,
  PCalc +0xbfc/0xc00, MemIF +0xf8/0xfc, PCRTC +0x3c/0x40.
- TXM and PPOut have the vptr at **offset 0** — consistent with inheriting
  it from a base whose own data is empty (TXM: probably DDATXM, whose local
  vtable txm.o carries; PPOut: an interface class).  Tentative until the
  class declarations are reconstructed and byte-verified.

## Vtable emission

Vtables are emitted in every TU that needs them; some copies are global
(`_vt.4Pre3` in pre3.o), some weak, some **local** (`_vt.5PPOut`
separately in gpu2.o, dbg.o, gpu2vec.o; `_vt.6DDATXM` local in txm.o).
Distinct local copies of the "same" vtable coexist in the archive — which
copy an object uses depends on where the code was compiled.

## Static constructors

`.ctors` carries relocations to `_GLOBAL_.I.*` functions
(txm.o -> `_GLOBAL_.I._3TXM.valid8` building the 256-entry PSM validity
table, dbg.o -> `_GLOBAL_.I.DbgInit__FiPv`).  Modern ld folds `.ctors`
into `.init_array`, so they do run in our relinked binaries.  FINDINGS.md
§9's ".ctors are empty" claim is wrong.

## Inlining tells us where methods lived

`GPU2::GPU2` contains the Pre3/DDA/TXM/MemIF/PCalc/PCRTC/XWindow ctor
bodies **inlined** (vtable stores + member init in the caller) but calls
`Pre1::Pre1` and `param::param` as functions.  Inline-expanded ctors were
defined in headers; called ones in their .c files.  The same test applies
to every method: an out-of-line copy in its own object = .c definition.
(A fossil of this: the inlined TXM ctor carries an empty countdown loop,
`gpu2.o:+0x123` — some `for(...);` delay idiom preserved by -O2.)

## Floating point

i386 with x87 only.  Intermediate results live in 80-bit registers at the
compiler's whim; this is observable behaviour.  Modern comparable codegen
needs `-m32 -mno-sse` (clang) or `-m32 -mfpmath=387` (gcc), and even then
spill points differ — tier-3 byte-matching is the only full answer.
