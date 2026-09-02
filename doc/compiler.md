# The 1998 compiler

The 23 objects in `orig/lib/` are ELF32 i386, unstripped, and every one of
them carries

```
$ objdump -s -j .comment orig/lib/pre1.o
 0000 00474343 3a202847 4e552920 322e372e  .GCC: (GNU) 2.7.
 0010 322e3300                             2.3.
```

so to byte-compare reconstructed source against them we need a working
**g++ 2.7.2.3 producing i386 ELF**. This is how we got one, what it does and
does not reproduce, and what to watch out for.

Everything lives in `tools/gcc272/`. `tools/gcc272/README.md` is the terse
how-to-rebuild; this file is the story and the evidence.

---

## 1. What we did

### 1.1 No build — run the 1998 binaries

Debian 2.0 "hamm" (1998) is the last Debian whose default C compiler was
2.7.2.3, and it is still on `archive.debian.org`. One trap: hamm's `g++`
package is **2.90.29, i.e. egcs 1.0** — *not* 2.7.2.3. The 2.7.2.3 C++
compiler is in a separate package called **`g++272`**.

From `http://archive.debian.org/debian-archive/debian`, dist `hamm`,
arch `i386`:

| package | version | path under `dists/hamm/main/binary-i386/` |
|---|---|---|
| `g++272` | 2.7.2.3-4.8 | `devel/g++272_2.7.2.3-4.8.deb` |
| `gcc` | 2.7.2.3-4.8 | `devel/gcc_2.7.2.3-4.8.deb` |
| `cpp` | 2.7.2.3-4.8 | `interpreters/cpp_2.7.2.3-4.8.deb` |
| `libc6` | 2.0.7t-1 | `base/libc6_2.0.7t-1.deb` |
| `libc6-dev` | 2.0.7t-1 | `devel/libc6-dev_2.0.7t-1.deb` |
| `libg++272` | 2.7.2.8-0.1 | `libs/libg++272_2.7.2.8-0.1.deb` |
| `libg++272-dev` | 2.7.2.8-0.1 | `devel/libg++272-dev_2.7.2.8-0.1.deb` |
| `binutils` | 2.9.1-0.2 | `devel/binutils_2.9.1-0.2.deb` |

A hamm `.deb` is already the modern ar-archive format, so unpacking needs no
`dpkg` and no root:

```sh
ar x pkg.deb && tar xzf data.tar.gz -C root/
```

**The extracted i386 binaries run as-is.** No chroot, no era loader, no
`LD_LIBRARY_PATH` gymnastics. They ask for `/lib/ld-linux.so.2`, which on
this Void box is a symlink to `/usr/lib32/ld-linux.so.2` (glibc 2.41), and
glibc's backwards compatibility does the rest — a 1998 glibc-2.0.7-linked
binary still runs on a 2026 glibc:

```
$ root/usr/lib/gcc-lib/i486-linux/2.7.2.3/cc1plus -quiet x.i -o x.s
$ head -4 x.s
	.file	"x.i"
	.version	"01.01"
gcc2_compiled.:
	.ident	"GCC: (GNU) 2.7.2.3"
```

That made the suggested fallback (building 2.7.2.3 from source with clang
`-m32` and heavy K&R patching) unnecessary.

### 1.2 The wrappers

`tools/gcc272/g++272` and `tools/gcc272/gcc272` do not re-implement the
driver; they *drive the 1998 driver*, which is the most faithful option
because the era `specs` file then supplies the era predefines and the era
`cpp` -> `cc1plus` sequencing. Three things make that work off its home
filesystem:

* `GCC_EXEC_PREFIX=root/usr/lib/gcc-lib/` — the era driver hard-codes
  `/usr/lib/gcc-lib/`; this environment variable relocates it, so it finds
  our `cc1plus`, `cpp` and `specs` instead of the host's.
* `-B tools/gcc272/shim/` — a prefix directory holding `as` and `ld` shims
  (the era driver would otherwise run the host's `as`, which defaults to
  x86-64 and fails).
* `-nostdinc -nostdinc++` plus `-isystem` for the three era include
  directories, so nothing from the host `/usr/include` leaks in. `-isystem`
  rather than `-I` so the caller's own `-I` still comes first.

Verified working from an unrelated cwd, by absolute path, by relative path
and via `PATH`; `-c`, `-S`, `-E`, `-I`, `-D`, `-U`, `-O`, `-O2` all behave.

### 1.3 The assembler matters

The first pass used the host `as --32`. It works, but it is not
byte-faithful. Two differences:

* Modern GAS adds a `.note.gnu.property` section the 1998 objects do not
  have (suppressible with `-mx86-used-note=no`), and picks different section
  alignments.
* More importantly it fills `.align` gaps with different nops. For a 10-byte
  gap:

  ```
  era GAS 2.9.1 : 8d 76 00  8d bc 27 00 00 00 00      (3 + 7)
  host GAS 2.44 : 2e 8d b4 26 00 00 00 00  66 90      (8 + 2)
  ```

  The 1998 objects use the first form — `orig/lib/pre1.o` at 0x886 is
  literally `8d 76 00 8d bc 27 00 00 00 00`.

So `shim/as` runs **GAS 2.9.1 from the hamm `binutils` package** by default
(`GCC272_MODERN_AS=1` switches back to the host assembler). With the era
assembler the section table matches the originals exactly:
`.text .data .bss .note .rodata .comment`, `.data`/`.bss` at `2**2`,
`.note` at `2**0`, and no `.note.gnu.property`.

