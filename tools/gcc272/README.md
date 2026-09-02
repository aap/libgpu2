# tools/gcc272 — GNU C/C++ 2.7.2.3 for i386 ELF, on a modern host

How to re-create this directory, and how to use it.
Narrative, findings and acceptance-test output live in `../../doc/compiler.md`.

## What's here

```
g++272                    wrapper: era cpp -> era cc1plus -> era as   (C++)
gcc272                    same for C (cc1)
fetch.sh                  re-downloads and unpacks root/ from archive.debian.org
build-patched-cc1plus.sh  rebuilds patched/ (optional 1998-codegen cc1plus)
patches/                  the one-hunk source patch that build script applies
shim/as, shim/ld          assembler/linker shims the era gcc driver picks up
root/                     unpacked 1998 Debian "hamm" tree (the compiler)
patched/                  vanilla FSF gcc-2.7.2.3 cc1plus + patches/ (optional)
debs/                     the downloaded .deb files
src/                      gcc-2.7.2.3.tar.gz and the Debian source diff
SHA256SUMS                checksums of debs/ and src/
```

Nothing was installed system-wide; nothing needs root; there is no chroot.
The 1998 i386 ELF binaries run directly against the host's 32-bit glibc
(`/lib/ld-linux.so.2` -> `/usr/lib32/ld-linux.so.2`, glibc 2.41 here).

## Re-creating it

```sh
sh tools/gcc272/fetch.sh                    # root/ ; ~30 s
sh tools/gcc272/build-patched-cc1plus.sh    # patched/ ; ~2 min, optional
```

`fetch.sh` pulls these from `http://archive.debian.org/debian-archive/debian`,
distribution `hamm` (Debian 2.0, 1998), architecture i386:

| package | version | what we use from it |
|---|---|---|
| `g++272` | 2.7.2.3-4.8 | `cc1plus`, the `g++272` driver |
| `gcc` | 2.7.2.3-4.8 | `cc1`, the `gcc` driver, `specs`, `libgcc.a` |
| `cpp` | 2.7.2.3-4.8 | the era preprocessor |
| `libc6` | 2.0.7t-1 | era `ld.so`/`libc.so.6` (only needed for linking) |
| `libc6-dev` | 2.0.7t-1 | era `/usr/include`, `crt*.o`, `libc.a` |
| `libg++272` / `-dev` | 2.7.2.8-0.1 | era `/usr/include/g++`, `libg++`/`libstdc++` |
| `binutils` | 2.9.1-0.2 | era `as` and `ld` |

Note the package name: hamm's `g++` is **2.90.29 (egcs 1.0)**, not 2.7.2.3.
The 2.7.2.3 C++ compiler is in the separate **`g++272`** package.

A hamm `.deb` is an `ar` archive of `debian-binary` + `control.tar.gz` +
`data.tar.gz`, so `ar x` + `tar xzf` is enough — no `dpkg` required.

`build-patched-cc1plus.sh` additionally needs `src/gcc-2.7.2.3.tar.gz`
(`https://ftp.gnu.org/old-gnu/gcc/gcc-2.7.2.3.tar.gz`). It builds vanilla
gcc-2.7.2.3 **using the extracted era gcc as `$(CC)`**, so there is no
K&R-era porting work: the 1998 compiler compiles its own source unmodified.

## Using it

```sh
tools/gcc272/g++272 -O2 -c foo.cc -o foo.o     # C++
tools/gcc272/g++272 -O2 -S foo.cc -o foo.s     # stop at assembly
tools/gcc272/g++272 -E -DX=1 -Iinc foo.cc      # era cpp only
tools/gcc272/gcc272 -O2 -c foo.c  -o foo.o     # C
tools/gcc272/g++272 -O2 foo.cc -o foo          # link + run (era glibc)
```

Works from any cwd, via absolute path, relative path or `PATH`.

### Environment knobs

| variable | effect |
|---|---|
| `GCC272_1998=1` | use `patched/…/cc1plus` (+ implies `-m486`). Reproduces the 1998 libgpu2 virtual-call code. Only at `-O`; `-O2` undoes it. |
| `GCC272_MODERN_AS=1` | assemble with the host `as --32 -mx86-used-note=no` instead of GAS 2.9.1 |
| `GCC272_MODERN_LD=1` | link with the host `ld -m elf_i386` |
| `GCC272_HOST_INTERP=1` | record `/lib/ld-linux.so.2` and drop the rpath (relocatable binary, host glibc) |
| `GCC272_HOSTINC=1` | additionally search the host `/usr/include` |
| `GCC272_VERBOSE=1` | print the driver command line |

### Defaults, and why

* **Header path** is the era one only (`-nostdinc -nostdinc++` plus
  `-isystem root/usr/include/g++`, `root/usr/lib/gcc-lib/…/include`,
  `root/usr/include`). Your own `-I` still comes first, because the era
  directories are added with `-isystem`.
* **Assembler** is GAS 2.9.1 from hamm, not the host's GAS 2.44. The era
  assembler reproduces the 1998 `.align` nop-fill patterns and section
  layout byte-for-byte; modern GAS does not (see doc/compiler.md).
* **Vtables** are the non-thunk `_vt.<class>` form — that is the gcc 2.7
  default and matches the 1998 objects. **Never pass `-fvtable-thunks`**;
  it produces `__vt_<class>` with 4-byte entries instead.
* **Exceptions are off.** gcc 2.7 spells the switch `-fhandle-exceptions`
  (not `-fexceptions`); turning it on adds a `.gcc_except_table` section
  and `__throw` calls that the 1998 objects do not have.
* **RTTI does not exist** in gcc 2.7.2.3 at all.

### Flag translation

gcc 2.7 rejects some spellings that are reflexes today. The wrappers quietly
drop `-fno-exceptions`, `-fno-rtti`, `-fno-strict-aliasing`,
`-fno-threadsafe-statics` (all already the 2.7 behaviour) and rewrite
`-fexceptions` to `-fhandle-exceptions`. `gcc272` additionally drops
`-fno-stack-protector`, `-std=gnu89`, `-std=c89`. Everything else is passed
through untouched.
