# tools/gcc272/alt — other vendors' gcc 2.7.2.x, for comparison

These exist to answer one question: *did some vendor's gcc 2.7.2.x, out of the
box, produce the 1998 libgpu2 codegen?* The answer turned out to be **no** —
see `../../../doc/compiler.md` §7. They are kept so the negative result is
reproducible, and because `rh42-2721` is a genuine 2.7.2.1, which is what
`orig/lib/libgpu2.o` (alone among the 23 objects) was built with.

Select one with `GCC272_ALT=<name>`:

```sh
GCC272_ALT=rh50      tools/gcc272/g++272 -O -c foo.c -o foo.o
GCC272_ALT=rh42-2721 tools/gcc272/g++272 -O -c foo.c -o foo.o
```

`GCC272_ALT` swaps only `cc1plus`. The driver, `cpp`, `specs`, headers and the
assembler stay the hamm ones, so the comparison isolates the compiler proper.
It implies `-m486`, because neither alternate is i486-configured.

| name | what | source |
|---|---|---|
| `rh50` | Red Hat 5.0 `gcc-2.7.2.3-8` cc1plus, target `i386-redhat-linux`, glibc-linked. Runs natively. | `rpms/gcc-c++-2.7.2.3-8.i386.rpm` |
| `rh42-2721` | gcc **2.7.2.1** rebuilt here from Red Hat 4.2's `gcc-2.7.2.1-2.src.rpm` with RH's `rth-gcc-2.7.2-960814` and `gcc-2.7.2-flow` patches applied. | `rpms/gcc-2.7.2.1-2.src.rpm` |
| `rh42/` | the unpacked Red Hat 4.2 `gcc-2.7.2.1-2` / `gcc-c++-2.7.2.1-2` binaries. **Not runnable here** — they are libc5 (`/lib/ld-linux.so.1`) and libc5's malloc fails immediately on this kernel; that is why `rh42-2721` was rebuilt from source instead. No `exec/` dir, so `GCC272_ALT=rh42` is rejected. | `rpms/gcc-*-2.7.2.1-2.i386.rpm` |

`rh42/libc5/` holds Red Hat 4.2's `ld-linux.so.1` + `libc.so.5` in case someone
wants another go at running the libc5 binaries. What was tried and how far it
got is in `doc/compiler.md` §7.3.

Re-create everything here with `sh ../fetch-alt.sh`.