`shim/ld` likewise uses era `ld` 2.9.1 against the extracted glibc 2.0.7, so
`g++272 foo.cc -o foo` produces a *running* 1998 binary:

```
$ tools/gcc272/g++272 -O2 hello.cc -o hello && ./hello
hello 7
```

---

## 2. Acceptance test

Source (as specified):

```c++
struct Pre3;
struct Pre1 { Pre3 *p; int x; Pre1(Pre3 *q); void Send(); };
struct Pre3 { int a[82]; virtual void Put(Pre1 *); };
Pre1::Pre1(Pre3 *q) { p = q; x = 0; }
void Pre1::Send() { p->Put(this); }
void Pre3::Put(Pre1 *q) { a[0] = q->x; }
```

### 2.1 Default configuration

```
$ tools/gcc272/g++272 -O2 -c t.cc -o t.o
$ objdump -dtr t.o

t.o:     file format elf32-i386

SYMBOL TABLE:
00000000 l    df *ABS*	00000000 t.cc
00000000 l    d  .text	00000000 .text
00000000 l    d  .data	00000000 .data
00000000 l    d  .bss	00000000 .bss
00000000 l       .text	00000000 gcc2_compiled.
00000000 l    d  .rodata	00000000 .rodata
00000000 l    d  .note	00000000 .note
00000000 l    d  .comment	00000000 .comment
00000000 g     F .text	00000018 __4Pre1P4Pre3
00000020 g     F .text	0000001f Send__4Pre1
00000040 g     F .text	00000012 Put__4Pre3P4Pre1
00000000 g     O .rodata	00000010 _vt.4Pre3


RELOCATION RECORDS FOR [.rodata]:
OFFSET   TYPE              VALUE
0000000c R_386_32          Put__4Pre3P4Pre1


Disassembly of section .text:

00000000 <__4Pre1P4Pre3>:
   0:	55                   	push   %ebp
   1:	89 e5                	mov    %esp,%ebp
   3:	8b 55 08             	mov    0x8(%ebp),%edx
   6:	8b 45 0c             	mov    0xc(%ebp),%eax
   9:	89 02                	mov    %eax,(%edx)
   b:	c7 42 04 00 00 00 00 	movl   $0x0,0x4(%edx)
  12:	89 d0                	mov    %edx,%eax
  14:	89 ec                	mov    %ebp,%esp
  16:	5d                   	pop    %ebp
  17:	c3                   	ret
  18:	90                   	nop
  19:	8d b4 26 00 00 00 00 	lea    0x0(%esi,%eiz,1),%esi

00000020 <Send__4Pre1>:
  20:	55                   	push   %ebp
  21:	89 e5                	mov    %esp,%ebp
  23:	8b 55 08             	mov    0x8(%ebp),%edx
  26:	8b 02                	mov    (%edx),%eax
  28:	8b 88 48 01 00 00    	mov    0x148(%eax),%ecx
  2e:	52                   	push   %edx
  2f:	0f bf 51 08          	movswl 0x8(%ecx),%edx
  33:	01 d0                	add    %edx,%eax
  35:	50                   	push   %eax
  36:	8b 41 0c             	mov    0xc(%ecx),%eax
  39:	ff d0                	call   *%eax
  3b:	89 ec                	mov    %ebp,%esp
  3d:	5d                   	pop    %ebp
  3e:	c3                   	ret
  3f:	90                   	nop

00000040 <Put__4Pre3P4Pre1>:
  40:	55                   	push   %ebp
  41:	89 e5                	mov    %esp,%ebp
  43:	8b 45 08             	mov    0x8(%ebp),%eax
  46:	8b 55 0c             	mov    0xc(%ebp),%edx
  49:	8b 52 04             	mov    0x4(%edx),%edx
  4c:	89 10                	mov    %edx,(%eax)
  4e:	89 ec                	mov    %ebp,%esp
  50:	5d                   	pop    %ebp
  51:	c3                   	ret

$ objdump -s -j .rodata t.o
Contents of section .rodata:
 0000 00000000 00000000 00000000 00000000  ................
```

Against the four criteria:

1. **Mangling — PASS.** `__4Pre1P4Pre3`, `Send__4Pre1`, `Put__4Pre3P4Pre1`,
   and the vtable symbol `_vt.4Pre3`. This is the gcc 2.x non-thunk vtable
   name; `-fvtable-thunks` would give `__vt_4Pre3`.
2. **Vtable — PASS.** `_vt.4Pre3` is `0x10` = 16 bytes in `.rodata`: two
   zero words of prefix (bytes 0..7), then one 8-byte entry — `{short delta =
   0; short index = 0}` at 8..11 and a function pointer at 12..15 carrying
   the single `R_386_32` relocation to `Put__4Pre3P4Pre1`. The four zero
   bytes at 12..15 in the hexdump are the relocation's addend slot.
3. **Dispatch sequence — PASS on structure, differs in one instruction.**
   The vptr is loaded from `Pre3+0x148` (`a[82]` = 0x148 bytes, vptr last),
   entry 0 is at vtable+8, the 16-bit delta is `movswl`-ed and added to the
   object pointer, and the call goes indirect through entry+4. What differs
   is that stock 2.7.2.3 folds the `+8` into the two memory operands
   (`movswl 0x8(%ecx)` / `mov 0xc(%ecx)`) where the 1998 objects keep a
   separate `add $0x8,%eax` and use `(%eax)` / `0x4(%eax)`. Section 3 below
   is entirely about this.
4. **Constructor returns `this` — PASS.** `mov %edx,%eax` at offset 0x12,
   immediately before the epilogue.

`tools/gcc272/g++272` was exercised from a directory unrelated to
`tools/gcc272/` for all of the above, and additionally by relative path and
through `PATH`.

---

## 3. The one codegen deviation, and how it was closed

### 3.1 The symptom

Every virtual call in the 1998 objects has this shape (here
`Pre1::SendRegister`, `orig/lib/pre1.o+0xa00`):

```
 a1f:	8b 11                	mov    (%ecx),%edx
 a21:	8b 82 48 01 00 00    	mov    0x148(%edx),%eax
 a27:	83 c0 08             	add    $0x8,%eax          <-- separate add
 a2a:	51                   	push   %ecx
 a2b:	0f bf 10             	movswl (%eax),%edx        <-- no displacement
 a2e:	03 11                	add    (%ecx),%edx
 a30:	52                   	push   %edx
 a31:	8b 50 04             	mov    0x4(%eax),%edx     <-- +4, not +12
 a34:	ff d2                	call   *%edx
```

Stock 2.7.2.3 instead emits `movswl 0x8(%eax),%edx` / `mov 0xc(%eax),%edx`
and no `add`. Three bytes shorter per virtual call.

This is **systematic, not incidental**. Counting the two forms across all
23 objects:

```
dbg.o       folded=0 unfolded=1     pcalc.o   folded=0 unfolded=2
dda.o       folded=0 unfolded=3     pcrtc.o   folded=0 unfolded=47
drawprim.o  folded=0 unfolded=4     pre1.o    folded=0 unfolded=2
gpu2.o      folded=0 unfolded=4     pre3.o    folded=0 unfolded=4
gpu2reg.o   folded=0 unfolded=110   txm.o     folded=0 unfolded=3
gpu2vec.o   folded=0 unfolded=4     xif.o     folded=0 unfolded=2
```

186 virtual calls, 0 folded. The offsets vary (`add $0x10`, `$0x18`,
`$0x38`, `$0x48` …), so it is not an artefact of index 0.

Control experiment: `libg++.a` from the **same 1998 Debian `libg++272-dev`
package**, i.e. compiled in 1998 by the very binary we are running, comes out
**folded=90, unfolded=12**. So the Debian 2.7.2.3 behaved then exactly as it
behaves now, and the libgpu2 compiler was a different build.

### 3.2 What it is not

* **Not an optimisation level.** `-O0` is unfolded but has a completely
  different frame (`push %esi`/`push %ebx`, `add $0x8,%esp` after the call)
  that the originals do not have. `-O2` folds *and* keeps the object pointer
  in a register (`add %edx,%eax`) where the originals reload it from memory
  (`add (%ecx),%edx`). Only `-O` matches everything else.
* **Not a flag.** All 78 `-f` options accepted by 2.7.2.3 (extracted from
  `toplev.c`'s `f_options[]` and `cp/decl2.c`'s `lang_f_options[]`), in both
  `-f` and `-fno-` spellings, plus all 29 i386 `-m` options, were swept at
  `-O`. None produce the unfolded form. (`-fomit-frame-pointer` and
  `-fhandle-exceptions` looked like hits until the grep was corrected —
  it was matching the `add $0x8,%esp` stack pop.)
* **Not a Debian patch.** `gcc_2.7.2.3-4.8.diff.gz` touches only
  `expr.c` (a `#ifdef __PPC__` guard around `emit_stack_save` in
  `__builtin_apply`), `config/i386/next.h`, m68k/sparc configs, drivers,
  regenerated bison output, docs and objc. Nothing that can change i386
  codegen.
* **Not vanilla vs Debian.** We built a pristine FSF `gcc-2.7.2.3.tar.gz`
  cc1plus (see 3.4) configured `--host/--target=i386-pc-linux-gnu`, matching
  the `i386-pc-linux-gnu` directory the originals were built in. It folds
  too.

### 3.3 Where it happens

`cp/class.c:build_vfn_ref()` wraps the vtable-entry address in a
`save_expr()` precisely so the address is computed once. Instrumenting the
vanilla build confirms the `SAVE_EXPR` node *is* created. But the folding is
already present in the very first RTL dump (`-da`, `.rtl`), before any
optimisation pass:

```
(insn 23 21 25 (set (reg:SI 25) (mem/s:SI (plus:SI (reg:SI 24) (const_int 328)))))
(insn 25 23 27 (set (reg:SI 26) (reg:SI 25)))                 ; the SAVE_EXPR
(insn 28 27 29 (set (reg:SI 27)
        (sign_extend:SI (mem/s:HI (plus:SI (reg:SI 26) (const_int 8))))))
(insn 33 31 35 (set (reg:SI 29) (mem/s:SI (plus:SI (reg:SI 26) (const_int 12)))))
```

`expand_expr` expands the `SAVE_EXPR` with `EXPAND_SUM`, so the saved value
ends up being just the vtable pointer and the constant entry offset is
re-materialised inside each `MEM`. So this is a middle-end/`expand_expr`
behaviour, identical in Debian's binary and in vanilla — which means **the
1998 libgpu2 compiler was not a stock gcc 2.7.2.3**, despite the
`GCC: (GNU) 2.7.2.3` `.comment`. Most likely a vendor build (the tree is
named `i386-pc-linux-gnu`) carrying a local patch, or a 2.7.2.x variant that
still reports 2.7.2.3. We did not identify it.

### 3.4 A reconstructed compiler that does match

Since the era gcc runs, it can compile its own source — so building a
patched cc1plus needs **no K&R porting work at all**:

```sh
CC=tools/gcc272/gcc272 ./configure --host=i386-pc-linux-gnu --target=i386-pc-linux-gnu
GCC272_HOST_INTERP=1 make CC=$CC HOST_CC=$CC CFLAGS=-O2 LANGUAGES="c c++" cc1plus
```

`tools/gcc272/patches/01-vfn-ref-keep-entry-address.patch` is one hunk in
`build_vfn_ref` that forces the entry address through a real value (a
`NOP_EXPR` to `ptr_type_node` and back around the `save_expr`), which stops
`expand_expr` folding the offset:

```c
-    TREE_OPERAND (aref, 0) = save_expr (TREE_OPERAND (aref, 0));
+    {
+      tree vfa = TREE_OPERAND (aref, 0);
+      tree vft = TREE_TYPE (vfa);
+      vfa = save_expr (build1 (NOP_EXPR, ptr_type_node, vfa));
+      TREE_OPERAND (aref, 0) = build1 (NOP_EXPR, vft, vfa);
+    }
```

Enable it with `GCC272_1998=1` (which also implies `-m486`, see 4.1). On a
faithful reconstruction of `Pre1::SendRegister` the result is
**instruction-for-instruction and register-for-register identical to the
1998 object**:

```
        reconstruction, GCC272_1998=1 -O           orig/lib/pre1.o+0xa00
   0:  55                push   %ebp          a00: 55              push   %ebp
   1:  89 e5             mov    %esp,%ebp     a01: 89 e5           mov    %esp,%ebp
   3:  8b 4d 08          mov    0x8(%ebp),%ecx a03: 8b 4d 08       mov    0x8(%ebp),%ecx
   6:  8b 55 0c          mov    0xc(%ebp),%edx a06: 8b 55 0c       mov    0xc(%ebp),%edx
   9:  c7 41 70 01…      movl   $0x1,0x70(%ecx) a09: c7 41 74 01…  movl   $0x1,0x74(%ecx)
  10:  89 51 58          mov    %edx,0x58(%ecx) a10: 89 51 58      mov    %edx,0x58(%ecx)
  13:  8b 55 10          mov    0x10(%ebp),%edx a13: 8b 55 10      mov    0x10(%ebp),%edx
  16:  8b 45 14          mov    0x14(%ebp),%eax a16: 8b 45 14      mov    0x14(%ebp),%eax
  19:  89 51 5c          mov    %edx,0x5c(%ecx) a19: 89 51 5c      mov    %edx,0x5c(%ecx)
  1c:  89 41 60          mov    %eax,0x60(%ecx) a1c: 89 41 60      mov    %eax,0x60(%ecx)
  1f:  8b 11             mov    (%ecx),%edx    a1f: 8b 11          mov    (%ecx),%edx
  21:  8b 82 48 01…      mov    0x148(%edx),%eax a21: 8b 82 48 01… mov    0x148(%edx),%eax
  27:  83 c0 08          add    $0x8,%eax      a27: 83 c0 08       add    $0x8,%eax
  2a:  51                push   %ecx           a2a: 51             push   %ecx
  2b:  0f bf 10          movswl (%eax),%edx    a2b: 0f bf 10       movswl (%eax),%edx
  2e:  03 11             add    (%ecx),%edx    a2e: 03 11          add    (%ecx),%edx
  30:  52                push   %edx           a30: 52             push   %edx
  31:  8b 50 04          mov    0x4(%eax),%edx a31: 8b 50 04       mov    0x4(%eax),%edx
  34:  ff d2             call   *%edx          a34: ff d2          call   *%edx
  36:  89 ec             mov    %ebp,%esp      a36: 89 ec          mov    %ebp,%esp
  38:  5d                pop    %ebp           a38: 5d             pop    %ebp
  39:  c3                ret                   a39: c3             ret
```

(The `0x70` vs `0x74` is our guess at the member padding in the
reconstructed struct, not a compiler difference.)

The same acceptance snippet under `GCC272_1998=1 -O`:

```
00000020 <Send__4Pre1>:
  20:	55                   	push   %ebp
  21:	89 e5                	mov    %esp,%ebp
  23:	8b 55 08             	mov    0x8(%ebp),%edx
  26:	8b 02                	mov    (%edx),%eax
  28:	8b 88 48 01 00 00    	mov    0x148(%eax),%ecx
  2e:	83 c1 08             	add    $0x8,%ecx
  31:	52                   	push   %edx
  32:	0f bf 01             	movswl (%ecx),%eax
  35:	03 02                	add    (%edx),%eax
  37:	50                   	push   %eax
  38:	8b 41 04             	mov    0x4(%ecx),%eax
  3b:	ff d0                	call   *%eax
  3d:	89 ec                	mov    %ebp,%esp
  3f:	5d                   	pop    %ebp
  40:	c3                   	ret
```

Symbols and vtable are unchanged (`_vt.4Pre3`, 16 bytes, one `R_386_32` at
offset 12).

**This is a reconstruction, not the original compiler.** It is off by
default; the default path is the unmodified 1998 Debian binary. Use it when
you want byte-comparable virtual dispatch, and say so in any writeup.

Caveat: **the patch only holds at `-O`.** At `-O2` the extra CSE pass
re-folds the offset, so `GCC272_1998=1 -O2` gives the stock form again.
That is fine, because `-O` is what the originals were built with anyway
(see 4.2).

---

## 4. What the 1998 build used — settings to match

### 4.1 `-m486` (or an i486-configured compiler)

The function epilogue is the tell. i386 tuning emits `leave` (`c9`);
i486 tuning emits `mov %ebp,%esp; pop %ebp` (`89 ec 5d`). The originals use
the second form, so the 1998 compiler was i486-tuned.

Debian's `gcc` is configured `i486-linux`, so its baked-in default is
already right and the default wrapper path needs no flag. Our vanilla
patched cc1plus is configured `i386-pc-linux-gnu`, so `GCC272_1998=1`
adds `-m486` automatically.

### 4.2 `-O`, not `-O2`

At `-O2` gcc 2.7.2.3 keeps the object pointer in a register across the
delta add:

```
-O   : movswl 0x8(%ecx),%edx ; add (%edx),%eax     <- memory reload
-O2  : movswl 0x8(%ecx),%edx ; add %edx,%eax       <- register
```

The originals reload from memory (`add (%ecx),%edx`), i.e. `-O`.
`-O2` also changes register allocation elsewhere in the same function.

### 4.3 Exceptions OFF

gcc 2.7 spells the switch **`-fhandle-exceptions`**, not `-fexceptions`, and
it is off by default. Turning it on adds a `.gcc_except_table` section, a
`.ctors` entry, `__throw` calls and per-function EH regions:

```
with -fhandle-exceptions:
  4 .ctors            00000004
  5 .gcc_except_table 0000003c
```

None of the 23 originals have those sections, so the 1998 build had
exceptions off. The wrappers keep the default and additionally accept and
discard `-fno-exceptions` (which 2.7.2.3 does not recognise).

### 4.4 Non-thunk vtables — never pass `-fvtable-thunks`

Default (correct, matches 1998):

```
_vt.4Pre3          16 bytes   { {0,0,0}, {short delta; short index; pfn} }
call:  movswl <delta> ; add to object ptr ; call *<pfn>
```

With `-fvtable-thunks`:

```
__vt_4Pre3         12 bytes   plain function pointers
call:  mov 0x8(%ecx),%eax ; call *%eax        (no delta arithmetic)
```

Real vtables in the originals confirm the 8-byte-entry layout:
`_vt.4Pre3` = 0x10 (1 virtual), `_vt.7XWindow` = 0x50 (9 virtuals),
`_vt.8PCRTCdmy` = 0x20 (3 virtuals) — all `8 + 8*n`.

### 4.5 No RTTI

gcc 2.7.2.3 has no RTTI at all; `-fno-rtti` is not a recognised flag (the
wrappers swallow it). Source using `dynamic_cast`/`typeid` will not compile.

---

## 5. Things that will bite on larger translation units

* **`-O` only.** Use `-O`. `-O2` diverges in register allocation and undoes
  the vfn patch; `-O0` diverges in frame setup.
* **The era assembler is not optional** if you want byte comparison. Leave
  `GCC272_MODERN_AS` unset. Modern GAS differs in `.align` fill bytes,
  section alignment, and adds `.note.gnu.property`.
* **The era preprocessor is not optional either.** `cpp` 2.7.2.3 is
  traditional (non-ISO) — no `#pragma once`, no variadic macros, different
  token pasting, and it will happily produce output modern `cc1plus`
  would reject and vice-versa. The wrappers always use the era `cpp`.
* **Header set.** The wrappers use only the era `/usr/include`,
  `/usr/include/g++` (libg++ 2.7.2.8) and the gcc `include/` directory.
  Do not add host headers (`GCC272_HOSTINC=1` exists but will pull in glibc
  2.41 headers that a 1998 preprocessor cannot parse).
* **C++ dialect is pre-standard.** No namespaces worth the name, no
  `bool` as a keyword in all contexts, `for`-scope defaults to the old
  rules-with-a-warning (`flag_new_for_scope = 1`; `-ffor-scope` selects
  ANSI scoping, `-fno-for-scope` the silent old rules), templates
  are early-1990s, `<iostream.h>` not `<iostream>`, and the standard library
  is libg++ 2.7.2.8 + libstdc++ 2.7.2.8, not the SGI STL. Reconstructed
  source has to be written in that dialect.
* **`gcc2_compiled.`, `.comment`, `.note`.** Every object gets a local
  `gcc2_compiled.` symbol, and both metadata sections come out
  byte-identical to the originals, so nothing to do here — but worth knowing
  when diffing:

  ```
  ours              orig/lib/pre1.o
  .comment  00474343 3a202847 4e552920 322e372e 322e3300   ".GCC: (GNU) 2.7.2.3."
  .note     08000000 00000000 01000000 30312e30 31000000   the .version "01.01" note
  ```
* **Struct layout.** gcc 2.7 puts the vtable pointer **after** the data
  members (hence `Pre3+0x148` for `int a[82]`), which is the opposite of
  every modern ABI. Reconstructed class definitions must not assume a
  leading vptr.
* **`long long` works** (used by `Pre1::SendRegister`); it is passed as two
  stack words, low first.
* **Linking.** `g++272 foo.cc -o foo` produces a binary against 1998 glibc
  with an rpath into `tools/gcc272/root`; it runs, but the tree is not
  relocatable in that mode. `GCC272_HOST_INTERP=1` gives a relocatable
  binary against the host's 32-bit glibc instead (that is how the patched
  `cc1plus` itself is linked).

---

## 6. Sources

* `http://archive.debian.org/debian-archive/debian/dists/hamm/main/binary-i386/`
  — the eight `.deb` files listed in 1.1, kept in `tools/gcc272/debs/`.
* `http://archive.debian.org/debian-archive/debian/dists/hamm/main/source/devel/gcc_2.7.2.3-4.8.diff.gz`
  — Debian's patch set, kept in `tools/gcc272/src/` (checked, and it does not
  touch i386 codegen).
* `https://ftp.gnu.org/old-gnu/gcc/gcc-2.7.2.3.tar.gz`
  — vanilla FSF source, kept in `tools/gcc272/src/`, used only for
  `patched/`.
* `tools/gcc272/SHA256SUMS` — checksums for all of the above.

---

## 7. Follow-up: was it a vendor compiler? (Red Hat / Cygnus)

Section 3 concluded that the 1998 compiler was not stock gcc 2.7.2.3, and left
open *whose* it was. Three pieces of evidence pointed at Red Hat 4.x:

* the shipped sample binary `gpu2/i386-pc-linux-gnu/sample/smplgpu2` is
  **libc5** (`/lib/ld-linux.so.1`, `libc.so.5`, `libm.so.5`) — a 1997-era
  Red Hat 4.x box, not a glibc distribution;
* `orig/lib/libgpu2.o` alone says **`GCC: (GNU) 2.7.2.1`**; the other 22 say
  `2.7.2.3` — the version mix you would get from a box whose compiler was
  updated mid-project;
* Sony's PS2 toolchains were Cygnus-built, and Cygnus shipped locally
  patched gcc 2.7.2.x in GNUPro.

One more fingerprint clinches the distribution. The sample binary's linked-in
`libgcc.a` still carries its build path:

```
$ strings -a gpu2/i386-pc-linux-gnu/sample/smplgpu2 | grep /usr/src
/usr/src/colgate/BUILD/gcc-2.7.2.1/
```

`colgate` is Red Hat's build root for **Red Hat Linux 4.0 "Colgate"**, whose
gcc was 2.7.2.1. So the host really was a Red Hat 4.x machine and the 2.7.2.1
half of the version mix is Red Hat's own RPM.

### 7.1 Candidates tested

All from `http://ftp.icm.edu.pl/packages/linux-redhat/linux/`, kept in
`tools/gcc272/rpms/`. `.deb`-style extraction is not possible for RPMs and
neither `rpm` nor `rpm2cpio` is installed here, so `tools/gcc272/rpms/rpm2cpio.py`
(30 lines: skip the 96-byte lead, walk the two headers, gunzip the cpio
payload) does the job, piped into `cpio -idmu`.

| # | candidate | how obtained |
|---|---|---|
| 1 | Debian hamm `g++272` **2.7.2.3-4.8** | baseline, §1 |
| 2 | Red Hat 5.0 `gcc-c++-2.7.2.3-8` | RPM, runs natively (glibc) |
| 3 | Red Hat 4.2 `gcc-c++-2.7.2.1-2` | RPM — libc5, **would not run**, see 7.3 |
| 4 | gcc **2.7.2.1** rebuilt from RH 4.2's `gcc-2.7.2.1-2.src.rpm` with Red Hat's `rth-gcc-2.7.2-960814` + `gcc-2.7.2-flow` patches | built here with the era gcc as `$(CC)` |
| 5 | vanilla FSF gcc-2.7.2.3 | §3.2 |
| 6 | vanilla FSF gcc-2.7.2.3 + inverted `ADDRESS_COST` (Pentium-tuning hypothesis, 7.4) | built here |

### 7.2 Results

Test (a) is the vtable-dispatch form; (b) is `src/addrconv.c` compiled `-O`
and its `.text` compared byte-for-byte with `orig/lib/addrconv.o`
(`cmp -l | wc -l`, both 1463 bytes).

| candidate | (a) vfn form | (b) addrconv diff |
|---|---|---|
| 1 Debian hamm 2.7.2.3-4.8 | folded | **51** |
| 2 Red Hat 5.0 2.7.2.3-8 | folded | **51** |
| 4 Red Hat 4.2 2.7.2.1 (RH patches) | folded | **51** |
| 5 vanilla FSF 2.7.2.3 | folded | **51** |
| 6 vanilla + inverted `ADDRESS_COST` | folded | **51** |
| — vanilla + `patches/01-*` (§3.4) | **unfolded (matches 1998)** | 51 |

And the strongest single result of this round:

> **All five stock/vendor builds emit byte-identical `.text` for
> `addrconv.c`.** Not merely the same diff count — the same 1463 bytes.

```
o_rh50   vs hamm: IDENTICAL
o721     vs hamm: IDENTICAL      (2.7.2.1, Red Hat patches)
o_pat    vs hamm: IDENTICAL      (vanilla + vfn patch)
```

So gcc 2.7.2.x i386 codegen for straight-line C is completely stable across
vendors *and* across the 2.7.2.1 → 2.7.2.3 point releases. Red Hat's patch
sets do not touch it: the 5.0 spec applies nine patches, of which only
`rth-gcc-2.7.2-960814.diff` (Richard Henderson's, mostly Alpha) reaches
`combine.c` and `c-common.c`, and Red Hat 4.2's `gcc-2.7.2-linux.diff` turns
out to touch only `Makefile.in` and two Ada files. `cp/class.c`'s
`build_vfn_ref` is **textually identical** in 2.7.2.1 and 2.7.2.3.

### 7.3 The Red Hat 4.2 binaries would not run

They are libc5. `/lib/ld-linux.so.1` and `libc.so.5` were extracted from
`libc-5.3.12-18.5` and `ld.so-1.7.14-5` and supplied through an unprivileged
`bwrap` mount namespace (`--dir /lib --ro-bind …`), which is how a libc5
binary can be run here without root. The loader works, the binary starts —
and then every one of them dies in its first allocation:

```
gcc:  virtual memory exhausted          (the driver's xmalloc)
cc1:  <file>: Out of memory             (fopen -> malloc, ENOMEM)
```

`strace` shows the `brk()` calls succeeding and **no `open()` of the input
file at all**, i.e. `fopen` returns NULL before it ever gets to the syscall —
libc5's malloc failing. `setarch linux32 -L -R` (legacy VA layout, no
randomisation), swapping in Debian hamm's much later `libc5 5.4.38`, short vs
long paths, and an empty `/etc/ld.so.cache` all made no difference. Rather
than keep digging, candidate 4 was rebuilt from Red Hat's own source RPM,
which answers the same question. The extracted libc5 tree is kept in
`tools/gcc272/alt/rh42/libc5/` for anyone who wants another go.

### 7.4 The Pentium-tuning hypothesis, and why it is wrong

gcc 2.7.2.3's `config/i386/i386.h` has

```c
#define ADDRESS_COST(RTX) \
  ((CONSTANT_P (RTX)						\
    || (GET_CODE (RTX) == PLUS && CONSTANT_P (XEXP (RTX, 1))	\
	&& REG_P (XEXP (RTX, 0)))) ? 0				\
   : REG_P (RTX) ? 1						\
   : 2)
```

`disp(%reg)` costs **0** and a bare register costs **1** — CSE is told to
*prefer* folding a constant into an address. That looks exactly like the
knob that would explain the fold, and a Pentium-tuned vendor compiler (pgcc,
or anyone worried about AGI stalls) would plausibly invert it.

It is not the knob. Candidate 6 inverts the two costs and changes **nothing**:
still folded, still 51. That is consistent with §3.3 — the fold is already
present in the *initial* RTL emitted by `expand_expr`, before CSE runs at
all, so no back-end cost model can undo it.

This matters for the vendor hypothesis: a compiler that emits the 1998 form
must differ in the **front end or `expr.c`**, not in i386 tuning. That is a
much narrower and less likely thing for a vendor to have patched.

(No pgcc build could be obtained — it is in no Debian release and the goof.com
distribution site is gone. Cygnus GNUPro was commercial and no i386-linux-host
gcc 2.7.2.x binary or source tree surfaced. The PS2 toolchain sources that are
held locally, under `/u/aap/src/ps2rev/eegcc/`, start at `2.9-sky-990318`;
there is no 2.7.2.x SKY or EE compiler, so the ee-gcc lineage is not the
source of a patched host compiler either.)

### 7.5 The addrconv residual is a source problem, not a compiler problem

Because five independent 2.7.2.x builds agree byte-for-byte, the 51 bytes
cannot be blamed on the compiler. Looking at where they sit
(7 clusters, all inside `address_convert__8AddrConviiiiiRiN56`):

**(2) the 2D table index, offset 0x36c** — same instruction count, opposite
operand order:

```
        original (x term first)                 ours (y term first)
 36c: mov 0xc(%ebp),%edi     ; x          36c: lea 0x0(,%ebx,8),%edx   ; y*8
 36f: mov %edi,%edx                       373: and $0x10,%edx
 371: add %edi,%edx          ; x*2        376: mov 0xc(%ebp),%esi      ; x
 373: and $0xc,%edx                       379: mov %esi,%eax
 376: lea 0x0(,%ebx,8),%eax  ; y*8        37b: add %esi,%eax           ; x*2
 37d: and $0x10,%eax                      37d: and $0xc,%eax
 380: add %eax,%edx          ; y into x   380: add %edx,%eax           ; x into y
 382: mov 0x0(%edx),%eax                  382: mov 0x0(%eax),%eax
```

gcc 2.7 evaluates the two addends in source order and accumulates into the
first. So the 1998 source writes the **x-derived term first** in that index
expression; ours writes it second. Swapping the two terms in the source
should close this cluster.

**(1) the Z arm, offset 0x328** — the original keeps the value in a fresh
register:

```
 original: mov %eax,%edi ; and $0x1,%edi ; mov 0x24(%ebp),%esi ; mov %edi,(%esi)
 ours:                     and $0x1,%eax ; mov 0x24(%ebp),%esi ; mov %eax,(%esi)
```

The extra `mov` means the unmasked `(x>>6)^1` was still live in the original —
i.e. that subexpression is bound to something in the source (an extra local,
or reused) rather than consumed in place.

**(3) the `%esi`/`%edi` swaps** in the remaining clusters (0x841, 0x864, 0x920,
0x1343, 0x1398, 0x1449) are all downstream ripples of (1) and (2): the same
instructions with the two callee-saved registers exchanged.

Note this is also why the `.align` padding differs there — ours pads a
14-byte gap (`8d b4 26 …` twice) where the original pads 12 (`8d b6 00 …`
twice). Same GAS nop family, different gap, because the preceding code lengths
differ. Not an assembler discrepancy.

### 7.6 Verdict

**Stock vendor compiler: ruled out.** Every gcc 2.7.2.x obtainable —
Debian hamm 2.7.2.3, Red Hat 5.0 2.7.2.3-8, Red Hat 4.2's 2.7.2.1 (with Red
Hat's patches), and vanilla FSF 2.7.2.3 — folds the vtable entry offset into
the memory operand, and all of them produce identical code otherwise. The host
distribution *is* now firmly identified as Red Hat 4.x (the `colgate` build
path plus the libc5 sample binary), but Red Hat's compiler is not the one that
built the 23 objects.

**Custom patch: still the best explanation, and now a narrower one.** §7.4
shows the difference cannot come from i386 tuning; it has to come from the
C++ front end or `expr.c`. `tools/gcc272/patches/01-vfn-ref-keep-entry-address.patch`
is a three-line change in exactly that place and reproduces the 1998 form
exactly, which is at least a plausible shape for whatever the real change was.
Whether the 1998 box ran a locally patched cc1plus, an unreleased Cygnus
build, or something else entirely is still unresolved — but it was **not** a
stock Red Hat, Debian or FSF compiler.

**Most useful practical outcome of this round:** the addrconv residual is
compiler-invariant, so chasing it belongs in `src/addrconv.c`, not in the
toolchain. 7.5 says where to start.

### 7.7 Addendum: the genuine Red Hat 4.2 binary does run, and it confirms all of the above

The libc5 wall in 7.3 has a way through, found independently while
`test/run_libgpu2_721.sh` was being written: under libc5 on this kernel,
`fopen()` **for reading** fails, but writing is fine — so the preprocessed
source has to arrive on **stdin**. Two more things are needed:

* patch `PT_INTERP` in a private copy from `/lib/ld-linux.so.1` to the
  relative `ld1` (same length, NUL-padded) and drop an `ld1` symlink beside it;
* run it with `env -i` (a small environment) and `setarch linux32 -R`
  (randomisation off). With the full environment or with ASLR on it still
  dies in `xmalloc` — that was what defeated the earlier attempt.

Results from the real `gcc-c++-2.7.2.1-2` binary:

| test | result |
|---|---|
| vtable dispatch | **folded** (`movswl 8(%eax),%edx`) — same as everything else |
| `addrconv` `-O -m486` | **51**, and the emitted assembly is **byte-identical** to the rebuilt candidate 4 |
| `addrconv` `-O` (its own default tuning) | 1349 bytes vs the original's 1463 — size mismatch |

The last row is a useful cross-check on §4.1. Red Hat 4.2's gcc is configured
`i386-linux`, so it emits `leave` epilogues by default; `-m486` is what makes
it agree with the originals. And that shows up in the archive itself:

```
object        compiler   leave   mov %ebp,%esp
libgpu2.o     2.7.2.1        9        0        <- i386 tuning
addrconv.o    2.7.2.3        0        1        <- i486 tuning
pre1.o        2.7.2.3        0        7
pre3.o        2.7.2.3        0       11
dda.o         2.7.2.3        0       33
```

**`libgpu2.o`, the one 2.7.2.1 object, is i386-tuned; all 22 of the 2.7.2.3
objects are i486-tuned.** That is exactly the signature of a Red Hat 4.x box
(`i386-linux` gcc 2.7.2.1, `leave` by default) that later acquired a
*different*, i486-configured 2.7.2.3 — which is also the compiler with the
unexplained vtable behaviour. So the version split in the `.comment` strings
is a real toolchain change mid-project, not noise.

Practical consequence, and a useful one: `libgpu2.o` is **not** in the list of
objects containing virtual dispatch (§3.1), so the one anomaly does not apply
to it. For `src/libgpu2.c` the stock Red Hat 4.2 compiler, at plain `-O` with
its own i386 default tuning, should be exactly right:

```sh
GCC272_ALT=rh42-2721 tools/gcc272/g++272 -O -m386 -c src/libgpu2.c -o libgpu2.o
```

(`GCC272_ALT` implies `-m486`, so pass `-m386` after it to get 2.7.2.1's own
default back. `alt/rh42-2721` is the source rebuild; it emits byte-identical
assembly to the shipped Red Hat binary, and unlike it needs no libc5, no
stdin trick and no `setarch`.)

Run against the current `src/libgpu2.c` that recipe gives a `.text` of 3360
bytes with **9 `leave` epilogues — exactly matching `orig/lib/libgpu2.o`'s 9**,
against its 3350 bytes. So the tuning and the compiler are right and the
remaining 10 bytes are source, same conclusion as 7.5.
